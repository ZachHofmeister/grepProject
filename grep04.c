/*
 * Editor
 */

#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

/* make BLKSIZE and LBSIZE 512 for smaller machines */
#define	BLKSIZE	4096
#define	NBLK	2047

#define	FNSIZE	128
#define	LBSIZE	4096
#define	ESIZE	256
#define	GBSIZE	256
#define	NBRA	5
#define	EOF	-1
#define	KSIZE	9

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12
#define	CBACK	14
#define	CCIRC	15

#define	STAR	01

char	Q[]	= "";
char	T[]	= "TMP";
#define	READ	0
#define	WRITE	1

int	peekc;
int	lastc;
char	file[FNSIZE];
char	linebuf[LBSIZE];
char	expbuf[ESIZE+4];
int	given;
unsigned int	*addr1, *addr2;
unsigned int	*dot, *dol, *zero;
char	genbuf[LBSIZE];
long	count;
char	*nextip;
int	ninbuf;
int	io;
int	pflag;
long lseek(int, long, int);
int	open(char *, int);
int	read(int, char*, int);
int	write(int, char*, int);
int	close(int);

int	oflag;
int	listf;
int	listn;
int	col;
char	*globp;
int	tline;
char	*loc1;
char	*loc2;
char	ibuff[BLKSIZE];
int	iblock	= -1;
char	obuff[BLKSIZE];
int	oblock	= -1;
int	ichanged;
int	nleft;
char	*braslist[NBRA];
char	*braelist[NBRA];
int	nbra;
char regex[BLKSIZE];
char * regexp;

char *getblock(unsigned int atl, int iof);
char *getline(unsigned int tl);
int advance(char *lp, char *ep);
int append(int (*f)(void), unsigned int *a);
int backref(int i, char *lp);
int cclass(char *set, int c, int af);
void commands(void);
void compile(int eof);
void error(char *s);
int execute(unsigned int *addr);
void exfile(void);
int getchr(void);
int getfile(void);
void global(int k);
void init(void);
void newline(void);
void print(void);
void putchr(int ac);
void putd(void);
int putline(void);
void puts(char *sp);

jmp_buf	savej;

/* these two are not in ansi, but we need them */
#define	SIGHUP	1	/* hangup */

int main(int argc, char *argv[]) {
	char *p1, *p2;
	argv++;

	if (argc>1) { //TODO this is where handling the regular expression will take place
		strcat(regex, "g/");
		strcat(regex, *argv);
		strcat(regex, "\n");
		++argv; --argc;
	}

	while (argc>1) { //Handles the file to read
		regexp = regex;
		strcpy(file, *argv++);
		globp = "r";
		zero = (unsigned *)malloc(128*sizeof(unsigned));
		init();
		commands();
		argc--;
	}

	
	return 0;
}

void commands(void) {
	unsigned int *a1=0;	int c;	char lastsep;
	for (;;) {
		c = '\n';
		for (addr1 = 0;;) {
			lastsep = c;
			c = getchr();
			if (c!=',' && c!=';') break; //TODO figure out why this is needed
		}
		//Can this be simplified? vvv
		if (lastsep!='\n') a1 = dol;
		if ((addr2 = a1) == 0) {
			given = 0;
			addr2 = dot;	
		} else given = 1;
		if (addr1 == 0) addr1 = addr2;

		switch(c) {
			case 'g': global(1); return;
			case 'p':
			case 'P': newline(); print(); continue;
			case 'r':
				globp = 0;
				if ((io = open(file, 0)) < 0) { //opens file as read only, is it < 0? open returns -1 if it fails.
					lastc = '\n';
					error(file);
				}
				ninbuf = 0;
				c = zero != dol;
				append(getfile, addr2); //opens the file, getfile reads it
				exfile(); //closes the file once it has been read
				continue;
			case EOF: return;
		}
		error(Q);
	}
}

void print(void) {
	unsigned int *a1;
	a1 = addr1;
	do {
		puts(file);
		putchr(':');
		puts(getline(*a1++));
		putchr('\n');
	} while (a1 <= addr2);
	dot = addr2;
	listf = 0;
	listn = 0;
	pflag = 0;
}

void newline(void) {
	int c;
	if ((c = getchr()) == '\n' || c == EOF) return;
	if (c=='p' || c=='l' || c=='n') {
		pflag++;
		if (c=='l') listf++;
		else if (c=='n') listn++;
		if ((c=getchr())=='\n') return;
	}
	error(Q);
}

void exfile(void) {
	close(io);
	io = -1;
}

void error(char *s) {
	int c;

	listf = 0;
	listn = 0;
	putchr('?');
	puts(s);
	count = 0;
	lseek(0, (long)0, 2);
	pflag = 0;
	if (globp) lastc = '\n';
	globp = 0;
	peekc = lastc;
	if (lastc) while ((c = getchr()) != '\n' && c != EOF) {}
	if (io > 0) { close(io); io = -1; }
	longjmp(savej, 1);
}

