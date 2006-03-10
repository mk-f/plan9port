/* input event and data structure translation */

#include <u.h>
#include "x11-inc.h"
#ifdef __APPLE__
#define APPLESNARF
#define Boolean AppleBoolean
#define Rect AppleRect
#define EventMask AppleEventMask
#define Point ApplePoint
#define Cursor AppleCursor
#include <Carbon/Carbon.h>
AUTOFRAMEWORK(Carbon)
#undef Boolean
#undef Rect
#undef EventMask
#undef Point
#undef Cursor
#endif
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include "x11-memdraw.h"
#include "x11-keysym2ucs.h"
#undef time

static KeySym
__xtoplan9kbd(XEvent *e)
{
	KeySym k;

	if(e->xany.type != KeyPress)
		return -1;
	needstack(64*1024);	/* X has some *huge* buffers in openobject */
		/* and they're even bigger on SuSE */
	XLookupString((XKeyEvent*)e,NULL,0,&k,NULL);
	if(k == XK_Multi_key || k == NoSymbol)
		return -1;

	if(k&0xFF00){
		switch(k){
		case XK_BackSpace:
		case XK_Tab:
		case XK_Escape:
		case XK_Delete:
		case XK_KP_0:
		case XK_KP_1:
		case XK_KP_2:
		case XK_KP_3:
		case XK_KP_4:
		case XK_KP_5:
		case XK_KP_6:
		case XK_KP_7:
		case XK_KP_8:
		case XK_KP_9:
		case XK_KP_Divide:
		case XK_KP_Multiply:
		case XK_KP_Subtract:
		case XK_KP_Add:
		case XK_KP_Decimal:
			k &= 0x7F;
			break;
		case XK_Linefeed:
			k = '\r';
			break;
		case XK_KP_Space:
			k = ' ';
			break;
		case XK_Home:
		case XK_KP_Home:
			k = Khome;
			break;
		case XK_Left:
		case XK_KP_Left:
			k = Kleft;
			break;
		case XK_Up:
		case XK_KP_Up:
			k = Kup;
			break;
		case XK_Down:
		case XK_KP_Down:
			k = Kdown;
			break;
		case XK_Right:
		case XK_KP_Right:
			k = Kright;
			break;
		case XK_Page_Down:
		case XK_KP_Page_Down:
			k = Kpgdown;
			break;
		case XK_End:
		case XK_KP_End:
			k = Kend;
			break;
		case XK_Page_Up:	
		case XK_KP_Page_Up:
			k = Kpgup;
			break;
		case XK_Insert:
		case XK_KP_Insert:
			k = Kins;
			break;
		case XK_KP_Enter:
		case XK_Return:
			k = '\n';
			break;
		case XK_Alt_L:
		case XK_Meta_L:	/* Shift Alt on PCs */
		case XK_Alt_R:
		case XK_Meta_R:	/* Shift Alt on PCs */
			k = Kalt;
			break;
		default:		/* not ISO-1 or tty control */
			if(k>0xff) {
				k = _p9keysym2ucs(k);
				if(k==-1) return -1;
			}
		}
	}

	/* Compensate for servers that call a minus a hyphen */
	if(k == XK_hyphen)
		k = XK_minus;
	/* Do control mapping ourselves if translator doesn't */
	if(e->xkey.state&ControlMask)
		k &= 0x9f;
	if(k == NoSymbol) {
		return -1;
	}

	return k+0;
}

static Rune*
xtoplan9latin1(XEvent *e)
{
	static Rune k[10];
	static int alting, nk;
	int n;
	int r;

	r = __xtoplan9kbd(e);
	if(r < 0)
		return nil;
	if(alting){
		/*
		 * Kludge for Mac's X11 3-button emulation.
		 * It treats Command+Button as button 3, but also
		 * ends up sending XK_Meta_L twice.
		 */
		if(r == Kalt){
			alting = 0;
			return nil;
		}
		k[nk++] = r;
		n = _latin1(k, nk);
		if(n > 0){
			alting = 0;
			k[0] = n;
			k[1] = 0;
			return k;
		}
		if(n == -1){
			alting = 0;
			k[nk] = 0;
			return k;
		}
		/* n < -1, need more input */
		return nil;
	}else if(r == Kalt){
		alting = 1;
		nk = 0;
		return nil;
	}else{
		k[0] = r;
		k[1] = 0;
		return k;
	}
}

