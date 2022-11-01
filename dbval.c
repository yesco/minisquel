#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "utils.c"
#include "hash.c"
#include "csv.c"


const uint64_t LINF_MASK= 0x7ff0000000000000l; // 11 bits exp
const uint64_t LMASK    = 0x7ff0000000000000l;
const uint64_t LNAN_MASK= 0x7ff8000000000000l;
const uint64_t LMASK_53=  0x0007ffffffffffffl;
const uint64_t LSIGN_BIT= 0x8000000000000000l;
//const uint64_t LMASK_52=  0x0003ffffffffffffl;

const long L53MAX= 2251799813685248l-2; // 111 ... 11 excluded

double make53(int64_t l) {
  uint64_t ul= l<0? -l : l;
  if (ul<2 || ul>=L53MAX) {
    printf("make53: l= %ld outside of range\n", l);
    printf("make53: l= %ld  outside of range\n", l);
    exit(1);
  }
  ul |= LINF_MASK;
  double d= *(double*)&ul;
  d= l<0 ? -d : +d;
  //printf("make53: %ld => %g\n", l, d);
  return d;
}

int64_t is53(double d) {
  uint64_t ul= *(uint64_t*)&d;
  int neg= (ul & LSIGN_BIT)!=0;
  ul &= LMASK_53; 
  if (!isnan(d) || ul<2 || ul>L53MAX) return 0;
  ul &= LMASK_53;
  //return d<0 ? -ul : +ul;
  return neg ? -ul : +ul;
}

// A database value is a double.
// It can represent 3 types:
// - NULL
// - double
// - string
//
// The NAN encoding have redundancies.
// THese are used to unambigiously store
// NULL, or string indices.
//
// The strings pointers are managed in
// dbstrings array.
// 
// TODO: consider using hashstrings
//   w reference(?) counting.
//
// API:
//   mknull() mknum() mkstr() mkdupstr()
//              num()   str()
//   isnull() isnum()   str()!=NULL
//                   dbfree()   dbfree()
typedef union dbval {
  double d;
  long l;
} dbval;
  
#define MAXSTRINGS 1024*1024
// first 3 values are protected
#define SNULL 2

char* dbstrings[MAXSTRINGS]= {NULL,"-INVALID-",NULL};
int nstrings= 3;
int nstrfree= 0;
int strnext= 4;

dbval mknull() {
  dbval v;
  v.d= make53(SNULL);
  return v;
}

dbval mknum(double d) {
  return (dbval){d};
}

dbval mkstrfree(char* s, int free) {
  int start= strnext;
  while(nstrfree && dbstrings[strnext]) {
    printf("TRYING %d\n", strnext);
    if (++strnext>=nstrings) strnext= SNULL+1;
    if (strnext==start) error("mkstrfree: inconsistency says nstrfree!");
  }
  if (!nstrfree) {
    // allocate one more, nothing to reuse
    strnext= nstrings++;
    if (nstrings>=MAXSTRINGS)  error("out of dbval strings");
  } else nstrfree--;
  if (dbstrings[strnext])
    error("mkstringfree: inconsistency, found non-free");

  dbval v;
  v.d= make53(free?-strnext:+strnext);
  dbstrings[strnext]= s;
  return v;
}

// only use for constants, if you sure
dbval mkstrconst(char* s) {
  return mkstrfree(s, 0);
}

// make a copy and free it
dbval mkstrdup(char* s) {
  return mkstrfree(s?strdup(s):s, 1);
}

int isnull(dbval v) {
  return isnan(v.d) && is53(v.d)==SNULL;
}

int isnum(dbval v) {
  return isnan(v.d) ? !is53(v.d) : 1;
}

int isint(dbval v) {
  int i= v.d;
  return i==v.d;
}

#define SFALSE (LMASK_53-3) // just use 0
#define STRUE  (LMASK_53-3) // just use 1
#define SFAIL  (LMASK_53-2) // ???
#define SERROR (LMASK_53-1) // max-1

// These valuies are transient
// Don't store permanently!
typedef enum{TNULL=0, TNUM, TSTR, TATOM, TBAD=99,TERROR=100} dbtype;

dbtype type(dbval v) {
  if (!isnan(v.d)) return TNUM;
  // In NAN number we
  long i= is53(v.d);
  long u= i<0 ? -i : i;
  if (!i) return TNUM; // +NAN/-NAN
  if (u==1) return TBAD;
  if (u==SNULL) return TNULL;
  if (u==LMASK_53) return TBAD;
  if (u==LMASK_53-1) return TERROR;
  if (i>0) return TSTR;
  // TODO: use for more types?
  // possibly use lower 4 bits as type
  // xxxx ... xxxx bbbb bbbb tttt
  // b...b: bank number (arena!)
  return TATOM;
}
	 
