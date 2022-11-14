#include <stdio.h>
#include <stdlib.h>

int debug=0, stats=0, foffset=0, lineno= 0;

#include "../utils.c"

#include "../csv.c"
#include "../vals.c"
#include "../dbval.c"

int main(int argc, char** argv) {
  long pp= argc>1 ? atol(argv[1]) : 2124679;

  if (1) {
    // using dbval
    // - no slowdown!
    // why did it in objectlog.c ???
    dbval pd= (dbval){.d=pp};
    dbval sum= (dbval){.d=0};
    for(dbval a=(dbval){.d=2}; a.d<=(dbval){.d=100.0l}.d; a.d++) {
      dbval b= (dbval){.d=pd.d-1.0l};
      for(dbval c=(dbval){.d=2.0l}; c.d<=b.d; c.d++) {
	dbval r= (dbval){.d=((long)pd.d) % ((long)c.d)};
	if (r.d==0.0l) {
	  sum.d+= c.d;
	  printf("%lg is not prime says %lg\n", pd.d, c.d);
	}
      }
    }
    //printf("sum: %lg\n", sum);
  } else if (1) {
    // using doubles instead of longs
    // 
    double pd= pp;
    double sum= 0;
    for(double a=2; a<=100.0l; a++) {
      double b= pp-1.0l;
      for(double c=2.0l; c<=b; c++) {
	double r= ((long)pd) % ((long)c);
	if (r==0.0l) {
	  sum+= c;
	  printf("%lg is not prime says %lg\n", pd, c);
	}
      }
    }
    //printf("sum: %lg\n", sum);
  } else if (0) {
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
