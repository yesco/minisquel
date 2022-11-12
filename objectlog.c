//  ObjectLog Interpreter
// ----------------------
// (>) 2022 jsk@yesco.org

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

int debug= 0, stats= 0;

#include "utils.c"

#define MAXNUM 20000000l

#define MAXVARS 1024
#define MAXPLAN 2048

int trace= 0;
long olops= 0;

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
  if (v>-MAXNUM && v<MAXNUM)
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

// Returns 0 if fail, or 1 if true (r)
//   or came to end
int run(int* start) {
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

#define Pa     a=N
#define Pab    a=N;b=N
#define Pabc   a=N;b=N;c=N

int result= 1;
  
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
  // -- control flow
  case   0: goto done;             // end = true
  case 't': goto done;             // true (ret)
  case 'f': goto fail;             // fail
  case 'r': result = !result;break;// reverse/not?
  case 'j': Pr; p += r; break;     // jump +47
  case 'g': Pr; p= plan + r; break;// goto 3
  case 127: while(127==N); break;  // nop
  // (127 = DEL, means overwritten/ignore!)
    
  // - manual prime
  // 10: (o 17 11 13 15) // 17 is goto!
  // 11: OR (! "n" "2")
  // 12:    (t)
  // 13: OR (! "n" "3")
  // 14:    (t)
  // 15: OR (! "n" "5")
  // 16:    (t)
  // 17: (. "Maybe prime" "n")
  case 'o': // OR run pos: a b c ... 0 or fail
    Pr; // where to jump
    while(*p)
      if (run(N+plan)) {
	p= plan+r; continue;
      }
    goto fail;

  // set var
  case ':': Pra;  R= A; break;

  // arith
  case '+': Prab; R= A+B; break;
  case '-': Prab; R= A-B; break;
  case '*': Prab; R= A*B; break;
  case '/': Prab; R= A/B; break;
  case '%': Prab; R= L(A)%L(B); break;

  // cmp
  case '=': Pab; if (A!=B) goto fail; break;
  case '!': Pab; if (A==B) goto fail; break;
  case '<': Pab; if (A>=B) goto fail; break;
  case '>': Pab; if (A<=B) goto fail; break;
//case TWO('<', '>'):

  // logic - mja
  case '&': Prab; R= A&&B; break; // implicit
  case '|': Prab; R= A||B; break; // special

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
  
  olops++;
}

// if goto here will return 1 !
// (needed as to break loop from inside switch)
done: result = !result; 

// if goto here will return 0 !
fail: result = !result;
  

  // TODO: revisit?
  //  (what if want to retain some values?)
 
  // cleanup down to start
  while(--p>=start) {
    if (*p < 0) {
      //printf("clean %ld: %d\n", p-plan, *p);
      var[-*p] = 0;
    }
  }
  // Return 1/true if (r) or reach end, no fail
  return result;
}

int strisnum(char* s) {
  return isdigit(s[0]) || *s=='-' && isdigit(s[1]);
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
      *++nextvar = strisnum(s) ? atol(s) : (long)s;
      printf("var[%ld] = %ld\n", nextvar-var, *nextvar);
    } else {
      // add plan
      printf("%s ", *argv);
      *p = strisnum(s) ? atol(s) : *s+256*s[1];
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
  long ms= mstime();
  run(plan);
  ms = mstime()-ms;
  printf("Program took %ld ms and performed %ld ops\n", ms, olops);
  hprint(olops*1000/ms, " ologs (/s)\n");
}

// ./dbrun 'select 3,2 from int(1,10000000) i\
 where "F"="Fx"'
// - takes 2750 ms for 10M "cmp"
// - (old ./run took 4010 and dependent on string size)

// 10M/2.750s = 3.64 Mops

// We're talking 20x speed difference

// try to use dbval inside objectlog...
// slowdown expected. 5x?
//
// That's still quadruple speed
