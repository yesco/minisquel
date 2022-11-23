#include <stdio.h>

#define TYPE int
#define SIZE 1024*100

long sum= 0;
void foo(TYPE *a, TYPE *b, TYPE *c) {
  int i;
  for(i = 0; i < SIZE; i++) {
    a[i] = b[i] + c[i];
    sum+= a[i] = 2*a[i]+3*c[i];
  }
}

void plus(TYPE* a, TYPE* b, TYPE* c) {
  *a = *b + *c;
  sum+= *a= 2* *a + 3* *c;
}

void minus(TYPE* a, TYPE* b, TYPE* c) {
  *a = *b - *c;
  sum+= *a= 2* *a - 3* *c;
}

typedef void(*fun)(TYPE*a, TYPE*b, TYPE*c);

inline void foof(fun f, TYPE *a, TYPE *b, TYPE *c) {
  int i;
  for(i = 0; i < SIZE; i++) {
    f(&a[i], &b[i], &c[i]);
  }
}

/*

-- https://groups.google.com/g/llvm-dev/c/BwXgqTucjZM/m/fN-YUt2Ro34J

If you compile it with ‘clang -O3 -arch arm64 -S’, you should see the SIMD instructions. If you do see them, it means that your original test is too complicated for the vectorizer right now (that might be due to some bug) - feel free to file a bug.

Thanks,
Michael

*/

int main(void) {
  TYPE A[SIZE];
  TYPE B[SIZE];
  TYPE C[SIZE];
  fun f= (A[0]%2)==0?plus:minus;
  f= minus; // slower when known?
  for(int i=0; i<SIZE*5; i++) {
    if (0) 
      foo(A,B,C);
    else
      foof(f,A,B,C);
  }
  printf("sum= %ld\n", sum);
}
