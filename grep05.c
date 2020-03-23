#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define	BLKSIZE	4096
#define	NBRA	5
#define	EOF		-1
#define	KSIZE	9

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL		6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12
#define	CBACK	14
#define	CCIRC	15

#define	STAR	01

int peekc, lastc, ninbuf, io;
char file[BLKSIZE], linebuf[BLKSIZE], expbuf[BLKSIZE], genbuf[BLKSIZE], *nextip;
unsigned int *addr1, *addr2, *dot, *dol, *zero;
int	open(char *, int);
int	read(int, char*, int);
int	write(int, char*, int);
int	close(int);

char *globp;
int	tline;
char *loc1;
char *loc2;
char obuff[BLKSIZE]; //TODO find a way to remove, idk why it is needed since it is used once and never initialized
char *braslist[NBRA];
char *braelist[NBRA];
int	nbra;
char regex[BLKSIZE];
char * regexp;

char *getblock(unsigned int atl);
char *getline(unsigned int tl);
int advance(char *lp, char *ep);
int append(unsigned int *a);
int backref(int i, char *lp);
int cclass(char *set, int c, int af);
void commands(void);
void compile(int eof);
int execute(unsigned int *addr);
void exfile(void);
int getchr(void);
int getfile(void);
void global(int k);
void print(void);
void putchr(int ac);
int putline(void);
void puts(char *sp);

int main(int argc, char *argv[]) {
	argv++;
	strcat(regex, "g/");
	strcat(regex, *argv++);
	strcat(regex, "\n");
	--argc;
	while (argc>1) { //Handles the file to read
		regexp = regex;
		strcpy(file, *argv++);
		globp = "r";
		dot = dol = zero = (unsigned *)malloc(128*sizeof(unsigned));
		commands();
		argc--;
	}
	return 0;
}

void commands(void) {
	for (;;) {
		addr1 = addr2 = dot;
		switch(getchr()) {
			case 'p':
				print();
				continue;
			case 'r':
				globp = 0;
				io = open(file, 0);
				ninbuf = 0;
				append(addr2); //opens the file and reads it
				exfile(); //closes the file once it has been read
				continue;
			case 'g': global(1);
			case EOF: return;
		}
	}
}

void print(void) {
	puts(file);
	putchr(':');
	puts(getline(*addr1));
	putchr('\n');
}

void exfile(void) {
	close(io);
	io = -1;
}

int getchr(void) {
	char c;
	if (lastc=peekc) {
		peekc = 0;
		return(lastc);
	}
	if (globp) { //if globp != 0 aka if globp points to memory (note: globp is initially assigned to "r" in the main)
		if ((lastc = *globp++) != 0) return(lastc); //if the value globp points to is a vaild character (not 0), return that
		globp = 0;
		return(EOF);
	}
	// if (read(0, &c, 1) <= 0) return(lastc = EOF); //reads a character from stdin or the open file
	if ((c = *regexp++) == '\0') return(lastc = EOF);
	lastc = c&0177;
	return(lastc);
}

int getfile(void) { //probably a good function as it is
	int c;
	char *lp, *fp;
	lp = linebuf;
	fp = nextip;
	do {
		if (--ninbuf < 0) {
			if ((ninbuf = read(io, genbuf, BLKSIZE)-1) < 0) { //this call to read fills genbuf with the text from the input
				if (lp>linebuf) *genbuf = '\n';
				else return(EOF);
			}
			fp = genbuf;
		}
		*lp++ = c = *fp++;
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	return(0);
}

int append(unsigned int *a) {
	unsigned int *a1, *a2, *rdot;
	int nline, tl;
	nline = 0;
	dot = a;
	while (getfile() == 0) {
		tl = putline();
		nline++;
		a1 = ++dol;
		a2 = a1+1;
		rdot = ++dot;
		while (a1 > rdot) *--a2 = *--a1;
		*rdot = tl;
	}
	return(nline);
}

char * getline(unsigned int tl) {
	char *bp, *lp;
	lp = linebuf;
	bp = getblock(tl);
	tl &= ~((BLKSIZE/2)-1);
	while (*lp++ = *bp++) {}
	return(linebuf);
}

int putline(void) {
	char *bp, *lp;
	int nl;
	unsigned int tl;
	lp = linebuf;
	tl = tline;
	bp = getblock(tl);
	tl &= ~((BLKSIZE/2)-1);
	while (*bp = *lp++)
		if (*bp++ == '\n') {
			*--bp = 0;
			break;
		}
	nl = tline;
	tline += (((lp-linebuf)+03)>>1)&077776;
	return(nl);
}

char * getblock(unsigned int atl) { return(obuff + ((atl<<1) & (BLKSIZE-1) & ~03)); }

void global(int k) {
	char *gp;
	int c;
	unsigned int *a1;
	char globuf[BLKSIZE];
	addr1 = zero + (dol>zero);
	addr2 = dol;
	c = getchr();
	compile(c);
	gp = globuf;
	getchr();
	//following is all important
	if (gp == globuf) *gp++ = 'p';
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		*a1 &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(a1)==k) *a1 |= 01;
	}
	for (a1=zero; a1<=dol; a1++) {
		if (*a1 & 01) {
			*a1 &= ~01;
			dot = a1;
			globp = globuf;
			commands();
			a1 = zero;
		}
	}
}

