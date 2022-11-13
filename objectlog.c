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

#define L(x) ((long)x)

// PLAN
//
// indices into vars[i] (names[i])

int* plan= NULL;

long** lplan= NULL;

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
  var = realloc(var, size*sizeof(*var));
  nextvar= n+var;
  
  name= realloc(name, size*sizeof(*name));
  plan= realloc(plan, size*sizeof(*plan));
  lplan= realloc(lplan, size*sizeof(*lplan));
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
    printf("%3d: ", i);
    //printf("\t%p", &var[i]);
    printvar(i);
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

long** lprinthere(long** p) {
  if (!*p) return NULL;
  
  long f= L(*p);
  printf("%2ld ", p-lplan);
  if (f < 256*256)
    printf("(%c", isprint(f)?(int)f:'?');
  else
    printf("(%p", *p);
  
  while(*++p)
    printf(" %ld", *p-var);
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

void lprintplan(long** p, int lines) {
  if (lines>1 || lines<0)
    printf("LPLAN\n");

  for(int i=0; i<lines || lines<0; i++) {
    if (!p || !p[i]) break;

    p= lprinthere(p);
    putchar('\n');
  }
  printf("\n");
}

// Returns 0 if fail, or 1 if true (r)
//   or came to end
int run(int* start) {
  int* p= start;
  long* v= var;

#define N abs(*p++)

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

#undef N
  
#undef R
#undef A
#undef B
#undef C

#undef Pr    
#undef Pra   
#undef Prab  
#undef Prabc 

#undef Pa    
#undef Pab   
#undef Pabc  
}

// if goto here will return 1 !
// (needed as to break loop from inside switch)
done: result = !result; 

// if goto here will return 0 !
fail: result = !result;
  

  // TODO: revisit?
  //  (what if want to retain some values?)
 
  // cleanup down to start
//printf("\tip= %ld\n\tistart=%ld\n\tstart=%d\n", p-plan, start-plan, start-plan);
 while(--p>=start) {
    if (*p < 0) {
      //printf("clean %ld: %d\n", p-plan, *p);
      var[-*p] = 0;
    }
  }
  // Return 1/true if (r) or reach end, no fail
  return result;
}

