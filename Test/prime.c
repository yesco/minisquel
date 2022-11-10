// Willans' formula from 1964:
//   F(n) = [ cos2 π ((n-1)! + 1)/n ]
//   where [ ... ] denotes the
// greatest integer function.
// Then for n ≥ 2,
//   F(n) = 1 iff n is a prime.

#include <stdio.h>
#include <math.h>

double x= 0;
int F(int n) {
  double fac= 1;
  for(int i=1; i<n; i++) fac*= i;
  //printf("fac(%d) =%g\n", n-1, fac);
  double f= cosl(2*M_PI*(fac+1)/n);
  x= f*f;
  return floor(f*f);
}

double F2(int n) {
  double fac= 1;
  for(int i=1; i<n; i++) fac*= i;
  double f= (fac+1)/n;
  return f;
}
    
    

int main(int argc, char** argv) {
  for(int i=0; i<100; i++) {
    int f;
    if (0) {
      f= F(i);
      printf("%3d %d %lf\n", i, f, x);
    } else {
      f= F2(i);
      printf("%3d %lf\n", i, f);
    }
  }
}

/*

  

     z= ((n-1)!+1) / n,   n > 2
 
     n is prime iff z==floor(z)
  


*/

  











