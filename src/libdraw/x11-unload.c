#include "x11-inc.h"

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include "x11-memdraw.h"

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	if(i->X)
		xgetxdata(i, r);
	return _unloadmemimage(i, r, data, ndata);
}

