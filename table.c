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

// TODO: remove
extern int debug;
extern int stats;


struct table;
char* tablestr(struct table*, dbval);

int tdbprinth(struct table* t, dbval v, int width, int human) {
  long i= is53(v.d);

  // Save 31% (when printing 3 "long" & 1 string)
  // Save additionally 2.5% by not doing type check!
  // It's safeb because num, NAN, INF not double equal

  // TODO: benchmark and remove?
  if (0) {
    long l= (long)v.d;
    if (l==v.d) return human
      ? hprint(l, "\t") : printf("%8ld\5", l);
  }  
  switch(type(v)) {
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
    return 1;
  }
    // TODO: does this slow stuff down?
  default: dbprinth(v, width, human);
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

  dbval v= mkstr7ASCII(s);
  if (!isfail(v)) return v;

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

  // TODO: cleanup! don't calculate every time

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
    printf("INF= %16lx  %lu\n", LINF_MASK, LINF_MASK);
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
  
  v.d= make53(i);

  return v;
}

char* tablestr(table* t, dbval v) {
  long i= is53(v.d);
  //printf("tablestr => %ld\n", i);
  if (i<0) i= -i;
  //if (debug) printf("\ntablestr: %ld\n", i);
  i&= maskforoffset;
  //if (debug) printf("  offset: %ld\n", i);
  return i<0?NULL:arenaptr(t->strings->arena, i);
}

