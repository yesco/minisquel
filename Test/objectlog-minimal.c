//        Minimal
//  ObjectLog Interpreter
// ----------------------
// (>) 2022 jsk@yesco.org

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

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

int* printhere(int* p) {
  if (!*p) return NULL;
  
  printf("%2ld (%c",
	 p-plan, *p>31?*p:'?');
  
  while(*++p)
    printf(" %d", *p);
  printf(")");
  return p+1;
}

void printplan(int* p, int lines) {
  if (lines>1 || lines<0)
    printf("PLAN\n");

  for(int i=0; i<lines || lines<0; i++) {
    if (!p || !p[i]) break;

    p= printhere(p);
    putchar('\n');
  }
  printf("\n");
}

// Returns 0 if fail, or 1 if true (t)
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
    case 't': goto done; // true (ret)
    case 'f': goto fail; // fail

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

      // loop
    case 'i': Prab;
      for(R=A; R<=B; R++) run(p+1); goto done;
    
      // print
    case '.': while(*p) { printvar(N); } break;
    case 'n': putchar('\n'); break;
    
    default:
      printf("\n\n%% Illegal opcode at %ld: %d '%c'\n",
	     p-plan-1, f, f>31?f:'?');
      exit(0);
    }

    // step over zero
    assert(!*p);
    p++;
  
    olops++;
  }

 done: result = !result;
 fail: result = !result;
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
  run(plan);
}
