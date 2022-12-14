//
// Float 32 bit store string
// ==========================
//   (>) 2022 jsk@yesco.org
//
// The idea is to store data inside the
// actual double bit-pattern.

// Float binary format:
//   ----------------- 32 -------------------
//   1 ----8---- --------------23------------
//   s eeee eeee mmm mmmm mmmm mmmm mmmm mmmm
//
// integer 0
//   0 0000 0000 ....
//
// infinity (+ and -)
//   0 1111 1111 000 0000 0000 0000 0000 0000
//   1 1111 1111 000 0000 0000 0000 0000 0000
//
// infinity (+ and -)
//   0 1111 1111 000 0000 0000 0000 0000 0000
//   1 1111 1111 000 0000 0000 0000 0000 0000
//
// nan (+ and -)
//   0 1111 1111 100 0000 0000 0000 0000 0000
//   1 1111 1111 100 0000 0000 0000 0000 0000
//
// qNaN (on x86 and ARM processors)
//   x 1111 1111 100 0000 0000 0000 0000 00012
//
// sNaN (on x86 and ARM processors)
//   x 1111 1111 000 0000 0000 0000 0000 00012

// "not used" (some x!=0) interpreted as nan
//   0 1111 1111 0xx xxxx xxxx xxxx xxxx xxxx
//   1 1111 1111 1xx xxxx xxxx xxxx xxxx xxxx

// Conclusion
// ----------
// 1+22 bits, as long as lower bits are not 0
// (or 1 on x86 and ARM lol)
//
// 2^23 = 8388608 = 8 388 608 = 8M valuies
//
// However! We still have the "s" sign bit!
// So we'll make it signed.

// REF - https://en.m.wikipedia.org/wiki/Double-precision_floating-point_format


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

void p32(uint32_t i);
void p32f(float f);

void nl() { putchar('\n'); }

// Store a limited signed 23-bit number in a float!
// NOTE: the values 0 cannot be stored!
//
// NOTE: Ranges allowed:
// 
//          0   - NOT ALLOWED
//          1   - (better not use! LOL)
//          2   - ok
//        ...
//    2^22 -1   - ok
//       2^22   - NOT ALLOWED
//        ...
//    2^23 -1   - max
// 
// and their negatie values
//
// s 1111 1111 f xx xxxx  xxxx xxxx  xxxx xxxx
// |           | -----------22 bits-----------
// sign bit    |__ funky bit
//
// This allows you to encode 16,777,212 values.
// ( 2^24 - 4 )
//
// Consider a mixed-type datastorage:
//  - float / int / strings / json / blobs
//
// sf
// ==
// 00  a) index to private heap strings
// 10  b) index to global symbols/strings
// 01  c) index to tuple (for multicol index)
// 11  d) index to pointer to "object"
//
// sf         Special values encoding
// == =======================================
// 11 ... 1111 1111 1111 1111 1010 = 
// 11 ... 1111 1111 1111 1111 1001 = 
// 11 ... 1111 1111 1111 1111 1010 = 
// 11 ... 1111 1111 1111 1111 1011 = 
// 11 ... 1111 1111 1111 1111 1100 = FALSE
// 11 ... 1111 1111 1111 1111 1101 = TRUE
// 11 ... 1111 1111 1111 1111 1110 = NULL
//     e) NULL VALUE!

// TODO: same for 64-bit double:
//    => 53 bits (one sign + 52 bits)
//   Values excluded 0, 1, 1111...1111
//
//   2^52 ==  4 503 599 627 370 496 = 4.5 Exa
// 
// REF: https://en.m.wikipedia.org/wiki/Double-precision_floating-point_format

const uint32_t INF_MASK= 0x7f800000;
const uint32_t MASK    = 0x7f000000; // 0111 1111 0
const uint32_t NAN_MASK= 0x7fc00000;
const uint32_t MASK_23=  0x007fffff;
const uint32_t SIGN_BIT= 0x80000000;
const uint32_t MASK_22=  0x003fffff;