// lrun is 56% faster!
int lrun(long** start) {
  long** p= start;
  long* v= var;

#define N (*p++)
  
  long *r, *a, *b, *c;
  
#define R (*r)
#define A (*a)
#define B (*b)
#define C (*c)

#define Pr     r=N
#define Pra    Pr;a=N
#define Prab   Pra;b=N
#define Prabc  Prab;c=N

#define Pa     a=N
#define Pab    a=N;b=N
#define Pabc   a=N;b=N;c=N

int result= 1;
  
int f;
 while((f=L(N))) {

  if (trace) {
    printf("\n[");
    lprinthere(p-1);
    putchar('\t');

    // print vars
    for(int i=1; i<=10; i++) {
      //      if (!v[i]) break; // lol
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
    //case 'j': Pr; p += r; break;     // jump +47
    //case 'g': Pr; p= plan + r; break;// goto 3
  case 127: while(127l==L(N)); break;  // nop
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
    /*
  case 'o': // OR run pos: a b c ... 0 or fail
    Pr; // where to jump
    while(*p)
      //      if (run(N+plan)) {
      //	p= plan+r; continue;
      //      }
    goto fail;
    */
    
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
    for(R=A; R<=B; R++) lrun(p+1); goto done;
    
  case '.': while(*p) { printvar((long*)N-var); } break;
  case 'n': putchar('\n'); break;
    
  default:
    printf("\n\n%% Illegal opcode at %ld: %d '%c'\n", p-lplan-1, f, f>31?f:'?');
    exit(0);
  }

  // step over zero
  assert(!*p);
  p++;
  
  olops++;

#undef N
  
#undef R
#undef A
#undef B
#undef C

#undef Pr    
#undef Pra   
#undef Prab  
#undef Prabc 

#undef Pa    
#undef Pab   
#undef Pabc  
}

// if goto here will return 1 !
// (needed as to break loop from inside switch)
done: result = !result; 

// if goto here will return 0 !
fail: result = !result;
  

  // TODO: revisit?
  //  (what if want to retain some values?)
 
  // cleanup down to start


 // TODO: lrun lost ability to clear values
 //   as -values not retained (can read from plan... lol
 if (0){
 int *ip= plan + (p-lplan);
 int *istart= plan + (start-lplan);
 // printf("\tip= %ld\n\tistart=%ld\n\tstart=%d\n", ip-plan, istart-plan, start-lplan);
 while(--ip>=istart) {
   if (*ip < 0) {
     //printf("clean %ld: %d\n", ip-plan, *ip);
     var[-*ip] = 0;
   }
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
  long** lp= lplan;
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
      long f= s[0] + 256*s[1];
      long isnum= strisnum(s);
      long num= atol(s);
      *p = isnum ? (num ? num : 0) : f;
      long* v= *lp = isnum? (num ? var+labs(num) : NULL) : (long*)f;
      //printf("v= %p\n", v);
      if (!*p) putchar('\n');
      p++; lp++;
    }
  }

  *p++ = 0; // just to be sure!
  *p++ = 0; // just to be sure!

  *lp++ = 0; // just to be sure!
  *lp++ = 0; // just to be sure!

  putchar('\n');

  printf("\nLoaded %ld constants %ld plan elemewnts\n", nextvar-var, p-plan);
  
  printvars();

  printplan(plan, -1);
  lprintplan(lplan, -1);



  //trace= 1;

  // can't run both since vars lost!
  if (0)
  {
    printf("\n\nPLAN---Running...\n");
    long ms= mstime();
    run(plan);
    ms = mstime()-ms;
    printf("\n\n! Program took %ld ms and performed %ld ops\n", ms, olops);
    hprint(olops*1000/ms, " ologs (/s)\n");
  }
  else
  {
    printf("\n\nLPLAN---Running...\n");
    long ms= mstime();
    lrun(lplan);
    ms = mstime()-ms;
    printf("\n\n! Program took %ld ms and performed %ld ops\n", ms, olops);
    hprint(olops*1000/ms, " ologs (/s)\n");
  }

  printf("\n\n");
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


// a OR b === !(!a AND !b)

// x<s OR x>e (outside interval/NOT BETWEEN)
//
// (| x < s)
// (|)
// (| x > e)
// (| fail)

// out-of-bounds

// (x<s AND foo) OR (x>e AND bar)
//                                    sum
// (OR_BEGIN)     3     0      3-2=+1  +1
//   (x < s)      < x s 0      
//   (foo)        foo   0      
// (OR_BREAK)     2     0      2-2=0   +1
//   (x > e)      > x e 0      
//   (BAR)        bar   0      
// (OR_END)       1     0      1-2=-1   0
//
// QED: out-of-bounds
//

// when scanning/skipping just sum n-2
// when n=1,2,3
//
// when it's 0 we're out of last OR_END!
//
// if fail find next OR_BREAK/OR_END
//   if end up on OR_END fail
//
// if no fail and arrive OR_BREAK/OR_END
//   find OR_END and continue


//       ==
//  !(x!<5 AND x!>10)
//  !(x>=5 AND x<=10)      inside bounds

// x = 7   pass all, NOT_END will fail 
// x = 3   fail first, go NOT_END continue

// reverse action
//
// x = 7
//   NOT x < 5  == ok, continue
//   NOT x > 10 == ok, continue
//   NOT_END fail

// x = 3
//   NOT x < 5  == fail, find NOT_END
//            continue after
//   NOT_END 

// (NOT_BEGIN)
//   (NOT x < 5)
//   (NOT x > 10)
// (NOT_END)
//
// if NOT x < s go next
// if (x<s) return fail
//    (scan and find NOT_END
//    continue after)
//
// if NOT x > e go next
// if (x>e) return fail
//    (scan and find NOT_END
//     continue after)
//
// if arrive NOT_END 
//    fail
