#include <u.h>
#include <libc.h>
#include <draw.h>
#include <bio.h>
#include "dat.h"
#include "fns.h"

enum{
	Vertsz = 4,
	Sectsz = 26,
	Sidesz = 30,
	Linesz = 14,
	Thingsz = 10
};

Vertex *verts;
int nverts;
Sector *sects;
int nsects;
Side *sides;
int nsides;
Line *lines;
int nlines;
Thing *things;
int nthings;

#define	PBIT32(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24

static void
loadthings(char *f)
{
	int n;
	vlong l;
	char *s;
	uchar u[Thingsz];
	Biobuf *bf;
	Thing *p;
	static u32int sel = CFthing;

	s = smprint("%s/things", f);
	bf = Bopen(s, OREAD);
	if(bf == nil)
		sysfatal("loadthings: %r");
	free(s);
	l = filelen(Bfildes(bf));
	if(l % sizeof u != 0)
		sysfatal("invalid THINGS lump");
	n = l / sizeof u;
	things = emalloc(n * sizeof *things);
	Blethal(bf, nil);
	for(p=things; Bread(bf, u, sizeof u) == sizeof u; p++, sel++){
		if(nthings++ == n)
			sysfatal("loadthings: overflow\n");
		p->i = nthings;
		p->x = (s16int)(u[0] | u[1]<<8);
		p->y = -(s16int)(u[2] | u[3]<<8);
		p->an = (s16int)(u[4] | u[5]<<8);
		p->type = (s16int)(u[6] | u[7]<<8);
		p->f = (s16int)(u[7] | u[8]<<8);
		PBIT32(p->sel, sel);
	}
	Bterm(bf);
}

static void
loadlines(char *f)
{
	int n;
	vlong l;
	char *s;
	uchar u[Linesz];
	Biobuf *bf;
	Line *p;
	static u32int sel = CFline;

	s = smprint("%s/linedefs", f);
	bf = Bopen(s, OREAD);
	if(bf == nil)
		sysfatal("loadlines: %r");
	free(s);
	l = filelen(Bfildes(bf));
	if(l % sizeof u != 0)
		sysfatal("invalid LINEDEFS lump");
	n = l / sizeof u;
	lines = emalloc(n * sizeof *lines);
	Blethal(bf, nil);
	for(p=lines; Bread(bf, u, sizeof u) == sizeof u; p++, sel++){
		if(nlines++ == n)
			sysfatal("loadsides: overflow\n");
		p->i = nlines;
		/* FIXME: are these supposed to be sign extended? */
		p->vid = (s16int)(u[0] | u[1]<<8);
		p->v = verts + p->vid;
		p->wid = (s16int)(u[2] | u[3]<<8);
		p->w = verts + p->wid;
		p->f = (s16int)(u[4] | u[5]<<8);
		p->type = (s16int)(u[6] | u[7]<<8);
		p->tag = (s16int)(u[8] | u[9]<<8);
		/* FIXME: if == (s16int)-1, dont set pointer */
		p->rid = (s16int)(u[10] | u[11]<<8);
		p->r = sides + p->rid;
		p->lid = (s16int)(u[12] | u[13]<<8);
		p->l = sides + p->lid;
		PBIT32(p->sel, sel);
	}
	Bterm(bf);
}

static void
loadsides(char *f)
{
	int n;
	vlong l;
	char *s;
	uchar u[Sidesz];
	Biobuf *bf;
	Side *p;

	s = smprint("%s/sidedefs", f);
	bf = Bopen(s, OREAD);
	if(bf == nil)
		sysfatal("loadsides: %r");
	free(s);
	l = filelen(Bfildes(bf));
	if(l % sizeof u != 0)
		sysfatal("invalid SIDEDEFS lump");
	n = l / sizeof u;
	sides = emalloc(n * sizeof *sides);
	Blethal(bf, nil);
	for(p=sides; Bread(bf, u, sizeof u) == sizeof u; p++){
		if(nsides++ == n)
			sysfatal("loadsides: overflow\n");
		p->i = nsides;
		p->x = (s16int)(u[0] | u[1]<<8);
		p->y = -(s16int)(u[2] | u[3]<<8);
		memcpy(p->high, u+4, 8);
		memcpy(p->low, u+12, 8);
		memcpy(p->mid, u+20, 8);
		p->sid = (s16int)(u[28] | u[29]<<8);
		p->s = sects + p->sid;
	}
	Bterm(bf);
}

static void
loadsects(char *f)
{
	int n;
	vlong l;
	char *s;
	uchar u[Sectsz];
	Biobuf *bf;
	Sector *p;

	s = smprint("%s/sectors", f);
	bf = Bopen(s, OREAD);
	if(bf == nil)
		sysfatal("loadsects: %r");
	free(s);
	l = filelen(Bfildes(bf));
	if(l % sizeof u != 0)
		sysfatal("invalid SECTORS lump");
	n = l / sizeof u;
	sects = emalloc(n * sizeof *sects);
	Blethal(bf, nil);
	for(p=sects; Bread(bf, u, sizeof u) == sizeof u; p++){
		if(nsects++ == n)
			sysfatal("loadsects: overflow\n");
		p->i = nsects;
		p->y = (s16int)(u[0] | u[1]<<8);
		p->x = -(s16int)(u[2] | u[3]<<8);
		memcpy(p->fflat, u+4, 8);
		memcpy(p->cflat, u+12, 8);
		p->lum = (s16int)(u[20] | u[21]<<8);
		p->type = (s16int)(u[22] | u[23]<<8);
		p->tag = (s16int)(u[24] | u[25]<<8);
	}
	Bterm(bf);
}

static void
loadverts(char *f)
{
	int n;
	vlong l;
	char *s;
	uchar u[Vertsz];
	Biobuf *bf;
	Vertex *p;
	static u32int sel = CFvert;

	s = smprint("%s/vertexes", f);
	bf = Bopen(s, OREAD);
	if(bf == nil)
		sysfatal("loadverts: %r");
	free(s);
	l = filelen(Bfildes(bf));
	if(l % sizeof u != 0)
		sysfatal("invalid VERTEXES lump");
	n = l / sizeof u;
	verts = emalloc(n * sizeof *verts);
	Blethal(bf, nil);
	for(p=verts; Bread(bf, u, sizeof u) == sizeof u; p++, sel++){
		if(nverts++ == n)
			sysfatal("loadverts: overflow\n");
		p->i = nverts;
		p->x = (s16int)(u[0] | u[1]<<8);
		p->y = -(s16int)(u[2] | u[3]<<8);
		PBIT32(p->sel, sel);
	}
	Bterm(bf);
}

void
load(char *f)
{
	if(f == nil)
		f = ".";
	loadverts(f);
	loadsects(f);
	loadsides(f);
	loadlines(f);
	loadthings(f);
}
