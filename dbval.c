#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "utils.c"
#include "hash.c"

/* --- double float64 binary64
    64-bit doubles are encoded as follows:

    -- s52
(* 6 8)
1 -----11---- ------------------------- 52 -----------------------------
                 1       2   3   4   5     6 bytes 
s eeeeeeeeeee BBBBBBBB BBBBB-----BBBBBB BBBBBBBB bbbb = number/int53
s 11111111111                                         = +INF/-INF
s 11111111111 0 = NAN                                 = NAN normal
s 11111111111 1 = NAN                                 = NAN -SigNNA
  11111111111 11111111 11111-----111111 11111111 1111 = NAN arm x86

0             nnnnnnnn nnnnn-----nnnnnn nnnnnnnn nnn0 = "+offset" 
0             dddddddd ddddd-----dddddd dddddddd nnn1 = typed data
1             NNNNNNNN NNNNN-----NNNNNN NNNNNNNN NNN0 = "-offset"
1             DDDDDDDD DDDDD-----DDDDDD DDDDDDDD DDD1 = TYPED DATA

Let's look at what we can hide behind redundant NAN numbers!

48 bits => 2^48 addresses a terabyte!

   6 bytes  type
   -------  ----
0  NNN NNN  bnk0 = use for string offset
0  DDD DDD  typ1 = encode linear types

1  NNN NNN  BNK0 = big (file) offset
1  DDD DDD  TYP1 = various encodings

We've observed that string sort/ccmp takes
time to search as it gives many cache misses.



STRING COMPARABLE OFFSETS!
--------------------------

We will store a prefix of the string inside
the "offset" pointer!

The alphabet of strings is binned into 64
nubmers. Thus 6 bits:

4 chars a 6 bits is 24 bits

52-24 => 28 bits = 2^28 = 256 MB addresses
If strings are aligned on 2 bytes, then
low bit is always zero, indicating this type.

  24 bits  256 MB hash strings
  4 chars        28 bits
  -------  -------------------
0   cccc   nnnnnnn--nnnnnn n 0


VARIOUS SMALL TYPES
===================


     48 bits     3 bits
    -----------  --/  
0   ddddd---ddd  typ  1

0   AAAA AAAAAA  a00  1  \
0                000  1   } 7 ASCII !
0                100  1  /
		 

    TSTSTSTSTST  001  1  5k years Timestamp

                 010  1      maybe date
		 011  1      maybe time

                 101  1         free
                 110  1         free
		 111  1         free

7 INLINE ASCII CHARACTERS
\-------------------------

        48   +   1
	  49 bits
     ===============
     'H i W o r l d'



TIME STAMP - UNIX TIME
----------------------

--- Unix Time
10k years 365.25 days
              24h 60m 60s 1000ms
bc> 10000*365.25*24*60*60*1000
315576000000000.00
bc> l(last)/l(2)
bc> 48.16498081895322801908

 ==> so we need 49 bits
    to store 10000 years!
    48 for 5000 years


BIG DATA, lol 
=========----
When it's "negative" we can have a differnt
meanings:

       48 bits  
1   DDDDDDD---DDDDDD 1  = 51 bits (D)
1   DDDDD---DDD  TYP 1  = or 48 bits

    48 bits 281474976710656 = 281 474 976 710 656 = 256 T
    
    40 bits 1099511627776 = 109 9511 627 776 = 1 TB
    
    heap heap index to 256 different heaps?

    tttt = 4 bit "type" info

    double lmax= 4503599627370496l;
    long lmax= 2251799813685248l;
*/

// TODO: same for 64-bit double:
//    => 53 bits (one sign + 52 bits)
//   Values excluded 0, 1, 1111...1111
//
//   2^52 ==  4 503 599 627 370 496 = 4.5 Exa
// 
// REF: https://en.m.wikipedia.org/wiki/Double-precision_floating-point_format




const uint64_t LINF_MASK= 0x7ff0000000000000l; // 11 bits exp
const uint64_t LMASK    = 0x7ff0000000000000l;
const uint64_t LNAN_MASK= 0x7ff8000000000000l;
const uint64_t LMASK_53=  0x0007ffffffffffffl;
const uint64_t LSIGN_BIT= 0x8000000000000000l;
//const uint64_t LMASK_52=  0x0003ffffffffffffl;

const long L53MAX= 2251799813685248l-2; // 111 ... 11 excluded

double make53(int64_t l) {
  uint64_t ul= l<0? -l : l;
  if (ul<2 || ul>=L53MAX) {
    printf("make53: l= %ld outside of range\n", l);
    printf("make53: l= %ld  outside of range\n", l);
    exit(1);
  }
  ul |= LINF_MASK;
  double d= *(double*)&ul;
  d= l<0 ? -d : +d;
  //printf("make53: %ld => %g\n", l, d);
  return d;
}

int64_t is53(double d) {
  uint64_t ul= *(uint64_t*)&d;
  int neg= (ul & LSIGN_BIT)!=0;
  ul &= LMASK_53; 
  if (!isnan(d) || ul<2 || ul>L53MAX) return 0;
  ul &= LMASK_53;
  //return d<0 ? -ul : +ul;
  return neg ? -ul : +ul;
}

