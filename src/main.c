/*
 * TCHunt-ng - reveal (possibly) encrypted files stored on a filesystem.
 * Copyright (C) 2016, 2017  CodeWard.org
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <magic.h>
#include <unistd.h>

#include "strolldir.h"
#include "test_magic.h"
#include "test_chidist.h"

#define TCHUNTNG_VERSION "1.2"

enum
{
	EXIT_NOTFOUND = 2,
	EXIT_SIGNAL = 3
};

static struct args
{
	int recursive;
	int quiet;
	int noatime;
} arg;

static int sig_int = 0;

static void
interrupt (int signo)
{
	sig_int = signo;
	signal (signo, SIG_DFL);
}

static void
usage (const char *p)
{
	fprintf (stdout, "Usage: %s [options] <file>\n\n\
Options:\n\
 -r  recursively traverse a directory\n\
 -q  treat no results as success\n\
 -v  show version information\n", p);
}

static void
version_info (void)
{
	fprintf (stdout, "TCHunt-ng %s\n", TCHUNTNG_VERSION);
}

static int
scan_dir (const char *p, struct args *arg, const char *dirname)
{
	stroller_t dir;
	const char *cat;
	struct file_list_path *path_iter;
	const struct file_list *files;
	struct testmagic testmagic;
	int exitno, has_file;

	exitno = EXIT_SUCCESS;
	has_file = 0;

	if ( strolldir_open (&dir, dirname) != 0 ){
		fprintf (stderr, "%s: %s\n", p, strerror (errno));
		exitno = EXIT_FAILURE;
		goto cleanup;
	}

	if ( testmagic_init (&testmagic, TESTMAGIC_FLAGS) == -1 ){
		fprintf (stderr, "%s: %s\n", p, testmagic_error (&testmagic));
		exitno = EXIT_FAILURE;
		goto cleanup;
	}

	do {
		if ( strolldir_scan (&dir) != 0 ){
			fprintf (stderr, "%s: '%s': %s\n", p, strolldir_getdir (&dir), strerror (errno));

			// Terminate if the error is something severe rather than access
			// permissions violation.
			if ( errno != EACCES && errno != ENOENT ){
				exitno = EXIT_FAILURE;
				goto cleanup;
			}
		}

		files = strolldir_getfiles (&dir);

		for ( path_iter = files->head; path_iter != NULL && !sig_int; path_iter = path_iter->next ){

			switch ( testmagic_test (&testmagic, path_iter->path, &cat) ){
				case -1:
					fprintf (stderr, "%s: '%s': %s\n", p, path_iter->path, testmagic_error (&testmagic));
					exitno = EXIT_FAILURE;
					goto cleanup;

				case 0:
					// Read a next file...
					continue;

				case TMAGIC_CAT_DATA:
					// Follow up with other tests...
					break;

				default:
					goto test_success;
			}

			switch ( testchidist_x2 (path_iter->path) ){
				case -1:
					fprintf (stderr, "%s: '%s': %s\n", p, path_iter->path, strerror (errno));
					exitno = EXIT_FAILURE;
					goto cleanup;

				case 0:
					// Read a next file...
					continue;

				case 1:
					break;
			}

test_success:
			has_file = 1;
			fprintf (stdout, "%s [%s]\n", path_iter->path, cat);
		}

	} while ( strolldir_nextdir (&dir) && !sig_int );

	if ( ! has_file ){
		if ( ! arg->quiet )
			exitno = EXIT_NOTFOUND;
	}

	if ( sig_int )
		exitno = EXIT_SIGNAL;

cleanup:
	strolldir_close (&dir);
	testmagic_free (&testmagic);

	return exitno;
}

static int
scan_file (const char *p, struct args *arg, const char *filename)
{
	struct testmagic testmagic;
	const char *cat;
	int exitno;

	exitno = EXIT_SUCCESS;

	if ( testmagic_init (&testmagic, TESTMAGIC_FLAGS) == -1 ){
		fprintf (stderr, "%s: %s\n", p, testmagic_error (&testmagic));
		exitno = EXIT_FAILURE;
		goto cleanup;
	}

	switch ( testmagic_test (&testmagic, filename, &cat) ){
		case -1:
			fprintf (stderr, "%s: '%s': %s\n", p, filename, testmagic_error (&testmagic));
			exitno = EXIT_FAILURE;
			goto cleanup;

		case 0:
			exitno = EXIT_NOTFOUND;
			goto cleanup;

		case TMAGIC_CAT_DATA:
			// Follow up with other tests...
			break;

		default:
			goto test_success;
	}

	switch ( testchidist_x2 (filename) ){
		case -1:
			fprintf (stderr, "%s: '%s': %s\n", p, filename, strerror (errno));
			exitno = EXIT_FAILURE;
			goto cleanup;

		case 0:
			exitno = EXIT_NOTFOUND;
			goto cleanup;

		case 1:
			break;
	}

test_success:
	fprintf (stdout, "%s [%s]\n", filename, cat);

cleanup:
	testmagic_free (&testmagic);

	if ( exitno == EXIT_NOTFOUND && arg->quiet )
		exitno = EXIT_SUCCESS;
	
	return exitno;
}

int
main (int argc, char *argv[])
{
	struct stat file_stat;
	int exitno, c, is_dir;

	exitno = EXIT_SUCCESS;
	is_dir = 0;

	memset (&arg, 0, sizeof (struct args));

	signal (SIGTERM, interrupt);
	signal (SIGINT, interrupt);

	while ( (c = getopt (argc, argv, "rqv")) != -1 ){
		switch ( c ){
			case 'r':
				arg.recursive = 1;
				break;

			case 'q':
				arg.quiet = 1;
				break;

			case 'v':
				version_info ();
				exitno = EXIT_SUCCESS;
				return exitno;

			case '?':
				exitno = EXIT_FAILURE;
				return exitno;
		}
	}

	if ( (argc - optind) == 0 ){
		usage (argv[0]);
		exitno = EXIT_FAILURE;
		return exitno;
	}

	if ( stat (argv[optind], &file_stat) == -1 ){
		fprintf (stderr, "%s: %s: %s\n", argv[0], argv[optind], strerror (errno));
		exitno = EXIT_FAILURE;
		return exitno;
	}

	switch ( file_stat.st_mode & S_IFMT ){
		case S_IFDIR:
			is_dir = 1;
			break;
	}

	if ( is_dir ){
		if ( arg.recursive ){
			exitno = scan_dir (argv[0], &arg, argv[optind]);
		} else {
			fprintf (stderr, "%s: omitting directory '%s'\n", argv[0], argv[optind]);
			exitno = EXIT_FAILURE;
		}
	} else {
		exitno = scan_file (argv[0], &arg, argv[optind]);
	}

	return exitno;
}

