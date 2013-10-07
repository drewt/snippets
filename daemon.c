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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * Do all the unix magic necessary to become a background process, and
 * optionally redirect stdout and stderr to a log file.
 */
void daemonize(const char *log_file)
{
	pid_t pid, sid;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (pid > 0)
		exit(EXIT_SUCCESS);

	umask(0);

	if (log_file != NULL) {
		freopen(log_file, "w", stdout);
		freopen(log_file, "w", stderr);
	} else {
		fclose(stdout);
		fclose(stderr);
	}
	fclose(stdin);

	sid = setsid();
	if (sid == -1) {
		perror("setsid");
		exit(EXIT_FAILURE);
	}

	if (chdir("/") == -1) {
		perror("chdir");
		exit(EXIT_FAILURE);
	}
}
