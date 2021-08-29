#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "fns.h"

int
fsign(float f)
{
	return f < 0. ? -1 : f > 0.;
}

float
fround(float x)
{
	if(x > 0.){
		if(x - (int)x < 0.1)
			return (int)x;
		else if(x - (int)x > 0.9)
			return (int)x + 1;
		else
			return x;
	}
	if((int)x - x < 0.1)
		return (int)x;
	else if((int)x - x > 0.9)
		return (int)x - 1;
	return x;
}

vlong
filelen(int fd)
{
	vlong l;
	Dir *d;

	d = dirfstat(fd);
	if(d == nil)
		sysfatal("filelen: %r");
	l = d->length;
	free(d);
	return l;
}

void *
erealloc(void *p, ulong n, ulong oldn)
{
	if((p = realloc(p, n)) == nil)
		sysfatal("realloc: %r");
	setrealloctag(p, getcallerpc(&p));
	if(n > oldn)
		memset((uchar *)p + oldn, 0, n - oldn);
	return p;
}

void *
emalloc(ulong n)
{
	void *p;

	if((p = mallocz(n, 1)) == nil)
		sysfatal("emalloc: %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}
