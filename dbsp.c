#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include "dat.h"
#include "fns.h"

static void
usage(void)
{
	fprint(2, "%s [mapdir]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	Mouse m;
	Point mo;
	double f, vx, vy;
	Rune r;

	ARGBEGIN{
	default:
		usage();
	}ARGEND
	load(*argv);
	buildnodes();
	threadexitsall(nil);
}