int
_xtoplan9kbd(XEvent *e)
{
	static Rune *r;

	if(e == (XEvent*)-1){
		assert(r);
		r--;
		return 0;
	}
	if(e)
		r = xtoplan9latin1(e);
	if(r && *r)
		return *r++;
	return -1;
}

int
_xtoplan9mouse(XDisplay *xd, XEvent *e, Mouse *m)
{
	int s;
	XButtonEvent *be;
	XMotionEvent *me;

	if(_x.putsnarf != _x.assertsnarf){
		_x.assertsnarf = _x.putsnarf;
		XSetSelectionOwner(_x.mousecon, XA_PRIMARY, _x.drawable, CurrentTime);
		if(_x.clipboard != None)
			XSetSelectionOwner(_x.mousecon, _x.clipboard, _x.drawable, CurrentTime);
		XFlush(xd);
	}

	switch(e->type){
	case ButtonPress:
		be = (XButtonEvent*)e;
		/* 
		 * Fake message, just sent to make us announce snarf.
		 * Apparently state and button are 16 and 8 bits on
		 * the wire, since they are truncated by the time they
		 * get to us.
		 */
		if(be->send_event
		&& (~be->state&0xFFFF)==0
		&& (~be->button&0xFF)==0)
			return -1;
		/* BUG? on mac need to inherit these from elsewhere? */
		m->xy.x = be->x;
		m->xy.y = be->y;
		s = be->state;
		m->msec = be->time;
		switch(be->button){
		case 1:
			s |= Button1Mask;
			break;
		case 2:
			s |= Button2Mask;
			break;
		case 3:
			s |= Button3Mask;
			break;
		case 4:
			s |= Button4Mask;
			break;
		case 5:
			s |= Button5Mask;
			break;
		}
		break;
	case ButtonRelease:
		be = (XButtonEvent*)e;
		m->xy.x = be->x;
		m->xy.y = be->y;
		s = be->state;
		m->msec = be->time;
		switch(be->button){
		case 1:
			s &= ~Button1Mask;
			break;
		case 2:
			s &= ~Button2Mask;
			break;
		case 3:
			s &= ~Button3Mask;
			break;
		case 4:
			s &= ~Button4Mask;
			break;
		case 5:
			s &= ~Button5Mask;
			break;
		}
		break;

	case MotionNotify:
		me = (XMotionEvent*)e;
		s = me->state;
		m->xy.x = me->x;
		m->xy.y = me->y;
		m->msec = me->time;
		break;

	default:
		return -1;
	}

	m->buttons = 0;
	if(s & Button1Mask)
		m->buttons |= 1;
	if(s & Button2Mask)
		m->buttons |= 2;
	if(s & Button3Mask)
		m->buttons |= 4;
	if(s & Button4Mask)
		m->buttons |= 8;
	if(s & Button5Mask)
		m->buttons |= 16;
	return 0;
}

void
_xmoveto(Point p)
{
	XWarpPointer(_x.display, None, _x.drawable, 0, 0, 0, 0, p.x, p.y);
	XFlush(_x.display);
}

static int
revbyte(int b)
{
	int r;

	r = 0;
	r |= (b&0x01) << 7;
	r |= (b&0x02) << 5;
	r |= (b&0x04) << 3;
	r |= (b&0x08) << 1;
	r |= (b&0x10) >> 1;
	r |= (b&0x20) >> 3;
	r |= (b&0x40) >> 5;
	r |= (b&0x80) >> 7;
	return r;
}