int getchr(void) {
	char c;
	if (lastc=peekc) { peekc = 0; return(lastc); }
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
			if ((ninbuf = read(io, genbuf, LBSIZE)-1) < 0) { //this call to read fills genbuf with the text from the input
				if (lp>linebuf) *genbuf = '\n';
				else return(EOF);
			}
			fp = genbuf;
			while(fp < &genbuf[ninbuf]) { if (*fp++ & 0200) break; } //while fp < address of last non-\0 in genbuf, idk
			fp = genbuf;
		}
		c = *fp++;
		if (c=='\0') continue; //skips the next lines if \0 found
		if (c&0200 || lp >= &linebuf[LBSIZE]) { lastc = '\n'; error(Q); }
		*lp++ = c;
		count++;
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	return(0);
}

int append(int (*f)(void), unsigned int *a) {
	unsigned int *a1, *a2, *rdot;
	int nline, tl;

	nline = 0;
	dot = a;
	while ((*f)() == 0) {
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
	int nl;
	lp = linebuf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl &= ~((BLKSIZE/2)-1);
	while (*lp++ = *bp++) if (--nl == 0) { bp = getblock(tl+=(BLKSIZE/2), READ); nl = nleft; }
	return(linebuf);
}

int putline(void) {
	char *bp, *lp;
	int nl;
	unsigned int tl;
	lp = linebuf;
	tl = tline;
	bp = getblock(tl, WRITE);
	nl = nleft;
	tl &= ~((BLKSIZE/2)-1);
	while (*bp = *lp++) {
		if (*bp++ == '\n') { *--bp = 0; break; }
		if (--nl == 0) { bp = getblock(tl+=(BLKSIZE/2), WRITE); nl = nleft; }
	}
	nl = tline;
	tline += (((lp-linebuf)+03)>>1)&077776;
	return(nl);
}

char * getblock(unsigned int atl, int iof) {
	int bno, off;
	bno = (atl/(BLKSIZE/2));
	off = (atl<<1) & (BLKSIZE-1) & ~03;
	if (bno >= NBLK) { lastc = '\n'; error(T); }
	nleft = BLKSIZE - off;
	if (bno==iblock) { ichanged |= iof; return(ibuff+off); }
	if (bno==oblock) return(obuff+off);
	if (iof==READ) {
		// if (ichanged) blkio(iblock, ibuff, write);
		ichanged = 0;
		iblock = bno;
		// blkio(bno, ibuff, read);
		return(ibuff+off);
	}
	// if (oblock>=0) blkio(oblock, obuff, write);
	oblock = bno;
	return(obuff+off);
}

void init(void) {
	tline = 2;
	iblock = -1;
	oblock = -1;
	ichanged = 0;
	dot = dol = zero;
}

void global(int k) {
	char *gp;
	int c;
	unsigned int *a1;
	char globuf[GBSIZE];
	if (globp) error(Q);
	if (!given) { addr1 = zero + (dol>zero); addr2 = dol; }
	if ((c=getchr())=='\n') error(Q);
	compile(c);
	gp = globuf;
	while ((c = getchr()) != '\n') { //skips right past this because the next c is a new line
		if (c==EOF) error(Q);
		if (c=='\\') { c = getchr(); if (c!='\n') {*gp++ = '\\'; } }
		*gp++ = c;
		if (gp >= &globuf[GBSIZE-2]) error(Q);
	}
	if (gp == globuf) *gp++ = 'p';
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		*a1 &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(a1)==k) *a1 |= 01;
	}
	for (a1=zero; a1<=dol; a1++) {
		if (*a1 & 01) { *a1 &= ~01; dot = a1; globp = globuf; commands(); a1 = zero; }
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
	if ((c = getchr()) == '\n') { peekc = c; c = eof; }
	if (c == eof) { if (*ep==0) { error(Q); } return; }
	nbra = 0;
	if (c=='^') { c = getchr(); *ep++ = CCIRC; }
	peekc = c;
	lastep = 0;
	for (;;) {
		if (ep >= &expbuf[ESIZE]) goto cerror;
		c = getchr();
		if (c == '\n') { peekc = c; c = eof; }
		if (c==eof) { if (bracketp != bracket) { goto cerror; } *ep++ = CEOF; return; }
		if (c!='*') lastep = ep;
		switch (c) {
			case '\\':
				if ((c = getchr())=='(') {
					if (nbra >= NBRA) goto cerror;
					*bracketp++ = nbra;
					*ep++ = CBRA;
					*ep++ = nbra++;
					continue;
				}
				if (c == ')') {
					if (bracketp <= bracket) goto cerror;
					*ep++ = CKET;
					*ep++ = *--bracketp;
					continue;
				}
				if (c>='1' && c<'1'+NBRA) { *ep++ = CBACK; *ep++ = c-'1'; continue; }
				*ep++ = CCHR;
				if (c=='\n') goto cerror;
				*ep++ = c;
				continue;
			case '.': *ep++ = CDOT; continue;
			case '\n': goto cerror;
			case '*': if (lastep==0 || *lastep==CBRA || *lastep==CKET) { goto defchar; } *lastep |= STAR; continue;
			case '$': if ((peekc=getchr()) != eof && peekc!='\n') { goto defchar; } *ep++ = CDOL; continue;
			case '[':
				*ep++ = CCL;
				*ep++ = 0;
				cclcnt = 1;
				if ((c=getchr()) == '^') { c = getchr(); ep[-2] = NCCL; }
				do {
					if (c=='\n') goto cerror;
					if (c=='-' && ep[-1]!=0) {
						if ((c=getchr())==']') { *ep++ = '-'; cclcnt++; break; }
						while (ep[-1]<c) {
							*ep = ep[-1]+1;
							ep++;
							cclcnt++;
							if (ep>=&expbuf[ESIZE]) goto cerror;
						}
					}
					*ep++ = c;
					cclcnt++;
					if (ep >= &expbuf[ESIZE]) goto cerror;
				} while ((c = getchr()) != ']');
				lastep[1] = cclcnt;
				continue;
			default: defchar: *ep++ = CCHR; *ep++ = c;
		}
	}
	cerror:
	expbuf[0] = 0;
	nbra = 0;
	error(Q);
}

