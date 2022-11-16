//  ObjectLog Interpreter
// ----------------------
// (>) 2022 jsk@yesco.org

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <string.h>

int debug=0, stats=0, lineno=0, foffset=0, security= 0;

int nfiles= 0;

#define NAMELEN 64
#define VARCOUNT 256
#define MAXCOLS 32

#include "utils.c"

#include "csv.c"
#include "vals.c"
#include "dbval.c"
#include "table.c"

#include "malloc-count.c"

#define MAXNUM 20000000l

int trace= 0;
long olops= 0, nresults= 0;

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
    //printvar(i);
    printf("%s", STR(var[i]));
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

void* jmp[TWORANGE]= {0};



// lrun is 56% faster!
long lrun(dbval** start) {
  static void* jmp[]= {
    [  0]= &&END,
    ['t']= &&TRUE,
    ['f']= &&FAIL,
    ['r']= &&REVERSE,
    ['j']= &&JUMP,
    ['g']= &&GOTO,
    [127]= &&DEL,
    ['o']= &&OR,

    [TWO(":=")]= &&SET,

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
    ['&']= &&LAND,
    ['|']= &&LOR,
    ['i']= &&IOTA,
    ['d']= &&DOTA,
    ['l']= &&LINE,

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
    [TWO("LT")]= &&LTRIM,
    [TWO("RT")]= &&RTRIM,
    [TWO("TR")]= &&TRIM,
    [TWO("ST")]= &&STR,

    [TWO("ts")]= &&TS,

  };

// Not sure can tell the speed diff!

//#define JUMPER
  
#ifdef JUMPER
  static int firsttime= 1;
  if (firsttime) {
    printf("--- JUMPER!\n");
    // TODO: for all plans
    dbval** p= start;
    while(*p) {
      int f= L(*p);
      void* x= jmp[f];
      if (!x || !debug) printf("TRANS '%c' %p\n", f, x);
      if (!x) error("ObjectLog: Function not recognized");
      *p= (dbval*)x;
      while(*p++);
    }

    firsttime= 0;
  }
#endif

  //lprintplan(start, -1);

  dbval** p= start;
  dbval* v= var;

#define N (*p++)
  
  dbval *r, *a, *b, *c;
  
#define R (*r)
#define A (*a)
#define B (*b)
#define C (*c)

#define Pr     r=N;dbfree(R)
#define Pra    Pr;a=N
#define Prab   Pra;b=N
#define Prabc  Prab;c=N

#define Pa     a=N
#define Pab    a=N;b=N
#define Pabc   a=N;b=N;c=N

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
	int i= p-lplan-3;
	while(plan[i]) i--;
	i++;
	if (i<0) i= 0;
	//\n%*s", 2*i, "");
	printf("@%2d", i);
	printf("(%c", plan[i]);
	int v;
	while((v= plan[++i])) {
	  printf(" %d:", v);
	  printf("%s", STR(var[abs(v)]));
	}
	printf(")\n");
      } else {
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
      case  1: goto done;
      case  2: result = !result; break;
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
    if (trace) printf("NEXT '%c'(%d) @%ld %p\n", plan[p-lplan], plan[p-lplan], p-lplan, *p); \
    if (!*p) goto done;	\
      goto *(void*)*p++;}
#endif
    
    // TODO: why need !*p ??? 
    // ONLY 100% slower than OPTIMAL!
    