static void
xcursorarrow(void)
{
	if(_x.cursor != 0){
		XFreeCursor(_x.display, _x.cursor);
		_x.cursor = 0;
	}
	XUndefineCursor(_x.display, _x.drawable);
	XFlush(_x.display);
}


void
_xsetcursor(Cursor *c)
{
	XColor fg, bg;
	XCursor xc;
	Pixmap xsrc, xmask;
	int i;
	uchar src[2*16], mask[2*16];

	if(c == nil){
		xcursorarrow();
		return;
	}
	for(i=0; i<2*16; i++){
		src[i] = revbyte(c->set[i]);
		mask[i] = revbyte(c->set[i] | c->clr[i]);
	}

	fg = _x.map[0];
	bg = _x.map[255];
	xsrc = XCreateBitmapFromData(_x.display, _x.drawable, (char*)src, 16, 16);
	xmask = XCreateBitmapFromData(_x.display, _x.drawable, (char*)mask, 16, 16);
	xc = XCreatePixmapCursor(_x.display, xsrc, xmask, &fg, &bg, -c->offset.x, -c->offset.y);
	if(xc != 0) {
		XDefineCursor(_x.display, _x.drawable, xc);
		if(_x.cursor != 0)
			XFreeCursor(_x.display, _x.cursor);
		_x.cursor = xc;
	}
	XFreePixmap(_x.display, xsrc);
	XFreePixmap(_x.display, xmask);
	XFlush(_x.display);
}

struct {
	QLock lk;
	char buf[SnarfSize];
#ifdef APPLESNARF
	Rune rbuf[SnarfSize];
	PasteboardRef apple;
#endif
} clip;

char*
_xgetsnarf(XDisplay *xd)
{
	uchar *data, *xdata;
	Atom clipboard, type, prop;
	ulong len, lastlen, dummy;
	int fmt, i;
	XWindow w;

	qlock(&clip.lk);
	/*
	 * Have we snarfed recently and the X server hasn't caught up?
	 */
	if(_x.putsnarf != _x.assertsnarf)
		goto mine;

	/*
	 * Is there a primary selection (highlighted text in an xterm)?
	 */
	clipboard = XA_PRIMARY;
	w = XGetSelectionOwner(xd, XA_PRIMARY);
	if(w == _x.drawable){
	mine:
		data = (uchar*)strdup(clip.buf);
		goto out;
	}

	/*
	 * If not, is there a clipboard selection?
	 */
	if(w == None && _x.clipboard != None){
		clipboard = _x.clipboard;
		w = XGetSelectionOwner(xd, _x.clipboard);
		if(w == _x.drawable)
			goto mine;
	}

	/*
	 * If not, give up.
	 */
	if(w == None){
		data = nil;
		goto out;
	}
		
	/*
	 * We should be waiting for SelectionNotify here, but it might never
	 * come, and we have no way to time out.  Instead, we will clear
	 * local property #1, request our buddy to fill it in for us, and poll
	 * until he's done or we get tired of waiting.
	 *
	 * We should try to go for _x.utf8string instead of XA_STRING,
	 * but that would add to the polling.
	 */
	prop = 1;
	XChangeProperty(xd, _x.drawable, prop, XA_STRING, 8, PropModeReplace, (uchar*)"", 0);
	XConvertSelection(xd, clipboard, XA_STRING, prop, _x.drawable, CurrentTime);
	XFlush(xd);
	lastlen = 0;
	for(i=0; i<10 || (lastlen!=0 && i<30); i++){
		usleep(100*1000);
		XGetWindowProperty(xd, _x.drawable, prop, 0, 0, 0, AnyPropertyType,
			&type, &fmt, &dummy, &len, &data);
		if(lastlen == len && len > 0)
			break;
		lastlen = len;
	}
	if(i == 10){
		data = nil;
		goto out;
	}
	/* get the property */
	data = nil;
	XGetWindowProperty(xd, _x.drawable, prop, 0, SnarfSize/sizeof(ulong), 0, 
		AnyPropertyType, &type, &fmt, &len, &dummy, &xdata);
	if((type != XA_STRING && type != _x.utf8string) || len == 0){
		if(xdata)
			XFree(xdata);
		data = nil;
	}else{
		if(xdata){
			data = (uchar*)strdup((char*)xdata);
			XFree(xdata);
		}else
			data = nil;
	}
out:
	qunlock(&clip.lk);
	return (char*)data;
}

