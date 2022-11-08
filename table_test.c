#include <stdio.h>
#include <stdlib.h>

// TODO: remove
int debug= 1;
int stats= 1;
int delim= 0;

int lineno;
int foffset;

#define NAMELEN 64
#define VARCOUNT 256
#define MAXCOLS 32

#include "utils.c"
#include "csv.c"
#include "vals.c"
#include "dbval.c"
#include "table.c"

typedef struct printerdata {
  table* t;
  long n;
} printerdata;

dbval printer(dbval v, printerdata* data) {
  return v;
  data->n++;
  tdbprinth(data->t, v, 8, 1);
  return v;
}

// 5x faster than index.c (create index)!
// and using less mem.
void dotable(char* name, int col) {
  debug= 0;
  stats= 1;
  int details= debug?+100:0;

  FILE* f= fopen(name, "r");

  struct printerdata {
    table* t;
    long n;
  } data = {newtable(name, 0,0,NULL), 0};

  dbread(f, data.t, (void*)printer, &data);

  exit(0);

  table* t= newtable(name, 0, 0, NULL);
  //table* t= newtable(name, 0, 3, NULL);
  loadcsvtable(t, f);

  tablesort(t, -col, NULL);
  tablesort(t, col, NULL);

  tablesort(t, -col, NULL);
  tablesort(t, col, NULL);
  printtable(t, details);
  fclose(f);
  printf("ms lol ncmp=%ld ncmpother=%ld ncmptab=%ld ncmptabmiss=%ld nbadstrings=%ld\n", ncmp, ncmpother, ncmptab, ncmptabmiss, nbadstrings);
}

void tabletest() {
  table* t= NULL;

  int details= -1;
  if (0) {
    details= +1;
    t= newtable("foo", 1, 3, (char**)&(char*[]){"a", "b", "c"});
  tableaddrow(t, (dbval*)&(dbval[]){mknull(), mknum(42), tablemkstr(t, "foo")});

  } else if (1) {
    char* name= "Test/foo.csv"; details=1;
    //char* name= "Test/happy.csv";
    //char* name= "Data/Data7602DescendingYearOrder.csv"; // 100MB
    //char* name="Data/Sample-Spreadsheet-500000-rows.csv"; // 6MB 10s
    FILE* f= fopen(name, "r");
    table* t= newtable(name, 0, 0, NULL);
    loadcsvtable(t, f);
  }

  if (1) {
    printtable(t, details);
    
    tablesort(t, 3, NULL);
    tablesort(t, -3, NULL);
    tablesort(t, 3, NULL);
    
    printtable(t, details);
  } else {
    printtable(t, -1);
  }
  
  //freetable(t);
}


void dbtypetest() {
  debug= 0;
  // compare types!
  table* t= newtable("cmp", 0, 0, NULL);

  printf("---NUMS\n");
  printf("na %d (0)\n", tablecmp(NULL, mknum(3), mknum(3)));
  printf("nb %d (-1)\n", tablecmp(NULL, mknum(0), mknum(3)));
  printf("nc %d (0)\n", tablecmp(NULL, mknum(-3), mknum(-3)));
  printf("nc %d (-1)\n", tablecmp(NULL, mknum(-333), mknum(-3)));
  printf("nd %d (-1)\n", tablecmp(NULL, mknum(-333), mknum(3)));
  printf("ne %d (-1)\n", tablecmp(NULL, mknum(7), mknum(77)));
  printf("nN %d (-1)\n", tablecmp(NULL, mknum(7), mknum(NAN)));
  printf("IN %d (-1)\n", tablecmp(NULL, mknum(-INFINITY), mknum(NAN)));
  printf("-IN %d (-1)\n", tablecmp(NULL, mknum(-INFINITY), mknum(NAN)));
  printf("+IN %d (-1)\n", tablecmp(NULL, mknum(+INFINITY), mknum(NAN)));

  printf("---NULL\n");
  printf("-n %d (0)\n", tablecmp(NULL, mknull(), mknull()));
  printf("-n %d (-1)\n", tablecmp(NULL, mknull(), mknum(0)));
  printf("-s %d (-1)\n", tablecmp(t, mknull(), tablemkstr(t, "foo")));
  printf("-N %d (-1)\n", tablecmp(NULL, mknull(), mknum(NAN)));


  printf("---STRINGS\n");
  printf("bb %d (0)\n", tablecmp(t, tablemkstr(t, "bar"), tablemkstr(t, "bar")));
  printf("bf %d (-1)\n", tablecmp(t, tablemkstr(t, "bar"), tablemkstr(t, "foo")));
  printf("sN %d (+1)\n", tablecmp(NULL, tablemkstr(t, "foobarfiefum"), mknum(NAN)));
  printf("sN %d (+1)\n", tablecmp(NULL, tablemkstr(t, "foo"), mknum(NAN)));

  printf("---MIXED\n");
  printf("ns %d (-1)\n", tablecmp(t, mknum(3), tablemkstr(t, "bar")));
  printf("sn %d (+1)\n", tablecmp(t, tablemkstr(t, "bar"), mknum(3)));
}

void test7(dbval a) {
  printf("7ASCII.l: %16lx\n", a.l);
  printf("7ASCII.d: %16lg\n", a.d);
  long l= is7ASCII(a);
  printf("is7ASCII: %16lx %s\n", l, l?"7ASCII":"NOT!");
  printf("    FAIL: %16lx %d\n", mkfail().l, l==mkfail().l);
  printf("7ASCII: dbprinth: '");
  dbprinth(a, 8, 1);
  printf("'\n");

  // TODO: make a dbcmp()
  if (0) {
  printf("cmpw null ==> %d\n",
	 dbstrcmp(mknull(), a));
  printf("cmpw NAN ==> %d\n",
	 dbstrcmp(mknum(NAN), a));
  printf("cmpw 'longstring' ==> %d\n",
	 dbstrcmp(mkstrconst("longstring"), a));
  }
  
  nl();
}

int main(int argc, char** argv) {
  printf("- using mkstr7ASCII()\n");
  test7(mkstr7ASCII("foobarfabc"));
  test7(mkstr7ASCII("foobar"));
  test7(mkstr7ASCII("foo"));
  printf("- using mkstr()\n");
  test7(mkstrconst("foobarfabc"));
  test7(mkstrconst("foobarf"));
  test7(mkstrconst("foo"));

  // TODO:make strings align 2 bytes...
  
  if (argc>1) {
    int col=1;
    if (argc>2) col= atoi(argv[2]);
    dotable(argv[1], col);
    exit(0);
  }
  dbtypetest(); exit(0);
  //tabletest(); exit(0);
}
