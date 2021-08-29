#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>
#include "dat.h"
#include "fns.h"

/* FIXME: inserting/modifying vertices/things: snap to grid is essential */
/* FIXME: need a node builder to test shit */

enum{
	Cbg,
	Cwall,
	Cline,
	Cthing,
	Cvert,
	Cgrid,
	Ctext,
	Cend,

	Nview = 1000,
	Nzoom = 300,
};

static int viewdiv = Nzoom;
static int grunit = 128;
static int skipms, mode, join;
static Image *col[Cend], *fb, *fbsel, *fbselc;
static Point center, view;
static Keyboardctl *kc;
static Mousectl *mc;

Image *
eallocimage(Rectangle r, ulong chan, int repl, ulong c)
{
	Image *i;

	if((i = allocimage(display, r, chan, repl, c)) == nil)
		sysfatal("allocimage: %r");
	return i;
}

Point
viewpt(Point p)
{
	double f;

	f = (double)viewdiv / Nview;
	p.x = view.x + p.x * f;
	p.y = view.y + p.y * f;
	return p;
}

Point
abspt(Point p)
{
	double f;

	f = (double)viewdiv / Nview;
	p.x = (p.x - view.x) / f;
	p.y = (p.y - view.y) / f;
	return p;
}

void
drawgrid(void)
{
	Point p, v;

	p = Pt(0, 0);
	for(;;){
		v = viewpt(p);
		if(v.x >= fb->r.max.x && v.y >= fb->r.max.y)
			break;
		line(fb, Pt(0, v.y), Pt(Dx(fb->r), v.y), 0, 0, 0, col[Cgrid], ZP);
		line(fb, Pt(v.x, 0), Pt(v.x, Dy(fb->r)), 0, 0, 0, col[Cgrid], ZP);
		p.x += grunit;
		p.y += grunit;
	}
	p = Pt(0 - grunit, 0 - grunit);
	for(;;){
		v = viewpt(p);
		if(v.x <= fb->r.min.x && v.y <= fb->r.min.y)
			break;
		line(fb, Pt(0, v.y), Pt(Dx(fb->r), v.y), 0, 0, 0, col[Cgrid], ZP);
		line(fb, Pt(v.x, 0), Pt(v.x, Dy(fb->r)), 0, 0, 0, col[Cgrid], ZP);
		p.x -= grunit;
		p.y -= grunit;
	}
}

void
doline(Point m1, Point m2, int type, uchar *sel)
{
	int et;
	Image *c;

	c = col[type];
	et = type == Cthing && mode == 2 || type != Cthing && mode == 1 ? Endarrow : 0;
	line(fb, m1, m2, 0, et, 0, c, ZP);
	loadimage(fbselc, fbselc->r, sel, 4);
	line(fbsel, m1, m2, 0, et, 0, fbselc, ZP);
}

void
doellipse(Point p, int r, int type, uchar *sel)
{
	fillellipse(fb, p, r, r, col[type], ZP);
	loadimage(fbselc, fbselc->r, sel, 4);
	fillellipse(fbsel, p, r, r, fbselc, ZP);
}

void
drawlines(void)
{
	Line *l;
	Point m1, m2;

	for(l=lines; l<lines+nlines; l++){
		m1 = viewpt(*l->v);
		m2 = viewpt(*l->w);
		if(!ptinrect(m1, fb->r) && !ptinrect(m2, fb->r)
		&& !((m1.x >= 0 && m1.x < Dx(fb->r) && (m1.y < 0 || m1.y >= Dy(fb->r)))
		|| (m2.x >= 0 && m2.x < Dx(fb->r) && (m2.y < 0 || m2.y >= Dy(fb->r)))))
			continue;
		doline(m1, m2, l->f & LFimpass ? Cwall : Cline, l->sel);
	}

}

void
drawvertices(void)
{
	Vertex *v;
	Point p;
	int r;

	r = mode==0 ? 2 : 1;
	for(v=verts; v<verts+nverts; v++){
		p = viewpt(*v);
		if(!ptinrect(p, fb->r))
			continue;
		doellipse(p, r, Cvert, v->sel);
	}
}

