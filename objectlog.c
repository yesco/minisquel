//  ObjectLog Interpreter
// ----------------------
// (>) 2022 jsk@yesco.org

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

#define MAXVARS 1024
#define MAXPLAN 2048

int trace= 0;

// all variables in a plan are numbered
// zero:th var isn't used.
// a zero index is a delimiter

long*  var= NULL;
char** name=NULL;

long*  nextvar= NULL;


// PLAN
//
// indices into vars[i] (names[i])

int* plan= NULL;

// Plan is a serialized array of indices
//
// Example:
//
//   set, -var, const, 0
//   fun, -ivar, ivar, ..., 0
//   fun, ivar, 0
//   fun, 0
//   fun, 0,
//   0

void init(int size) {
  // lol
  long n= nextvar-var;
  var = realloc(var, size);
  nextvar= n+var;
  
  name= realloc(name, size);
  plan= realloc(plan, size);
}

void printvar(int i) {
  long v= var[i];
  if (v>-1000 && v<1000)
    printf("%ld ", v);
  else
    printf("%s ", (char*)v);
}

void printvars() {
  printf("\n\nVARS\n");
  for(int i=1; i<=nextvar-var; i++) {
    printf("%3d: ", i); printvar(i);
    putchar('\n');
  }
}

void printplan(int* p, int lines) {
  if (lines>1 || lines<0)
    printf("PLAN\n");

  for(int i=0; i<lines || lines<0; i++) {
    if (!p[i]) break;

    printf("%3d: %3d   ( %c  ",
	   i, p[i], p[i]>31?p[i]:'?');

    while(p[++i])
      printf("%d ", p[i]);
    printf(")\n");

  }
  printf("\n");
}

void printhere(int* p) {
  if (!*p) return;
  
  printf("%2ld (%c",
	 p-plan, *p>31?*p:'?');
  
  while(*++p)
    printf(" %d", *p);
  printf(")");
}

void run(int* start) {
  int* p= start;
  long* v= var;

#define N abs(*p++)
#define L(x) ((long)x)
  
#define R v[r]
#define A v[a]
#define B v[b]
#define C v[c]

  int r, a, b, c;
  
#define Pr     r=N
#define Pra    Pr;a=N
#define Prab   Pra;b=N
#define Prabc  Prab;c=N


int f;
while((f=N)) {

  if (trace) {
    printf("\n[");
    printhere(p-1);
    putchar('\t');

    // print vars
    for(int i=1; i<=10; i++) {
      if (!v[i]) break; // lol
      printvar(i);
    }

    printf("]\t");
  }

  switch(f) {
  // ctrl flow
  case 0  : goto done;

  // set
  case ':': Pra;  R= A; break;

  // arith
  case '+': Prab; R= A+B; break;
  case '-': Prab; R= A-B; break;
  case '*': Prab; R= A*B; break;
  case '/': Prab; R= A/B; break;
  case '%': Prab; R= L(A)%L(B); break;

  // cmp
  case '=': Prab; R= A==B; break;
  case '<': Prab; R= A<B; break;
  case '>': Prab; R= A>B; break;
//case TWO('<', '>'):

  // logic
  case '!': Pra;  R= !A; break;
  case '&': Prab; R= A&&B; break;
  case '|': Prab; R= A||B; break;

  case 'i': Prab;
    for(R=A; R<=B; R++) run(p+1); goto done;
    
  case '.': while(*p) { printvar(N); } break;
  case 'n': putchar('\n'); break;
    
  default:
    printf("\n\n%% Illegal opcode at %ld: %d '%c'\n", p-plan-1, f, f>31?f:'?');
    exit(0);
  }

  // step over zero
  assert(!*p);
  p++;
}

done:
  
  // cleanup down to start
  while(--p>=start) {
    if (*p < 0) {
      //printf("clean %ld: %d\n", p-plan, *p);
      var[-*p] = 0;
    }
  }
}

int isnum(int c) {
  return isdigit(c) || c=='-';
}
    
int main(int argc, char** argv) {
  init(1024);

  // read plan from arguments
  int* p= plan;
  nextvar= var;
  while(--argc>0 && *++argv) {
    char* s= *argv;
    if (*s == ':') {
      // init var
      // TODO: "can't just store strptr!"
      --argc; s= *++argv;
      *++nextvar = isnum(*s) ? atoi(s) : (long)s;
      printf("var[%ld] = %ld\n", nextvar-var, *nextvar);
    } else {
      // add plan
      printf("%s ", *argv);
      *p = isnum(*s) ? atoi(s) : *s+256*s[1];
      if (!*p) putchar('\n');
      p++;
    }
  }
  *p++ = 0; // just to be sure!
  *p++ = 0; // just to be sure!
  putchar('\n');

  printf("\nLoaded %ld constants %ld plan elemewnts\n", nextvar-var, p-plan);
  
  printvars();
  printplan(plan, -1);

  //trace= 1;
  run(plan);
}
