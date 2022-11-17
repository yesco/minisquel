// run with:
//
//   clang -O3 junk.c && ./a.out
//
// junk.c:22:14: warning: shift count >= width of type [-Wshift-count-overflow]
//
//   if (u & (1 << 48))
//              ^  ~~

// loop forever!
// (it calls main again and again!)
// WFT?
//
// jsk@yesco.org

#include <stdlib.h>
#include <stdio.h>

void fun(int p) {
  long u= p;

  int z= u & 7;
  printf("z = %d\n", z);

  if (z)
    printf("why? z is not true? %d\n", z);

  if (u & (1 << 48))
    printf("mkptr: 49th bit set!");
}

int foo= 0;

int main(void) {
  foo++;
  printf("\n\nfoo=%d\n", foo);
  fun(0);
  exit(0);
}

// clang version 15.0.4
// Target: aarch64-unknown-linux-android24
// Thread model: posix
// InstalledDir: /data/data/com.termux/files/usr/bin

// Linux localhost 4.14.186+ #1 SMP PREEMPT Tue Aug 23 18:28:47 CST 2022 aarch64 Android