void
drawthings(void)
{
	Thing *t;
	Point m1, m2;

	for(t=things; t<things+nthings; t++){
		m1 = viewpt(*t);
		if(!ptinrect(m1, fb->r))
			continue;
		if(mode != 2)
			doellipse(m1, 1, Cthing, t->sel);
		else{
			m2 = t->an == 0 ? (Point){m1.x+1, m1.y}
				: t->an == 45 ? (Point){m1.x+1, m1.y-1}
				: t->an == 90 ? (Point){m1.x, m1.y-1}
				: t->an == 135 ? (Point){m1.x-1, m1.y-1}
				: t->an == 180 ? (Point){m1.x-1, m1.y}
				: t->an == 225 ? (Point){m1.x-1, m1.y+1}
				: t->an == 270 ? (Point){m1.x, m1.y+1}
				: t->an == 315 ? (Point){m1.x+1, m1.y+1}
				: m1;
			doline(m1, m2, Cthing, t->sel);
		}
	}
}

void
redraw(void)
{
	draw(fb, fb->r, col[Cbg], nil, ZP);
	draw(fbsel, fbsel->r, col[Cbg], nil, ZP);
	drawgrid();
	drawlines();
	drawvertices();
	drawthings();
	string(fb, Pt(0,0), col[Ctext], Pt(0,0), font, join ? "join" : "split");
	draw(screen, screen->r, fb, nil, ZP);
	flushimage(display, 1);
}

void
drawstats(Point p)
{
	char buf[128];
	uchar sel[4];
	u32int x;
	Line *l;
	Thing *t;
	Side *d;
	Sector *s;
	Vertex *v;

	unloadimage(fbsel, rectaddpt(fbselc->r, p), sel, sizeof sel);
	x = sel[3] << 24 | sel[2] << 16 | sel[1] << 8 | sel[0];
	draw(screen, screen->r, fb, nil, ZP);
	p = abspt(p);
	sprint(buf, "%d,%d", p.x & ~(grunit-1), p.y & ~(grunit-1));
	p = Pt(screen->r.max.x - strlen(buf) * font->width,
		screen->r.max.y - font->height);
	string(screen, p, col[Ctext], Pt(0,0), font, buf);
	if(x == 0)
		goto end;
	p = Pt(screen->r.min.x, screen->r.max.y - font->height * 5);
	switch(x & 7<<21){
	case CFline:
		l = lines + (x & 0xffff);
		sprint(buf, "(%d,%d %d,%d len N) type %d tag %d",
			l->v->x, l->v->y, l->w->x, l->w->y, l->type, l->tag);
		string(screen, p, col[Ctext], Pt(0,0), font, buf);
		p.y += font->height;
		d = l->r;
		sprint(buf, "front: %-8s %-8s %-8s ofs %d,%d",
			d->high, d->mid, d->low, d->x, d->y);
		string(screen, p, col[Ctext], Pt(0,0), font, buf);
		p.y += font->height;
		s = d->s;
		sprint(buf, "floor %d %-8s ceiling %d %-8s (height %d) light %d type %d tag %d",
			s->x, s->fflat, s->y, s->cflat, s->y - s->x,
			s->lum, s->type, s->tag);
		string(screen, p, col[Ctext], Pt(0,0), font, buf);
		p.y += font->height;
		if((l->f & LFtwosd) == 0)
			goto end;
		d = l->l;
		sprint(buf, "back:  %-8s %-8s %-8s ofs %d,%d",
			d->high, d->mid, d->low, d->x, d->y);
		string(screen, p, col[Ctext], Pt(0,0), font, buf);
		p.y += font->height;
		s = d->s;
		sprint(buf, "floor %d %-8s ceiling %d %-8s (height %d) light %d type %d tag %d",
			s->x, s->fflat, s->y, s->cflat, s->y - s->x,
			s->lum, s->type, s->tag);
		string(screen, p, col[Ctext], Pt(0,0), font, buf);
		break;
	case CFthing:
		t = things + (x & 0xffff);
		p.y += font->height * 4;
		sprint(buf, "%d,%d ∠ %d type %d flags %#08ux",
			t->x, t->y, t->an, t->type, t->f);
		string(screen, p, col[Ctext], Pt(0,0), font, buf);
		break;
	case CFvert:
		v = verts + (x & 0xffff);
		p.y += font->height * 4;
		sprint(buf, "%d,%d", v->x, v->y);
		string(screen, p, col[Ctext], Pt(0,0), font, buf);
		break;
	}
end:
	flushimage(display, 1);
}

