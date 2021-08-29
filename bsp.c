#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "fns.h"

/* a grid 8 is used for maps; points are considered on a line if they are
 * within 8px of it; this accounts for floating error */

typedef struct Bsp Bsp;
typedef struct Divline Divline;
typedef struct Node Node;

enum{
	Maxv = 0x7fffffff,
};

struct Divline{
	Point p;
	float dx;
	float dy;
};
struct Bsp{
	Seg *s;
	Divline;
	Bsp *side[2];
};
Seg *segs;

struct Node{
	short x;
	short y;
	short dx;
	short dy;
	short bound[2][4];
	ushort child[2];
};
Node *nodes;
int nnodes;

static float bbox[4];

int
subsect(Seg *s)
{
	USED(s);
	return 0;
}

int
mknodes(Bsp *b, short box[4])
{
	int i, f;
	short sbox[2][4];

	if(b->s != nil){
		f = subsect(b->s);
		for(i=0; i<4; i++)
			box[i] = bbox[i];
		return f | 1<<15;
	}
	/* ... */
	return 0;
}

static void
divline(Divline *d, Seg *s)
{
	d->p = s->p1;
	d->dx = s->p2.x - s->p1.x;
	d->dy = s->p2.y - s->p1.y;
}

/* fractional intercept point along first vector */
static float
InterceptVector(Divline *v2, Divline *v1)
{
	float frac, num, den;

	den = v1->dy * v2->dx - v1->dx * v2->dy;
	if(den == 0.)
		sysfatal("InterceptVector: parallel");
	num = (v1->p.x - v2->p.x) * v1->dy + (v2->p.y - v1->p.y) * v1->dx;
	frac = num / den;
	if(frac <= 0. || frac >= 1.)
		sysfatal("InterceptVector: intersection outside line");
	return frac;
}

static int
ptside(Point *p, Divline *l)
{
	float dx, dy, left, right, a, b, c, d;

	if(l->dx == 0){
		if (p->x > l->p.x - 2 && p->x < l->p.x + 2)
			return -1;
		if (p->x < l->p.x)
			return l->dy > 0;
		return l->dy < 0;
	}
	if(l->dy == 0){
		if(p->y > l->p.y - 2 && p->y < l->p.y + 2)
			return -1;
		if(p->y < l->p.y)
			return l->dx < 0;
		return l->dx > 0;
	}
	dx = l->p.x - p->x;
	dy = l->p.y - p->y;
	a = l->dx * l->dx + l->dy * l->dy;
	b = 2 * (l->dx*dx + l->dy * dy);
	c = dx * dx + dy * dy - 2 * 2;	/* 2 unit radius */
	d = b * b - 4 * a * c;
	if(d > 0)
		return -1; /* colinear: within 4px of line */
	dx = p->x - l->p.x;
	dy = p->y - l->p.y;
	left = l->dy * dx;
	right = dy * l->dx;
	if(abs(left - right) < 0.5)	/* allow slope on line */
		return -1;
	if(right < left)
		return 0;	/* front */
	return 1;	/* back */
}

/* if the line is colinear, it will be placed on the front side if going
 * the same direction as the dividing line */
static int
lineside(Seg *wl, Divline *bl)
{
	int s1, s2;
	float dx, dy;

	s1 = ptside(&wl->p1, bl);
	s2 = ptside(&wl->p2, bl);
	if(s1 == s2){	/* colinear: see if the directions are the same */
		if(s1 == -1){
			dx = wl->p2.x - wl->p1.x;
			dy = wl->p2.y - wl->p1.y;
			if(fsign(dx) == fsign(bl->dx)
			&& fsign(dy) == fsign(bl->dy))
				return 0;
			return 1;
		}
		return s1;
	}
	if(s1 == -1)
		return s2;
	if(s2 == -1)
		return s1;
	return -2;	/* line must be split */
}

/* truncate given world line to the front side of the divline and return
 * the cut off back side in a newly allocated world line */
