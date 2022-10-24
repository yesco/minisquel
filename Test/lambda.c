#include <stdio.h>
#include <stdlib.h>

// LAMBDA/THUNK

typedef struct slambda {
  //void* (*f)(struct lambda *self);
  void* f;
  void *a1, *a2, *a3, *a4, *a5;
} slambda;

typedef void(**xlambda)();
  
typedef void(**ilambda)(void*,void*,void*,void*,void*);

#define OLAMBDA(...) (xlambda)&(slambda){__VA_ARGS__}

#define LAMBDA(...) tramp((xlambda)&(slambda){__VA_ARGS__})

#define LARG(F, I) ((slambda*)F)->I

#define CALL(F) (**((ilambda)F))(LARG(F, a1),LARG(F,a2),LARG(F,a3),LARG(F,a4),LARG(F,a5))

typedef struct trampline {
  void* f;
  xlambda l;
} trampoline;

void tramp0();
void tramp1();
void tramp2();
void tramp3();
void tramp4();

trampoline tramps[] = {
  {tramp0, NULL},
  {tramp1, NULL},
  {tramp2, NULL},
  {tramp3, NULL},
  {tramp4, NULL},
};  

#define TRAMP(N) void tramp##N(){ xlambda l= tramps[N].l; CALL(l); }

TRAMP(0)
TRAMP(1)
TRAMP(2)
TRAMP(3)
TRAMP(4)

typedef void*(*lambda)();

trampoline* curtramp= &tramps[0];
const int TRAMPCOUNT= sizeof(tramps)/sizeof(trampoline);

lambda tramp(xlambda l) {
  for(int i=0; i<TRAMPCOUNT; i++) {
    if (++curtramp >= tramps + TRAMPCOUNT)
      curtramp= &tramps[0];

    if (!curtramp->l) {
      curtramp->l= l;
      return curtramp->f;
    }
  }
  fprintf(stderr, "ERROR: run out of tramps!\n");
  exit(66);
  return NULL;
}

void releasetramp(lambda f) {
  for(int i=0; i<TRAMPCOUNT; i++) {
    if (tramps[i].f== f) {
      tramps[i].l= NULL;
      return;
    }
  }
}


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
  lambda f= LAMBDA(print, "Lambda");
  f(); f();
  printf("\n");
  releasetramp(f);
  
  lambda g = LAMBDA(print, "FOOBAR\n");
  g();
  releasetramp(g);
  //g(); // dumps core

  g= tramp(OLAMBDA(print, "SECOND\n"));
  g(); g();
  releasetramp(g);
  
  // OLD STYLE
  printf("\n-------------\n");
  
  lambda h= LAMBDA(printhello);
  lambda w= LAMBDA(print, "world!\n");

  int i= 7;
  lambda n= LAMBDA(inc, &i);
	 
  n(); printf("%d\n", i);
  n(); printf("%d\n", i);
  h(); h(); w();
  n(); printf("%d\n", i);
  n(); printf("%d\n", i);

  releasetramp(h);
  releasetramp(w);
  releasetramp(n);
}