void
_xputsnarf(XDisplay *xd, char *data)
{
	XButtonEvent e;

	if(strlen(data) >= SnarfSize)
		return;
	qlock(&clip.lk);
	strcpy(clip.buf, data);
	/* leave note for mouse proc to assert selection ownership */
	_x.putsnarf++;

	/* send mouse a fake event so snarf is announced */
	memset(&e, 0, sizeof e);
	e.type = ButtonPress;
	e.window = _x.drawable;
	e.state = ~0;
	e.button = ~0;
	XSendEvent(xd, _x.drawable, True, ButtonPressMask, (XEvent*)&e);
	XFlush(xd);
	qunlock(&clip.lk);
}

int
_xselect(XEvent *e, XDisplay *xd)
{
	char *name;
	XEvent r;
	XSelectionRequestEvent *xe;
	Atom a[4];

	memset(&r, 0, sizeof r);
	xe = (XSelectionRequestEvent*)e;
if(0) fprint(2, "xselect target=%d requestor=%d property=%d selection=%d\n",
	xe->target, xe->requestor, xe->property, xe->selection);
	r.xselection.property = xe->property;
	if(xe->target == _x.targets){
		a[0] = XA_STRING;
		a[1] = _x.utf8string;
		a[2] = _x.text;
		a[3] = _x.compoundtext;

		XChangeProperty(xd, xe->requestor, xe->property, xe->target,
			8, PropModeReplace, (uchar*)a, sizeof a);
	}else if(xe->target == XA_STRING 
	|| xe->target == _x.utf8string 
	|| xe->target == _x.text 
	|| xe->target == _x.compoundtext
	|| ((name = XGetAtomName(xd, xe->target)) && strcmp(name, "text/plain;charset=UTF-8") == 0)){
		/* text/plain;charset=UTF-8 seems nonstandard but is used by Synergy */
		/* if the target is STRING we're supposed to reply with Latin1 XXX */
		qlock(&clip.lk);
		XChangeProperty(xd, xe->requestor, xe->property, xe->target,
			8, PropModeReplace, (uchar*)clip.buf, strlen(clip.buf));
		qunlock(&clip.lk);
	}else{
		if(strcmp(name, "TIMESTAMP") != 0)
			fprint(2, "%s: cannot handle selection request for '%s' (%d)\n", argv0, name, (int)xe->target);
		r.xselection.property = None;
	}

	r.xselection.display = xe->display;
	/* r.xselection.property filled above */
	r.xselection.target = xe->target;
	r.xselection.type = SelectionNotify;
	r.xselection.requestor = xe->requestor;
	r.xselection.time = xe->time;
	r.xselection.send_event = True;
	r.xselection.selection = xe->selection;
	XSendEvent(xd, xe->requestor, False, 0, &r);
	XFlush(xd);
	return 0;
}

