//  ObjectLog Interpreter
// ----------------------
// (>) 2022 jsk@yesco.org

// An early "simple" variant of this is
// in Test/objectlog-simple.c read that
// one instead!

// This is experimential code, just a for trying out and
// experimenting with different way of doing interpreter
// dispatch.
//
// 1. tight loop, calling a series of func[] pointers
// 2. a) VM-style switch-case dispatch
//    b) GCC-style goto-label dispatch
//
// Variants for 2a and 2b is implmenting NEXT by
//     i) while-loop/break
//    ii) goto next
//   iii) jump label


// The structure of this code is using global variables.
// It compiles and runs a single plan.

// TODO: generalize?



#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <string.h>


// Use labels for jumping
// Not sure can tell the speed diff!
#define JUMPER
  

// global statistics used elsewhere
//int debug=0, stats=0, lineno=0, foffset=0, security= 0;
int debug=1, stats=0, lineno=0, foffset=0, security= 0;
int nfiles= 0;

// parameters for parsing etc
#define NAMELEN 64
#define VARCOUNT 256
#define MAXCOLS 32
#include "utils.c"
#include "csv.c"
#include "vals.c"
#include "dbval.c"
#include "table.c"

#include "darr.c"

#include "malloc-count.c"
//#include "malloc-simple.c"

#define MAXNUM 20000000l

int trace= 0;
long olops= 0, nresults= 0;

// all variables in a plan are numbered
// zero:th var isn't used.
// a zero index is a delimiter

// This is code for experimenting, thus it's using globals
// interpreting a single objectlog program

// TODO: make it more "functional" and composable?
// the drawback of this is passing around more variables.
// ALT: just fork for background tasks?

dbval*  var= NULL;
char** name=NULL;

// bad name as it points to prev. lol
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

// A Plan for a query, can contain
// several objectlog functions.
// Use as "template" - can't exec!
typedef struct Plan {
  char*   name; // or be in val?
  int     plansize;
  int*    plan;
  int     varsize;
  dbval*  var;
} Plan;


//  dbval** lplan; // compiled into ptrs

Plan* mkplan(char* name, int* plan, dbval* var, int plansize, int varsize) {
  Plan* p= calloc(1, sizeof(plan));
  p->name = strdup(name);

  p->plansize= plansize;
  p->plan= memdup(plan, plansize*sizeof(*plan));

  p->varsize= varsize;
  p->var= memdup(var, varsize*sizeof(*var));
		  
  return p;
}


typedef int(*fun)(dbval**p);

typedef int(*lfun)(dbval**p, dbval* var);

// A thunk is an executble plan, more like
// a continuation.
typedef struct thunk {
  Plan*   plan;
  dbval** lplan;
  dbval*  var;
  int     paramix;
  // int params; // TODO: not overwrite
  struct thunk* out;
  fun     ccode;
} thunk;


// Two letter function names are mapped
// to an interger in TWORANGE (512 values).
// This keeps SWITCH fast, more values
// performance deterioates too much!

  // SLOWER - adds 130ms to 1945 ms 5.3%
  //
  //#define TWO(s) (s[0]+256*s[1])
  //#define TWO(s) (s[0]-32+(s[1]?3*(s[1]-96):0))
  //
  // conclusion: too big space thus can't do arrayjmp?

// no performance loss!
// (keep target space small)
#define TWO(s) (s[0]+3*(s[1]))
#define TWORANGE 128*4

// pointers to functions
fun func[TWORANGE]= {0};

// pointer to labels inside lrun
void* jmp[TWORANGE]= {0};

darr* plans= NULL;

// actually, the plan is represented by
// a "default" thunk. The thunk can only
// be used "once" in another plan. It
// provides it's own storage.
//
// Thunk.plan -> Plan ...
void regplan(Plan* p, thunk t) {
  thunk* tp= memdup(&t, sizeof(t));
  plans= dpush(plans, (dint)tp);
}

thunk* findplan(char* name) {
  int len= dlen(plans);
  for(int i=0; i<len; i++) {
    thunk* t= (void*)dget(plans, i);
    if (0==strcmp(name, t->plan->name))
      return t;
  }
  return NULL;
}

void lprintplan(dbval** lplan, dbval* var, int lines);

