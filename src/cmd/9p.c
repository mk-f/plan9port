#include <u.h>
#include <signal.h>
#include <libc.h>
#include <bio.h>
#include <fcall.h>
#include <9pclient.h>
#include <auth.h>
#include <thread.h>

char *addr;

void
usage(void)
{
	fprint(2, "usage: 9p [-a address] [-A aname] cmd args...\n");
	fprint(2, "possible cmds:\n");
	fprint(2, "	read name\n");
	fprint(2, "	readfd name\n");
	fprint(2, "	write [-l] name\n");
	fprint(2, "	writefd name\n");
	fprint(2, "	stat name\n");
	fprint(2, "	rdwr name\n");
	fprint(2, "	ls [-ld] name\n");
	fprint(2, "without -a, name elem/path means /path on server unix!$ns/elem\n");
	threadexitsall("usage");
}

char *aname;
void xread(int, char**);
void xwrite(int, char**);
void xreadfd(int, char**);
void xwritefd(int, char**);
void xstat(int, char**);
void xls(int, char**);
void xrdwr(int, char**);
void xcon(int, char**);

struct {
	char *s;
	void (*f)(int, char**);
} cmds[] = {
	"con", xcon,
	"read", xread,
	"write", xwrite,
	"readfd", xreadfd,
	"writefd", xwritefd,
	"stat", xstat,
	"rdwr", xrdwr,
	"ls", xls,
};

void
threadmain(int argc, char **argv)
{
	char *cmd;
	int i;

	ARGBEGIN{
	case 'A':
		aname = EARGF(usage());
		break;
	case 'a':
		addr = EARGF(usage());
		if(strchr(addr, '!') == nil)
			addr = netmkaddr(addr, "tcp", "9fs");
		break;
	case 'D':
		chatty9pclient = 1;
		break;
	default:
		usage();
	}ARGEND

	signal(SIGINT, SIG_DFL);

	if(argc < 1)
		usage();

	cmd = argv[0];
	for(i=0; i<nelem(cmds); i++){
		if(strcmp(cmds[i].s, cmd) == 0){
			cmds[i].f(argc, argv);
			threadexitsall(0);
		}
	}
	usage();	
}

CFsys*
xparse(char *name, char **path)
{
	int fd;
	char *p;
	CFsys *fs;

	if(addr == nil){
		p = strchr(name, '/');
		if(p == nil)
			p = name+strlen(name);
		else
			*p++ = 0;
		*path = p;
		fs = nsamount(name, aname);
		if(fs == nil)
			sysfatal("mount: %r");
	}else{
		*path = name;
		if((fd = dial(addr, nil, nil, nil)) < 0)
			sysfatal("dial: %r");
		if((fs = fsamount(fd, aname)) == nil)
			sysfatal("fsamount: %r");
	}
	return fs;
}

CFid*
xopen(char *name, int mode)
{
	CFid *fid;
	CFsys *fs;

	fs = xparse(name, &name);
	fid = fsopen(fs, name, mode);
	if(fid == nil)
		sysfatal("fsopen %s: %r", name);
	return fid;
}

int
xopenfd(char *name, int mode)
{
	CFsys *fs;

	fs = xparse(name, &name);
	return fsopenfd(fs, name, mode);
}

void
xread(int argc, char **argv)
{
	char buf[4096];
	int n;
	CFid *fid;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fid = xopen(argv[0], OREAD);
	while((n = fsread(fid, buf, sizeof buf)) > 0)
		write(1, buf, n);
	fsclose(fid);
	if(n < 0)
		sysfatal("read error: %r");
	threadexitsall(0);	
}

void
xreadfd(int argc, char **argv)
{
	char buf[4096];
	int n;
	int fd;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fd = xopenfd(argv[0], OREAD);
	while((n = read(fd, buf, sizeof buf)) > 0)
		write(1, buf, n);
	if(n < 0)
		sysfatal("read error: %r");
	threadexitsall(0);	
}