struct table;
char* tablestr(struct table*, dbval);

int dbprint(struct table* t, dbval v, int width) {
  switch(type(v)) {
  case TNULL: printf("NULL\t"); break;
  case TNUM:  printf("%7.7lg\t", v.d); break;
  case TATOM: 
  case TSTR:  {
    char* s= tablestr(t, v);
    int i= 6;
    while(*s && i--) {
      if (*s=='\n') { printf("\\n"); i--;}
      else if (*s=='\r') { printf("\\r"); i--;}
      else putchar(*s++);
    }
    if (*s) putchar(s[1] ? '*' : *s);
    putchar('\t');
  } break;
  case TBAD:  printf("ILLEGAL\t"); break;
  case TERROR:printf("ERROR\t"); break;
  }
  return 1;
}
	    
double num(dbval v) {
  return v.d;
}

// Get (interned) string from VALUE
// 
char* str(dbval v) {
  int i= is53(v.d);
  return dbstrings[i<0?-i:+i];
}

void dbfree(dbval v) {
  // if not nan can't be string!
  if (!isnan(v.d)) return;
  int i= is53(v.d);
  if (i<0) free(dbstrings[(i=-i)]);
  dbstrings[i]= NULL;
  nstrfree++;
  // TODO: freelist?
  strnext= i;
}

void dumpdb() {
  printf("\n---dumpdb---\n");
  for(int i=0; i<nstrings; i++) {
    char* s= dbstrings[i];
    printf("%03d : %s %s\n", i,
      i==1||i>SNULL ? (s?s:"   -"):"-NULL-",
      nstrfree&&i==strnext?"<--- NEXT":"");
  }
  printf("nstrings=%d nstrfree=%d strnext=%d\n", nstrings, nstrfree, strnext);
}

void testint(dbval v) {
  printf("%lg => %d   is... %d\n", v.d, isint(v), (int)v.d);
}






typedef struct table {
  char* name;
  long count;
  char** colnames;
  arena* data;
  hashtab* strings;
  long bytesread;
  
  int cols;
  int keys;
  // int ...

  int sort_col;
} table;

dbval tablemkstr(table* t, char* s) {
  // '' ==> NULL
  if (!s || !*s) return mknull();
  hashentry* e= findhash(t->strings, s);
  long i= -1;
  if (e) {
    i= (long)e->data;
  } else {
    i= addarena(
      t->strings->arena, s, strlen(s)+1);
    e= addhash(t->strings, s, (void*)i);
  }

  dbval v;
  v.d= make53(i);
  return v;
}
dbval xtablemkstr(table* t, char* s) {
  // '' ==> NULL
  if (!s || !*s) return mknull();
  hashentry* e= findhash(t->strings, s);
  long i= -1;
  if (e) {
    i= (long)e->data;
  } else {
    i= addarena(
      t->strings->arena, s, strlen(s)+1);
    e= addhash(t->strings, s, (void*)i);
  }

  dbval v;
  v.d= make53(-i);
  return v;
}

char* tablestr(table* t, dbval v) {
  long i= is53(v.d);
  //printf("tablestr => %ld\n", i);
  if (i<0) i= -i;
  return i<0?NULL:arenaptr(t->strings->arena, i);
}

table* newtable(char* name, int keys, int cols, char* colnames[]){
  table* t= calloc(1, sizeof(*t));
  t->name= strdup(name?name:"noname");
  t->keys= keys;
  t->cols= cols;
  t->colnames= colnames;

  t->data= newarena(1024*sizeof(dbval), sizeof(dbval));
  t->strings= newhash(0, (void*)hashstr_eq, 0);
  t->strings->arena= newarena(1024, 1);
  char zeros[3]= {}; // <3 is NULL,ILLEGAL,NULL
  addarena(t->strings->arena, &zeros, sizeof(zeros));

  // just add colnames as any string!
  if (colnames) {
    for(int col=0; col<cols; col++) {
      dbval v= tablemkstr(t, colnames[col]);
      addarena(t->data, &v, sizeof(v));
    }
    t->count++;
  }

  return t;
}

int tableaddrow(table* t, dbval v[]) {
  t->count++;
  return addarena(t->data, v, t->cols * sizeof(dbval));
}