// Compiles a PLAN to a concrete thunk.
// The funk can be run/called.
// The compilation transforms integer
// Plan->plan to pointer to variables
// and direct "opcode" pointer to labels.
thunk compilePlan(Plan* plan) {
  // Use Plan as template
  thunk t= {};
  // Copy vars from template
  t.plan= plan;
  t.var= memdup(plan->var, plan->varsize*sizeof(*t.var));
  // Generate pointer array
  t.lplan= malloc(plan->plansize*sizeof(*t.lplan));
  int* p= plan->plan;
  dbval** d= t.lplan;
  for(int i=0; i < plan->plansize; i++) 
    t.lplan[i]= (void*)~0L;
  int i= 0;
  //printf("PLANSXIZE=%d\n", plan->plansize);
  //  while(i < plan->plansize) {
  while(1) {

    // -- function (int) -> label
    void* x= jmp[*p];

    // "optimize"
    if (x==jmp['=']) {
      int ta= type(plan->var[abs(p[1])]);
      int tb= type(plan->var[abs(p[2])]);
      //printf("----EQ %d %d\n", ta, tb);
      if (ta==TNUM || tb==TNUM)
	x= jmp[TWO("N=")]; // 4 % savings
      else if (ta==TSTR || tb==TSTR)
	x= jmp[TWO("S=")];
      else if (ta==TPTR || tb==TPTR)
	x= jmp[TWO("S=")];
    }

    // store function pointer
    *d++ = x; p++; i++;

    if (debug)
      printf("%2d %3d '%c' %p\n", i-1, p[-1], isprint(p[-1])?p[-1]:'?', d[-1]);

    if (!x) {
      printf("%2d %3d '%c' %p\n", i-1, p[-1], isprint(p[-1])?p[-1]:'?', d[-1]);
    error("objectlog.compile: no such function");
    }

    if (p[-1] == 0) {
      //printf("STOP COMPILE\n");
      *d++= 0; p++; i++;
      //assert(i == p - plan->plan);
      break;
    }

    // -- vars
    if (x==jmp[TWO("call")]) {
      // skip "actual" pointer...
      // TODO: could just ptr atr str value
      // and here do the lookup . same same..
      //printf(" ... CALLLLLL %p\n", *p);
      *d++ = (void*)findplan("double"); 
      p++; i++;
    }

    while(*p) {
      *d++= t.var + abs(*p++); i++;
      if (debug) printf("\t%d %p\n", p[-1], d[-1]);
    }
    *d++ = 0; p++; i++; // 0
  }

  //lprintplan(t.lplan, t.var, -1);
  return t;
}

int varplansize= 0;

// We provide our own global "Memory hEAP"
// It's basically an arena, and as all allocations are
// local and structures, deallocation is implicit by
// reasetting the meap pointer after a "function" call.
char* meaporig= NULL;
char* meap= NULL;

void init(int size) {
  meap= meaporig= malloc(1024*1024*1);

  varplansize= size;
  // These are used for building,
  // once read/loaded they are named
  // and stored.
  long n= nextvar-var;
  var = realloc(var, size*sizeof(*var));
  nextvar= n+var;
  
  plan= realloc(plan, size*sizeof(*plan));
  lplan= realloc(lplan, size*sizeof(*lplan));
}

void cleandefault() {
  memset(var, 0, varplansize*sizeof(*var));
  memset(plan, 0, varplansize*sizeof(*var));
  memset(plan, 0, varplansize*sizeof(*var));
}

void printvar(int i, dbval* var) {
  dbp(var[i]);
}

void printvars(dbval* var, int n) {
  printf("\n\nVARS\n");
  for(int i=1; i<=n; i++) {
    printf("%3d: ", i);
    //printf("\t%p", &var[i]);
    //printvar(i);
    printf("%p ", var[i].p);
    printf("%s", STR(var[i]));
    putchar('\n');
  }
}

dbval** lprinthere(dbval** start, dbval** p, dbval* var) {
  if (!*p) return NULL;
  long f= L(*p);
  printf("%2ld ", p-start);
  if (f < 256*256)
    printf("(%c", isprint(f)?(int)f:'?');
  else
    printf("(%p", *p);
  
  while(*++p)
    printf(" %ld", *p-var);
  printf(")");
  return p+1;
}

void lprintplan(dbval** start, dbval* var, int lines) {
  dbval** p= start;
  if (lines>1 || lines<0)
    printf("LPLAN\n");

  for(int i=0; i<lines || lines<0; i++) {
    if (!p || !*p) break;

    p= lprinthere(start, p, var);
    putchar('\n');
  }
  printf("\n");
}


typedef int(*plancall)(thunk* t, dbval* var);

// a reference doesn't own ptr
// (so it won't free it)
// ptr must be guaranteed to stay around
// otherwise "copy" (dbcopy)
dbval dbref(dbval d) {
  void* p= ptr(d);
  if (!p) return d;
  return mkptr(p, 0);
}

long trun(thunk* t, thunk* toOut);
long lrun(dbval** start, dbval* var, thunk* toOut);

long initlabels() {
  return lrun(NULL, NULL, NULL);
}

