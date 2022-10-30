#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "utils.c"

const uint64_t LINF_MASK= 0x7ff0000000000000l; // 11 bits exp
const uint64_t LMASK    = 0x7ff0000000000000l;
const uint64_t LNAN_MASK= 0x7ff8000000000000l;
const uint64_t LMASK_53=  0x0007ffffffffffffl;
const uint64_t LSIGN_BIT= 0x8000000000000000l;
const uint64_t LMASK_52=  0x0003ffffffffffffl;

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
  int neg= !!(ul & LSIGN_BIT);
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
//   isnull() isnum() isstr()
//                   dbfree()   dbfree()
typedef struct dbval {
  double d;
} dbval;
  
#define MAXSTRINGS 1024*1024
// first 3 values are protected
#define SNULL 2

char* dbstrings[MAXSTRINGS]= {NULL,"-INVALID-",NULL};
int nstrings= 3;
int nstrfree= 0;
int strnext= 4;

dbval mknull() {
  dbval v;
  v.d= make53(SNULL);
  return v;
}

dbval mknum(double d) {
  return (dbval){d};
}

dbval mkstrfree(char* s, int free) {
  dbval v;
  int start= strnext;
  while(nstrfree && dbstrings[strnext]) {
    printf("TRYING %d\n", strnext);
    if (++strnext>=nstrings) strnext= SNULL+1;
    if (strnext==start) error("mkstrfree: inconsistency says nstrfree!");
  }
  if (!nstrfree) {
    // allocate one more, nothing to reuse
    strnext= nstrings++;
    if (nstrings>=MAXSTRINGS)  error("out of dbval strings");
  } else nstrfree--;
  if (dbstrings[strnext])
    error("mkstringfree: inconsistency, found non-free");
  v.d= make53(free?-strnext:+strnext);
  dbstrings[strnext]= s;
  return v;
}

// only use for constants, if you sure
dbval mkstr(char* s) {
  return mkstrfree(s, 0);
}

// make a copy and free it
dbval mkstrdup(char* s) {
  return mkstrfree(s?strdup(s):s, 1);
}

int isnull(dbval v) {
  return isnan(v.d) && is53(v.d)==SNULL;
}

int isnum(dbval v) {
  return isnan(v.d) ? !is53(v.d) : 1;
}

int isint(dbval v) {
  int i= v.d;
  return i==v.d;
}

double num(dbval v) {
  return v.d;
}

char* str(dbval v) {
  int i= is53(v.d);
  return dbstrings[i<0?-i:+i];
}

void dbfree(dbval v) {
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
      i==1||i>SNULL ? (s?s:"   -"):"-NULL-",
      nstrfree&&i==strnext?"<--- NEXT":"");
  }
  printf("nstrings=%d nstrfree=%d strnext=%d\n", nstrings, nstrfree, strnext);
}

void testint(dbval v) {
  printf("%lg => %d   is... %d\n", v.d, isint(v), (int)v.d);
}

int main(void) {
  dbval n= mknum(42);
  dbval s= mkstrdup("foo");
  for(int i=0; i<10; i++) {
    char s[20];
    snprintf(s, sizeof(s), "num %d", i);
    mkstrdup(s);
  }
  printf("bum=%lg str='%s'\n", num(n), str(s));
  dumpdb();
  dbfree(n);
  dbfree(s);
  dumpdb();
  dbval a= mkstrdup("bar");
  dumpdb();
  dbval b= mkstrdup("fie");
  dumpdb();
  
  testint(mknum(42));
  testint(mknum(3.14));
  testint(mknull());
  testint(mkstr("foo"));
  testint(mknum(1.0/0));
  testint(mknum(INT_MAX));
  testint(mknum(INT_MIN));
}