static Seg *
cutline(Seg *s, Divline *bl)
{
	int side, ofs;
	float f;
	Point intr;
	Seg *p;
	Divline d;

	divline(&d, s);
	p = emalloc(sizeof *p);
	*p = *s;
	f = InterceptVector(&d, bl);
	intr.x = d.p.x + fround(d.dx * f);
	intr.y = d.p.y + fround(d.dy * f);
	ofs = s->ofs + fround(f * sqrt(d.dx * d.dx + d.dy * d.dy));
	side = ptside(&s->p1, bl);
	if(side == 0){	/* line starts on front side */
		s->p2 = intr;
		p->p1 = intr;
		p->ofs = ofs;
	}else{	/* line starts on back side */
		s->p1 = intr;
		s->ofs = ofs;
		p->p2 = intr;
	}
	return p;
}

static void
addnode(Seg **ss, int *n, Seg *s, Seg *p)
{
	if(s - *ss >= *n){
		*ss = erealloc(*ss, (*n+64) * sizeof **ss, *n * sizeof **ss);
		*n += 64;
	}
	*s = *p;
}

/* actually split line list as EvaluateLines predicted */
static void
split(Seg *ss, Seg *s, Seg **front, Seg **back)
{
	int side, be, fe;
	Seg *p, *q, *b, *f;
	Divline d;

	fe = be = ss->e - s;
	f = *front = emalloc(fe * sizeof **front);
	b = *back = emalloc(be * sizeof **back);
	for(p=ss; p<ss->e; p++){
		side = p == s ? 0 : lineside(p, &d);
		switch(side){
		case 0: addnode(front, &fe, f++, p); break;
		case 1: addnode(back, &be, b++, p); break;
		case -2:
			q = cutline(p, &d);
			addnode(front, &fe, f++, p);
			addnode(back, &be, b++, q);
			break;
		}
	}
	(*front)->e = f;
	(*back)->e = b;
}

/* grade quality of a split along given line for the current list of lines.
 * evaluation is halted as soon as it is determined that a better split
 * already exists. a split is good if it divides the lines evenly without
 * cutting many lines. a horizontal or vertical split is better than a sloped
 * split. the lower the returned value, the better. if the split line does
 * not divide any of the lines at all, Maxv is returned */
static int
evalsplit(Seg *ss, Seg *s, int bestv)
{
	int side, s1, s2, v;
	Divline d;
	Seg *p;

	divline(&d, s);
	for(p=ss, v=s1=s2=0; p<ss->e; p++){
		side = p == s ? 0 : lineside(p, &d);
		switch(side){
		case -2: s2++; /* wet floor */
		case 0: s1++; break;
		case 1: s2++; break;
		}
		v = (s1 > s2 ? s1 : s2) + (s1 + s2 - (p - ss)) * 8;
		if(v > bestv)
			return v;
	}
	if(s1 == 0 || s2 == 0)
		return Maxv;
	return v;
}

/* recursively partition seg list */
static Bsp *
bsp(Seg *ss)
{
	int v, bestv;
	Bsp *b;
	Seg *s, *bests, *front, *back;

	b = emalloc(sizeof *b);
	/* find the best line to partition on */
	for(s=ss, bestv=Maxv, bests=nil; s<ss->e; s++){
		v = evalsplit(ss, s, bestv);
		if(v < bestv){
			bestv = v;
			bests = s;
		}
	}
	/* if none of the lines should be split, the remaining lines
	 * are convex, and form a terminal node */
	if(bestv == Maxv){
		b->s = ss;
		return b;
	}
	/* divide the line list into two nodes along the best split line */
	divline(b, bests);
	split(ss, bests, &front, &back);
	/* recursively divide the lists */
	b->side[0] = bsp(front);
	b->side[1] = bsp(back);
	return b;
}

static void
mkseg(void)
{
	Line *l;
	Seg *s;

	/* FIXME: free all nodes and segs first */
	segs = emalloc(nsides * sizeof *segs);
	for(s=segs, l=lines; l<lines+nlines; s++, l++){
		s->p1 = *l->v;
		s->p2 = *l->w;
		s->l = l;
		s->s = l->r;
		if(l->f & LFtwosd){
			s++;
			s->p1 = *l->v;
			s->p2 = *l->w;
			s->l = l;
			s->s = l->l;
		}
	}
	segs->e = s;
}

void
buildnodes(void)
{
	Bsp *b;
	short bounds[4];

	mkseg();
	b = bsp(segs);
	nnodes = nsides;
	nodes = emalloc(nnodes * sizeof *nodes);
	mknodes(b, bounds);
	/* FIXME: free everything */
}