dbval val2tdbval(table* t, val* v) {
  if (v->s) return tablemkstr(t, v->s);
  return val2dbval(v);
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

void freetable(table* t) {
  if (!t) return;
  if (t->name) free(t->name);
  //if (t->colnames) free(t->colnames);  only used for testing? there it's constant!
  freearena(t->data);
  freehash(t->strings);
  free(t);
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

// - fil10M.csv
// 41 427 ms ./run 'sel...from..where 1=0'
//
// 19 500 ms loading all into memory
//= 1 000 ms to store in memory)
// 18 426 ms LOAD + PARSE
//=15 000 ms PARSE
//= 2 804 ms LOAD without storing
//  2 239 ms LOAD _csvgetline (no insert)
//    856 ms LOAD _getline (no insert)

//    360 ms DuckDB - LOL!
//    539 ms .output /dev/null "hmid'
//   1500 ms .   "      "       "*"
//
// LOAD: 2.3s (_getline is 856)
//  - problem: \n in quoted value
// PARSE: 15s
// STORE:  1s
//----------- TOTAL: 18s

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

long dbread(FILE* f, table* t, dbval action(dbval,void*), void* data) {
  char *line= NULL, delim= 0;
  size_t len= 0;
  ssize_t n= 0;
  long r= 0;
  while((n= getline(&line, &len, f))!=EOF) {
    r+= n;
    if (!line || !*line) continue;
    if (delim) delim= decidedelim(line);

    char str[len];
    char* cur= line;
    while(!isend(action(dbreadCSV(
      t, &cur, str, len, delim), data)));
  }
  free(line);
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
  
long ncmp= 0, ncmpother= 0;
long ncmptab= 0, ncmptabmiss= 0;

// NULL==NULL < -num < num < "bar" < "foo"
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
  //tdbprinth(t, a, 8, 1); printf("type=%d\n", type(a)); putchar('\n');
  //tdbprinth(t, b, 8, 1); printf("type=%d\n", type(b)); putchar('\n');
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
  if (debug || stats) printf("TABLESORT %d\n", n);
  long ms= timems();

  // TODO:lol
  t->sort_col= n;
  
  qsort_table= t;
  int row_size= t->cols*sizeof(dbval);
  // skip first row (headers)
  int actual_size= t->data->top+ t->count;
  if (actual_size > t->data->size)
    ; // not stored!
  else
    qsort(t->data->mem+row_size, t->count, row_size, (void*)sorttablecmp);
  qsort_table= NULL;

  // TODO: consider inline qsort?
  // - https://stackoverflow.com/questions/33726723/why-is-this-implementation-of-quick-sort-slower-than-qsort

  ms= timems()-ms;
  if (stats || debug)
    printf("sort %s on %d took %ld ms\n", t->name, t->sort_col, ms);
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
      tdbprinth(t, v, 8, 1);
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



#include "ansi.c"

void mode_header(int odd) {
  B(odd ? rgb(0,5,0) : green); 
  C(black);
  clearend();
}
void mode_lineno(){B(gray(5));C(green);}
void mode_body(){B(black);C(white);clearend();}

void pretty_printtable(table *t, long row, long rows) {
  // TODO: why neeed to skip first value?
  dbval* start= (void*)(t->data->mem);
  dbval* v= start;
  dbval* end= (void*)(t->data->mem + t->data->top);
  //  dbval* end= (void*)(start + t->count*t->cols);


  // TODO:takes between 2000-900 ms!
  //   insert in table takes same time
  //   as print the original. Why is it so slow
  //   to print dbval? inline? data type
  //   detetion order?
  // = pretending type() saying all is TNUM
  //   800 ms, little fster, but very stable
  // = must be cost of strings... random access!
  //   MOVING UP string test in type() made it
  //   900ms mostly, code w too many ifs?

  inverse(1);
  printf("MiniSQueL BROWSER:");
  inverse(0); reset();  printf("   ");
  B(rgb(0,0,10)); C(yellow+8); printf(" %s \n", t->name);
  B(black); C(white); clearend();

  long ms= timems();
  mode_body();
  mode_header(1);
  printf("\t");
  int r= 0, n= 0;
  while(v<end) {
    // header alternating colors
    if (n < t->cols) mode_header(n%2);

    // print only after "row" rows
    n++;
    if (r>=0 && r < row) {
      if (n % t->cols==0) r++; // hmm
      v++;
    } else {
      tdbprinth(t, *v++, 8, 1);

      // print rowno as first column
      if (n % t->cols==0) {
	r++; // hmmm
	if (v<end) {
	  if (r==1) mode_body();
	  putchar('\n'); clearend();
	  mode_lineno();
	  tdbprinth(t, mknum(r), 8, 1);
	  mode_body(); clearend();
	}
      }

      if (r > row+rows && rows >= 0) break;
    }
  }
  printf("\n%s: %ld rows\n", t->name, t->count);
  if (debug) printf("\nPrinting table took %ld ms\n", timems()-ms);
  cleareos();
}

void browsetable(table* t) {
  jio();

  clear();
  
  const long pagerows= screen_rows-10;
  
  char* cmd= NULL;
  size_t len= 0;
  long row= 0;
  gotorc(0, 0);
  pretty_printtable(t, row, pagerows);
  
  while(1) {
    printf("> "); fflush(stdout);
    cursoron();
    getline(&cmd, &len, stdin);
    cursoroff();
    char* arg= &cmd[1];
    char c= cmd[0];
    switch(c) {
    case 'q': return;
    case 'L'-64: clear(); break;
    case 'f': case 'n': case '\n':
      row+= pagerows; break;
    case 'b': case 'p':
      row-= pagerows; break;
    case '<': row= 0; break;
    case '>': row= t->count - pagerows; break;

    case 'o': tablesort(t, atoi(arg), NULL); break;
    case 'g': break;
    case 'a': pretty_printtable(t, 0, -1); continue;
    case '#': row= atoi(arg)-1; break;
    case '?': case 'h':
      printf("\nUsage: q)uit o)rder:3 g)group:2  h)elp a)ll <start >end n)ext p)rev #35 \n"); continue;
    }
    if (row < 0) row= 0;
    if (row > t->count - pagerows) row= t->count - pagerows;

    gotorc(0, 0);
    pretty_printtable(t, row, pagerows);
  }
  //printtable(t, 0);
  cursoron();
  
  _jio_exit();
}
