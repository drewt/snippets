/* Copyright 2013 Drew Thoreson */

/*
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>
 */

/* 
 * deltalist.c
 *
 * This file implements a delta list (backed by a hash table).  This is a
 * linked list in which each member of the list has an associated time-to-live,
 * with the property that updating the timers is O(1).  This is accomplished by
 * storing at each node the delta relative to the previous node in the list.
 * That is, for a node with index N and time-to-live T(N), the delta D(N) is
 * equal to T(N) - T(N - 1), where T(-1) = 0.  Or equavalently, D(N) is equal
 * to T(N) minus D(n) for all 0 <= n < N (this is how deltas are actually
 * calculated).  Each time interval or "tick", the delta of the first node is
 * decremented.  When it reaches 0, a callback is executed and the node is
 * removed from the list.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "deltalist.h"

struct delta_node {
	const data_t *data;
	unsigned int delta;         // delta for delta list
	struct delta_node *ht_next; // hash table next pointer
	struct delta_node *dl_next; // delta list next pointer
	struct delta_node *dl_prev; // delta list prev pointer
};

/*
 * Finds the struct delta_node associated with a given element, if that element
 * exists in the table.  If the element does not exist, NULL is returned. If
 * `prev' is not NULL, then prev will be set to the previous element in the
 * hash table bucket when this function returns.
 */
static struct delta_node *get_node(struct delta_list *table,
		const data_t *data, struct delta_node **prev)
{
	unsigned long index;
	struct delta_node *it, *last;

	index = table->hash(data) % HT_SIZE;

	last = NULL;
	for (it = table->table[index]; it; it = it->ht_next) {
		if (table->equals(it->data, data))
			break;
		last = it;
	}

	if (prev)
		*prev = last;

	return it;
}

/*
 * Inserts a node into a bucket in the hash table.
 */
static void hash_insert(struct delta_list *table, struct delta_node *node)
{
	unsigned long index = table->hash(node->data) % HT_SIZE;

	node->ht_next = table->table[index];
	table->table[index] = node;
}

/*
 * Inserts a node into the delta list.
 */
static void dl_insert_node(struct delta_list *table, struct delta_node *node)
{
	node->delta = table->interval;

	if (!table->delta_head) {
		table->delta_head = table->delta_tail = node;
		node->dl_prev = NULL;
	} else {
		node->delta -= table->delta;

		table->delta_tail->dl_next = node;
		node->dl_prev = table->delta_tail;
		table->delta_tail = node;
	}
	node->dl_next = NULL;

	table->delta = table->interval;
}

/*
 * Removes a node from the main linked list (but not the hash table).
 */
static void dl_remove_node(struct delta_list *table, struct delta_node *node)
{
	if (node->dl_next) {
		node->dl_next->delta += node->delta;
		node->dl_next->dl_prev = node->dl_prev;
	} else {
		table->delta -= node->delta;
	}

	if (node->dl_prev)
		node->dl_prev->dl_next = node->dl_next;
	else
		table->delta_head = node->dl_next;
}

/*
 * Removes an element from the table.  Returns 0 on success, or -1 if the given
 * element is not in the table.
 */
static int delta_delete(struct delta_list *table, const data_t *data)
{
	unsigned long index;
	struct delta_node *node, *prev;

	if (!(node = get_node(table, data, &prev)))
		return -1;

	/* remove from hash table */
	if (prev) {
		prev->ht_next = node->ht_next;
	} else {
		index = table->hash(data) % HT_SIZE;
		table->table[index] = node->ht_next;
	}

	/* remove from delta list */
	dl_remove_node(table, node);

	table->size--;
	table->free((data_t*)node->data);
	free(node);

	return 0;
}

/*
 * Increases "time" by one tick, removing expired nodes when appropriate.
 */
static void delta_tick(struct delta_list *table)
{
	data_t *tmp_data;

	pthread_mutex_lock(&table->lock);

	if (!table->delta_head) {
		pthread_mutex_unlock(&table->lock);
		return;
	}

	table->delta--;
	table->delta_head->delta--;

	/* remove any expired elements */
	while (table->delta_head && !table->delta_head->delta) {
		tmp_data = (data_t*) table->delta_head->data;

		table->act(tmp_data);

		delta_delete(table, tmp_data);
	}
	pthread_mutex_unlock(&table->lock);
}