#ifdef APPLESNARF
char*
applegetsnarf(void)
{
	char *s, *t;
	CFArrayRef flavors;
	CFDataRef data;
	CFIndex nflavor, ndata, j;
	CFStringRef type;
	ItemCount nitem;
	PasteboardItemID id;
	PasteboardSyncFlags flags;
	UInt32 i;

//	fprint(2, "applegetsnarf\n");
	qlock(&clip.lk);
	if(clip.apple == nil){
		if(PasteboardCreate(kPasteboardClipboard, &clip.apple) != noErr){
			fprint(2, "apple pasteboard create failed\n");
			qunlock(&clip.lk);
			return nil;
		}
	}
	flags = PasteboardSynchronize(clip.apple);
	if(flags&kPasteboardClientIsOwner){
		s = strdup(clip.buf);
		qunlock(&clip.lk);
		return s;
	}
	if(PasteboardGetItemCount(clip.apple, &nitem) != noErr){
		fprint(2, "apple pasteboard get item count failed\n");
		qunlock(&clip.lk);
		return nil;
	}
	for(i=1; i<=nitem; i++){
		if(PasteboardGetItemIdentifier(clip.apple, i, &id) != noErr)
			continue;
		if(PasteboardCopyItemFlavors(clip.apple, id, &flavors) != noErr)
			continue;
		nflavor = CFArrayGetCount(flavors);
		for(j=0; j<nflavor; j++){
			type = (CFStringRef)CFArrayGetValueAtIndex(flavors, j);
			if(!UTTypeConformsTo(type, CFSTR("public.utf16-plain-text")))
				continue;
			if(PasteboardCopyItemFlavorData(clip.apple, id, type, &data) != noErr)
				continue;
			ndata = CFDataGetLength(data);
			qunlock(&clip.lk);
			s = smprint("%.*S", ndata/2, (Rune*)CFDataGetBytePtr(data));
			CFRelease(flavors);
			CFRelease(data);
			for(t=s; *t; t++)
				if(*t == '\r')
					*t = '\n';
			return s;
		}
		CFRelease(flavors);
	}
	qunlock(&clip.lk);
	return nil;		
}

void
appleputsnarf(char *s)
{
	CFDataRef cfdata;
	PasteboardSyncFlags flags;

//	fprint(2, "appleputsnarf\n");

	if(strlen(s) >= SnarfSize)
		return;
	qlock(&clip.lk);
	strcpy(clip.buf, s);
	runesnprint(clip.rbuf, nelem(clip.rbuf), "%s", s);
	if(clip.apple == nil){
		if(PasteboardCreate(kPasteboardClipboard, &clip.apple) != noErr){
			fprint(2, "apple pasteboard create failed\n");
			qunlock(&clip.lk);
			return;
		}
	}
	if(PasteboardClear(clip.apple) != noErr){
		fprint(2, "apple pasteboard clear failed\n");
		qunlock(&clip.lk);
		return;
	}
	flags = PasteboardSynchronize(clip.apple);
	if((flags&kPasteboardModified) || !(flags&kPasteboardClientIsOwner)){
		fprint(2, "apple pasteboard cannot assert ownership\n");
		qunlock(&clip.lk);
		return;
	}
	cfdata = CFDataCreate(kCFAllocatorDefault, 
		(uchar*)clip.rbuf, runestrlen(clip.rbuf)*2);
	if(cfdata == nil){
		fprint(2, "apple pasteboard cfdatacreate failed\n");
		qunlock(&clip.lk);
		return;
	}
	if(PasteboardPutItemFlavor(clip.apple, (PasteboardItemID)1,
		CFSTR("public.utf16-plain-text"), cfdata, 0) != noErr){
		fprint(2, "apple pasteboard putitem failed\n");
		CFRelease(cfdata);
		qunlock(&clip.lk);
		return;
	}
	/* CFRelease(cfdata); ??? */
	qunlock(&clip.lk);
}
static int useapplesnarf = -1;
static int
checkapplesnarf(void)
{
	char *x;
	
	x = getenv("USEX11SNARF");
	if(x && x[0])
		return 0;
	return 1;
}
#endif	/* APPLESNARF */

void
putsnarf(char *data)
{
#ifdef APPLESNARF
	appleputsnarf(data);
#endif
	_xputsnarf(_x.snarfcon, data);
}

char*
getsnarf(void)
{
	return _xgetsnarf(_x.snarfcon);
}

