/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>
#include "fsimpl.h"

long
fspread(Fid *fid, void *buf, long n, vlong offset)
{
	Fcall tx, rx;
	void *freep;
	uint msize;

	msize = fid->fs->msize - IOHDRSZ;
	if(n > msize)
		n = msize;
	tx.type = Tread;
	tx.fid = fid->fid;
	if(offset == -1){
		qlock(&fid->lk);
		tx.offset = fid->offset;
		qunlock(&fid->lk);
	}else
		tx.offset = offset;
	tx.count = n;

	if(fsrpc(fid->fs, &tx, &rx, &freep) < 0)
		return -1;
	if(rx.type == Rerror){
		werrstr("%s", rx.ename);
		free(freep);
		return -1;
	}
	if(rx.count){
		memmove(buf, rx.data, rx.count);
		if(offset == -1){
			qlock(&fid->lk);
			fid->offset += rx.count;
			qunlock(&fid->lk);
		}
	}
	free(freep);
	
	return rx.count;
}

long
fsread(Fid *fid, void *buf, long n)
{
	return fspread(fid, buf, n, -1);
}