float make24(int32_t i) {
  uint32_t u= i<0 ? -i : i; // abs
  //  if (!!(u & INF_MASK) || !(u & MASK_22)) {
  if ((u & INF_MASK) || !i) {
    printf("make23: %u doesn't fit in 23!\n", i);
    fprintf(stderr, "make23: %u doesn't fit in 23!\n", i);
    exit(1);
  }
  u|= INF_MASK;
  float f= *(float*)&u;
  return i<0 ? -f : f;
}

int32_t is24(float f) {
  uint32_t i= *(uint32_t*)&f;
  int s= i & SIGN_BIT;
  if ((i & INF_MASK) != INF_MASK) return 0;
  i= (i & MASK_22) ? i & MASK_23 : 0;
  return s ? -i : i;
}


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
    printf("make53: l= %ld outside of range", l);
    printf("make53: l= %ld  outside of range", l);
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

void p32(unsigned int v) {
  unsigned int vv= v;
  for(int i=0; i<32; i++) {
    putchar((vv & 0x80000000)? '1': '0');
    vv<<= 1;
    if (i==0 || i==4 || i==8 || i>8 && (i-11)%4==0)
      putchar(' ');
  }
  float f= *(float*)&v;
  printf(" %g", f);
  if (isinf(f)) printf(" INF");
  if (isnan(f)) printf(" NAN");
  if (is24(f)) printf(" 23");
  nl();
}

void p32f(float f) {
  p32(*(unsigned int*)&f);
}

void p64(uint64_t v) {
  uint64_t vv= v;
  for(int i=0; i<64; i++) {
    putchar((vv & 0x8000000000000000l)? '1': '0');
    vv<<= 1;
    if (i==0 || i==4 || i==8 || i>8 && (i-11)%4==0)
      putchar(' ');
  }
  double d= *(double*)&v;
  printf(" %lg", d);
  if (isinf(d)) printf(" INF");
  if (isnan(d)) printf(" NAN");
  if (is53(d)) printf(" 53");
  nl();
}

void p64d(double d) {
  p64(*(uint64_t*)&d);
}

void printbits() {
  for(int i=-10; i<20; i++) {
    p32f(i);
  }

  nl();
  double v= 1;
  for(int i=0; i<64; i++){
    p32f(v);
    v= v+v;
  }
  nl();
  
  p32(0xff800000); // -inf CANONICAL
  p32(0X7F800000); // +inf CANONICAL
  p32f(INFINITY);
  p32f(-INFINITY);
  nl();
  p32(0xffc00000); // -nan CANONICAL
  p32(0x7fc00000); // +nan CANONICAL
  p32f(NAN);
  p32f(-NAN);
  nl(); nl();
  p32(0x7fc00001); // nan PERVERTED
  p32(0X7F800001); // nan PERVERTED
  p32(0X7F8fffff); // nan PERVERTED
  nl(); nl();
  p32(0x7fc00001); // +nan  PERVERTED
  p32(0x7fffffff); // +nan  PERVERTED
  nl(); nl(); nl();
  p32(0x000000000); // 0
  p32(0x000000001); // 1.4013e-45
  p32(0x000000002); // 2.8...
  p32(0x0007fffff); // 1.17549e-38
  p32f(1.17549e-38); // 1.17549e-38 ~same;
 
}

#include "syms.c"

void readdict() {
  //FILE* f= fopen("wordlist-1.1M.txt", "r");
  FILE* f= fopen("Test/count.txt", "r");
  char* word= NULL;
  size_t len= 0;
  int n= 0;
  while(getline(&word, &len, f)!=EOF) {
    int l= strlen(word);
    if (word[l-1]=='\n') word[l-1]= 0;
    n++;
    //printf("startADD: %s\n", word);
    int s= sym_owned(word);
    //printf("endADD: %s\n====\n\n", symstr(s));
    word= NULL;
    if (n%1000 == 0) fputc('.', stderr);
    //if (n%1000 == 0) fputc('.', stdout);
  }
  printf("# %d\n", n);
  fclose(f);

  dump();
}

