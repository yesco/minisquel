#include <stdio.h>
#include <ctype.h>
#include <math.h>

int countbits(unsigned long v) {
  int n= 0;
  for(int i=0; i<8; i++) {
    char c= v && 0xff;
    if (c) {
      while(c & 0x01) {
	n++;
	c<<= 1;
      }
    }
    v>>= 8;
  }
}

int nhead= 0; //global param@
int c64(long chars) {
  char head= 0;
  int n= 0;
  long v= chars;
  int a2z= 1;
  // TODO:int digits= 0;
  for(int i=0; i<8; i++) {
    head<<= 1;
    if (v & 0xff) {
      if (i<4) a2z= 0; // < '@'
      if (i==4 && (v & 0x01)) a2z= 0; // @
      if (i==7 && (v & 0xf0)) a2z= 0; // {\}~
      head+= 1; n++;
    }
    v>>= 8;
  }
  n++; // for the head

  // TODO: remove
  // 0000 0000  => 64 chars all in!
  //      xxxx  lower c<64
  // (actual bits is waste, can just num?)
  //    1       a-z proper in (no @[\]_
  //   1        digits all in
  //  1         ABCD EFGH  IJKL MNOP in!
  // 1          a-z: 3(!) bytes pick chars
  //              ABCD EFGH
  //              IJKL MNOP
  //            ! RSTU VWY (qxz)= 1 bit!

  char chead= head & 0x0f;
  nhead= 1; // global!
  if (n > 32-6) chead= 0; // 64 chars all in!
  else if (!(chars & 0xfff20000ffffffffl)) chead|= 0x40; // a-p all in
  else if (n > 16-6 && a2z) chead= 0x10; // a-z ONLY (?)
  // TODO: above adjut bits needed to 5 !!!
  else if (chars & 0xffffffff00000000l) {
    chead|= 0x80;
    if (countbits(chars & 0x00ffffff00000000l)>8)
      nhead+= 1; // only need R-Z !!
    else
      nhead+= 3; // 3 bytes bitmap follow (compressed)
  }
  
  printf("DATA c64: head=%02x bytes=%d COMPRESSED\n", chead, nhead);
  printf("DATA c64: head=%02x bytes=%d\n", head, n);
  return n;
}

typedef struct alphaout {
  int bits;
  int c64;
  int nhead;
} alphaout;

alphaout alphabet(char* s) {
  long a[128/8]= {0};
  int c, n= 0;
  printf("DATA alphachars\t\t'");
  while((c= *s++)) {
    // these are already allocated always
    if (strchr("\n .,!-", c)) continue;
    // fold small to CAPS (more dense => 64)
    c= toupper(c); // mostly correct
    // TODO: CAP WRODS w prefix/shift
    c-= 32;
    int i= c/64;
    long m= 1l<<(c & 63);
    if (!(a[i] & m)) {
      putchar(c+32);
      n++;
      a[i] |= m;
    }
  }
  printf("'\n");
  printf("DATA alpha ");
  for(int c=0; c<128; c++) {
    if (c % 8 == 0) putchar('|');
    int i= c/64;
    long m= 1l<<(c & 63);
    //putchar((a[i] & m)? c+32 : ' ');
    if (a[i] & m) putchar(c+32);
  }
  printf("  %d bits set\n", n);

  int r= c64(a[1]);
  if (r!=1) printf("DATA alpha c64 ERROR! not 64char range! like: `{|}~ \n");

  int ncomp= c64(a[0]); // global param passing!
  return (alphaout){n, ncomp, nhead};
}