void compile(int eof) { //reads characters from input as a regex expression
	int c;
	char *ep;
	char *lastep;
	char bracket[NBRA], *bracketp;
	int cclcnt;

	ep = expbuf;
	bracketp = bracket;
	if ((c = getchr()) == '\n') {
		peekc = c;
		c = eof;
	}
	if (c == eof) return;
	nbra = 0;
	if (c=='^') {
		c = getchr();
		*ep++ = CCIRC;
	}
	peekc = c;
	lastep = 0;
	for (;;) {
		c = getchr();
		if (c == '\n') {
			peekc = c;
			c = eof;
		}
		if (c==eof) {
			*ep++ = CEOF;
			return;
		}
		if (c!='*') lastep = ep;
		switch (c) {
			case '\\':
				if ((c = getchr())=='(') {
					*bracketp++ = nbra;
					*ep++ = CBRA;
					*ep++ = nbra++;
					continue;
				}
				if (c == ')') {
					*ep++ = CKET;
					*ep++ = *--bracketp;
					continue;
				}
				if (c>='1' && c<'1'+NBRA) {
					*ep++ = CBACK;
					*ep++ = c-'1';
					continue;
				}
				*ep++ = CCHR;
				*ep++ = c;
				continue;
			case '.': *ep++ = CDOT;
				continue;
			case '\n': return;
			case '*': if (lastep==0 || *lastep==CBRA || *lastep==CKET) goto defchar;
				*lastep |= STAR;
				continue;
			case '$': if ((peekc=getchr()) != eof && peekc!='\n') goto defchar;
				*ep++ = CDOL;
				continue;
			case '[':
				*ep++ = CCL;
				*ep++ = 0;
				cclcnt = 1;
				if ((c=getchr()) == '^') {
					c = getchr();
					ep[-2] = NCCL;
				}
				do {
					if (c=='-' && ep[-1]!=0) {
						if ((c=getchr())==']') {
							*ep++ = '-';
							cclcnt++;
							break;
						}
						while (ep[-1]<c) {
							*ep = ep[-1]+1;
							ep++;
							cclcnt++;
						}
					}
					*ep++ = c;
					cclcnt++;
				} while ((c = getchr()) != ']');
				lastep[1] = cclcnt;
				continue;
			default: defchar: *ep++ = CCHR;
				*ep++ = c;
		}
	}
}

int execute(unsigned int *addr) {
	char *p1, *p2;
	int c;
	for (c=0; c<NBRA; c++) braslist[c] = braelist[c] = 0;
	p2 = expbuf;
	if (addr == (unsigned *)0) {
		if (*p2==CCIRC) return(0);
		p1 = loc2;
	} else if (addr==zero) return(0);
	else p1 = getline(*addr);
	if (*p2==CCIRC) {
		loc1 = p1;
		return(advance(p1, p2+1));
	}
	do { //regular algorithm
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

int advance(char *lp, char *ep) {
	char *curlp;
	int i;
	for (;;) switch (*ep++) {
		case CCHR: if (*ep++ == *lp++) continue;
			return(0);
		case CDOT: if (*lp++) continue;
			return(0);
		case CDOL: if (*lp==0) continue;
			return(0);
		case CEOF: loc2 = lp;
			return(1);
		case CCL: if (cclass(ep, *lp++, 1)) {
				ep += *ep;
				continue;
			}
			return(0);
		case NCCL: if (cclass(ep, *lp++, 0)) {
				ep += *ep;
				continue;
			}
			return(0);
		case CBRA: braslist[*ep++] = lp;
			continue;
		case CKET: braelist[*ep++] = lp;
			continue;
		case CBACK:
			i = *ep++;
			if (backref(i, lp)) {
				lp += braelist[i] - braslist[i];
				continue;
			}
			return(0);
		case CBACK|STAR:
			i = *ep++;
			curlp = lp;
			while (backref(i, lp)) lp += braelist[i] - braslist[i];
			while (lp >= curlp) {
				if (advance(lp, ep)) return(1);
				lp -= braelist[i] - braslist[i];
			}
			continue;
		case CDOT|STAR: curlp = lp;
			while (*lp++) {}
			goto star;
		case CCHR|STAR: curlp = lp;
			while (*lp++ == *ep) {}
			ep++;
			goto star;
		case CCL|STAR:
		case NCCL|STAR: curlp = lp;
			while (cclass(ep, *lp++, ep[-1]==(CCL|STAR))) {}
			ep += *ep;
			goto star;
		star:
			do {
				lp--;
				if (advance(lp, ep)) return(1);
			} while (lp > curlp);
			return(0);
	}
}

int backref(int i, char *lp) {
	char *bp;
	bp = braslist[i];
	while (*bp++ == *lp++) if (bp >= braelist[i]) return(1);
	return(0);
}

int cclass(char *set, int c, int af) {
	int n;
	if (c==0) return(0);
	n = *set++;
	while (--n) if (*set++ == c) return(af);
	return(!af);
}

void puts(char *sp) { while (*sp) putchr(*sp++); }

char line[70], *linp = line;

void putchr(int ac) {	
	char *lp;
	int c;

	lp = linp;
	c = ac;
	*lp++ = c;
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		write(1, line, lp-line);
		return;
	}
	linp = lp;
} 