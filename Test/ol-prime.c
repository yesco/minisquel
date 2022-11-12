#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  long p= argc>1 ? atol(argv[1]) : 2124679;

  for(long a=2; a<=100; a++) {
    long b= p-1;
    for(long c=2; c<=b; c++) {
      long r= p % c;
      if (r==0) {
	printf("%ld is not prime says %ld\n",
	       p, c);
      }
    }
  }
}
