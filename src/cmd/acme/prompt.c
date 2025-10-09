#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

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

static	Image	*back;
static	Image	*high;
static	Image	*bord;
static	Image	*text;
static	Image	*htext;
static
void
menucolors(void)
{
	back = allocimagemix(display, DPalebluegreen, DWhite);
	high = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalegreygreen);
	bord = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPurpleblue);
	if(back==nil || high==nil || bord==nil)
		goto Error;
	text = display->black;
	htext = display->black;
	return;

    Error:
	freeimage(back);
	freeimage(high);
	freeimage(bord);
	back = display->white;
	high = display->black;
	bord = display->black;
	text = display->black;
	htext = display->white;
}

int
acme_prompt(Mousectl *mc, Keyboardctl *kbc)
{
	Rectangle r, menur, sc, textr;
	Image *b, *backup;
	Point pt, tpt;
	Rune rn;
	char *c;

	if(back == nil)
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
	draw(b, menur, back, nil, ZP);

	/* and a border */
	border(b, menur, Blackborder, bord, ZP);

	tpt = string(b, pt, text, pt, font, "");
	flushimage(display, 1);

	/* this just works in the keyboard-thread! Blocking! */
	while(recv(kbc->c, &rn)) {
		print("%C\n", rn);
		c = smprint("%C", rn);
		tpt = string(b, tpt, text, tpt, font, c);
		flushimage(display, 1);
		if(rn == 'c')
			break;
	}

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