long tableaddline(table* t, char* csv, char delim) {
  if (!t || !csv) return -1;

  int len= strlen(csv)+1;
  char* str= strdup(csv); // !
  double d;
  dbval v;
  
  int col= 0, r;
  while((r= sreadCSV(&csv, str, len, &d, delim))) {
    if (r==RNEWLINE) break;
    if (t->cols && col >= t->cols) break; 
    col++;
    switch(r){
    case RNULL:   v= mknull(); break;
    case RNUM:    v= mknum(d); break;
    case RSTRING:  v= tablemkstr(t, str); break;
    }
    // 1.2% faster if we chunked it - bah
    addarena(t->data, &v, sizeof(v));
  }
  free(str);
  
  // first row?
  // assume it's header if not defined
  if (!t->cols) t->cols= col;

  // fill with nulls till t->col
  v= mknull();
  for(int i=col; i<t->cols; i++)
    addarena(t->data, &v, sizeof(v));

  t->count++;
  return 1;
}

// 3x slower without storing rows
int loadcsvtable_csvgetline(table* t, FILE* f) {
  if (!f || !t) return 0;
  char delim= 0;
  long ms= timems();
  // header is a line!
  char* line= NULL;
  while((line= csvgetline(f, delim))) {
    if (!*line) continue;
    if (!delim) delim= decidedelim(line);
    t->bytesread+= strlen(line); // 10% cost!...
    if (1) 
      tableaddline(t, line, delim);
    else
      t->count++;
    free(line); line= NULL;
    if (debug && t->count%10000==0) { printf("\rload_csvgl: %ld bytes %ld rows", t->bytesread, t->count); fflush(stdout); }
  }
  ms= timems()-ms;
  if (debug) printf("\rload_csvgl: %ld ms %ld %ld rows\n", ms, t->bytesread, t->count);
  return 1;
}

// 20% FASTER! (NO FREE/MALLOC, AND STRLEN TO COUNT) - API issue
int loadcsvtable_getline(table* t, FILE* f) {
  if (!f || !t) return 0;
  char delim= 0;
  long ms= timems();
  // header is a line!
  char* line= NULL;
  size_t ln= 0;
  ssize_t r= 0;
  while((r=getline(&line, &ln, f))>=0) { // 10% faster, no strlen?
    if (!line || !*line) continue;
    if (!delim) delim= decidedelim(line);
    t->bytesread+= r;
    if(1)
      tableaddline(t, line, delim);
    else
      t->count++;
    if (debug && t->count%10000==0) { printf("\rload_gl: %ld bytes %ld rows", t->bytesread, t->count); fflush(stdout); }
  }
  free(line); line= NULL;
  ms= timems()-ms;
  if (debug) printf("\rload_gl: %ld ms %ld %ld rows\n", ms, t->bytesread, t->count);
  return 1;
}
  
int loadcsvtable(table* t, FILE* f) {
  //  if (1) return loadcsvtable_newcsvgetline(name, f); else
  if (1) return loadcsvtable_csvgetline(t, f);
  else return loadcsvtable_getline(t, f);
}
  
// NULL==NULL < -num < num < "bar" < "foo"
long ncmp= 0;
int tablecmp(table* t, dbval a, dbval b) {
  ncmp++;
  // 10M rows numbers

  // - simplified double dmp col 2 
  // sort -2 took 412 ms
  // sort 2 took 300 ms

  // - generic compare col 2
  // sort -2 took 459 ms
  // sort 2 took 354 ms 

  // ==> (/ 459 412.0) = 11% difference
    
  // - same file but only str cmp col 1
  // sort -1 took 1718 ms
  // sort 1 took 1721 ms

  // TODO: compare using "sorted interned strings

  // - Using same table but storing all cols
  // 2038 ms (str) vs 628 ms (num)

  // return (a.d>b.d)-(b.d>a.d);
  
  if (a.l==b.l) return 0;
  long la= is53(a.d), lb= is53(b.d);
  // ordered! comparable as -la and -lb
  if (la<0 && lb<0) return (lb>la)-(la>lb);
  // resort to cmp strings
  if (la>0 && lb>0) {
    if (!t) error("tablecmp: needs table!");
    char* sa= tablestr(t, a);
    char* sb= tablestr(t, b);
    if (sa==sb) return 0;
    if (!sa) return -1;
    if (!sb) return +1;
    return strcmp(sa, sb);
  }
  if (isnull(a)) return -1;
  if (isnull(b)) return +1;
  if ((!!la != !!lb)) return (la>lb)-(lb>la);
  return (a.d>b.d)-(b.d>a.d);
}

table* qsort_table= NULL;

int sorttablecmp(dbval *a, dbval *b) {
  table* t= qsort_table;
  int col= t->sort_col;
  int cd= col<0? -col: +col;
  cd= cd ? cd-1: 0;
  a+= cd; b+= cd;
  if (col<0)
    return -tablecmp(qsort_table, *a, *b);
  else
    return tablecmp(qsort_table, *a, *b);
}