// A database value is a double.
// It can represent 3 types:
// - NULL
// - double
// - string
//
// The NAN encoding have redundancies.
// THese are used to unambigiously store
// NULL, or string indices.
//
// The strings pointers are managed in
// dbstrings array.
// 
// TODO: consider using hashstrings
//   w reference(?) counting.
//
// API:
//   mknull() mknum() mkstr() mkdupstr()
//              num()   str()
//   isnull() isnum()   str()!=NULL
//                   dbfree()   dbfree()
typedef union dbval {
  double d;
  long l;
} dbval;
  
#define MAXSTRINGS 1024*1024
// first 3 values are protected
#define INULL 2
// ... strings ...
#define ISTRLIMIT (LMASK_53-16)
// logic
#define IFALSE (LMASK_53-15) // -1
#define IFAIL  (LMASK_53-14) //  0
#define ITRUE  (LMASK_53-13) // +1
// query state
#define IDEL   (LMASK_53-13) // DELeted

// ... to be used ...

// control flow
#define IWAIT  (LMASK_53-3) // call again?
#define IEND   (LMASK_53-2) // EOF
// variants of error
#define IERROR (LMASK_53-1) // max-1

#define CNULL  (INULL  | LINF_MASK)
#define CEND   (IEND   | LINF_MASK)
#define CFAIL  (IFAIL  | LINF_MASK)
#define CERROR (IERROR | LINF_MASK)

dbval mknull(){return(dbval){.l=CNULL};}
// end/last of something
dbval mkend() {return(dbval){.l=CEND};}
// db eval "fail" but not program error
dbval mkfail(){return(dbval){.l=CFAIL};}
// programming error
dbval mkerr() {return(dbval){.l=CERROR};}
dbval mknum(double d){return(dbval){d};}

// These valuies are transient
// Don't store permanently!
typedef enum{TNULL=0, TNUM, TSTR, T7ASCII, TATOM, TEND=100,TBAD,TFAIL,TERROR} dbtype;

	 
int isnull(dbval v) {return v.l==CNULL;}
int isfail(dbval v) {return v.l==CFAIL;}
int isend(dbval v)  {return v.l==CEND;}
int iserr(dbval v)  {return v.l==CERROR;}
int isint(dbval v)  {return((int)v.d)==v.d;}
int islong(dbval v)  {return((long)v.d)==v.d;}
long is7ASCII(dbval v) {
  long l= is53(v.d);
  return (((l & 0x03) == 0x01) && l<ISTRLIMIT) ?
    l>>2 : 0;
}

dbtype type(dbval v) {
  if (!isnan(v.d)) return TNUM;
  // In NAN number we
  long i= is53(v.d);
  long u= i<0 ? -i : i;
  if (u>INULL && u<ISTRLIMIT) {
    if ((u & 0x03)==0x01) return T7ASCII;
    else return TSTR;
  }
  if ((u&0x03)==0x01) return T7ASCII;
  if (!i) return TNUM; // +NAN/-NAN
  if (u==1) return TBAD;
  if (u==INULL) return TNULL;
  if (u==LMASK_53) return TBAD;
  if (u==IERROR) return TERROR;
  if (u==IFAIL) return TFAIL;
  if (u==IEND) return TEND;
  // TODO: use for more types?
  // possibly use lower 4 bits as type
  // xxxx ... xxxx bbbb bbbb tttt
  // b...b: bank number (arena!)
  return TATOM;
}

int isbad(dbval v)  {return type(v)==TBAD;}    

// TODO: 1? 2? and max-1?
int isnum(dbval v) {
  return isnan(v.d) ? !is53(v.d) : 1;
}

double num(dbval v) {return v.d;}

char* dbstrings[MAXSTRINGS]= {NULL,"-INVALID-",NULL};
int nstrings= 3;
int nstrfree= 0;
int strnext= 4;

// TODO:concider it to use "s" bit instead?

// 52 bits alternatives:
// - 4b (16 alp) * 13 chars + 0 bits
// - 5b (32 alp) * 10 chars + 2 bits
// - 6b (64 alp) *  8 chars + 4 bits
// - 7b (ascii)  *  7 chars + 3 bits 
// - 8b (utf8)   *  6 chars + 0 bits

// 52 = 5 bits * 10 char = 50 bits
// 2 bits more
//
//     1234567890
// 00:  ---  OFFSET ---
// 01: amsterdam  san jose
// 10: Amsterdam  San Jose
// 11: AMSTERDAM  SAN JOSE

// below:
//   50 bits:
//   12 (hex)digits + 2 bits
//   '2022-06-15T15:42:60.123'
//    1234 56 78 90 12 3   = nah

//  1 6bit letter    
// 11 4bit hexdigits?
// 11 or ' 0123456789-/.()' (16 chars)
// not good/enough for phone nums
// +14155551234 (sf)
// 123456789012

