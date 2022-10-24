#include <stdio.h>
#include <stdlib.h>

// LAMBDA/THUNK

typedef struct slambda {
  //void* (*f)(struct lambda *self);
  void* f;
  void *a1, *a2, *a3, *a4, *a5;
} slambda;

typedef void(**lambda)();
  
typedef void(**ilambda)(void*,void*,void*,void*,void*);

#define LAMBDA(...) (lambda)&(slambda){__VA_ARGS__}
#define LARG(F, I) ((slambda*)F)->I

#define CALL(F) (**((ilambda)F))(LARG(F, a1),LARG(F,a2),LARG(F,a3),LARG(F,a4),LARG(F,a5))

typedef struct trampline {
  void* f;
  lambda l;
} trampoline;

void* tramp1(void* _l) {
  printf("tramp1 a\n");
  static lambda l= NULL;
  printf("tramp1 b\n");
  if (!l) {
    printf("tramp1   1\n");
    l= malloc(sizeof(slambda));
    printf("tramp1   2\n");
    memcpy(l, (void*)*(void**)_l, sizeof(slambda));
    printf("tramp1   3\n");
    return l;
  }
  printf("tramp1 c\n");
  CALL(l);
  printf("tramp1 d\n");
  return l;
}

void tramp2() {
  //tramps[2].f(tramps[2].);
}

trampoline tramps[2] = {
  {tramp1, NULL},
  {tramp2, NULL},
};  

// Examples
void print(char* s) {
  printf("%s", s);
}

void printhello() {
  printf("Hello ");
}

void inc(int* i) {
  (*i)++;
}

int main(){
  //lambda h= tramp1(LAMBDA(printhello));

  lambda h= LAMBDA(printhello);
  lambda w= LAMBDA(print, "world!\n");

  int i= 7;
  lambda n= LAMBDA(inc, &i);
	 
  CALL(n); printf("%d\n", i);
  CALL(n); printf("%d\n", i);
  CALL(h); CALL(h); CALL(w);
  CALL(n); printf("%d\n", i);
  CALL(n); printf("%d\n", i);
}
