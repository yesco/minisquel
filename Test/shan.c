#include <stdio.h>

#include "shan-gen.h"

typedef unsigned long ulong;

ulong findshan(char c) {
  shannon* s= shan;
  while(s->code) {
    if (s->c == c) {
      return s->code;
    }
    s++;
  }
  return 0;
}

ulong writeshan(ulong rest, ulong s) {
  int rn= rest & 0xff;
  printf("writeshan: rest=%lx %d\n", rest, rn);
  int n= s & 0xff;
  rest &= ~0xffl;
  s &= ~0xffl;
  rest >>= 8;
  s >>= 8;
  
  printf("writeshan: rest=%lx %d\n", rest,rn);
  // transfer bits
  rest <<= n;
  rest |= s;
  rn += n;
  printf("writeshan: rest=%lx %d (transferred data)\n", rest,rn);

   
  while(rn>=8) {
    char b= rest >> 8;
    printf("OUTBYTE: ");
    printf("%x  ", b);
    for(int i=7; i>=0; i--) {
      putchar('0'+ !!(b & (1<<i)));
    }
    rest >>= 8;
    rn -= 8;
    printf(" rest=%lx rn=%d\n", rest, rn);
  }

  // encode rn
  rest <<= 8;
  rest |= rn;

  return rest;
}

ulong flushshan(ulong s) {
  printf("FLUSHING\n");
  printf("flush= %lx\n", s);
  int n= s & 0xff;
  if (n>0 && n<8) n+= 8;
  s &= ~0xff;
  s <<= 8;
  s |= n;
  printf("flush= %lx\n", s);
  writeshan(s, 0);
  return 0;
}


int main() {
  printf("\n\n\n\n\n\nShallo!\n");

  char c= 'e';
  ulong s= findshan(c);
  printf("'%c' => 0x%02lx\n", c, s);
  
  ulong rest= 0;
  printf("rest= 0x%lx\n", rest);
  rest= writeshan(rest, s);
  printf("rest= 0x%lx\n", rest);
  //  rest= writeshan(rest, s);
  //  rest= writeshan(rest, s);
  flushshan(rest);
}
