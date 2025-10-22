#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <libString.h>
#include <frame.h>
#include "prompt.h"

enum
{
	Margin = 4,		/* outside to text */
	Border = 2,		/* outside to selection boxes */
	Blackborder = 2,	/* width of outlining border */
	Vspacing = 2,		/* extra spacing between lines of text */
};

void menucolors(void);

static	Image	*cols[NCOL];

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
prinit(Keyboardctl *kbc, Rune *buf, ulong nbuf)
{
	Prompt *p = emalloc(sizeof(*p));
	p->kbc = kbc;
	p->nbuf = nbuf;
	p->buf = buf;
	p->frame = nil;
	p->backup = nil;
	p->end = 0;
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
	free(p);
}
void
prdraw(Prompt *p, Image *scr, Point pos, ulong width)
{
	Rectangle textr, sc;
	uint l, h;
	Rune r;

	l = 1;
	h = (p->end) ? 1 : 0;

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
	frtick(p->frame, frptofchar(p->frame, p->tick), 1);

	/* frame does not show all text, grow down */
	if(p->frame->nchars < p->end){
		l = (p->end / p->frame->nchars) + ((p->end % p->frame->nchars) ? 1 : 0);
		goto redraw;
	}
	/* TODO
	frdrawsel(p->frame, frptofchar(p->frame, 0), p->frame->p0, p->frame->p1, h);
	*/
	flushimage(display, 1);

	while(recv(p->kbc->c, &r)) {
		frtick(p->frame, frptofchar(p->frame, p->tick), 0);	/* remove last tick */
		switch(r) {
			case '\n':
				goto out;
			case 0x08:		/* backspace, ^H */
				if(p->tick == 0)
					continue;
				if(p->tick < p->end)
					memmove(p->buf+p->tick-1,p->buf+p->tick,(p->end-p->tick)*sizeof(*(p->buf)));
				frdelete(p->frame, p->tick-1, p->tick);
				p->end--;
				p->tick--;
				break;
			case 0x15:			/* ^U: delete from cursor to start of line */
				if(p->tick == 0)
					continue;
				memmove(p->buf,p->buf+p->tick,(p->end-p->tick)*sizeof(*(p->buf)));
				frdelete(p->frame, 0, p->tick);
				p->end -= p->tick;
				p->tick = 0;
				break;
			case 0x17:			/* ^W: delete word before cursor */
				//TODO
				break;
			case 0x1b:			/* Esc */
				break;
			case 0xf011:		/* left */
				if(p->tick > 0)
					p->tick--;
				break;
			case 0xf012:		/* right */
				if(p->tick < p->end)
					p->tick++;
				break;
			case 0x01:			/* ^A: beginning of line */
				p->tick = 0;
				break;
			case 0x05:			/* ^E: end of line */
				p->tick = p->end;
				break;
			default:
				if(p->end == p->nbuf - 1)
					continue;
				if(p->tick < p->end)
					memmove(p->buf+p->tick+1,p->buf+p->tick,(p->end-p->tick)*sizeof(*(p->buf)));
				p->buf[p->tick] = r;
				frinsert(p->frame, p->buf+p->tick, p->buf+p->tick+1, p->tick);

				p->end++;
				p->tick++;

				if(p->frame->lastlinefull){
					l++;
					goto redraw;
				}
				break;
		}

		/* frinsert/frdelete put tick and end of frame */
		frtick(p->frame, frptofchar(p->frame, p->end), 0);
		frtick(p->frame, frptofchar(p->frame, p->tick), 1);

		flushimage(display, 1);
	}
out:
	p->buf[p->end] = 0;
	draw(scr, p->r, p->backup, nil, p->r.min);
	flushimage(display, 1);

	replclipr(screen, 0, sc);

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
void pd(Rune *r, int n){
		for(int i = 0; i < n; i++)
			print("%C", r[i]);
		print("\n");
}
