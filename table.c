// Tables
// ======================
// (>) 2022 jsk@yesco.org

// This is an in-memory table storage
// implementation using dbval.c -
// all values stored as "double"s.
//
// Strings are stored "interned".
//
// For mixed CSV with strings with
// lots of repetition this saves mem.
// 
// This Table can be:
// - use to materialize resultset
// - has ways to multi-column sort
// - for ORDER BY (TODO:)
// - for GROUP BY (TODO:)
// - generic ordered index
//


#include <math.h>

#include "dbval.c"

// TODO: remove
extern int debug;
extern int stats;


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
  case TFAIL: printf("FAIL\t"); break;
  case TERROR:printf("ERROR\t"); break;
  case TEND:  printf("END\n"); break; // \n!
  }
  return 1;
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


// TODO: cleanup!
int bitsforoffset= 0;
uint64_t maskforoffset= 0;
uint64_t maskforchs= 0;

// if equal can't really say anything...
// TODO: remove dbvals....
long cmptablestr(long a, long b) {
  long ca= a & maskforchs;
  long cb= b & maskforchs;
  // one isn't comparable (illegal char)
  if (!ca || !cb) return 0;
  if (debug)
    printf("CMP:\n%16lx\n%16lx\n", ca, cb);
  //if (debug) printf("CMP.str: '%s' '%s' %ld\n", sa, sb, ca-cb);
  // Thwy will never be equal!
  return ca-cb;
}

long nbadstrings= 0;
dbval tablemkstr(table* t, char* s) {
  // '' ==> NULL
  if (!s || !*s) return mknull();
  hashentry* e= findhash(t->strings, s);
  long i= -1;
  if (e) {
    i= (long)e->data;
  } else {
    // TODO: put long strings in bigarena!
    //   longer strings aren't CMP/ordered
    //   > 15 chars?
    // can use negative offset?
    i= addarena(
      t->strings->arena, s, strlen(s)+1);
    e= addhash(t->strings, s, (void*)i);
  }

  // put few chars of string in value!
  long chs= 0;
  char* p= s;
  int c;
  // more bits chars inline limits storage
  // space for strings...
  //
  // 5*6 = 30   52-30=22 =>2^22= 4 MB str
  // 4*6 = 24         28 =>    256 MB str
  int nchars= 4;
  int nbits= 6*nchars;
  int bits= nbits;

  bitsforoffset= 52-nbits;

  maskforoffset= (1 << bitsforoffset) -1;
  maskforchs= (1<<nbits) -1;
  maskforchs<<= bitsforoffset;
  
  assert(!(maskforchs & LINF_MASK));
  assert(!(maskforchs & maskforoffset));
  
  if (debug) printf("\n");
  if (debug) putchar('>');
  // doesn't have to be exact chars...
  while((c= *p++) && bits>=6) {
    if (debug) putchar(c);

    if (c<'0') c= 0; // 0
    else if (c<='9') c= c-'0' + 1; // 1-10
    else if (c<'A') c= 11; // 11
    else if (c<='Z') c= 12 + c-'A'; // 12-38
    else if (c<'a') c= 39; // 39
    else if (c<='z') c= 39 + c-'a';
    if (c>63) c= 63;

    if (c<0 || c>=64) {
      // Mostly ' ' and digits.
      // could map:
      // ' '     1
      // ...     1
      // 0-9    10
      // ...     1
      // A..Z = 26 38
      // a..z = 26 64
      if (debug)
	printf("NON-STRING: >%s<\n", s);
      //error("FOO");
      nbadstrings++;
      chs= 0;
      break;
    }
    chs<<= 6;
    chs|= c;
    bits-= 6;
  }
  bits+= 51-nbits; // 52 ???
  if (debug) printf("<\n");
  if (debug) printf("chs= %16lx  %.1f\n", chs, log2f(chs)/log2f(2));
  //  int nstrings= expl(52-chsbits)/expl(10);
  //int ndigits= (int)(log2f(nstrings)/log2f(10));
  //printf("--- %d %c %d digits\n", nstrings, ".kMGTPE"[ndigits/3], ndigits);
  //chsbits+=1;

  if (bits>0) chs<<= bits;

  chs &= maskforchs;
  
  if (debug) {
    printf("INF= %16lx  %.1f\n", LINF_MASK, LINF_MASK);
    printf("CHS= %16lx\n", maskforchs);
    printf("chs= %16lx  %.1f\n", chs, log2f(chs)/log2f(2));
    printf("i  = %16lx  %.1f\n", i, log2f(i)/log2f(2));
    printf("OFS= %16lx  %ld  ", maskforoffset, maskforoffset); hprint(maskforoffset, "B\n");
  }
  assert(!(chs & LINF_MASK)); // no overlap
  assert(!(chs & i)); // no overlap

  // mix them!
  i|= chs;
  if (debug) printf("OFi= %16lx\n", i);
  if (debug) printf("\n");
  
  dbval v;
  v.d= make53(i);

  return v;
}

