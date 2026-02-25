#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"

void menucolors(void);
enum {
	Blackborder = 2,
	Vspacing = 2,
};

static	Image	*cols[NCOL];
void pd(Rune *r, int n){
		for(int i = 0; i < n; i++)
			print("%C", r[i]);
		print("\n");
}

static
void
error(char *s)
{
	fprint(2, "prompt: %s: %r\n", s);
	threadexitsall(nil);
}
static
void*
emalloc(uint n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		error("malloc failed");
	setmalloctag(p, getcallerpc(&n));
	memset(p, 0, n);
	return p;
}

Prompt *
prinit(Keyboardctl *kbc, ulong nbuf)
{
	Prompt *p = emalloc(sizeof(*p));
	p->kbc = kbc;
	p->nbuf = nbuf;
	p->buf = emalloc(sizeof(*(p->buf))*nbuf);
	p->frame = nil;
	p->backup = nil;
	p->end = 0;
	p->hl = 0;
	p->tick = 0;
	return p;
}
void
prfree(Prompt *p)
{
	if(p->frame)
		free(p->frame);
	if(p->backup)
		free(p->backup);
	free(p->buf);
	free(p);
}
/* remove everything between p0 and p1, p1 is first *not* to be deleted */
void
prdelrunes(Prompt *p, ulong p0, ulong p1)
{
	/* end and tick point to the *next* place to add */
	if(p->end == 0 || p1 <= p0)
		return;

	/* move everything above deletion to start of deletion */
	memmove(p->buf+p0,p->buf+p1,(p->end-p1)*sizeof(*(p->buf)));

	if(p->frame)
		frdelete(p->frame, p0, p1);

	p->end -= p1 - p0;
	p->tick = p0;
}
void
praddrune(Prompt *p, Rune r)
{
	if(p->hl)
		prdelrunes(p, 0, p->end);

	if(p->end == p->nbuf - 1)
		return;
	/* move everything above cursor up */
	if(p->tick < p->end)
		memmove(p->buf+p->tick+1,p->buf+p->tick,(p->end-p->tick)*sizeof(*(p->buf)));
	p->buf[p->tick] = r;
	if(p->frame)
		frinsert(p->frame, p->buf+p->tick, p->buf+p->tick+1, p->tick);

	p->end++;
	p->tick++;
}
void
prsettick(Prompt *p, ulong tick)
{
	if(tick >= 0 && tick <= p->end)
		p->tick = tick;
}
ulong
lastspace(Prompt *p)
{
	int i;
	for(i=p->tick-1;i>0;i--)
		if(p->buf[i] != ' ' && p->buf[i] != '\t')
			break;
	for(;i>0;i--)
		if(p->buf[i] == ' ' || p->buf[i] == '\t')
			break;

	return (i==0)? 0 : i + 1;
}


int
prdraw(Prompt *p, Image *scr, Point pos, ulong width)
{
	Rectangle textr, sc;
	uint l;
	Rune r;
	int ret;

	ret = 1;
	l = 1;

	// set hl = -1 from outside to suppress
	if(p->hl<0)
		p->hl = 0;
	else
		p->hl = p->end;

	if(cols[BACK] == nil)
		menucolors();

	sc = screen->clipr;
	replclipr(screen, 0, screen->r);

redraw:
	p->r = rectaddpt(
			insetrect(Rect(0, 0, width, (font->height+Vspacing)*l), -Margin),
			pos);

	textr.max.x = p->r.max.x-Margin;
	textr.min.x = textr.max.x-width;
	textr.min.y = p->r.min.y+Margin;
	textr.max.y = textr.min.y + l*(font->height+Vspacing);

	if(p->frame)
		free(p->frame);
	p->frame = emalloc(sizeof(*(p->frame)));

	if(p->backup){
		draw(scr, p->r, p->backup, nil, p->r.min);
		free(p->backup);
	}
	p->backup = allocimage(display, p->r, screen->chan, 0, -1);
	if(p->backup == 0)
		error("allocimage: server out of mem");
	draw(p->backup, p->r, screen, nil, p->r.min);

	draw(scr, p->r, cols[BACK], nil, ZP);
	border(scr, p->r, Blackborder, cols[BORD], ZP);

	frinit(p->frame, textr, font, scr, cols);

	frinsert(p->frame, p->buf, p->buf+p->end, 0);
	frtick(p->frame, frptofchar(p->frame, p->end), 0);
	frdrawsel(p->frame, frptofchar(p->frame, 0), 0, p->end, p->hl);
	if(!p->hl)
		frtick(p->frame, frptofchar(p->frame, p->tick), 1);

	/* frame does not show all text, grow down */
	if(p->frame->nchars < p->end){
		l = (p->end / p->frame->nchars) + ((p->end % p->frame->nchars) ? 1 : 0);
		goto redraw;
	}
	flushimage(display, 1);

	while(recv(p->kbc->c, &r)) {
		frtick(p->frame, frptofchar(p->frame, p->tick), 0);	/* remove last tick */
		switch(r) {
			case '\n':
			case 0x10:		/* ctrl + p */
				goto out;
			case 0x08:		/* backspace, ^H */
				if(!p->hl)
					prdelrunes(p, p->tick-1, p->tick);
				else
					prdelrunes(p, 0, p->end);
				break;
			case 0x15:			/* ^U: delete from cursor to start of line */
				prdelrunes(p, 0, p->tick);
				break;
			case 0x17:			/* ^W: delete word before cursor */
				prdelrunes(p, lastspace(p), p->tick);
				break;
			case 0x1b:			/* Esc */
				ret = 0;
				goto out;
				break;
			case 0xf011:		/* left */
				prsettick(p, p->tick - 1);
				break;
			case 0xf012:		/* right */
				prsettick(p, p->tick + 1);
				break;
			case 0x01:			/* ^A: beginning of line */
				prsettick(p, 0);
				break;
			case 0x05:			/* ^E: end of line */
				prsettick(p, p->end);
				break;
			default:
				praddrune(p, r);
				break;
		}
		if(p->frame->lastlinefull){
			l++;
			goto redraw;
		}
		if(p->hl){
			frdrawsel(p->frame, frptofchar(p->frame, 0), 0, p->hl, 0);
			p->hl = 0;
		}
		/* frinsert/frdelete put tick and end of frame */
		frtick(p->frame, frptofchar(p->frame, p->end), 0);
		frtick(p->frame, frptofchar(p->frame, p->tick), 1);

		flushimage(display, 1);
	}
out:
	p->buf[p->end] = 0;
	draw(scr, p->r, p->backup, nil, p->r.min);
	free(p->backup);
	p->backup = nil;
	flushimage(display, 1);

	replclipr(screen, 0, sc);
	return ret;
}

void
menucolors(void)
{
	cols[BACK] = allocimagemix(display, DPalebluegreen, DWhite);
	cols[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalegreygreen);
	cols[BORD] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPurpleblue);
	if(cols[BACK]==nil || cols[HIGH]==nil || cols[BORD]==nil)
		goto Error;
	cols[TEXT] = display->black;
	cols[HTEXT] = display->black;
	return;

    Error:
    print("ERROR IN MENUCOLORS\n");
	freeimage(cols[BACK]);
	freeimage(cols[HIGH]);
	freeimage(cols[BORD]);
	cols[BACK] = display->white;
	cols[HIGH] = display->black;
	cols[BORD] = display->black;
	cols[TEXT] = display->black;
	cols[HTEXT] = display->white;
}
