typedef struct Thing Thing;
typedef struct Line Line;
typedef struct Side Side;
typedef struct Vertex Vertex;
typedef struct Sector Sector;
typedef struct Seg Seg;

enum{
	CFline = 1<<23,
	CFthing = 1<<22,
	CFvert = 1<<21,
};

struct Vertex{
	int i;
	Point;
	uchar sel[4];
};
extern Vertex *verts;
extern int nverts;

struct Sector{
	int i;
	Point;	/* floor z, ceiling z */
	char fflat[8+1];
	char cflat[8+1];
	short lum;
	short type;
	short tag;
};
extern Sector *sects;
extern int nsects;

struct Side{
	int i;
	Point;	/* Δ */
	char high[8+1];
	char low[8+1];
	char mid[8+1];
	int sid;
	Sector *s;
};
extern Side *sides;
extern int nsides;

enum{
	LFimpass = 1<<0,
	LFmblock = 1<<1,
	LFtwosd = 1<<2,
	LFuunpeg = 1<<3,
	LFdunpeg = 1<<4,
	LFsecret = 1<<5,
	LFsblock = 1<<6,
	LFnodraw = 1<<7,
	LFmapped = 1<<8
};
struct Line{
	int i;
	int vid;
	int wid;
	Vertex *v;
	Vertex *w;
	int rid;
	int lid;
	Side *r;	/* FIXME: front/back, not right/left? */
	Side *l;
	short type;
	short f;
	short tag;
	uchar sel[4];
};
extern Line *lines;
extern int nlines;

enum{
	TFbaby = 1<<0,
	TFeasy = TFbaby,
	TFmed = 1<<1,
	TFhard = 1<<2,
	TFnite = TFhard,
	TFdeaf = 1<<3,
	TFmponly = 1<<4
};
struct Thing{
	int i;
	Point;
	short an;
	short type;
	short f;
	uchar sel[4];
};
extern Thing *things;
extern int nthings;

struct Seg{
	Point p1;
	Point p2;
	Line *l;
	Side *s;
	int ofs;
	int grouped;
	Seg *e;
};
extern Seg *segs;
