#define NAMELEN 64
#define VARCOUNT 100

#include <stdlib.h>
#include <stdio.h>


int debug= 0, stats= 0, lineno= -1, foffset= 0, security= 0, nfiles= 0;

#include "utils.c"

#include "csv.c" 
#include "vals.c"

#include "malloc-count.c"

#include "dbval.c"

void testint(dbval v) {
  printf("%lg => %d   is... %d\n", v.d, isint(v), (int)v.d);
}

void testtype(dbval v) {
  printf("%7lg ", v.d);
  printf("%16lx ", v.l);
  switch(type(v)){
  case TNULL: printf("TNULL\t"); break;
  case TNUM: printf("TNUM\t"); break;
  case TSTR: printf("TSTR\t"); break;
  case TATOM: printf("TATOM\t"); break;
  case TEND: printf("TEND\t"); break;
  case TFAIL: printf("TFAIL\t"); break;
  case TERROR: printf("TERROR\t"); break;
  default: printf("T?{%d}\t",type(v)); break;
  }
 fflush(stdout);

  if (isnull(v)) printf("NULL ");
  if (isnum(v)) printf("num ");
  if (str(v)) printf("str ");
  if (isfail(v)) printf("FAIL ");
  if (isbad(v)) printf("BAD ");
  if (iserr(v)) printf("ERROR ");
  if (isend(v)) printf("END ");

  putchar('\n');
}


void aa(dbval a, dbval b, char op, double d) {
  printf("\t%c %lg ", op, d); dbp(mknum(d)); nl();
}

void arith(dbval a, dbval b) {
  nl();
  dbp(a); dbp(b); nl();
  aa(a,b, '+', a.d+b.d);
  aa(a,b, '-', a.d-b.d); 
  aa(a,b, '*', a.d*b.d);
  aa(a,b, '/', a.d/b.d);
  //  printf("\t+"); dbprinth(mknum(a+b), 8, 1); nl();
}

void testptr(char* s, int tofree) {
  printf("---- '%s'\n", s);
  dbval p= mkptr(s, tofree);
  char* x= ptr(p);
  printf("s '%s'\n", ptr(p));
  printf(" p= %016p\n x= %016p\n", p.p, x);
  printf("HELLO\n");
  dbfree(p);
}

void pointers() {
  char bar[]= "x";
  char foo[]="FOOO";
  const char x[]= "xyz";
  const char f[]= "fishxy";
  testptr(foo, 0);
  testptr(strdup("foo"), 1);
  testptr("fishxy", 0);
}

int foo= 0;
int main(void) {
  foo++;
  printf("foo=%d\n", foo);
  pointers();
  exit(1);
  
  if (1) {
    nl();
    printf("nan==nan %d, != %d\n",
	   NAN==NAN, NAN!=NAN);
    double nn= NAN;
    printf("nan= %lx \n", *(long*)&nn);
    // funny:
    // num and NULL gives NULL
    // nan and NULL gives NULL
    // str and NULL gives STR
    // str and str gives first
    dbval n42= mknum(42);
    dbval s= mkstrconst("foobarfiefum");
    dbval s2= mkstrconst("twotwotwotwo");
    dbval n= mknull();
    dbval nan= mknum(NAN);

    arith(n42, n42);
    arith(n42, nan);
    arith(nan, n42);

    arith(n42, n);
    arith(n, n42);

    arith(nan, n);
    arith(n, nan);

    arith(n42, s);
    arith(s, n42);
    arith(s, s);

    arith(nan, s);
    arith(s, nan);

    arith(s, s2);
    arith(s2, s);

    
  }
  
  if(0){
    printf("%16lx\n", (long)INULL);
    printf("%16lx\n", (long)IEND);
    printf("%16lx\n", (long)IFAIL);
    printf("%16lx\n", (long)IERROR);

    printf("%16lx\n", CNULL);
    printf("%16lx\n", CEND);
    printf("%16lx\n", CFAIL);
    printf("%16lx\n", CERROR);
    exit(1);
  }
  
  if (1) {
    printf("- test double val comparision\n");
    // NAN can't be compared
    printf("%d\n", NAN==NAN);
    printf("%d\n", -NAN<NAN);
    // inf yes!
    printf("%d\n", INFINITY==INFINITY);
    printf("%d\n", -INFINITY<INFINITY);
    printf("%d\n", 0<INFINITY);
  }
  
  dbval n= mknum(42);
  dbval s= mkstrdup("foo");
  for(int i=0; i<10; i++) {
    char s[20];
    snprintf(s, sizeof(s), "num %d", i);
    mkstrdup(s);
  }
  printf("bum=%lg str='%s'\n", num(n), str(s));
  dumpdb();
  dbfree(n);
  dbfree(s);
  dumpdb();
  dbval a= mkstrdup("bar");
  dumpdb();
  dbval b= mkstrdup("fie");
  dumpdb();
  
  printf("-- test types\n");
  testtype(mknull());
  testtype(mknum(42));
  testtype(mkstrconst("foo"));
  testtype(mkfail());
  testtype(mkerr());
  testtype(mkend());

  testint(mknum(42));
  testint(mknum(3.14));
  testint(mknull());
  testint(mkstrconst("foo"));
  testint(mknum(1.0/0));
  testint(mknum(INT_MAX));
  testint(mknum(INT_MIN));
}
