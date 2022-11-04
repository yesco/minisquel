#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "utils.c"
#include "hash.c"

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
#define IEND   (LMASK_53-3) // EOF
#define IFAIL  (LMASK_53-2) // ???
#define IERROR (LMASK_53-1) // max-1
#define ISTRLIMIT (LMASK_53-16)

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

char* dbstrings[MAXSTRINGS]= {NULL,"-INVALID-",NULL};
int nstrings= 3;
int nstrfree= 0;
int strnext= 4;

dbval mkstrfree(char* s, int free) {
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

  dbval v;
  v.d= make53(free?-strnext:+strnext);
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

// These valuies are transient
// Don't store permanently!
typedef enum{TNULL=0, TNUM, TSTR, TATOM, TEND=100,TBAD,TFAIL,TERROR} dbtype;

dbtype type(dbval v) {
  if (!isnan(v.d)) return TNUM;
  // In NAN number we
  long i= is53(v.d);
  long u= i<0 ? -i : i;
  if (!i) return TNUM; // +NAN/-NAN
  if (u==1) return TBAD;
  if (u==INULL) return TNULL;
  if (u==LMASK_53) return TBAD;
  if (u==IERROR) return TERROR;
  if (u==IFAIL) return TFAIL;
  if (u==IEND) return TEND;
  if (u>INULL && i<ISTRLIMIT) return TSTR;
  // TODO: use for more types?
  // possibly use lower 4 bits as type
  // xxxx ... xxxx bbbb bbbb tttt
  // b...b: bank number (arena!)
  return TATOM;
}
	 
int isnull(dbval v) {return v.l==CNULL;}
int isfail(dbval v) {return v.l==CFAIL;}
int isend(dbval v)  {return v.l==CEND;}
int iserr(dbval v)  {return v.l==CERROR;}
int isint(dbval v)  {return((int)v.d)==v.d;}
int isbad(dbval v)  {return type(v)==TBAD;}    
// TODO: 1? 2? and max-1?
int isnum(dbval v) {
  return isnan(v.d) ? !is53(v.d) : 1;
}



double num(dbval v) {return v.d;}

// Get (interned) string from VALUE
// 
// TODO: add arena code unless globalatom
char* str(dbval v) {
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

int dbprint(dbval v, int width) {
  switch(type(v)) {
  case TNULL: printf("NULL\t"); break;
  case TNUM:  printf("%7.7lg\t", v.d); break;
  case TATOM: 
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