// - only IMDB 'NM0000042' - 9 chars
//  2  6bit letter  'TT' / 12
// 10  4bit digits       / 40


dbval mkstr7ASCII(char* s) {
  if (!s) return mknull();
  int len= strlen(s);
  if (len>7) return mkfail();
  // 64: 0 eee---eee AAA AAAA a01
  // eee---eee= 111---111
  // 7*7 = 49+3 = 52 !
  // 52+1+11=64
  long l= 0;
  for(int i=0; i<7; i++) {
    printf("7ASCII: %16lx\n", l);
    l<<= 7;
    printf("7ASCII: %16lx\n", l);
    if (*s & 0x80) return mkfail();
    l|= *s & 0x7f;
    printf("7ASCII: %16lx\n", l);
    s++;
  }
  printf("7ASCII: %16lx\n", l);
  l<<= 2;
  l|= 1;
  printf("7ASCII: %16lx\n", l);
  // l|= LINF_MASK;
  // return *(dbval*)&l;
  dbval v;
  v.d= make53(l);
  return v;
}

dbval mkstrfree(char* s, int tofree) {
  dbval v= mkstr7ASCII(s);
  if (!isfail(v)) {
    if (tofree) free(s);
    return v;
  }
  int start= strnext;
  while(nstrfree && dbstrings[strnext]) {
    printf("TRYING %d\n", strnext);
    if (++strnext>=nstrings) strnext= INULL+1;
    if (strnext==start) error("mkstrfree: inconsistency says nstrfree!");
  }
  if (!nstrfree) {
    // allocate one more, nothing to reuse
    strnext= nstrings++;
    if (nstrings>=MAXSTRINGS)  error("out of dbval strings");
  } else nstrfree--;
  if (dbstrings[strnext])
    error("mkstringfree: inconsistency, found non-free");

  v.d= make53(tofree?-strnext:+strnext);
  dbstrings[strnext]= s;
  return v;
}

// only use for constants, if you sure
dbval mkstrconst(char* s) {
  return mkstrfree(s, 0);
}

// make a copy and free it
dbval mkstrdup(char* s) {
  return mkstrfree(s?strdup(s):s, 1);
}

// Get (interned) string from VALUE
// 
// TODO: add arena code unless globalatom
char* str(dbval v) {
  if (!is7ASCII(v)) error("Can't get string from 7ASCII use ...cmp");
  
  long i= is53(v.d);
  // 0 maps to NULL!
  return i<ISTRLIMIT?dbstrings[i<0?-i:+i]:NULL;
}

void dbfree(dbval v) {
  // TODO: use arena code unless globalatom
  // if not nan can't be string!
  if (!isnan(v.d)) return;
  int i= is53(v.d);
  if (i<0) free(dbstrings[(i=-i)]);
  dbstrings[i]= NULL;
  nstrfree++;
  // TODO: freelist?
  strnext= i;
}

void dumpdb() {
  printf("\n---dumpdb---\n");
  for(int i=0; i<nstrings; i++) {
    char* s= dbstrings[i];
    printf("%03d : %s %s\n", i,
      i==1||i>INULL ? (s?s:"   -"):"-NULL-",
      nstrfree&&i==strnext?"<--- NEXT":"");
  }
  printf("nstrings=%d nstrfree=%d strnext=%d\n", nstrings, nstrfree, strnext);
}

dbval val2dbval(val* v) {
  if (!v) return mkerr();
  dbval d= {v->d};
  if (!v->not_null) d= mknull();
  else if (v->s) d= mkstrconst(v->s);
  return d;
}

int dbprinth(dbval v, int width, int human) {
  // Save 31% (when printing 3 "long" & 1 string)
  // Save additionally 2.5% by not doing type check!
  // It's safeb because num, NAN, INF not double equal
  long l= (long)v.d;
  if (l==v.d) return human
   ? hprint(l, "\t") : printf("%8ld\5", l);
  
  switch(type(v)) {
  case TNULL: printf("NULL\t"); break;
  case TNUM:  printf("%7.7lg\t", v.d); break;
  case TATOM: 
  case T7ASCII: { // or TSTR and detect?
    char c7[8]= "1234567";
    long l= is7ASCII(v);
    int i= 6;
    while(l && i>=0) {
      char c= c7[i--]= l & 0x7f;
      l>>= 7;
    }
    // It fits for sure!
    // (TODO: width)
    printf("%s", c7);
  } break;
  case TSTR:  {
    char* s= str(v);
    int i= 6;
    while(*s && i--) {
      if (*s=='\n') { printf("\\n"); i--;}
      else if (*s=='\r') { printf("\\r"); i--;}
      else putchar(*s++);
    }
    if (*s) putchar(s[1] ? '*' : *s);
    putchar('\t');
  } break;
  case TBAD:  printf("ILLEGAL\t"); break;
  case TFAIL: printf("FAIL\t"); break;
  case TERROR:printf("ERROR\t"); break;
  case TEND:  printf("END\n"); break; // \n!
  }
  return 1;
}
