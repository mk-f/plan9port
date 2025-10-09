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
acme_prompt(Keyboardctl *kbc, char **res)
{
	Rectangle r, menur, sc, textr;
	Image *b, *backup;
	Point pt;
	Rune rn;
	int t, lt, end;
	Frame *fr;
	Rune rbuf[prompt_max];

	// for reasons unknown, Frame does not work correctly when
	// not setting up with malloc but using Frame fr;
	fr = emalloc(sizeof(*fr));
	if(cols[BACK] == nil)
		menucolors();

	sc = screen->clipr;

	replclipr(screen, 0, screen->r);

	r = insetrect(Rect(0, 0, 200, font->height+Vspacing), -Margin);

	pt.x = Dx(screen->r)/2;
	pt.y = Dy(screen->r)/2;

	menur = rectaddpt(r, pt);

	textr.max.x = menur.max.x-Margin;
	textr.min.x = textr.max.x-200;
	textr.min.y = menur.min.y+Margin;
	textr.max.y = textr.min.y + 1*(font->height+Vspacing);

	b = screen;

	/* backup current content befor drawing over it */
	backup = allocimage(display, menur, screen->chan, 0, -1);
	if(backup)
		draw(backup, menur, screen, nil, menur.min);

	/* now draw the menu onto the screen */
	draw(b, menur, cols[BACK], nil, ZP);

	/* and a border */
	border(b, menur, Blackborder, cols[BORD], ZP);
	frinit(fr, textr, font, b, cols);
	frtick(fr, frptofchar(fr, 0), 1);	/* remove last tick */
	flushimage(display, 1);

	end = 0;
	t = 0; /* tick pos */
	/* this just works in the keyboard-thread! Blocking! */
	while(recv(kbc->c, &rn)) {
		frtick(fr, frptofchar(fr, t), 0);	/* remove last tick */
		switch(rn) {
			case '\n':
				goto out;
			case 0x08:		/* backspace */
				if(t == 0)
					continue;
				if(t < end)
					memmove(rbuf+t-1,rbuf+t,(end-t)*sizeof(*rbuf));
				frdelete(fr, t-1, t);
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
				if(end == prompt_max)
					continue;
				if(t < end)
					memmove(rbuf+t+1,rbuf+t,(end-t)*sizeof(*rbuf));
				rbuf[t] = rn;
				frinsert(fr, rbuf+t, rbuf+t+1, t);

				end++;
				t++;
				break;
		}

		/* frinsert/frdelete put tick and end of frame */
		frtick(fr, frptofchar(fr, end), 0);
		frtick(fr, frptofchar(fr, t), 1);

		flushimage(display, 1);
	}
out:
	/*
	*res = emalloc(idx+1);
	memcpy(*res, buf, idx);
	(*res)[idx] = 0;
	*/
	free(fr);
	if(b != screen)
		freeimage(b);
	if(backup){
		draw(screen, menur, backup, nil, menur.min);
		freeimage(backup);
	}
	replclipr(screen, 0, sc);
	flushimage(display, 1);

	return -1;
}