int execute(unsigned int *addr) {
	char *p1, *p2;
	int c;
	for (c=0; c<NBRA; c++) { braslist[c] = 0; braelist[c] = 0; }
	p2 = expbuf;
	if (addr == (unsigned *)0) {
		if (*p2==CCIRC) return(0);
		p1 = loc2;
	} else if (addr==zero) return(0);
	else p1 = getline(*addr);
	if (*p2==CCIRC) { loc1 = p1; return(advance(p1, p2+1)); }
	if (*p2==CCHR) { //fast check for first character
		c = p2[1];
		do {
			if (*p1!=c) continue;
			if (advance(p1, p2)) { loc1 = p1; return(1); }
		} while (*p1++);
		return(0);
	}
	do { //regular algorithm
		if (advance(p1, p2)) { loc1 = p1; return(1); }
	} while (*p1++);
	return(0);
}

int advance(char *lp, char *ep) {
	char *curlp;
	int i;
	for (;;) switch (*ep++) {
		case CCHR: if (*ep++ == *lp++) { continue; } return(0);
		case CDOT: if (*lp++) { continue; } return(0);
		case CDOL: if (*lp==0) { continue; } return(0);
		case CEOF: loc2 = lp; return(1);
		case CCL: if (cclass(ep, *lp++, 1)) { ep += *ep; continue; } return(0);
		case NCCL: if (cclass(ep, *lp++, 0)) { 	ep += *ep; 	continue; } return(0);
		case CBRA: braslist[*ep++] = lp; continue;
		case CKET: braelist[*ep++] = lp; continue;
		case CBACK:
			if (braelist[i = *ep++]==0)
				error(Q);
			if (backref(i, lp)) {
				lp += braelist[i] - braslist[i];
				continue;
			}
			return(0);
		case CBACK|STAR:
			if (braelist[i = *ep++] == 0) error(Q);
			curlp = lp;
			while (backref(i, lp)) lp += braelist[i] - braslist[i];
			while (lp >= curlp) {
				if (advance(lp, ep)) return(1);
				lp -= braelist[i] - braslist[i];
			}
			continue;
		case CDOT|STAR: curlp = lp; while (*lp++) {} goto star;
		case CCHR|STAR: curlp = lp; while (*lp++ == *ep) {} ep++; goto star;
		case CCL|STAR:
		case NCCL|STAR: curlp = lp; while (cclass(ep, *lp++, ep[-1]==(CCL|STAR))) {} ep += *ep; goto star;
		star:
			do {
				lp--;
				if (advance(lp, ep)) return(1);
			} while (lp > curlp);
			return(0);
		default: error(Q);
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

void putd(void) {
	int r;
	r = count%10;
 	count /= 10;
	if (count) putd();
	putchr(r + '0');
}

void puts(char *sp) {
	col = 0;
	while (*sp) putchr(*sp++);
	// putchr('\n');
}

char	line[70];
char	*linp	= line;

void putchr(int ac) {
	char *lp;
	int c;

	lp = linp;
	c = ac;
	if (listf) {
		if (c=='\n') {
			if (linp!=line && linp[-1]==' ') { *lp++ = '\\'; *lp++ = 'n'; }
		} else {
			if (col > (72-4-2)) {
				col = 8;
				*lp++ = '\\';
				*lp++ = '\n';
				*lp++ = '\t';
			}
			col++;
			if (c=='\b' || c=='\t' || c=='\\') {
				*lp++ = '\\';
				if (c=='\b') c = 'b';
				else if (c=='\t') c = 't';
				col++;
			} else if (c<' ' || c=='\177') {
				*lp++ = '\\';
				*lp++ =  (c>>6)    +'0';
				*lp++ = ((c>>3)&07)+'0';
				c     = ( c    &07)+'0';
				col += 3;
			}
		}
	}
	*lp++ = c;
	if(c == '\n' || lp >= &line[64]) { linp = line; write(oflag?2:1, line, lp-line); return; }
	linp = lp;
} 