void
initfb(int v)
{
	Rectangle r;

	center = divpt(subpt(screen->r.max, screen->r.min), 2);
	if(v)
		view = center;
	r = rectsubpt(screen->r, screen->r.min);
	freeimage(fb);
	fb = eallocimage(r, screen->chan, 0, DNofill);
	freeimage(fbsel);
	fbsel = eallocimage(r, XRGB32, 0, DNofill);
	redraw();
	skipms++;
}

void
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
	if(initdraw(nil, nil, "dmap") < 0)
		sysfatal("initdraw: %r");
	load(*argv);
	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	col[Cbg] = display->black,
	col[Cwall] = eallocimage(Rect(0,0,1,1), screen->chan, 1, 0xccccccff);
	col[Cline] = eallocimage(Rect(0,0,1,1), screen->chan, 1, 0x5f5f5fff);
	col[Cthing] = eallocimage(Rect(0,0,1,1), screen->chan, 1, DGreen);
	col[Cvert] = eallocimage(Rect(0,0,1,1), screen->chan, 1, DGreyblue);
	col[Cgrid] = eallocimage(Rect(0,0,1,1), screen->chan, 1, 0x101010ff);
	col[Ctext] = eallocimage(Rect(0,0,1,1), screen->chan, 1, 0x884400ff);
	fbselc = eallocimage(Rect(0,0,1,1), XRGB32, 1, DNofill);
	initfb(1);
	vx = view.x - center.x;
	vy = view.y - center.y;

	Alt a[] = {
		{mc->resizec, nil, CHANRCV},
		{mc->c, &m, CHANRCV},
		{kc->c, &r, CHANRCV},
		{nil, nil, CHANEND}
	};
	for(;;){
		switch(alt(a)){
		case 0:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			initfb(0);
			break;
		case 1:
			if(skipms){
				skipms = 0;
				goto skip;
			}
			if(m.buttons & 2){
				f = (double)viewdiv / Nview;
				viewdiv += (m.xy.y - mo.y) / 2;
				if(viewdiv > 10 * Nview)
					viewdiv = 10 * Nview;
				else if(viewdiv < 1)
					viewdiv = 1;
				f = ((double)viewdiv / Nview) / f;
				vx *= f;
				vy *= f;
				view = addpt(center, Pt(vx, vy));
				redraw();
			}
			if(m.buttons & 1){
				mo = subpt(m.xy, mo);
				view = addpt(view, mo);
				vx = view.x - center.x;
				vy = view.y - center.y;
				redraw();
			}
			if(ptinrect(m.xy, screen->r))
				drawstats(subpt(m.xy, screen->r.min));
		skip:
			mo = m.xy;
			break;
		case 2:
			switch(r){
			paint: redraw(); break;
			case Kdel:
			case 'q': threadexitsall(nil);
			case 'a': mode = (mode + 1) % 3; goto paint;
			case 'j': join ^= 1; goto paint;
			case 'r':
				view = center;
				vx = view.x - center.x;
				vy = view.y - center.y;
				goto paint;
			case '+':
			case '=':
				if(grunit < 1024){
					grunit <<= 1;
					goto paint;
				}
				break;
			case '-':
				if(grunit > 1){
					grunit >>= 1;
					goto paint;
				}
				break;
			case 'z':
				viewdiv = Nzoom;
				goto paint;
			}
			break;
		default:
			threadexitsall("alt: %r");
		}
	}
}