long compress(char* s) {
  printf("\n  DATA  %s\n", s);
  char* p;
  int n, c;

  char SPECIALS[]= " ,.-!\n";

  printf("\nDATA --- delta\n");
  int lowest= 255, highest= 0;
  int lowlowest= 255;
  int last= 0;
  p= s-1; n= 0;
  while((c= *++p)) {

    if (!strchr(SPECIALS, c)) {
      if (tolower(c) < lowlowest)
	lowlowest= tolower(c);
    }

    if (c!=' ' && c < lowest && !strchr(SPECIALS, c))
	lowest= c;
    if (c > highest) highest= c;

    if (c==' ') c= 'a'-1;
    printf("%c %3d\n", c, c-last);
    last= c;
  }

  printf("\nDATA --- range shift # %d [%d-%d]\n", highest-lowest, lowest, highest );
  lowest--; // space!
  p= s-1; n= 0;
  while((c= *++p)) {
    char d= c;
    if (c==' ') d= lowest; // !
    printf("%c %3d\n", c, d-lowest);
    n++;
  }
  printf("DATA = # %d outputs\n", n);



  lowest= lowlowest;
  lowest--; // " "   +0
  lowest--; // ". C" +1
  lowest--; // " C"  +2
  lowest--; // ", "  +3
  lowest--; // " - " +4
  lowest--; // "! C" +5
  lowest--; // "\n"  +6
  //lowest--; // ".\n"  +7
  
  printf("\nDATA --- '. C' ', ' %d [%d-%d]\n", highest-lowest, lowest, highest );
  lowest--; // space!
  p= s; n= 0;
  char* tofree= p= strdup(s);
  while((c= *p)) {
    char d= c;

    if (n==0) {
      if (c==toupper(c)) {
	d= tolower(c);
      } else {
	printf("%% Start with lowercase\n");
      }
    }

    // ---   ", "
    if (0==strncmp(", ", p, 2)) {
      printf("x %c %3d\n", c, +3); n++;
      p+= 2;
      continue;
    }
    // ---   ".\n"
    // no savings, not common!
    if (0) 
    if (0==strncmp(",\n", p, 2)) {
      printf("x %c %3d\n", c, +7); n++;
      p+= 2;
      continue;
    }
    // ---   ". C"
    if (c=='.' && p[1]==' ' && p[2]==toupper(p[2])) {
      printf("x %c %3d\n", c, +1); n++;
      p+= 2;
      c= *p;
      *p= tolower(c);
      continue;
    }
    // ---   "! C"
    if (c=='!' && p[1]==' ' && p[2]==toupper(p[2])) {
      printf("x %c %3d\n", c, +5); n++;
      p+= 2;
      c= *p;
      *p= tolower(c);
      continue;
    }
    // ---   " - "
    if (0==strncmp(" - ", p, 3)) {
      printf("x %c %3d\n", c, +4); n++;
      p+= 3;
      continue;
    }
    // ---   " C" (as in name)
    if (c==' ' && p[1]==toupper(p[1])) {
      printf("x %c %3d\n", c, +2); n++;
      p+= 1;
      c= *p;
      *p= tolower(c);
      continue;
    }
    if (c==' ') d= lowest+0;
    if (c=='\n') d= lowest+6;

    printf("  %c %3d\n", c, d-lowest); n++;
    p++;
  }

  alphaout a= alphabet(s);
  int alpha= a.bits + 5;
  int abits= log2f(alpha)/log2f(2)+0.9999;
  printf("DATA = alphabet %d alphabits=%d\n", alpha, abits);
  float nabits= n * abits;
  int nabytes= (int)(nabits+7)/8 + a.nhead;
  printf("DATA n=%d bits=%d nbits=%d bytes= %d (%d) /%lu ALPHA %d\n", n, abits, (int)nabits, nabytes, a.nhead, strlen(s), alpha);  

  float apercent= 100.0-100*(nabytes+0.0)/strlen(s);
  printf("DATA = savings %4.3f %% ALPHA\n", apercent);

  printf("DATA = # %d outputs\n", n);
  int bits= log2f(highest-lowlowest)/log2f(2)+0.9999;
  float nbits= n * bits;
  int nbytes= (int)(nbits+7)/8 + 1; // to tell size!
  printf("DATA n=%d bits=%d nbits=%d bytes= %d /%lu\n", n, bits, (int)nbits, nbytes, strlen(s));

  free(tofree);

  return nbytes;
}

long report(char* s) {
  long nbytes= compress(s);

  // -- report
  float percent= 100.0-100*(double)nbytes/strlen(s);
  printf("DATA = savings %4.3f %%\n", percent);
  return nbytes;
}

int main(int argc, char** argv) {
  if (argc>1) return report(argv[1]);

  char* str= NULL;
  size_t ln= 0;

  long bytes= 0, cbytes= 0;
  int len= 0;
  while((len=getline(&str, &ln, stdin))!=EOF) {
    bytes+= len;
    cbytes+= report(str);
    //printf("data ===== bytes=%ld cbytes=%ld\n" , bytes, cbytes);
  }
  free(str);

  float percent= 100.0-100*(double)cbytes/bytes;
  printf("DATA = savings %4.3f %%\n", percent);
}

// most coomon cfreq.c uppercase characters in resume
// "space EAITNSORLCDMG newline PUH,FBV.WVK-0"
//
// "etaionshr"
// "TAODW" - begging of words
// "esdt"  - end of words
//
// The most commonly used letters of the English language are e, t, a, i, o, n, s, h, and r. The letters that are most commonly found at the beginning of words are t, a, o, d, and w. The letters that are most commonly found at the end of words are e, s, d, and t.Feb 14, 2015
// - http://www.thiagi.com/instructional-puzzles-original/2015/2/13/cryptograms#:~:text=The%20most%20commonly%20used%20letters,s%2C%20d%2C%20and%20t.