// Call another plan/thunk expecting result
int call(thunk* t, dbval** params, dbval* var, thunk* toOut) {
  fprintf(stderr, "CALL...%p\n", t);
  fprintf(stderr, "CALL...%p, %p\n", t->lplan, t->var);
  dbval** p= params;
  dbval* d= t->var + t->paramix + 6; // TODO: 6?
  // copy in params
  fprintf(stderr, "about to copy\n");
  while(*p) {
    fprintf(stderr, "1 copied: %s\n", STR(**p));
    *d++= dbref(**p++);
  }
  fprintf(stderr, "CALL.copied...\n");
  // using a future "ret" thunk
  // using "out" to copy out same param!
  // This makes us continuation based

  // TODO: paramsix not correct to return

  // p+1 is next statement after call!
  thunk ret= {NULL, params, var, -1, toOut };
  fprintf(stderr, "CALL.lrun...\n");
  int r= lrun(t->lplan, t->var, &ret);
  return r;
}

// Return (out) a result to a plan
// (by calling it's cont)
// When this value is processed, it'll
// backtrack to find next out in orig plan.
int out(thunk* t, dbval** params, dbval* var) {
  fprintf(stderr, "OUT....\n");
  if (!t) return 0;
  if (t->ccode) return (*t->ccode)(params);

  dbval** p= params;
  // points to first caller param!
  dbval** d= t->lplan;
  // copy OUT params to caller
  while(*d) {
    fprintf(stderr, "...copy out %ld <= %ld: %s\n", *d-t->var, *p-var, STR(**p));
    **d++= dbref(**p++);
  }
  fprintf(stderr, "OUT..lrun\n");
  // continue with original "toOut" function
  // called after "out" of caller
  return lrun(++d, t->var, t->out);
}
 
// Returns: ALWAYS a pointer to
//   a copied string in the MEAP
//   If null or emtpy return empty
char* mstrdup(char* s) {
  char* p= meap;
  if (s) while(*s) *meap++ = *s++;
  *meap++ = 0;
  return p;
}

// lrun is 56% faster! (mostly because no cleanup, lol)
long trun(thunk* t, thunk* toOut) {
  return lrun(t->lplan, t->var, toOut);
}