/*
 * Clock thread: calls the delta_tick() function every table->resolution
 * seconds.
 */
static _Noreturn void *clock_thread(void *data)
{
	struct delta_list *table = data;
	unsigned int left;

	pthread_detach(pthread_self());

	for (;;) {
		for (left = table->resolution; left; left = sleep(left));
			delta_tick(table);
	}
}

void delta_init(struct delta_list *table)
{
	pthread_t tid;

	if (pthread_mutex_init(&table->lock, NULL))
		perror("pthread_mutex_init");
	if (pthread_create(&tid, NULL, clock_thread, table))
		perror("pthread_create");
}

/*
 * Inserts an element into the list IFF an element corresponding to the
 * argument isn't already in the list.
 */
void delta_insert(struct delta_list *table, const data_t *data)
{
	struct delta_node *node, *prev;

	pthread_mutex_lock(&table->lock);

	if (!(node = get_node(table, data, &prev))) {
		node = malloc(sizeof(struct delta_node));
		node->data = data;
		hash_insert(table, node);
		dl_insert_node(table, node);
		table->size++;
	}

	pthread_mutex_unlock(&table->lock);
}

/*
 * Relocates a node to the end of the list if the data corresponding to the
 * argument is already in the list; otherwise allocates a new node and puts it
 * at the end of the list.
 */
int delta_update(struct delta_list *table, const data_t *data)
{
	struct delta_node *node, *prev;
	int rc;

	pthread_mutex_lock(&table->lock);

	if ((node = get_node(table, data, &prev))) {
		dl_remove_node(table, node);
		rc = 1;
	} else {
		node = malloc(sizeof(struct delta_node));
		node->data = data;
		hash_insert(table, node);
		table->size++;
		rc = 0;
	}
	dl_insert_node(table, node);

	pthread_mutex_unlock(&table->lock);

	return rc;
}

/*
 * Thread-safe interface to delta_delete().
 */
int delta_remove(struct delta_list *table, const data_t *data)
{
	int rc;

	pthread_mutex_lock(&table->lock);
	rc = delta_delete(table, data);
	pthread_mutex_unlock(&table->lock);

	return rc;
}

/*
 * Returns true if the given element exists in the table, or false if it does
 * not.
 */
int delta_contains(struct delta_list *table, const data_t *data)
{
	pthread_mutex_lock(&table->lock);
	struct delta_node *rv = get_node(table, data, NULL);
	pthread_mutex_unlock(&table->lock);

	return rv ? 1 : 0;
}

/*
 * Returns the element in the table equal to the given value (equal being
 * defined by the function dh_equals) if such an element exists.  Otherwise
 * returns NULL.
 */
const data_t *delta_get(struct delta_list *table, const data_t *data)
{
	struct delta_node *node;

	pthread_mutex_lock(&table->lock);
	node = get_node(table, data, NULL);
	pthread_mutex_unlock(&table->lock);

	return node ? node->data : NULL;
}

/*
 * Empties the table.
 */
void delta_clear(struct delta_list *table)
{
	struct delta_node *it, *tmp;

	pthread_mutex_lock(&table->lock);

	it = table->delta_head;
	while (it) {
		tmp = it;
		it = it->dl_next;
		free(tmp);
	}

	table->size = 0;
	table->delta = 0;
	table->delta_head = NULL;
	table->delta_tail = NULL;

	for (int i = 0; i < HT_SIZE; i++)
		table->table[i] = NULL;

	pthread_mutex_unlock(&table->lock);
}

/*
 * Calls the function `fun' on each element in the list.  A non-zero return
 * value from `fun' is taken to indicate that iteration should cease.
 */
void delta_foreach(struct delta_list *table,
		int (*fun)(const data_t *it, void *arg), void *arg)
{
	struct delta_node *it;

	pthread_mutex_lock(&table->lock);
	for (it = table->delta_head; it; it = it->dl_next) {
		if (fun(it->data, arg))
			break;
	}
	pthread_mutex_unlock(&table->lock);
}

/*
 * Returns the size of the given list.
 */
unsigned int delta_size(struct delta_list *table)
{
	unsigned int rv;
	pthread_mutex_lock(&table->lock);
	rv = table->size;
	pthread_mutex_unlock(&table->lock);
	return rv;
}
