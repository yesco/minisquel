#include "vals.c // TODO: temporary!

// needed by dbval
#include "utils.c"
#include "vals.c" 
//#include "csv.c" 

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



extern debug= 1;

int main(void) {
  if(0){
    printf("%16lx\n", INULL);
    printf("%16lx\n", IEND);
    printf("%16lx\n", IFAIL);
    printf("%16lx\n", IERROR);

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
