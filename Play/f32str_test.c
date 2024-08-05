
#include "f32str.c"

int main(int argc, char** argv) {
  if (1) {
    p64d(+INFINITY);
    p64d(-INFINITY);
    p64d(+NAN);
    p64d(-NAN);
    nl();
    p64d(1);
    p64d(2);
    p64d(3);
    p64d(4);
    p64d(5);
    p64d(8);
    p64d(16);
    double d;
    p64d(d=make53(2)); printf("==> %ld\n", is53(d));
    p64d(d=make53(3)); printf("==> %ld\n", is53(d));
    p64d(d=make53(256)); printf("==> %ld\n", is53(d));
    nl();
    // s52
    //   ----------------------------------- 52 -------------------------------
    // s xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  xxxx xxxx  tttt
    // s -------------------------------- 48 bits -----------------------  4 b-
    //   ---------------------40 bits-------------------------  heap heap
    //
    // 48 bits 281474976710656 = 281 474 976 710 656 = 256 T
    //
    // 40 bits 1099511627776 = 109 9511 627 776 = 1 TB
    //
    // heap heap index to 256 different heaps
    //
    // tttt = 4 bit "type" info
    // ----
    // 0000 = 


    //double lmax= 4503599627370496l;
    long lmax= 2251799813685248l;
    double max= lmax;
    
    p64d(d=make53(lmax-3)); printf("==> %ld\n", is53(d));
    //p64d(d=make53(lmax-2)); printf("==> %ld\n", is53(d));
    // TOOD: exclude lmax
    //    p64d(d=make53(lmax-1)); printf("==> %ld\n", is53(d));
    //    p64d(d=make53(lmax)); printf("==> %ld\n", is53(d));
    //p64d(d=make53(lmax+1)); printf("==> %ld\n", is53(d));
    nl();
    printf("--- negatives\n");
    p64d(d=make53(-2)); printf("==> %ld\n", is53(d));
    p64d(d=make53(-3)); printf("==> %ld\n", is53(d));
    p64d(d=make53(-256)); printf("==> %ld\n", is53(d));
    nl();

    exit(1);
  }


  //  readdict(); exit(0);
  
  if (1) {
    float a= make24(47);
    float b= make24(48);
    p32f(a);
    p32f(b);
    printf("%f <=> %f --> %d\n", a, b, (a>b)-(b>a));
    int ia= is24(a), ib= is24(b);
    printf("%d <=> %d --> %d\n", ia, ib, (ia>ib)-(ib>ia));
    exit(0);
  }
  if (1) {
      p32f(-1);
      p32(0);
      p32(0xffffffff);
  }
  if (1) {
    // Max int value: 16 777 215
    int i= (2<<23) -1; float f= i;
    printf("%d %f %d\n", i, f, (int)f);
    exit(0);
  }
  //printf("%8x\n", 0xff000000 >> 1); exit(1);
  if(0) {
    int i= (2<<21)+1;
    printf("i=%d\n", i);
    float f=- make24(i);
    printf("f\n");
    printf("%g ", f);
    printf("i->23 =>\n");
    p32f(f); nl();
    uint32_t v= is24(f);
    printf("f->v =>\n");
    printf("%d\n", v);
    exit(1);
  }

  if(0){
    int i= (2<<21)+1;
    printf("i=%d\n", i);
    float f= make24(i);
    printf("f\n");
    printf("%g ", f);
    printf("i->23 =>\n");
    p32f(f); nl();
    uint32_t v= is24(f);
    printf("f->v =>\n");
    printf("%d\n", v);
    exit(1);
  }

  printbits();

  printf("\n\n\nTEST *ALL* 23 numbers!\n\n");
  int n= 0;
  double d= 0;
  int s= 0;

  // skip 0
  for(int i=1; i< 2<<21; i++) {
    if (1) {
      float f= make24(i);
      uint32_t v= is24(f);
      if (v!=i) 
	printf("%d => %d\n", i, v);
    }
    n++;
  }
  // skip-2^22
  for(int i=(2<<21)+1; i< 2<<22; i++) {
    if (1) {
      float f= make24(i);
      uint32_t v= is24(f);
      if (v!=i) 
	printf("%d => %d\n", i, v);
    }
    n++;
  }

  // skip 0
  for(int i=-1; i> -(2<<21); i--) {
    if (1) {
      float f= make24(i);
      uint32_t v= is24(f);
      if (v!=i) 
	printf("%d => %d\n", i, v);
      n++;
    }
  }
  // skip 2^22
  for(int i=-((2<<21)+1); i> -(2<<22); i--) {
    if (1) {
      float f= make24(i);
      uint32_t v= is24(f);
      if (v!=i) 
	printf("%d => %d\n", i, v);
      n++;
    }
  }

  // end at 2^23 - 1
  printf("%f\n", d);
  printf("values %d\n", n);
  printf("missing values: %d (0 and 2^23)\n", (2<<23) - n);
  exit(0);

  if (0) {
    int i= 1;
    if ((i & MASK_23) != i) printf("iCRAP! %d\n", i);
    double f= make24(i);
    uint32_t v= *(uint32_t*)&f;
    v&= MASK_22;
    if (v != i) printf("vCRAP! %d %d\n", v, i);
    printf("VALUE= %u\n", v);
    //printf("%f ", f);
    if (!isnan(f)) printf("NOT NAN= %g %d\n", f, i);
    if (!is24(f)) printf("is not 23= %d\n", i);
    if (is24(f)!=i) {
      printf("\n%d    is not same %d %d %g\n", i, is24(f), i, f);
      p32f(f);
    }
    d+= f;
    n++;
  }
  // skip 2^22
  for(int i=(2<<21)+1; i< 2<<22; i++) {
    double f= make24(i);
    //printf("%f ", f);
    if (!isnan(f)) printf("NOT NAN= %g %d\n", f, i);
    if (!is24(f)) printf("is not 23= %d\n", i);
    if (is24(f)!=i) {
      printf("is not same %d %d %g\n", is24(f), i, f);
      p32f(f); nl();
    }
    d+= f;
    n++;
  }
  // end at 2^23 - 1
  printf("%f\n", d);
  printf("values %d\n", n);
  printf("missing values: %d (0 and 2^23)\n", (2<<22) - n);
}

