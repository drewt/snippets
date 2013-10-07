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

#ifndef _PSNET_DELTALIST_H_
#define _PSNET_DELTALIST_H_

#ifndef HT_SIZE
#define HT_SIZE 10
#endif

/*
 * The delta list stores pointers to data_t.  This may be changed to another
 * data type if a bit of type-safety is in order.
 */
typedef void data_t;

struct delta_list {
	unsigned int resolution;       // seconds per tick
	unsigned int interval;         // ticks per time-to-live
	unsigned int size;             // number of elements in the list
	unsigned int delta;            // sum of all individual deltas

	struct delta_node *delta_head; // head of the delta list
	struct delta_node *delta_tail; // tail of the delta list

	/* functions that operate on data_t */
	unsigned long (* const hash)(const data_t*);
	int (* const equals)(const data_t*,const data_t*);
	void (* const act)(const data_t*);
	void (* const free)(data_t*);

	pthread_mutex_t lock;

	struct delta_node *table[HT_SIZE]; // memory for the hash table
};

void delta_init(struct delta_list *table);
void delta_insert(struct delta_list *table, const data_t *data);
int delta_update(struct delta_list *table, const data_t *data);
int delta_remove(struct delta_list *table, const data_t *data);
int delta_contains(struct delta_list *table, const data_t *data);
const data_t *delta_get(struct delta_list *table, const data_t *data);
void delta_clear(struct delta_list *table);
void delta_foreach(struct delta_list *table,
		int (*fun)(const data_t *it, void *arg), void *arg);
unsigned int delta_size(struct delta_list *table);
#endif