#define FAIL(a) {if(a) goto fail; NEXT;}

    switch(f) {
    // -- control flow
END:    case   0: goto done;
TRUE:   case 't': goto done;
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
	if (lrun(lplan+L(N))) {
	  p= lplan+L(r); continue;
	}
      goto fail;
SET:  CASE(":="): Pra; R= A; NEXT;

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
EQ:   CASE("=="):
      case '=': Pab;
      if (A.l==B.l) NEXT;
      if (isnan(A.d) || isnan(A.d)) {
	if (type(A)!=type(B)) goto fail;
	// same type
	FAIL(dbstrcmp(A, B));
      } else
	FAIL(A.d != B.d);

NUMNEQ: CASE("N!"): Pab; FAIL(A.l==B.l)
STRNEQ: CASE("S!"): Pab; FAIL(!dbstrcmp(A,B));

// Generic
NEQ:  CASE("!="):
      CASE("<>"):
      // TODO: complete (see '=')
      case '!': Pab; FAIL(A.l==B.l);

// TODO: do strings...
LT:   case '<': Pab; FAIL(A.d>=B.d);
GT:   case '>': Pab; FAIL(A.d<=B.d);
GE:   CASE("!>"):
      CASE("<="): Pab; FAIL(A.d>B.d);
LE:   CASE("!<"):
      CASE(">="): Pab; FAIL(A.d<B.d);

// logic - mja (output? no shortcut?)
LAND: case '&': Prab; R.d= A.d&&B.d; NEXT;
LOR:  case '|': Prab; R.d= A.d||B.d; NEXT;

// generators
IOTA: case 'i': Prab;  for(R.d=A.d; R.d<=B.d; R.d+=  1) results+= lrun(p+1); goto done;
DOTA: case 'd': Prabc; for(R.d=A.d; R.d<=B.d; R.d+=C.d) results+= lrun(p+1); goto done;
LINE: case 'l': { Pa; FILE* fil= A.p;
	//   165ms time cat fil10M.tsv
	//  3125ms time wc fil...
	//  4054ms time ./olrun sql where 1=0
	// 14817ms time ./olrun sql p cols
	// 15976ms    full strings
	// 33s     time ./run sql p cols
	// 56         full strings
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
	// read lines
	while((n= getline(&line, &len, fil))!=EOF) {
	//while(!feof(fil) && ((line= csvgetline(fil, delim)))) {
	  //printf("LINE:%s<\n",line);
	  r+= n;
	  if (!line || !*line) continue;
	  char str[len];
	  char* s= line;
	  f= p;

	  // get values
	  // NO: malloc/strdup/free!
	  char* end= s;

	  
	  // Performance Experiments
	  // TODO: cleanup/call f?
	  // -- old ./run =>
	  
	  // We're 10x faster!
	  // - 30 610 ms !!!
	  // ./run 'select * from "fil10M.tsv" fil where 1=0' | tail
	  
	  if (simple) {
	    // 2600 ms
	    while(*f) {
	      dbfree(**f);

	      char* e= s;
	      while(*e && *e!=delim) e++;
	      *e++ = 0;

	      if (!*s)
		**f++ = mknull();
	      else if (isdigit(*s) || *s=='-' || *s=='+' || *s=='.') {
		char* ne;
		double d= strtod(s, &ne);
		if (ne!=s)
		  (**f++).d= d;
		else
		  **f++ = mkstrconst(s);
	      } else {
		**f++ = mkstrconst(s);
	      }
	      s= e;
	    }
	  } else if(1) {
	    // 3760 ms
	    // inlining loops, lol
	    // NO: malloc/strdup/free!
	    while(*f) {
	      dbfree(**f);

	      while(*s==' ') s++;

	      char q= *s=='"' ? *s++ :
		(*s=='\'' ? *s++ : 0);
	    
	      char* e= s;
	      while(*e && *e!=delim) {
		// not wholly correct
		// (remove extra " or \\)
		if (*e==q && e[1]!=q) break;
		if (*e=='\\') e++;
		e++;
	      }
	      char o= *e;
	      *e++ = 0;

	      if (o==delim && !*s)
		**f++ = mknull();
	      else if (isdigit(*s) || *s=='-' || *s=='+' || *s=='.') {
		char* ne;
		double d= strtod(s, &ne);
		if (ne!=s)
		  (**f++).d= d;
		else
		  **f++ = mkstrconst(s);
	      } else {
		**f++ = mkstrconst(s);
	      }

	      // go past one delim
	      if (o != delim)
		while(*e && *e!=delim) e++;
	      s= e;
	    }
	  } else {
	    // 10,000 ms
	    char* cur= line;
	    while(*f && (dbfree(**f), 1) &&
		  !isend((**f = dbreadCSV(
					  t, &cur, str, len, delim)))) {
	      //printf("\t'%s'\n", str);
	      //printf("\t%s\n", STR(**f));
	      f++;
	    }
	  }
	  // set missing values to null...
	  while(*f) {
	    dbfree(**f);
	    **f++= mknull();
	  }
	  lrun(f+1);
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
PRINT: case '.': while(*p) printvar(N-var); NEXT;
PRINC: case 'p': while(*p) printf("%s", STR(*N)); NEXT;
NEWLINE: case 'n': putchar('\n'); NEXT;
    
// -- strings
CONCAT: CASE("CO"): { Pr; int len= 1;
	dbval** n= p;
	while(*n) {
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
	SETR(mkstrfree(rr, 1));
	NEXT; }
      
ASCII:  CASE("AS"): Pra; SETR(mknum(STR(A)[0])); NEXT;
CHAR:   CASE("CA"): { Pra; char s[2]={L(A.d),0}; SETR(mkstr7ASCII(s)); NEXT; }
    
CHARIX: CASE("CI"): { Prab; char* aa= STR(A);
  	char* rr= strchr(aa, STR(B)[0]);
	SETR(aa ? mknum(rr-aa) : mknull());
	NEXT; }

LEFT:   CASE("LE"): Prab; dbfree(R);
        SETR(mkstrfree(strndup(STR(A), num(B)), 1)); NEXT;

RIGHT:  CASE("RI"): { Prab; char* aa= STR(A);
	int len= strlen(aa) - num(B);
	if (len<=0 || num(B)<=0) SETR(mkstrconst(""));
	else SETR(mkstrdup(aa+len));
	NEXT; }

LOWER:  CASE("LO"): { Pra; char* aa= strdup(STR(A));
	char* ab= aa;
	while(*ab) {
	  *ab= tolower(*ab); ab++;
	}
	SETR(mkstrfree(aa, 1));
	NEXT; }
  
UPPER:  CASE("UP"): { Pra; char* aa= strdup(STR(A));
	char* ab= aa;
	while(*ab) {
	  *ab= toupper(*ab); ab++;
	}
	SETR(mkstrfree(aa, 1));
	NEXT; }
      
LTRIM:  CASE("LT"): Pra; SETR(mkstrdup(ltrim(STR(A)))); NEXT;
RTRIM:  CASE("RT"): Pra; SETR(mkstrfree(rtrim(strdup(STR(A))), 1)); NEXT;
TRIM:   CASE("TR"): Pra; SETR(R= mkstrfree(trim(strdup(ltrim(STR(A)))), 1)); NEXT;
STR:    CASE("ST"): Pra; SETR(mkstrdup(STR(A))); NEXT;

TS    : CASE("ts"): Pr;  SETR(mkstrdup(isotime())); NEXT;
      
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

 done: result = !result; 
  results++;
  nresults++;
  
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

  //return result;
  return results;
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
  //debug= 1;

  printf("\n\nLPLAN---Running...\n");
  mallocsreset();
  long ms= mstime();
  long res= lrun(lplan);
  ms = mstime()-ms;
  printf("\n\n! %ld ... %ld Results in %ld ms and performed %ld ops\n", res, nresults, ms, olops);
  hprint(olops*1000/ms, " ologs (/s)\n");
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