// Sort the TABLE using N COLUMNS.
// If a colum number is -col, use desc.
// Columns start from "1" LOL. (SQL)
void tablesort(table* t, int n, int* cols) {
  long ms= timems();

  // TODO:lol
  t->sort_col= n;
  
  qsort_table= t;
  int row_size= t->cols*sizeof(dbval);
  // skip first row (headers)
  int actual_size= t->data->top+ t->count-1*row_size;
  if (actual_size > t->data->size)
    ; // not stored!
  else
    qsort(t->data->mem+row_size, t->count-1, row_size, (void*)sorttablecmp);
  qsort_table= NULL;

  ms= timems()-ms;
  if (debug)
    printf("sort %d took %ld ms\n", t->sort_col, ms);
}

long printtable(table* t, int details) {
  if (!t) return 0;

  // optimize
  long saved= 0;
  saved+= arenaoptimize(t->data);
  saved+= arenaoptimize(t->strings->arena);

  long bytes= 0;
  printf("\n=== TABLE: %s ===\n", t->name);

  dbval* vals= (void*)t->data->mem;
  for(int row=0; row<t->count; row++) {

    for(int col=0; col<t->cols; col++) {
      long i= row * t->cols + col;
      dbval v= vals[i];
      dbprint(t, v, 8);
    }
    putchar('\n');
    if (details > 2) details--;
    if (details==2) break;
    if (details < 0) break;
    // underline col names
    if (!row) {
      for(int col=0; col<t->cols; col++)
	printf("------- ");
      putchar('\n');
    }

  }
  bytes+= sizeof(table);
  putchar('\n');
  printf("--- %ld rows\n", t->count);
  printf("   data: "); bytes+= printarena(t->data, 0);
  // not add bytes as is part of hash!
  printf("strings: "); printarena(t->strings->arena, 0);
  printf("   hash: "); bytes+= printhash(t->strings, 0);
  // compression: to 33% of happy.csv
  // ( 18% if used floats! )
  // gzip only 22% LOL!
  printf("BYTES: %ld (optmized -%ld) compressed %.2f%% bytesread %ld\n", bytes, saved, (100.0*bytes)/t->bytesread, t->bytesread);
  return bytes;
}




void dotable(char* name, int col) {
  debug= 1;
  int details= +1024;

  FILE* f= fopen(name, "r");
  table* t= newtable(name, 0, 0, NULL);
  loadcsvtable(t, f);

  tablesort(t, -col, NULL);
  tablesort(t, col, NULL);

  tablesort(t, -col, NULL);
  tablesort(t, col, NULL);
  printtable(t, details);
  fclose(f);
  printf("ms lol ncmp=%ld\n", ncmp);
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





// TODO: remove

extern int debug= 1;

int main(int argc, char** argv) {
  if (argc>1) {
    int col=1;
    if (argc>2) col= atoi(argv[2]);
    dotable(argv[1], col);
    exit(0);
  }
  tabletest(); exit(0);

  // compare types!
  if (0) {
    table* t= newtable("cmp", 0, 0, NULL);

    printf("---NUMS\n");
    printf("na %d (0)\n", tablecmp(NULL, mknum(3), mknum(3)));
    printf("nb %d (-1)\n", tablecmp(NULL, mknum(0), mknum(3)));
    printf("nc %d (0)\n", tablecmp(NULL, mknum(-3), mknum(-3)));
    printf("nc %d (-1)\n", tablecmp(NULL, mknum(-333), mknum(-3)));
    printf("nd %d (-1)\n", tablecmp(NULL, mknum(-333), mknum(3)));
    printf("ne %d (-1)\n", tablecmp(NULL, mknum(7), mknum(77)));

    printf("---NULL\n");
    printf("-n %d (0)\n", tablecmp(NULL, mknull(), mknull()));
    printf("-n %d (-1)\n", tablecmp(NULL, mknull(), mknum(0)));
    printf("-s %d (-1)\n", tablecmp(t, mknull(), tablemkstr(t, "foo")));

    printf("---STRINGS\n");
    printf("bb %d (0)\n", tablecmp(t, tablemkstr(t, "bar"), tablemkstr(t, "bar")));
    printf("bf %d (-1)\n", tablecmp(t, tablemkstr(t, "bar"), tablemkstr(t, "foo")));

    printf("---MIXED\n");
    printf("ns %d (-1)\n", tablecmp(t, mknum(3), tablemkstr(t, "bar")));
    printf("sn %d (+1)\n", tablecmp(t, tablemkstr(t, "bar"), mknum(3)));

    exit(0);
  }

  if (0) {
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