long lrun(dbval** start, dbval* var, thunk* toOut) {
  // This has to be done inside this
  // function, little irritating, as it's
  // used in compilation outside.
  // Call lrun with NULL, NULL once.
  static void* ljmp[TWORANGE]= {
    [  0]= &&END,
    ['t']= &&TRUE,
    ['f']= &&FAIL,
    ['r']= &&REVERSE,
    ['j']= &&JUMP,
    ['g']= &&GOTO,
    [127]= &&DEL,
    ['o']= &&OR,

    [TWO(":=")]= &&SET,
    ['c']= &&CCODE,
    [TWO("ca")]= &&CALL,
    [TWO("ru")]= &&lrun,

    ['+']= &&PLUS,
    ['-']= &&MINUS,
    ['*']= &&MUL,
    ['/']= &&DIV,
    ['%']= &&MOD,

    [TWO("N=")]= &&NUMEQ,
    [TWO("S=")]= &&STREQ,
    ['=']= &&EQ,
    [TWO("==")]= &&EQ,
    ['!']= &&NEQ,
    [TWO("!=")]= &&NEQ,
    [TWO("<>")]= &&NEQ,
    [TWO("N!")]= &&NUMNEQ,
    [TWO("S!")]= &&STRNEQ,
    ['<']= &&LT,
    ['>']= &&GT,
    [TWO("<=")]= &&LE,
    [TWO("!>")]= &&LE,
    [TWO(">=")]= &&GE,
    [TWO("!<")]= &&GE,

    [TWO("li")]= &&LIKE,
    [TWO("il")]= &&ILIKE,

    ['&']= &&LAND,
    ['|']= &&LOR,
    [TWO("xo")] = &&LXOR,
    
    ['i']= &&IOTA,
    ['d']= &&DOTA,
    ['l']= &&LINE,

    [TWO("ou")]= &&OUT,
    ['.']= &&PRINT,
    ['p']= &&PRINC,
    ['n']= &&NEWLINE,

    ['F']= &&FIL,

    [TWO("CO")]= &&CONCAT,
    [TWO("AS")]= &&ASCII,
    [TWO("CA")]= &&CHAR,
    [TWO("CI")]= &&CHARIX,
    [TWO("LE")]= &&LEFT,
    [TWO("RI")]= &&RIGHT,
    [TWO("LO")]= &&LOWER,
    [TWO("UP")]= &&UPPER,
    [TWO("LN")]= &&LENGTH,
    [TWO("LT")]= &&LTRIM,
    [TWO("RT")]= &&RTRIM,
    [TWO("TR")]= &&TRIM,
    [TWO("ST")]= &&STR,

    [TWO("ts")]= &&TS,

#define DEF(a) [TWO(#a)]= &&a
    DEF(sr), DEF(si), DEF(co), DEF(ta),
    DEF(ab), DEF(ac), DEF(as), DEF(at), DEF(a2), DEF(cr), DEF(ce), DEF(fl), DEF(ep),
    DEF(ie), DEF(ii), DEF(in),
    DEF(ln), DEF(lg), DEF(l2),
    DEF(pi), DEF(pw), DEF(ss), DEF(de), DEF(rd), DEF(ra), DEF(rr), DEF(sg),
#undef DEF
  };

  // We copy this jmp table to see if putting directly
  // in plan improves performance.
  static int copiedjmp= 0;
  if (!copiedjmp) {
    memcpy(jmp, ljmp, sizeof(jmp));
    copiedjmp= 1;
  }

  // just to init lablels
  if (!start) return 0;

  //lprintplan(start, var, -1);

  // save ("mark")
  char* _meap= meap;

  dbval** p= start;
  dbval* v= var;

#define N (*p++)
  
  // parameter access for use in switch/case
  dbval *r, *a, *b, *c;
  
#define R (*r)
#define A (*a)
#define B (*b)
#define C (*c)

//#define Pr     r=N;dbfree(R)
#define Pr     r=N
#define Pra    Pr;a=N
#define Prab   Pra;b=N
#define Prabc  Prab;c=N

#define Pa     a=N
#define Pab    a=N;b=N
#define Pabc   a=N;b=N;c=N

//#define SETR(a) do{dbfree(R);R=(a);}while(0)
#define SETR(a) do{dbfree(R);R=(a);}while(0)
  
  long results= 0;
  int result= 1;
  
  long f;
  while(1) {
    // 2-3% faster cmp while(f=...)
    f=L(N);
    olops++;

    if (trace) {
      if (1) {
	// find prev statement
	int i= p-start-2;
	while(i>0 && start[i]) i--;
	if (i<0) i= 0;

	printf("TRACE  @%2d", i);
	//printf("(%c", plan[i]);
	printf(" %p", start[i]);
	dbval* vp;
	while((vp= start[++i])) {
	  printf(" %ld:", vp-var);
	  //printf("%s", STR(*vp));
	}
	printf(")\n");
      } else {
	printf("\n[");
	lprinthere(start, p-1, var);
	putchar('\t');

	// print vars
	for(int i=1; i<=10; i++) {
	  //if (!v[i]) break; // lol
	  printvar(i, var);
	}

	printf("]\t");
      }
    }

    // -- use switch to dispatch
    // (more efficent than func pointers!)

    // direct function pointers!
    if (0 || ((unsigned long)f) > (long)TWORANGE) {
      if (1) {
	// label
	//printf("...JMP\n");
	goto *(void*)f;

      } else {
	// TODO: funcall...
	printf("FOOL\n");
	fun fp= func[f];

	if (!fp) goto done;
	int ret= fp(p);
	//printf("\nf='%c' fp= %p\n", f, fp);
	//      printf("ret= %d\n", ret);
  
	switch(ret) {
	case -1: goto fail;
	case  0: while(*++p); break;
	case  1: goto succeed;
	case  2: result = !result; break;
	case  3: goto done;
	default: error("Unknown ret action!");
	}
      }
      p++;
      continue;
    }

#define CASE(s) case TWO(s)

#ifndef JUMPER
    #define NEXT break
#endif

//mabye 2-3% faster..
//#define NEXT {p++; continue;}

// 10% SLOWER !? WTF, lol
// (indirect jump?)
//#efine NEXT {N; olops++; goto *jmp[L(N)];}

// 8% FASTER w direct pointers...
#ifdef JUMPER
    #define NEXT {N; olops++; \
    if (!*p) goto succeed;	\
      goto *(void*)*p++;}
#endif
    
    // TODO:
    //#  inside NEXT  if (trace) printf("NEXT '%c'(%d) @%ld %p\n", plan[p-lplan], plan[p-lplan], p-lplan, *p); \


    // TODO: why need !*p ??? 
    // ONLY 100% slower than OPTIMAL!
    
#define FAIL(a) {if(a) goto fail; NEXT;}

    switch(f) {
    // -- control flow
END:    case   0: goto succeed;
TRUE:   case 't': goto succeed;
FAIL:   case 'f': goto fail;
REVERSE:case 'r': result = !result;NEXT;
JUMP:   case 'j': Pr; p+=L(r);     NEXT;
GOTO:   case 'g': Pr; p=lplan+L(r);NEXT;
DEL:    case 127: while(127==L(N));NEXT;
// (127 = DEL, overwritten/ignore!)
    
// 10: (o 17 11 13 15) // 17 is goto!
// 11: OR (! "n" "2")
// 12:    (t)
// 13: OR (! "n" "3")
// 14:    (t)
// 15: OR (! "n" "5")
// 16:    (t)
// 17: (. "Maybe prime" "n")
OR:   case 'o': // OR nxt a b c...
      Pr;
      while(*p)
	if (lrun(lplan+L(N), var, toOut)) {
	  p= lplan+L(r); continue;
	}
      goto fail;
SET:  CASE(":="): Pra; R= A; NEXT;
//lrun:  CASE("run"): Pr;
//      results+= lrun((dbval**)R.p, p);
//      NEXT;
CCODE:case 'c': { Pr;
      fun c= R.p;
      // TODO: have controlling codes?
      int ret= c(p);

      switch(ret) {
      case -1: goto fail;
      case  0: while(*++p); break;
      case  1: goto succeed;
      case  2: result = !result; break;
      case  3: goto done;
      default: error("Unknown ret action!");
      }
      NEXT; }

lrun: CASE("run"): ;
CALL: CASE("call"): Pr;
      fprintf(stderr, "...call...\n");
      fprintf(stderr, " thunk=%p \n", r);
      results+= call((thunk*)r, p, var, toOut);
      goto done;

// arith
PLUS:  case '+': Prab; R.d= A.d+B.d; NEXT;
MINUS: case '-': Prab; R.d= A.d-B.d; NEXT;
MUL:   case '*': Prab; R.d= A.d*B.d; NEXT;
DIV:   case '/': Prab; R.d= A.d/B.d; NEXT;
MOD:   case '%': Prab; R.d= L(A.d)%L(B.d); NEXT;

// cmp
//   number/string only compare - be sure!
//   (10-20% FASTER than generic!)
NUMEQ: CASE("N="): Pab; FAIL(A.l!=B.l);
STREQ: CASE("S="): Pab; FAIL(dbstrcmp(A, B));
      
// generic compare - 20% slower?
// TODO:
//   nullsafe =
//   null==null ->1 null=? -> 0
//   - https://dev.mysql.com/doc/refman/8.0/en/comparison-operators.html#operator_equal-to
EQ:   CASE("=="):
      case '=': Pab;
      if (A.l==B.l) NEXT;
      if (isnan(A.d) || isnan(A.d)) {
	if (type(A)!=type(B)) goto fail;
	// same type
	FAIL(dbstrcmp(A, B));
      } else // number
	FAIL(A.d != B.d);

NUMNEQ: CASE("N!"): Pab; FAIL(A.l==B.l)
STRNEQ: CASE("S!"): Pab; FAIL(!dbstrcmp(A,B));

// Generic
NEQ:  CASE("!="):
      CASE("<>"):
      case '!': Pab;
      if (A.l==B.l) goto fail;
      if (isnan(A.d) || isnan(A.d)) {
	if (type(A)!=type(B)) NEXT;
	// same type
	FAIL(!dbstrcmp(A, B));
      } else // number
	FAIL(A.d == B.d);

// TODO: do strings...
LT:   case '<': Pab; FAIL(A.d>=B.d);
GT:   case '>': Pab; FAIL(A.d<=B.d);
LE:   CASE("!>"):
      CASE("<="): Pab; FAIL(A.d>B.d);
GE:   CASE("!<"):
      CASE(">="): Pab; FAIL(A.d<B.d);

// like
LIKE: CASE("li"): Pab; FAIL(!like(STR(A), STR(B), 0));
ILIKE:CASE("il"): Pab; FAIL(!like(STR(A), STR(B), 1));

// and/or
LAND: case '&': Prab; R.d=L(A.d)&L(B.d); NEXT;
LOR:  case '|': Prab; R.d=L(A.d)|L(B.d); NEXT;
LXOR: CASE("xo"): Prab; R.d=L(A.d)^L(B.d); NEXT;

// generators
IOTA: case 'i': Prab; 
      for(R.d=A.d; R.d<=B.d; R.d+=  1) {
	results+= lrun(p+1, var, toOut);
      } goto done;
DOTA: case 'd': Prabc;
      for(R.d=A.d; R.d<=B.d; R.d+=C.d) {
	results+= lrun(p+1, var, toOut);
      } goto done;
LINE: case 'l': { Pa; FILE* fil= A.p;
      // TODO: "move out"?
      //   165ms time cat fil10M.tsv
      //  3125ms time wc fil...
      //  4054ms time ./olrun sql where 1=0
      // 14817ms time ./olrun sql p cols
      // 15976ms    full strings
      // 33s     time ./run sql p cols
      // 56         full strings

      // - 30 610 ms !!!
      // ./run 'select * from "fil10M.tsv" fil where 1=0' | tail

	char *line= NULL, delim= 0;
	size_t len= 0;
	ssize_t n= 0;
	long r= 0;
	table* t= NULL;
	// TODO: csv get line?
	dbval** f;
	// skip header
	getline(&line, &len, fil);
	if (!delim) delim= decidedelim(line);
	char simple= !!strchr("\t;:|", delim);
	char* (*nextfield)(char**, char)
	  = simple ? nextTSV : nextCSV;

	// read lines
	while((n= getline(&line, &len, fil))!=EOF) {
	//while(!feof(fil) && ((line= csvgetline(fil, delim)))) {
	  r+= n;
	  if (!line || !*line) continue;

	  char* s= line;
	  f= p;

	  // get values
	  // NO: malloc/strdup/free!
	  while(*f && s) {
	    dbfree(**f);
	    **f++ = conststr2dbval(
              nextfield(&s, delim));
	  }

	  // set missing values to null
	  while(*f) {
	    dbfree(**f);
	    **f++= mknull();
	  }
	  lrun(f+1, var, toOut);
	  //free(line); // for csvgetline
	}
	fclose(fil);
	free(line); // for getline
	p= f;
	goto done; }

FIL:   case 'F': { Pra;
       FILE* fil= magicfile(STR(A));
       if (!fil) goto fail;
       R.p= fil;
       NEXT; }

// print
OUT:   CASE("ou"): if (toOut) {
	 // basically a return to caller
	 out(toOut, p, var);
	 while(*p) p++;
	 NEXT;
       }

       // default "out" is print!
       // [4] is header... GRRRR TODO: fix
       if (0 && var[4].d>0) {
         for(int i= var[4].d; var[i].d; i++) {
	   printvar(i, var);
	 }
	 putchar('\n');
	 for(int i= var[4].d; var[i].d; i++) {
	   printf("======= ");
	 }
	 putchar('\n');
	 // mark as already printed header
	 //var[4].d= -var[4].d;
       }
       // print actual values
       while(*p) printvar(N-var, var);
       putchar('\n');
       NEXT;
PRINT: case '.': while(*p) printvar(*(p++)-var, var); NEXT;
PRINC: case 'p': while(*p) printf("%s", STR(*N)); NEXT;
NEWLINE: case 'n': putchar('\n'); NEXT;
    
// -- strings
CONCAT: CASE("CO"): { Pr;
	// 100% faster than alloca
        // 1032 ms long2str!
        // from 5800 ms...
        // from 6500 ms
        // 7x faster NOW!!! long2str

        // ./olrun 'select * from num10M num where concat(i,"foo",i+3,"bar",42)!="x" and 1=0'
        //   1800 ms for olog

        // duckdb reading csv: 
        //   1400 ms

        // Test/10Mconcat.c
        //    355 ms

        // olog with enumerator
        //    800-900 ms

        // OLD ol Thu Nov 17 05:56:55 2022 +0700
        //   17000 ms!

        // this morning Nov 23 16:49:40
        //   7676 ms ...
        
        char* ss= meap;
	while(*p) {
	  char* s= STR(*N);
	  while(*s) *meap++ = *s++;
	}
	*meap++ = 0;
	*r= mkstrfree(ss, 0);
	NEXT; }

ASCII:  CASE("AS"): Pra; SETR(mknum(STR(A)[0])); NEXT;

//CHAR:   CASE("CA"): { Pra; char s[2]={L(A.d),0}; SETR(mkstrconst7ASCII(s)); NEXT; }
CHAR:   CASE("CA"): { Pra;
	char* rs= meap;
	*meap++ = L(A.d);
	*meap++ = 0;
	*r= mkstrconst(rs);
	NEXT; }
    
CHARIX: CASE("CI"): { Prab; char* aa= STR(A);
	char* rr= strstr(aa, STR(B));
	*r= mknum(rr ? rr-aa+1 : 0);
	NEXT; }

LEFT:   CASE("LE"): { Prab; char* s= STR(A);
        int n= L(B.d);
        char* rs= meap;
	while(n-->=0 && *s)
	  *meap++ = *s++;
	*meap= 0;
	*r= mkstrconst(rs);
	NEXT; }

RIGHT:  CASE("RI"): { Prab; char* aa= STR(A);
	int i= strlen(aa) - num(B);
	if (i<=0 || num(B)<=0)
	  *r= mknull();
	else
	  *r= mkstrconst(aa+i);
	NEXT; }

LENGTH: CASE("LN"): Pra; *r= mknum(strlen(STR(A))); NEXT;

LOWER:  CASE("LO"): { Pra; char* s= STR(A);
	char* rs= meap;
	while(*s) *meap++ = tolower(*s++);
	*meap++ = 0;
	*r= mkstrconst(rs);
	NEXT; }
  
UPPER:  CASE("UP"): { Pra; char* s= STR(A);
	char* rs= meap;
	while(*s) *meap++ = toupper(*s++);
	*r= mkstrconst(rs);
	NEXT; }
      
LTRIM:  CASE("LT"): Pra; *r= mkstrconst(ltrim(STR(A))); NEXT;
RTRIM:  CASE("RT"): Pra; *r= mkstrconst(rtrim(mstrdup(STR(A)))); NEXT; //  TODO: meap
TRIM:   CASE("TR"): Pra; *r= mkstrconst(trim(mstrdup(ltrim(STR(A))))); NEXT; // TODO: meap
STR:    CASE("ST"): Pra; char* s= ptr(A);
        *r= s ? mkptr(s, 0) : mkstrconst(mstrdup(STR(A))); NEXT;


#define MATH(short, fun) short: CASE(#short): Pra; R.d=fun(A.d); NEXT
#define MATH2(short, fun) short: CASE(#short): Prab; R.d=fun(A.d, B.d); NEXT

// Use lowercase for math (str is upper)
SQ: CASE("sq"): Pra; R.d=A.d*A.d; NEXT;

MATH(sr, sqrt);
MATH(si, sin);
MATH(co, cos);
MATH(ta, tan);
  //MATH(ct, cot);
  // TODO:** ^
  // TODO: & | << >>
  // bit_count
MATH(ab, fabs);
MATH(ac, acos);
MATH(as, asin);
MATH(at, atan);
MATH2(a2, atan2);
MATH(cr, cbrt);
MATH(ce, ceil);
  //MATH(ev roundtoeven); NAH!
  //MATH(fa, fac);
MATH(fl, floor);
//MATH(mg, gamma);// aMmaG (android math?)
  //MATH(gr, greatest);
MATH(ep, exp); // ExP

MATH(ie, isfinite); // LOL IsfinitE
MATH(ii, isinf);
MATH(in, isnan);
  //MATH(LE, least)
MATH(ln, log);
MATH(lg, log10);
MATH(l2, log2);
pi: CASE("pi"): Pr; R.d=M_PI; NEXT;
ss: CASE("**"): case '^':
MATH2(pw, pow);
de: CASE("de"): Pra; R.d=A.d/180*M_PI; NEXT;
rd: CASE("rd"): Pra; R.d=A.d/M_PI*180; NEXT;
//MATH(RA, radians);
ra: CASE("ra"): Pr; R.d=random(); NEXT;
rr: CASE("rr"): Pa; srandom((int)A.d); NEXT;
//MATH(RO, round);
sg: CASE("sg"): Pra; R.d=A.d<0?-1:(A.d>0?+1:0); NEXT;

	
TS: CASE("ts"): Pr; *r= mkstrconst(mstrdup(isotime())); NEXT;
      
default:
      printf("\n\n%% Illegal opcode at %ld: %ld '%c'\n", p-lplan-1, f, (int)(isprint(f)?f:'?'));
      exit(0);

    } 

    // step over zero
    assert(!*p);
    p++;
  
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

 succeed:
  results++;
  nresults++;

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

  // restore ("release")
  // wow, really expensi!!!
  // 39ms -> 6000ms! wtf?
  meap= _meap;

  return results;
}

