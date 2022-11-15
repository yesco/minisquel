//  ObjectLog Interpreter
// ----------------------
// (>) 2022 jsk@yesco.org

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <string.h>

int debug=0, stats=0, lineno=0, foffset=0;

#include "utils.c"

#include "csv.c"
#include "vals.c"
#include "dbval.c"

#include "utils.c"

#define MAXNUM 20000000l

int trace= 0;
long olops= 0;

// all variables in a plan are numbered
// zero:th var isn't used.
// a zero index is a delimiter

dbval*  var= NULL;
char** name=NULL;

dbval*  nextvar= NULL;

#define L(x) ((long)x)

// PLAN
//
// indices into vars[i] (names[i])

int* plan= NULL;

dbval** lplan= NULL;

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
  dbp(var[i]);
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

dbval** lprinthere(dbval** p) {
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

void lprintplan(dbval** p, int lines) {
  if (lines>1 || lines<0)
    printf("LPLAN\n");

  for(int i=0; i<lines || lines<0; i++) {
    if (!p || !p[i]) break;

    p= lprinthere(p);
    putchar('\n');
  }
  printf("\n");
}

typedef int(*fun)(dbval**p);


  // adds 130ms to 1945 ms 5.3%
  //#define TWO(s) (s[0]+256*s[1])
  //#define TWO(s) (s[0]-32+(s[1]?3*(s[1]-96):0))

  // no performance loss!
#define TWO(s) (s[0]+3*(s[1]))
#define TWORANGE 128*4

fun func[TWORANGE]= {0};





// lrun is 56% faster!
int lrun(dbval** start) {
  //lprintplan(start, -1);

  dbval** p= start;
  dbval* v= var;

#define N (*p++)
  
  dbval *r, *a, *b, *c;
  
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
  
  long f;
  while((f=L(N))) {

    if (trace) {
      printf("\n[");
      lprinthere(p-1);
      putchar('\t');

      // print vars
      for(int i=1; i<=10; i++) {
	//if (!v[i]) break; // lol
	printvar(i);
      }

      printf("]\t");
    }

    // -- use switch to dispatch
    // (more efficent than func pointers!)

    // direct function pointers!
    if (0 || ((unsigned long)f) > (long)TWORANGE) {
      fun fp= func[f];

      if (!fp) goto done;
      int ret= fp(p);
      //printf("\nf='%c' fp= %p\n", f, fp);
      //      printf("ret= %d\n", ret);
  
      switch(ret) {
      case -1: goto fail;
      case  0: while(*++p); break;
      case  1: goto done;
      case  2: result = !result; break;
      default: error("Unknown ret action!");
      }

      p++;
      olops++;
      continue;
    }

#define CASE(s) case TWO(s)

    switch(f) {
    // -- control flow
    case   0: goto done; // end = true
    case 't': goto done; // true (ret)
    case 'f': goto fail; // fail

    case 'r': result = !result;break;// rev
    case 'j': Pr; p+=L(r);     break;// jmp
    case 'g': Pr; p=lplan+L(r);break;// go
    case 127: while(127==L(N));break;// nop
    // (127 = DEL, overwritten/ignore!)
    
    // 10: (o 17 11 13 15) // 17 is goto!
    // 11: OR (! "n" "2")
    // 12:    (t)
    // 13: OR (! "n" "3")
    // 14:    (t)
    // 15: OR (! "n" "5")
    // 16:    (t)
    // 17: (. "Maybe prime" "n")
    case 'o': // OR nxt a b c...
      Pr;
      while(*p)
	if (lrun(lplan+L(N))) {
	  p= lplan+L(r); continue;
	}
      goto fail;
      
    // set var
    CASE(":="): Pra;  R= A; break;

    // arith
    case '+': Prab; R.d= A.d+B.d; break;
    case '-': Prab; R.d= A.d-B.d; break;
    case '*': Prab; R.d= A.d*B.d; break;
    case '/': Prab; R.d= A.d/B.d; break;
    case '%': Prab; R.d= L(A.d)%L(B.d); break;

    // cmp
    //   number only compare - be sure!
    //   (10-20% FASTER than generic!)
    CASE("N="): Pab;
      if (A.l!=B.l) goto fail; break;

    // string only compare - be sure!
    CASE("S="): Pab;
      if (dbstrcmp(A, B)) goto fail; break;
      
    // generic compare - 20% slower?
    CASE("=="):
    case '=': Pab;
      if (A.l==B.l) break;
      if (isnan(A.d) || isnan(A.d)) {
	if (type(A)!=type(B)) goto fail;
	// same type
	if (dbstrcmp(A, B)) goto fail;
      } else
	if (A.d != B.d) goto fail;
      break;

    CASE("N!"): Pab;
      if (A.l!=B.l) break; goto fail;

    CASE("S!"): Pab;
      if (dbstrcmp(A,B)) break; goto fail;

    // Generic
    CASE("!="):
    CASE("<>"):
      // TODO: complete (see '=')
    case '!': Pab; if (A.l==B.l) goto fail; break;

    // TODO: do strings...
    case '<': Pab; if (A.d>=B.d) goto fail; break;
    case '>': Pab; if (A.d<=B.d) goto fail; break;

    CASE("!>"):
    CASE("<="): Pab; if (A.d>B.d) goto fail; break;

    CASE("!<"):
    CASE(">="): Pab; if (A.d<B.d) goto fail; break;

    // logic - mja (output? no shortcut?)
    case '&': Prab; R.d= A.d&&B.d; break;
    case '|': Prab; R.d= A.d||B.d; break;

    // generators
    case 'i': Prab;
      for(R.d=A.d; R.d<=B.d; R.d+=1) lrun(p+1); goto done;
    case 'd': Prabc;
      for(R.d=A.d; R.d<=B.d; R.d+=C.d) lrun(p+1); goto done;

    // print
    case '.': while(*p) printvar(N-var); break;
    case 'p': while(*p) printf("%s", STR(*N)); break;
    case 'n': putchar('\n'); break;
    
    // -- strings

    // CONCAT
    CASE("CC"): { Pr; int len= 1;
	dbval** n= p;
	while(*n) {
	  printf("\nSTR>>%s<<\n", STR(**n));
	  len+= strlen(STR(**n));
	  n++;
	}
	char* rr= calloc(1, len);
	char* rp= rr;
	while(*p) {
	  char* rs= STR(*N);
	  strcat(rp, rs);
	  //rp += strlen(rs);
	}
	R= mkstrfree(rr, 1);
	break; }
      
    // ASCII
    CASE("SA"):
      Pra; R= mknum(STR(A)[0]); break;

    // CHAR
    CASE("SC"): { Pra; char s[2]={L(A.d),0};
	R= mkstr7ASCII(s); break; }
    
    // CHARINDEX
    CASE("CI"): { Prab; char* aa= STR(A);
	char* rr= strchr(aa, STR(B)[0]);
	R= aa ? mknum(rr-aa) : mknull();
	break; }

    // LEFT      
    CASE("SL"): { Prab;
	R= mkstrfree(strndup(STR(A), num(B)), 1);
	break; }
    
    // RIGHT
    CASE("SR"): { Prab; char* aa= STR(A);
	int len= strlen(aa) - num(B);
	if (len<=0 || num(B)<=0) R= mkstrconst("");
	else R= mkstrdup(aa+len);
	break; }

    // LOWER
    CASE("LW"): { Pra; char* aa= strdup(STR(A));
	char* ab= aa;
	while(*ab) {
	  *ab= tolower(*ab); ab++;
	}
	R= mkstrfree(aa, 1);
	break; }
  
    // UPPER
    CASE("UP"): { Pra; char* aa= strdup(STR(A));
	char* ab= aa;
	while(*ab) {
	  *ab= toupper(*ab); ab++;
	}
	R= mkstrfree(aa, 1);
	break; }
      
    // LTRIM
    CASE("LT"): Pra; R= mkstrdup(ltrim(STR(A))); break;
      
    // RTRIM
    CASE("RT"): Pra; R= mkstrfree(rtrim(strdup(STR(A))), 1); break;

    // TRIM
    CASE("TR"): Pra; R= mkstrfree(trim(strdup(ltrim(STR(A)))), 1); break;
      
      
    // STR
    CASE("ST"): Pra; R= mkstrdup(STR(A)); break;

    // TIMESTAMP
    CASE("ts"): Pr; R= mkstrdup(isotime()); break;
      
    default:
      printf("\n\n%% Illegal opcode at %ld: %ld '%c'\n", p-lplan-1, f, (int)(isprint(f)?f:'?'));
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

 done: result = !result; 
 fail: result = !result;
  
  // -- cleanup down to start
  // -35% performance ???
  // (dbval instead of int -
  //  - still 10% faster...)
  if (1) ; 
  else {
    int *ip= plan + (p-lplan);
    int *istart= plan + (start-lplan);
    dbval x= mkerr();
    // forward loop 2-3% faster
    while(istart<ip) {
      if (*istart < 0) {
	//printf("clean %ld: %d\n", ip-plan, *ip);
	var[-*istart] = x;
      }
      istart++;
    }
  }

  return result;
}


#define R0 return 0;

int ft(dbval**p) { return 1; }
int ff(dbval**p) { return -1; }
int fr(dbval**p) { return 2; }
int fdel(dbval**p) { return 0; }
int fset(dbval**p) {p[0]->l=p[1]->l;R0}
int fplus(dbval**p){p[0]->d=p[1]->d+p[2]->d;R0}
int fminus(dbval**p){p[0]->d=p[1]->d-p[2]->d;R0}
int fmul(dbval**p){p[0]->d=p[1]->d * p[2]->d;R0}
int fdiv(dbval**p){p[0]->d=p[1]->d / p[2]->d;R0}
int fper(dbval**p){p[0]->d=L(p[1]->d)%L(p[2]->d);R0}
int feq(dbval**p){return p[0]->l==p[1]->l?0:-1;}
int fneq(dbval**p){return p[0]->l!=p[1]->l?0:-1;}
int fiota(dbval**p) {
  for(p[0]->d=p[1]->d; p[0]->d<=p[2]->d; (void)(p[0]->d++))
    lrun(p+4);
  return 1;
}
int fprint(dbval**p) {
  while(*p) {
    printvar(*p++-var);
  }
  return 0;
}
int fnewline(dbval**p){putchar('\n');R0}

void regfuncs() {
  func['t']= ft;
  func['f']= ff;
  func['r']= fr;
  func[127]= fdel;
  //  func['o']= JMP OR
  func[':']= fset;
  func['+']= fplus;
  func['-']= fminus;
  func['*']= fmul;
  func['/']= fdiv;
  func['%']= fper;
  func['=']= func[TWO("==")]= feq;
  func['!']= func[TWO("!+")]= func[TWO("<>")]= fneq;
  //  func['<']= 

    //  case '<': Pab; if (A>=B) goto fail; break;
  //  case '>': Pab; if (A<=B) goto fail; break;
  //  CASE("!>"):
  //  CASE("<="): Pab; if (A>B) goto fail; break;
  //  CASE("!<"):
  //  CASE(">="): Pab; if (A<B) goto fail; break;
//case TWO('<', '>'):

  // logic - mja
  //  case '&': Prab; R= A&&B; break; // implicit
  //  case '|': Prab; R= A||B; break; // special

  func['i']= fiota;

  /*
  CASE("DO"):
  case 'd': Prabc;
    for(R=A; R<=B; R+=C) lrun(p+1); goto done;
  */
  
  func['.']= fprint;
  func['n']= fnewline;
}






int strisnum(char* s) {
  return isdigit(s[0]) || *s=='-' && isdigit(s[1]);
}
    
int main(int argc, char** argv) {
  init(1024);
  regfuncs();

  // read plan from arguments
  int* p= plan;
  dbval** lp= lplan;
  nextvar= var;
  while(--argc>0 && *++argv) {
    char* s= *argv;
    if (*s == ':') {
      // init var
      // TODO: "can't just store strptr!"
      --argc; s= *++argv;
      *++nextvar = strisnum(s) ? mknum(atol(s)) : mkstrconst(s);
      printf("var[%ld] = ", nextvar-var); dbp(*nextvar);
    } else {
      // add plan
      printf("%s ", *argv);
      long f= TWO(s);
      long isnum= strisnum(s);
      long num= atol(s);
      *p = isnum ? (num ? num : 0) : f;
      dbval* v= *lp = isnum?( num ? var+labs(num) : NULL) : (void*)f;
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

  lprintplan(lplan, -1);

  //trace= 1;

  printf("\n\nLPLAN---Running...\n");
  long ms= mstime();
  lrun(lplan);
  ms = mstime()-ms;
  printf("\n\n! Program took %ld ms and performed %ld ops\n", ms, olops);
  hprint(olops*1000/ms, " ologs (/s)\n");

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
