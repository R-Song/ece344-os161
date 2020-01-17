/*
 * say.h
 *
 * The say function provides atomic and formatted printing, unlike printf
 *
 * Kuei Sun <kuei.sun@mail.utoronto.ca>
 *
 * University of Toronto, 2017
 */

#ifndef SAY_H
#define SAY_H

#include <sys/wait.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/*
 * Use this instead of just calling printf so we know each printout
 * is atomic; this prevents the lines from getting intermingled.
 */
static inline void say(const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	write(STDOUT_FILENO, buf, strlen(buf));
}

#endif

