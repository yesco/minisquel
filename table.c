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


#include "dbval.c"


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
long ncmp= 0, ncmpother= 0;
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
    char* sa= tablestr(t, a);
    char* sb= tablestr(t, b);
    if (sa==sb) return 0;
    if (!sa) return -1;
    if (!sb) return +1;
    return strcmp(sa, sb);
  }
  if (isnull(a)) return -1;
  if (isnull(b)) return +1;
  // this sorts all types!
  if ((!!la != !!lb)) return (la>lb)-(lb>la);
  error("shouldn't end up here!");
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


// 5x faster than index.c (create index)!
// and using less mem.
void dotable(char* name, int col) {
  debug= 1;
  int details= +1024;

  FILE* f= fopen(name, "r");
  table* t= newtable(name, 0, 0, NULL);
  //table* t= newtable(name, 0, 2, NULL);
  loadcsvtable(t, f);

  tablesort(t, -col, NULL);
  tablesort(t, col, NULL);

  tablesort(t, -col, NULL);
  tablesort(t, col, NULL);
  printtable(t, details);
  fclose(f);
  printf("ms lol ncmp=%ld ncmpother=%ld\n", ncmp, ncmpother);
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
  // compare types!
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
  dbtypetest();
  tabletest(); exit(0);
}
