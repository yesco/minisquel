#include "dbval.c"

void testint(dbval v) {
  printf("%lg => %d   is... %d\n", v.d, isint(v), (int)v.d);
}

extern debug= 1;

int main(void) {
  if (1) {
    // NAN can't be compared
    printf("%d\n", NAN==NAN);
    printf("%d\n", -NAN<NAN);
    // inf yes!
    printf("%d\n", INFINITY==INFINITY);
    printf("%d\n", -INFINITY<INFINITY);
    printf("%d\n", 0<INFINITY);
    exit(0);
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
  
  testint(mknum(42));
  testint(mknum(3.14));
  testint(mknull());
  testint(mkstrconst("foo"));
  testint(mknum(1.0/0));
  testint(mknum(INT_MAX));
  testint(mknum(INT_MIN));
}