char* tablestr(table* t, dbval v) {
  long i= is53(v.d);
  //printf("tablestr => %ld\n", i);
  if (i<0) i= -i;
  if (debug) printf("\ntablestr: %ld\n", i);
  i&= maskforoffset;
  if (debug) printf("  offset: %ld\n", i);
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

dbval dbreadCSV(table* t, char** csv, char* str, int len, char delim) {
  double d;
  int r= sreadCSV(csv, str, len, &d, delim);
  switch(r){
  case RNULL:    return mknull();
  case RNUM:     return mknum(d);
  case RSTRING:  return tablemkstr(t, str);
  case RNEWLINE: 
  default:       return mkend();
  }
}

// Fill in dbVALS max N using TABLE,
// parse from a CSV line string using DELIM
//
// N needs to be at least 1
// (TODO: reconsider?)
//
// Returns: number of vals,
//   including a mkend() dbval.
int dbvalsfromline(dbval* vals, int n, table* t, char* csv, char delim) {
  if (!t || !csv || !n) return -1;

  int len= strlen(csv)+1;
  char str[len];

  int col= 0;
  while(!isend((vals[col++]= dbreadCSV(
      t, &csv, str, len, delim)))) {
    if (col+1>=n) {
      vals[col++]= mkend();
      break;
    }
  }
  
  return col;
}

long tableaddline(table* t, char* csv, char delim) {
  if (!t || !csv) return -1;

  int cols= t->cols;
  if (!cols) {
    // estimate no of columns!
    char DELIM[]={delim?delim:',', 0};
    cols= strcount(csv, DELIM)+2;
  }

  dbval vals[cols+1];
  int col= dbvalsfromline(vals, cols+1, t, csv, delim);
  if (col<=1) return -1;

  col--;
  addarena(t->data, vals, sizeof(dbval)*col);

  // first row?
  // assume it's header if not defined
  if (!t->cols) t->cols= col;

  // fill with nulls if needed
  dbval v= mknull();
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
  if (stats || debug) printf("\rload_csvgl: %ld ms %ld %ld rows\n", ms, t->bytesread, t->count);
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
long ncmp= 0, ncmpother= 0;
long ncmptab= 0, ncmptabmiss= 0;
int tablecmp(table* t, dbval a, dbval b) {
  ncmp++;
  // 1% faster with it here for sort!
  if (a.l==b.l) return 0;

  // cheaper do it than test in advance
  //   468 ms instead of 3400 ms!
  int ra= (a.d>b.d);
  int rb= (b.d>a.d);
  //printf("\t\t\t(%lg %lg => %d %d => %d) =>\n", a.d, b.d, ra, rb, ra-rb);
  if (ra!=rb) return ra-rb;
  ncmpother++;
  
  long la= is53(a.d), lb= is53(b.d);
  // ordered! comparable as -la and -lb
  if (la<0 && lb<0) return (lb>la)-(la>lb);
  // resort to cmp strings
  if (la>0 && lb>0) {
    if (!t) error("tablecmp: needs table!");
    ncmptab++;
    // - previously
    // sort -3 took 7342 ms
    // sort 3  took 3701 ms
    // sort -3 took 3238 ms
    // sort 3  took 3703 ms
    // ncmp=545921074

    // - if cmptablestr gets 
    // sort -3 took 3674 ms
    // sort 3  took 3652 ms
    // sort -3 took 2171 ms
    // sort 3  took 2120 ms
    //    ncmptab    =404401657
    //    ncmptabmiss=298383822
    //    = 17% savings in strcmp

    // - if disabled
    // sort -3 took 8923 ms
    // sort 3  took 4377 ms
    // sort -3 took 4140 ms
    // sort 3  took 4259 ms
    //    ncmptab=513121514
			  
    long r= cmptablestr(la, lb);
    //r=0;
    if (r) return r;
    ncmptabmiss++;
    // == undecided
    // (unless same index, but cannot be)
    char* sa= tablestr(t, a);
    char* sb= tablestr(t, b);
    //if (debug) printf("CMP MISS: '%s' '%s'\n", sa, sb);
    if (sa==sb) return 0;
    if (!sa) return -1;
    if (!sb) return +1;
    if (0) {
      int rt = r<0?-1:(!r?0:1);
      int rr= strcmp(sa, sb);
      rr= rr<0?-1:(!r?0:1);
      if (rt && rt!=rr)
	error("Strings differ in compare!");
    } else 
    return strcmp(sa, sb);
  }
  if (isnull(a)) return -1;
  if (isnull(b)) return +1;
  // this sorts all types!
  if ((!!la != !!lb)) return (la>lb)-(lb>la);
  // ends here for NAN!!!
  // NAN is bigger than all number!
  // (same as postgress)
  if (isnan(a.d)) return +1;
  if (isnan(b.d)) return -1;
  // nan is equal (because same bits, lol)
  //dbprint(t, a, 8); printf("type=%d\n", type(a)); putchar('\n');
  //dbprint(t, b, 8); printf("type=%d\n", type(b)); putchar('\n');
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

  // TODO: consider inline qsort?
  // - https://stackoverflow.com/questions/33726723/why-is-this-implementation-of-quick-sort-slower-than-qsort

  ms= timems()-ms;
  if (stats || debug)
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
    if (details <= 0) break;
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


// 5x faster than index.c (create index)!
// and using less mem.
void dotable(char* name, int col) {
  debug= 0;
  stats= 1;
  int details= debug?+100:0;

  FILE* f= fopen(name, "r");
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
  printf("sN %d (-1)\n", tablecmp(NULL, tablemkstr(t, "foo"), mknum(NAN)));

  printf("---MIXED\n");
  printf("ns %d (-1)\n", tablecmp(t, mknum(3), tablemkstr(t, "bar")));
  printf("sn %d (+1)\n", tablecmp(t, tablemkstr(t, "bar"), mknum(3)));
}


// TODO: remove
extern int debug= 1;
extern int stats= 1;


int main(int argc, char** argv) {
  if (argc>1) {
    int col=1;
    if (argc>2) col= atoi(argv[2]);
    dotable(argv[1], col);
    exit(0);
  }
  dbtypetest(); exit(1);
  //tabletest(); exit(0);
}
