// fiend.h                                        Stan Eisenstat (06/17/20)
// Header file for C implementation of fiend

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>


// Write message to stderr using format FORMAT
#define WARN(format,...) fprintf (stderr, "fiend: " format "\n", __VA_ARGS__)

// Write message to stderr using format FORMAT and exit.
#define DIE(format,...)  WARN(format,__VA_ARGS__), exit (EXIT_FAILURE)
