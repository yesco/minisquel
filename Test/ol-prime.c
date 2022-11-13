#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  long pp= argc>1 ? atol(argv[1]) : 2124679;

  if (0) {
    // closer by using indirection
    // 745 s (100 ms slower, 13 % !)
    long aa, bb, cc, rr;
    long *a=&aa, *b=&bb, *c=&cc, *r=&rr, *p=&pp;
    for(*a=2; *a<=100; (*a)++) {
      *b= *p-1;
      for(*c=2; *c<=*b; (*c)++) {
	*r= *p % *c;
	if (*r==0) {
	  printf("%ld is not prime says %ld\n",
		 *p, *c);
	}
      }
    }
  } else {
    // 658 ms
    for(long a=2; a<=100; a++) {
      long b= pp-1;
      for(long c=2; c<=b; c++) {
	long r= pp % c;
	if (r==0) {
	  printf("%ld is not prime says %ld\n",
		 pp, c);
	}
      }
    }
  }
}
