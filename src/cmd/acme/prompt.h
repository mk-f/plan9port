typedef struct Prompt Prompt;
struct Prompt {
	Keyboardctl *kbc;
	Rectangle r;
	Image *backup;
	Frame *frame;
	Rune *buf;
	ulong nbuf;
	ulong end;
	ulong tick;
};

Prompt *prinit(Keyboardctl *kbc, Rune *buf, ulong nbuf);
void prfree(Prompt *p);
void prdraw(Prompt *p, Image *scr, Point pos, ulong width);
