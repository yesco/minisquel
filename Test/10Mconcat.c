#include <stdio.h>
#include <stdlib.h>

#define NAMELEN 64

int debug=0, foffset=0, stats=0, nfiles= 0, security=0;

#include "../utils.c"

int int2str(char* s, int i) {
  char x[32]= {};
  int j=0;
  int neg;
  neg= (i<0);
  if (neg) i= -i;
  while (i>0) {
    x[j++]= '0' + (i%10);
    i/= 10;
  }
  if (neg) *s++= '-';
  while(j)
    *s++= x[--j];
  return -1;
}

int main(void) {
  long ms= cpums();
  char s[1024]= {};
  int2str(s,  42);
  printf("42=%s\n", s);
  long sum= 0;

  if (0) {

    // simple naive implementation
    // 1856 ms
    for(int i=1; i<=10000000; i++) {
      sum+= snprintf(s, sizeof(s), "%d%s%d%s%d", i, "foo", i+3, "bar", 42);
    }
 
  } else if (0) {
    // inline sttrcpy
    // move out 42
    // 1472 ms
    char c[64]= {}; sprintf(c, "%d", 42);
    for(int i=1; i<=10000000; i++) {
      char a[64]= {}; sprintf(a, "%d", 99);
      char b[64]= {}; sprintf(a, "%d", 99+3);
      char* x;
      char* p= &s[0];
      x=a;	while(*x) *p++ = *x++;
      x="foo";	while(*x) *p++ = *x++;
      x=b;	while(*x) *p++ = *x++;
      x="bar";	while(*x) *p++ = *x++;
      x=c;	while(*x) *p++ = *x++;
      sum+= p-&s[0];
    }
  } else {
 
    // my  own int2str!
    // 377 ms
    char c[64]= {}; sprintf(c, "%d", 42);
    for(int i=1; i<=10000000; i++) {
      char a[64]= {}; int2str(a, i);
      char b[64]= {}; int2str(b, i+3);
      char* x;
      char* p= &s[0];
      x=a;	while(*x) *p++ = *x++;
      x="foo";	while(*x) *p++ = *x++;
      x=b;	while(*x) *p++ = *x++;
      x="bar";	while(*x) *p++ = *x++;
      x=c;	while(*x) *p++ = *x++;
      sum+= p-&s[0];
    }
  }	 

  printf("%ld in %ld ms\n", sum, cpums()-ms);
}
