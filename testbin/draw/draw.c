/*
 * Program to test execv() without fork()
 *
 * Kuei Sun <kuei.sun@utoronto.ca>
 *
 * University of Toronto, 2013
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

static void next(const char * x, const char * s, int n, int i)
{
	char nc[] = { '0' + n, '\0' };
	char ic[] = { '0' + i + 1, '\0' };
	char * argv[] = {
		(char *)x, 
		(char *)s,
		(char *)"--", 
		(char *)nc, 
		(char *)ic,
		NULL,
	};
	execv(x, argv);
	err(1, "execv() failed\n");
}

static void box(const char * x, const char * s, int n, int i)
{
	int j;
	for ( j = 0; j < n; j++ )
		putchar('*');
	putchar('\n');
	if ( i < n )
		next(x, s, n, i);
}

static void pyramid(const char * x, const char * s, int n, int i)
{
	const int z = 2*i - 1;
	int j;
	for ( j = i; j < n; j++ )
		putchar(' ');
	for ( j = 0; j < z; j++ )
		putchar('*');
	putchar('\n');
	if ( i < n )
		next(x, s, n, i);
}

static void triangle(const char * x, const char * s, int n, int i)
{
	int j;
	for ( j = 0; j < i; j++ )
		putchar('*');
	putchar('\n');
	if ( i < n )
		next(x, s, n, i);
}

static struct cmd {
	const char * name;
	void (* func)(const char *, const char *, int, int);
} table[] = {
	{ "box", box },
	{ "pyramid", pyramid },
	{ "triangle", triangle },
};

static void usage(const char * progname)
{
	const int count = sizeof(table)/sizeof(struct cmd);
	int j;
	
	printf("usage: %s SHAPE NUM\n", progname);
	printf("       NUM  : from 1 to 9\n");
	printf("       SHAPE: ");
	for ( j = 0; j < count; j++ )
		printf("%s ", table[j].name);
	printf("\n");
	exit(1);
}

static void draw(const char * x, const char * s, int n, int i)
{
	const int count = sizeof(table)/sizeof(struct cmd);
	int j;
	
	for ( j = 0; j < count; j++ ) {
		if ( !strcmp(s, table[j].name) ) {
			table[j].func(x, s, n, i);
			exit(0);
		}
	}
	
	printf("%s: unknown shape %s\n", x, s);
	usage(x);	
}

int
main(int argc, char *argv[])
{
	int i, n;

	if ( argc == 0 ) {
		printf("Warning: argc is 0. Drawing box 5.\n");
		draw("draw", "box", 5, 1);
	}
	else if ( argc == 3 ) {
		n = atoi(argv[2]);
		
		if ( n <= 0 || n > 9 ) {
			printf("%s: NUM must be between 1 and 9\n", argv[0]);
			usage(argv[0]);
		}
		
		draw(argv[0], argv[1], n, 1);
	}
	else if ( argc == 5 && !strcmp(argv[2], "--") ) {
		i = atoi(argv[4]);
		n = atoi(argv[3]);
		draw(argv[0], argv[1], n, i);
	}
	else {
		usage(argv[0]);
	}

	return 0;
}
