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
#include "fns.h"

char *MMsystem[] = {
	"Del",
	"Snarf",
	"Undo",
	"Redo",
	"Get",
	"Put",
	"Look",
	"Run",
};
static
int nbset(int nb)
{
	int n;
	for(n = 0; nb; nb&=(nb-1)) // Kernighans algorithem
		n++;
	return n;
}
void mminit(Window *w)
{
	w->mmenu.isys = 0;
	w->mmenu.iuser = nil;
	w->mmenu.niuser = 0;
	w->mmenu.nimax = 15;
	w->mmenu.udirty = 1;
	w->mmenu.istore =
		emalloc(sizeof(*(w->mmenu.istore))*(w->mmenu.nimax+1));
	w->mmenu.istore[MMUserStart] = nil;
	w->mmenu.menu.lasthit = MMSnarf;
}
void mmfree(Window *w)
{
	mmuserfree(w);
	free(w->mmenu.istore);
}
void mmupdate(Window *w)
{
	u8int isys;
	char **isysstart;
	int i, j, n, ln, lh;

	isysstart = w->mmenu.istore+8;

	/* check if system-part changed */
	isys = 0;
	isys |= (1<<MMDel);
	isys |= (1<<MMSnarf);
	isys |= (1<<MMLook);
	isys |= (1<<MMRun);
	if(w->filemenu){
		if(w->body.needundo || w->body.file->delta.nc>0 || w->body.ncache)
			isys |= (1<<MMUndo);
		if(w->body.file->epsilon.nc > 0)
			isys |= (1<<MMRedo);
		if(!w->isdir &&
			(w->body.file->nname &&
				(w->body.ncache || w->body.file->seq!=w->putseq)))
			isys |= (1<<MMPut);
	}
	if(w->isdir)
		isys |= (1<<MMPut);
	if(isys == w->mmenu.isys && !w->mmenu.udirty)
		return;
	n = nbset(isys);
	ln = nbset(w->mmenu.isys);
	isysstart = w->mmenu.istore + (MMUserStart-n);
	if(isys != w->mmenu.isys){
		/* last hit was in system area */
		if(w->mmenu.menu.lasthit < ln && w->mmenu.menu.lasthit != -1){
			/* figure out what was hit last time (MMsystem[lh]) */
			for(j=0,lh=0;lh<MMUserStart;lh++){
				if(w->mmenu.isys & (1<<lh))
					j++;
				if(j==w->mmenu.menu.lasthit+1)
					break;
			}
		}else{ /* last hit in user-area, adjust for bigger/smaller system area */
			lh = -1;
			w->mmenu.menu.lasthit += (n - ln);
		}
		for(j=0,i=0;i<MMUserStart;i++){
			if(isys & (1<<i)){
				isysstart[j] = MMsystem[i];
				if(i == lh){
					lh = -1;
					w->mmenu.menu.lasthit = j;
				}
				j++;
			}
		}
		if(lh != -1) /* could not adjust (e.g after put, put is gone) */
			w->mmenu.menu.lasthit = MMSnarf;
		w->mmenu.isys = isys;
	}
	if(w->mmenu.udirty){
		if(w->mmenu.niuser + MMUserStart + 1 > w->mmenu.nimax){
			w->mmenu.nimax +=
				(w->mmenu.niuser + MMUserStart + 1) - w->mmenu.nimax;
			w->mmenu.istore =
				erealloc(w->mmenu.istore,
					sizeof(*(w->mmenu.istore))*w->mmenu.nimax);
			isysstart = w->mmenu.istore + (MMUserStart-n);
		}
		for(i=MMUserStart,j=0;j<w->mmenu.niuser;i++,j++)
			w->mmenu.istore[i] = w->mmenu.iuser[j];
		w->mmenu.istore[i] = nil;
		w->mmenu.udirty = 0;
		/* last hit was in user-area, don't try to adjust, set
			to default */
		if(w->mmenu.menu.lasthit >= ln)
			w->mmenu.menu.lasthit = -1;
	}
	w->mmenu.menu.item = isysstart;
}
void mmuserfree(Window *w)
{
	int i;
	for(i=0;i<w->mmenu.niuser;i++)
		free(w->mmenu.iuser[i]);
	free(w->mmenu.iuser);
	w->mmenu.iuser = nil;
	w->mmenu.niuser = 0;
}
void mmuseritems(Window *w, char **items, int n)
{
	w->mmenu.iuser = items;
	w->mmenu.niuser = n;
	w->mmenu.udirty = 1;
}