/*

  51 words
  17 endings
  10 doubles
   3 punctuation ", but " ...
   8 questions "? Why "
   3 quotes: ' said "' ' says "' '", said ' 
 -----
  92 cases! 7 bits...

  36 more can take!
  

was
word
what
were
when
there
use
each
which
she
their
other
about
many
then
them
these
some
would
make
like
him
into
time
look
two
more
write
go
see
number
no
way
could
people
than
first
water
who
its
now
find
long
down
day
did
get
come
made
may
part


 (- 128 92)
  54 "missing" --

was
word
what
were
when
said
there
use
each
which
she
how
their
other
about
many
then
them
these
some
would
make
like
him
into
time
look
two
more
write
go
see
number
no
way
could
people
than
first
water
call
who
its
now
find
long
down
day
did
get
come
made
may
part



  WORD FREQUENCY
  --------------
  Short words provide useful clues. One-letter words are either:

  2:
  	a  or I.
  
  The most common two-letter words are:

  18:
  	to, of, in, it, is, as, at, be, we, he, so, on, an, or, do, if, up, by, and my.

  The most common three-letter words are:

  17:
  	the, and, are,for, not, but, had, has, was, all, any, one, man, out, you, his, her, and can.
	
  The most common four-letter words are:

  14:
  	that, with, have, this, will, your, from, they, want, been, good, much, some, and very.


  WORD ENDINGS
  ------------
  The most common word endings are:

  11:
  	-ed, -ing, -ion, -ist, -ous, -ent, -able, -ment, -tion, -ight, and -ance.

  The double letters that occur most commonly at the end of words are:

  4:
	-ee, -ll, -ss, and -ff.

Two letters that usually follow an apostrophe are:

  2:
	-'t  and -'s 

  ==> 17 endings:
    3-5 chars in maybe including space!
    5 bits 

  DOUBLED LETTERS
  ---------------
  The most frequent double-letter combinations are:

  10:
  	ee, ll, ss, oo, tt,ff, rr, nn, pp, and cc.
	
  >>> using 5 bits, 10 


  PUNCTUATION
  -----------
  A comma is often followed by:

  3:
  	but, and, or who.
	
  A question often begins with:

  8:
  	why, how, who, was, did, what, where, or which.
	
Two words that often precede quotation marks are:

  2	
	said " and says "

*/

/*

= 0
= 00
= 000
    3829	; ' '
= 001
    2221	; 'E'

= 0100
    1742	; 'A'
= 0101
    1489	; 'I'
= 0110
    1461	; 'T'
= 0111
    1444)	; 'N'

= 100
    1306	; 'S'
    1263	; 'O'
    1262	; 'R'
= 101
     902	; 'L'
     733	; 'C'
     721	; 'D'
     706)	; 'M'

= 110000
     621	; 'G'
= 110001
     597	; newline
= 110010
     471	; 'U'
= 110010
     595)	; 'P'

= 110100
     470	; 'H'
= 110101
     383	; ','
= 110110
     365)	; 'F'
= 110111
     354	; 'B'
     
= 1110
(+
= 111000
     299	; 'Y'
= 111001
     261	; '.'
= 111010
     256	; 'W'
= 111011
     184	; 'V'

= 11110
= 1111000
     170	; 'K'
= 1111001
     166	; '-'
= 1111010
     121	; '0'
= 1111011
     105	; '('

= 111110
= 11111000
     103	; '*'
= 11111001
      94	; '2'
= 11111010
      94	; ')'
= 11111111
      91	; 'J'

= 111111
= 1111110 000
      84	; ':'
= 1111110 001
      80	; '9'
= 1111110 010
      78	; '1'
= 1111110 011
      77	; 'X'
= 1111110 100
      68	; '/'
= 1111110 100
      60	; '"'
= 1111110 101
      37	; 'Q'
= 1111110 110
      35	; '8'
= 1111110 111
      31	; '='

= 1111111 0000
      26	; '6'
= 1111111 0001
      23	; '5'
= 1111111 0010
      21	; '7'
= 1111111 0011
      21	; '''
= 1111111 0100
      20	; '3'
= 1111111 0101
      17	; 'Z'
= 1111111 0110
      15	; '4'
= 1111 111 0111
      11	; '+'

= 1111 1111 0 000
      10	; '&'
= 1111 1111 0 001
       7	; ';'
= 1111 1111 0 010
       6	; '!'
= 1111 1111 0 011
       3	; '@'
= 1111 1111 0 100
       3	; '?'
= 1111 1111 0 101
       3	; tab
= 1111 1111 0 110
       2	; '_'
= 1111 1111 0 111
       1	; '>'

= 1111 1111 1 0 00
       1)	; '%'
....




*/