// Try function dispatch
// -- TODO: remove, not fast

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
    //TODO: fix his?
    ;//lrun(p+4, var);
  return 1;
}
int fprint(dbval**p) {// TODO:, dbval* var) {
  //TODO: need "context"
  while(*p) {
    ;//printvar(*p++-var, var);
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
  func['!']= func[TWO("!=")]= func[TWO("<>")]= fneq;
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



int printlines(dbval** p) {
  while(*p)
    printf("--- %s\n", STR(**p++));
  printf("==========\n");
  return 0;
}


// TODO: get rid of - make const explicit
int strisnum(char* s) {
  return isdigit(s[0]) || (*s=='-' && isdigit(s[1]));
}
    
void bang() {
  printf("%% SIGFAULT, or something...\n\n");
  install_signalhandlers(bang);
  
  while(1) {
    printf("%% Stacktrace? (Y/n/d/q) >"); fflush(stdout);
    char key= tolower(getchar());
    switch(key) {
    case 'n': break;
    case 'q': exit(77); break;
    case 'd': debugger(); break;
    case 'y':
    default:  print_stacktrace(); break;
    }
    //process_file(stdin, 1);
  }
}

// globals for nextarg() iterator
int gargc= 0;
char** gargv= 0;

char* nextarg() {
  --gargc;
  return *++gargv;
}

char *gline= 0, *gnext= 0;
size_t glsize= 0;

char* nextstdin() {
  char q=0, *r= 0;
  do {
    if (!gline || !*gline || !gnext || !*gnext) {
      if (getline(&gline, &glsize, stdin)==EOF) {
	free(gline); gnext= gline= 0; return NULL;
      }
      //printf(">>>%s\n", gline);
      gnext= gline;
    }

    // find space delimited token
    while(*gnext && isspace(*gnext)) gnext++;
    // string?
    q= (*gnext=='\"' || *gnext=='\'')? *gnext: 0;
    if (q) gnext++;

    // read till end of token/string
    r= gnext;
    while(*gnext && (q || !isspace(*gnext))) {
      if (*gnext=='\\') gnext++;
      else if (q && *gnext==q) break;
      gnext++;
    }
    if (*gnext) *gnext++= 0;

  } while (!q && !*r);
  return r;
}

Plan* readplan(char* nexttok()) {
  // read plan from arguments
  int* p= plan;
  dbval** lp= lplan;
  nextvar= var;
  char* s;
  while((s= nexttok())) {

    if (0==strcmp(":::", s)) {
      // plan... ::: name (of objectlog plan)
      s= nexttok();
      Plan* pl= mkplan(s, plan, var, p-plan+2, nextvar-var+1);
      thunk t= compilePlan(pl);
      regplan(pl, t);

      // prepare read another plan?
      // TODO: how about this func return a plan?
      //       need call readplan again? till NULL
      cleandefault();
      nextvar= var;
      p= plan;
      lp= lplan;

      // TODO: can we continue after this?
      
    } else if (0==strcmp("::", s)) {

      // :: head head head 0

      // TODO: WTF!!!
      
      // store "index" as num
      var[4]= mknum(nextvar-var+1);
      while (0!=strcmp("0", s)) {
	s= nexttok();
	*++nextvar = strisnum(s) ? mknum(atof(s)) : mkstrconst(s);
      }
	
      // header :: str str str ... 0
    } else if (0==strcmp(":", s)) {

      // init var
      // TODO: "can't just store strptr!"
      s= nexttok();
      *++nextvar = strisnum(s) ? mknum(atof(s)) : mkstrconst(s);
      //printf("var[%ld] = ", nextvar-var); dbp(*nextvar); putchar('\n');

    } else {

      // add to plan
      //  4711
      //  fun
      //  @olfun
      if (debug) printf("%s ", s);
      thunk* t= *s=='@' ? findplan(s+1) : NULL;
      long f= TWO(s);
      long isnum= strisnum(s);
      long num= atol(s);
      if (!isnum && !jmp[f]) {
	f= (ulong)t;
	//printf("\tCALL: %s -> %p\n", s, t);
      }

      *p = isnum ? (num ? num : 0) : f;
      dbval* v= *lp = isnum?( num ? var+labs(num) : NULL) : (void*)f;
      //if (debug)
      //printf("@%3ld '%s' %p\n", p-plan, s, v);

      if (!*p && debug) putchar('\n');
      p++; lp++;

    }
  }
  if (debug) putchar('\n');

  *p++ = 0; // just to be sure!
  *p++ = 0; // just to be sure!

  *lp++ = 0; // just to be sure!
  *lp++ = 0; // just to be sure!

  // WTF 4??? +1?? if another concat???
  if (0)
  for(int i=0; i<p-plan+4+1; i++)
    printf("PLAN: %3d %d\n", i, plan[i]);

  // Done input

  if (debug) {
    printf("\n\nLoaded %ld constants %ld plan elemewnts\n", nextvar-var, p-plan);
  
    printvars(var,10);
    lprintplan(lplan, var, -1);
  }

  return mkplan("query", plan, var, p-plan+4, nextvar-var+1);
}

int main(int argc, char** argv) {
  install_signalhandlers(bang);

  init(1024);
  regfuncs();
  initlabels();

  // Read plan from args/stdin
  gargc= argc; gargv= argv;
  Plan* pl= readplan(argc==1? nextstdin: nextarg);
  thunk t= compilePlan(pl);

  //lprintplan(t.lplan, t.var, -1);
  regplan(pl, t);

  // -- time to run
  if (debug) printf("\n\nLPLAN---Running...\n\n");  if (debug) printf("\n\nLPLAN---Running...\n\n");

  cleandefault();
  nextvar= var;
  mallocsreset();
  long ms= mstime();
  thunk myout= (thunk){.ccode=&printlines};
  //dbval** dblplan= lplan+10;
  //thunk dbl= {NULL, dblplan, nextvar++, 

  //long res= lrun(lplan, var, &myout);
  //printvars(t.var, t.plan->varsize);

  //trace= 1;
  //debug= 1;

  meap= meaporig;
  //long res= lrun(t.lplan, t.var, &myout);
  long res= lrun(t.lplan, t.var, NULL);
  // make sure all cleanup
  assert(meap==meaporig);

  // -- Report
  ms = mstime()-ms;
  printf("\n\n%ld Results in %ld ms and performed %ld ops\n", res, ms, olops);
  hprint_hlp(olops*1000/ms, " ologs (/s)\n", 0);
  fprintmallocs(stdout);
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