void
xwrite(int argc, char **argv)
{
	char buf[4096];
	int n, did;
	CFid *fid;
	Biobuf *b;
	char *p;
	int byline;

	byline = 0;
	ARGBEGIN{
	case 'l':
		byline = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	did = 0;
	fid = xopen(argv[0], OWRITE|OTRUNC);
	if(byline){
		n = 0;
		b = malloc(sizeof *b);
		if(b == nil)
			sysfatal("out of memory");
		Binit(b, 0, OREAD);
		while((p = Brdstr(b, '\n', 0)) != nil){
			n = strlen(p);
			did = 1;
			if(fswrite(fid, p, n) != n)
				fprint(2, "write: %r\n");
		}
		free(b);
	}else{
		while((n = read(0, buf, sizeof buf)) > 0){
			did = 1;
			if(fswrite(fid, buf, n) != n)
				sysfatal("write error: %r");
		}
	}
	if(n == 0 && !did){
		if(fswrite(fid, buf, 0) != 0)
			sysfatal("write error: %r");
	}
	if(n < 0)
		sysfatal("read error: %r");
	fsclose(fid);
	threadexitsall(0);	
}

void
xwritefd(int argc, char **argv)
{
	char buf[4096];
	int n;
	int fd;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fd = xopenfd(argv[0], OWRITE|OTRUNC);
	while((n = read(0, buf, sizeof buf)) > 0)
		if(write(fd, buf, n) != n)
			sysfatal("write error: %r");
	if(n < 0)
		sysfatal("read error: %r");
	threadexitsall(0);	
}

void
xstat(int argc, char **argv)
{
	Dir *d;
	CFsys *fs;
	char *name;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	name = argv[0];
	fs = xparse(name, &name);
	if((d = fsdirstat(fs, name)) == 0)
		sysfatal("dirstat: %r");
	fmtinstall('D', dirfmt);
	fmtinstall('M', dirmodefmt);
	print("%D\n", d);
	threadexitsall(0);
}

void
xrdwr(int argc, char **argv)
{
	char buf[4096];
	int n;
	CFid *fid;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fid = xopen(argv[0], ORDWR);
	for(;;){
		if((n = fsread(fid, buf, sizeof buf)) < 0)
			fprint(2, "read: %r\n");
		else{
			write(1, buf, n);
			write(1, "\n", 1);
		}
		n = read(0, buf, sizeof buf);
		if(n <= 0)
			break;
		if(buf[n-1] == '\n')
			n--;
		if(fswrite(fid, buf, n) != n)
			fprint(2, "write: %r\n");
	}
	fsclose(fid);
	threadexitsall(0);	
}

void
rdcon(void *v)
{
	char buf[4096];
	CFid *fid;
	
	fid = v;
	for(;;){
		n = read(0, buf, sizeof buf);
		if(n <= 0)
			threadexitsall(0);
		if(fswrite(fid, buf, n) != n)
			fprint(2, "write: %r\n");
	}
}

void
xcon(int argc, char **argv)
{
	char buf[4096], *r, *w, *e;
	int n, nocr;
	CFid *fid;
	
	nocr = 1;

	ARGBEGIN{
	case 'r':
		nocr = 0;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fid = xopen(argv[0], ORDWR);
	proccreate(rdcon, fid, STACK);
	for(;;){
		n = fsread(fid, buf, n);
		if(n <= 0)
			threadexitsall(0);
		if(nocr){
			for(r=w=buf, e=buf+n; r<e; r++)
				if(*r != '\r')
					*w++ = *r;
			n = w-buf;
		}
		if(write(1, buf, n) != n)
			threadexitsall(0);
	}
	fsclose(fid);
	threadexitsall(0);	
}

static char *mon[] = 
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


int
timefmt(Fmt *fmt)
{
	ulong u;
	static ulong time0;
	Tm *tm;
	
	if(time0 == 0)
		time0 = time(0);
	u = va_arg(fmt->args, ulong);
	tm = localtime(u);
	if((long)(time0-u) < 6*30*86400)
		return fmtprint(fmt, "%s %2d %02d:%02d",
			mon[tm->mon], tm->mday, tm->hour, tm->min);
	return fmtprint(fmt, "%s %2d %5d",
		mon[tm->mon], tm->mday, tm->year+1900);
}

static int
dircmp(const void *va, const void *vb)
{
	Dir *a, *b;
	
	a = (Dir*)va;
	b = (Dir*)vb;
	return strcmp(a->name, b->name);
}

void
xls(int argc, char **argv)
{
	char *err, *name, *xname, *f[4], buf[4096];
	int nf, i, j, l;
	int lflag, dflag, n, len[4];
	Dir *d;
	CFid *fid;
	CFsys *fs;

	err = nil;
	lflag = dflag = 0;
	ARGBEGIN{
	case 'l':
		lflag = 1;
		break;
	case 'd':
		dflag = 1;
		break;
	}ARGEND
	
	fmtinstall('D', dirfmt);
	fmtinstall('M', dirmodefmt);
	quotefmtinstall();
	fmtinstall('T', timefmt);
	
	for(i=0; i<argc; i++){
		name = argv[i];
		fs = xparse(name, &xname);
		if((d = fsdirstat(fs, xname)) == nil){
			fprint(2, "dirstat %s: %r\n", name);
			fsunmount(fs);
			err = "errors";
			continue;
		}
		if((d->mode&DMDIR) && !dflag){
			if((fid = fsopen(fs, xname, OREAD)) == nil){
				fprint(2, "open %s: %r\n", name);
				fsunmount(fs);
				free(d);
				err = "errors";
				continue;
			}
			free(d);
			n = fsdirreadall(fid, &d);
			fsclose(fid);
			if(n < 0){
				fprint(2, "dirreadall %s: %r\n", name);
				fsunmount(fs);
				err = "errors";
				continue;
			}
			qsort(d, n, sizeof d[0], dircmp);
			for(j=0; j<5; j++)
				len[j] = 0;
			for(i=0; i<n; i++){
				d[i].type = 'M';
				d[i].dev = 0;
				snprint(buf, sizeof buf, "%d %s %s %lld",
					d[i].dev, d[i].uid, d[i].gid, d[i].length);
				nf = getfields(buf, f, 4, 0, " ");
				for(j=0; j<4; j++){
					l = strlen(f[j]);
					if(l > len[j])
						len[j] = l;
				}
			}
			for(i=0; i<n; i++)
				print("%M %C %*d %*s %*s %*lld %T %q\n",
					d[i].mode, d[i].type, len[0], d[i].dev,
					-len[1], d[i].uid, -len[2], d[i].gid,
					len[3], d[i].length, d[i].mtime, d[i].name);
			
		}else{
			d->type = 'M';
			d->dev = 0;
			print("%M %C %d %s %s %lld %T %q\n",
				d->mode, d->type, d->dev,
				d->uid, d->gid, d->length, d->mtime, d->name);
		}
		free(d);
	}
	threadexitsall(err);
}

