#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <libString.h>
#include <frame.h>

#define prompt_max 256

enum
{
	Margin = 4,		/* outside to text */
	Border = 2,		/* outside to selection boxes */
	Blackborder = 2,	/* width of outlining border */
	Vspacing = 2,		/* extra spacing between lines of text */
	Maxunscroll = 25,	/* maximum #entries before scrolling turns on */
	Nscroll = 20,		/* number entries in scrolling part */
	Scrollwid = 14,		/* width of scroll bar */
	Gap = 4			/* between text and scroll bar */
};

typedef struct Prompt Prompt;
struct Prompt {
	Rectangle r;
	Image *backup;
	Image *prompt;
	Frame *frame;
	int lines;
	int width;
};

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

static
Prompt *
prompt_draw(Image *b, Point pos, int lines, int width)
{
	Rectangle textr;
	Prompt *p = emalloc(sizeof(*p));
	// for reasons unknown, Frame does not work correctly when
	// not setting up with malloc but using Frame frame;
	p->frame = emalloc(sizeof(*(p->frame)));
	p->r = rectaddpt(
			insetrect(Rect(0, 0, width, (font->height+Vspacing)*lines), -Margin),
			pos);

	p->backup = allocimage(display, p->r, screen->chan, 0, -1);
	if(p->backup == 0)
		error("allocimage: server out of mem");
	draw(p->backup, p->r, screen, nil, p->r.min);

	draw(b, p->r, cols[BACK], nil, ZP);
	border(b, p->r, Blackborder, cols[BORD], ZP);

	textr.max.x = p->r.max.x-Margin;
	textr.min.x = textr.max.x-width;
	textr.min.y = p->r.min.y+Margin;
	textr.max.y = textr.min.y + lines*(font->height+Vspacing);

	frinit(p->frame, textr, font, b, cols);
	return p;
}

static
void
prompt_free(Prompt *p)
{
	free(p->frame);
	draw(screen, p->r, p->backup, nil, p->r.min);
	free(p->backup);
	free(p);
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

int
acme_prompt(Keyboardctl *kbc, Rune **buf, ulong nrbuf, Point pt, uint width)
{
	Rectangle sc;
	Rune rn, *rbuf;
	int t, end, lines;

	rbuf = *buf;

	if(cols[BACK] == nil)
		menucolors();

	sc = screen->clipr;
	replclipr(screen, 0, screen->r);

	lines = 1;
	Prompt *pr = prompt_draw(screen, pt, lines, width);
	frtick(pr->frame, frptofchar(pr->frame, 0), 1);

	flushimage(display, 1);
	end = 0;
	t = 0; /* tick pos */
	/* this just works in the keyboard-thread! Blocking! */
	while(recv(kbc->c, &rn)) {
		frtick(pr->frame, frptofchar(pr->frame, t), 0);	/* remove last tick */
		switch(rn) {
			case '\n':
				goto out;
			case 0x08:		/* backspace */
				if(t == 0)
					continue;
				if(t < end)
					memmove(rbuf+t-1,rbuf+t,(end-t)*sizeof(*rbuf));
				frdelete(pr->frame, t-1, t);
				end--;
				t--;
				break;
			case 0xf011:		/* left */
				if(t > 0)
					t--;
				break;
			case 0xf012:		/* right */
				if(t < end)
					t++;
				break;
			case 0x01:			/* ^A: beginning of line */
				t = 0;
				break;
			case 0x05:			/* ^E: end of line */
				t = end;
				break;
			default:
				if(end == nrbuf - 1)
					continue;
				if(t < end)
					memmove(rbuf+t+1,rbuf+t,(end-t)*sizeof(*rbuf));
				rbuf[t] = rn;
				frinsert(pr->frame, rbuf+t, rbuf+t+1, t);

				end++;
				t++;

				if(pr->frame->lastlinefull){
					prompt_free(pr);
					pr = prompt_draw(screen, pt, ++lines, width);
					frinsert(pr->frame, rbuf, rbuf+t, 0);
				}
				break;
		}

		/* frinsert/frdelete put tick and end of frame */
		frtick(pr->frame, frptofchar(pr->frame, end), 0);
		frtick(pr->frame, frptofchar(pr->frame, t), 1);

		flushimage(display, 1);
	}
out:
	rbuf[end] = 0;
	prompt_free(pr);

	replclipr(screen, 0, sc);
	flushimage(display, 1);

	return -1;
}
