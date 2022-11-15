#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include "malloc-count.c"
//#include "malloc-simple.c"

// global state "external"

int stats= 1, debug= 0, security= 0;
long foffset= 0;

// stats
long lineno= -2, readrows= 0, nfiles= 0;

#define NAMELEN 64
#define VARCOUNT 256
#define MAXCOLS 32

#include "utils.c"

#include "csv.c"
#include "vals.c"

#include "dbval.c"

int nextvarnum= 1;

#define OLS(f, s) (printf("OL %s \"%s\"\n", f, s), nextvarnum++)
  
#define OL1(f, a) (printf("OL %s %d\n", f, (int)a), nextvarnum++)
  
#define OLcmp(f, a, b) (printf("OL %s %d %d 0\n", f, (int)a, (int)b))

#define OL3(f, a, b) (printf("OL %s -%d %d %d 0\n", f, nextvarnum, (int)a, (int)b), nextvarnum++)

#define ROLcmp(f, a, b) return OLcmp(f, a, b)

int varfind(char* table, char* col) {
  val* vp= findvar(table, col);
  //assert(vp);
  return vp ? vp->d : 0;
}

int defvar(char* table, char* col, int d) {
  // TODO: leak
  val v= {};
  d= d ? d : nextvarnum++;
  setnum(&v, d);
  setvar(table, col, &v);
  fprintf(stderr, "\t{OL@%d %s %s}\n", d, table, col);
  return d;
}

int defstr(char* s) {
  int n= varfind("$const", s);
  if (n) return n;
  
  fprintf(stderr, "@%d ", nextvarnum);
  defvar("$const", s, nextvarnum);
  return OLS(":", s);
}

int defnum(double d) {
  char name[NAMELEN]= {};
  snprintf(name, NAMELEN, "%-15lg", d);

  int n= varfind("$const", name);
  if (n) return n;

  fprintf(stderr, "@%d ", nextvarnum);
  defvar("$const", name, nextvarnum);
  return (printf("OL %s %-15lg\n", ":", d), nextvarnum++);
}

int defint(long d) {
  return defnum(d);
}

// global flag: skip some evals/sideeffects as printing during a "parse"/skip-only phase... (hack)
// TODO: change to eparse_num(disable_call, disable_print, print_header)
//   to handle header name def/print
//   to handle aggregates (not print every row, only last (in group))
int parse_only= 0;

char *query=NULL, *ps= NULL;

// -- Options

int batch= 0, force= 0, browse= 0;
int verbose= 0, interactive= 1;

int globalecho= 1, echo= 1;

char globalformat[30]= {0};
char format[30]= {0};

int parse(char* s) {
  free(query);
  query= ps= s;
  return 1;
}

int end() {
  return !ps || !*ps;
}

int gotc(char c) {
  if (end()) return 0;
  if (*ps != c) return 0;
  ps++;
  return c;
}

// OR
int gotcs(char* s) {
  if (end() || !s) return 0;
  char* f= strchr(s, *ps);
  if (!f) return 0;
  ps++;
  return *f;
}

int spcs() {
  int n= 0;
  while(gotcs(" \t\n\r")) n++;;
  return n;
}

int isid(char c) {
  return isalnum(c) || strchr("@$#_.", c);
}
 
// also removes starting/trailing spaces, no prefix match
// Use only for [alnum _ $ .]
// It's case insensitive!
// Don't match partial "words"
// got("foo") does NOT got "foobar"
int got(char* s) {
  if (end() || !s) return 0;
  spcs();
  char* p= ps;
  while(*p && *s && tolower(*p) == tolower(*s)) {
    p++; s++;
  }
  if (*s) return 0;
  // got("f") not match "foo"
  if (isid(*p)) return 0;
  ps= p;
  spcs();
  return 1;
}

int parse_num() {
  spcs();
  char* end= NULL;
  double d= hstrtod(ps, &end);
  if (end<=ps) return 0;
  ps= end;
  return defnum(d);
}

// Parses a string and gives it to a new
// dbval, it should be dbfree(d).
//
// A pointer to the parsed string
// is returned in *s.
// ! DO NOT FREE!
int parse_str(char** s) {
  spcs();

  char* start= ps;
  if (debug>2) printf("--- PARSE_STR\n");

  char q= gotcs("\"'");
  if (!q) return 0; // fail
  *s= ps;
  while(!end()) {
    if (*ps==q && *++ps!=q) break; // 'foo''bar'
    if (*ps=='\\') ps++; // 'foo\'bar'
    ps++;
  }
  ps--;
  if (*ps!=q) return 0; // fail

  char* a= strndup(*s, ps-*s);
  *s= a;
  ps++;
  // fix quoted \' and 'foo''bar'
  while(*a) {
    if (*a==q || *a=='\\')
      strcpy(a, a+1);
    a++;
  }

  int n= defstr(*s);
  free(*s);
  return n;
}

int parse_into_str(char name[NAMELEN]) {
  spcs();

  // TODO: catch cache... or in getname/getsymbol?

  char q= gotcs("\"'");
  if (!q) return 0;
  char* start= ps;
  while(!end()) {
    if (*ps==q && *++ps!=q) break; // 'foo''bar'
    if (*ps=='\\') ps++; // 'foo\'bar'
    ps++;
  }
  ps--;
  if (*ps!=q) return 0;
  if (ps-start+1>NAMELEN) return 0;

  char* a= strncpy(name, start, ps-start);
  ps++;
  // fix quoted \' and 'foo''bar'
  while(*a) {
    if (*a==q || *a=='\\')
      strcpy(a, a+1);
    a++;
  }

  // result in name already
  return 1;
}

// TODO: sql allows names to be quoted!
int getname(char name[NAMELEN]) {
  spcs();
  ZERO(*name);
  char *p= &name[0], *last= &name[NAMELEN-1];
  // first char already "verified"
  while(isid(*ps) && !end() && p < last) {
    *p++= *ps;
    ps++;
  }
  *p= 0;
  return p!=&name[0];
}

void expectname(char name[NAMELEN], char* msg) {
  if (getname(name)) return;
  expected(msg? msg: "name");
}

// allow: foo or "foo" but not 'foo'
// TODO: foo/bar
void expectsymbol(char name[NAMELEN], char* msg) {
  spcs();
  // "foo bar"
  if (*ps=='"') {
    if (!parse_into_str(name)) expected("string");
  } else if (gotc('[')) { // [foo bar]
    char* start= ps;
    while(!end() && !gotc(']')) ps++;
    if (ps-start >= NAMELEN)
      error("[] name too long");
    strncpy(name, start, ps-start-1);
  } else if (gotc('*')) {
    strcpy(name, "*");
  } else { // plain jane name
    expectname(name, msg);
  }
}

// functions
typedef struct func {
  char* name;
  void* f;
  struct func* next;
} func;

func* funcs= NULL;

void registerfun(char* name, void* f) {
  func* p= funcs;
  funcs= calloc(1, sizeof(func));
  funcs->name= name;
  funcs->f= f;
  funcs->next= p;
}

func* findfunc(char* name) {
  func* f= funcs;
  while(f) {
    if (0==strcmp(f->name, name))
      return f;
    f= f->next;
  }
  return NULL;
}

// hack to include C, haha
#include "funs.c"

// end functions

// parser

int expr();

int call(char* name) {
  #define MAXPARAMS 10

  val r= {};
  int params[MAXPARAMS]= {0};
  int pcount= 0;
  func* f= findfunc(name);
  if (!f) expected2("not found func", name);
  if (debug && !parse_only) printf("\n---CALL: %s\n", name);
  while(!gotc(')')) {
    int v= expr();
    if (!v) expected("expression");

    params[pcount++]= v;
    if (debug && !parse_only) {
      printf("\tARG %d: %d\n", pcount, v);
    }

    spcs();
    if (*ps!=')' && !gotc(',')) expected("comma");
  } 

  printf("OL %s %p %d", name, f->f, nextvarnum);
  for(int i=0; i<pcount; i++)
    printf(" %d", params[i]);
  putchar('\n');
  return nextvarnum++;

  #undef MAXPARAMS
}

// parse var name and get value
// WARNING: if not found=>NULL
// and always return true
int var() {
  char name[NAMELEN]= {};
  // TODO:? "variables) in prepared stmsts
  if (getname(name)) {
    val v= {};
    if (gotc('(')) return call(name);

    // "plain variable"
    char* dot= strchr(name, '.');
    if (dot) *dot= 0;
    char* column= dot?dot+1:name;
    char* table= dot?name:NULL;

    // find or define var
    int n= varfind(table, name);
    return n?n:defvar(table, name, 0);
  }
  return 0;
}

int prim() {
  int v= 0;
  spcs();
  if (gotc('(')) {
    v= expr();
    if (!v) expected("expr");
    spcs();
    if (!gotc(')')) expected("')'");
    return v;
  }
  int n= parse_num();
  if (n) return n;

  char* s= NULL;
  n= parse_str(&s);
  if (n) return n;
  
  // only if has name
  if (isid(*ps)) return var();
  return v;
}

int mult() {
  int v= prim();
  if (!v) return v;
  char op;
  while ((op= gotcs("*/%"))) {
    int vv= prim();
    if (!vv) return vv;
    // type num op num => num
    // other types => nan
    switch(op){
    case '*': v= OL3("*", v, vv); break;
    case '/': v= OL3("/", v, vv); break;
    case '%': v= OL3("%", v, vv); break;
    default: error("Undefined operator!");
      return 0;
    }
  }
  return v;
}

int expr() {
  spcs();
  int v= mult();
  if (!v) return v;
  char op;
  while ((op= gotcs("+-"))) {
    int vv= mult();
    if (!v) return v;
    if (op == '+') {
      v= OL3("+", v, vv);
    } else if (op == '-') {
      v= OL3("+", v, vv);
    }
    // TODO: null propagate?
  }
  return v;
}

#include "table.c"

char* print_header(char* e, int dodef) {
  char* old_ps= ps;
  ps= e;
  
  int col= 0;
  val v= {};
  int more= 0;

  if (dodef) printf("OL out");
  if (!dodef) parse_only= 1;
  do {
    // TODO: SELECT *, tab.*

    spcs();
    char* start= ps;
    char name[NAMELEN]= {};

    char q;
    if ((q= gotcs("[*%"))) {
      ps--;
      expectsymbol(name, "[] column name");

      // search names in defined order
      int n= 0;
      char varname[NAMELEN]= {};
      for(int i=0; i<varcount; i++) {
	// format name to test
	char* t= tablenames[i];
	snprintf(varname, sizeof(varname), "%s.%s", t?t:"", varnames[i]);
	// don't match '$' names,
	// unless asked for
	if (!strchr(varname, '$') ||
	    strchr(name, '$'))
	    if (like(varname, name, 0)) {
	      col++;
	      if (dodef) {
		int d= varfind(t, varnames[i]);
		printf(" %d", d);
	      }
	    }
      }
      spcs(); more= gotc(',');
    } else {
      //EXPECT(d, expr);
      int d= expr();
      if (!d) expected("expression");

      // select 42 AS foo
      if (got("as")) {
	expectsymbol(name, "alias name");
      } else {
	// give name as expression
	strncpy(name, start, min(ps-start, NAMELEN));
      }
      if (dodef) defvar(NULL, name, d);

      spcs(); more= gotc(',');
      col++;

      if (dodef) printf(" %d", d);
    }

    clearval(&v);
  } while(more);
  if (!dodef) parse_only= 0;
  if (dodef) putchar('\n');

  e= ps;
  ps= old_ps;
  return e;
}

int comparator(char cmp[NAMELEN]) {
  spcs();
  // TODO: not prefix?
  if (got("like")) { strcpy(cmp, "like"); return 1; }
  if (got("ilike")) { strcpy(cmp, "ilike"); return 1; }
  if (got("in")) { strcpy(cmp, "in"); return 1; }
  // TODO: (not) between...and...
  //   is (not)
  
  cmp[0]= gotcs("<=>!");
  cmp[1]= gotcs("<=>!");
  // third? <=> (mysql)
  //   nullsafe =
  //   null==null ->1 null=? -> 0
  //   - https://dev.mysql.com/doc/refman/8.0/en/comparison-operators.html#operator_equal-to
  if (!cmp[0]) expected("comparator");
  spcs();
  return 1;
}

#define TWO(a, b) (a+16*b)

int dcmp(char* cmp, int a, int b) {
  switch (TWO(cmp[0], cmp[1])) {
  case TWO('i', 'n'): expected("not implemented in");
    // lol

  case TWO('~','='):
    //TODO:

  case TWO('i','l'):  // ilike, like
  case TWO('l','i'):

  case TWO('=','='):
  case '=': ROLcmp("=", a, b);

  case TWO('<','>'):
  case TWO('!','='): ROLcmp("!", a, b);

  case TWO('!','>'):
  case TWO('<','='): ROLcmp("<=", a, b);
  case '<': ROLcmp("<", a, b);

  case TWO('!','<'):
  case TWO('>','='): ROLcmp(">=", a, b);
  case '>': ROLcmp(">", a, b);

  default: expected("dcmp: comparator");
  }
  return 0;
}

// TODO:??? optimize if know is string
int scmp(char* cmp, dbval da, dbval db) {
  // TODO: benchmark this...
  if (da.l==db.l && cmp[0]!='!' && (cmp[0]=='=' || cmp[1]=='=')) return 1;

  // relative difference
  int r= dbstrcmp(da, db);
  
  // the ops not requiring str()
  switch (TWO(cmp[0], cmp[1])) {
  case '=':
  case TWO('=','='): return !r;

  case TWO('<','>'):
  case TWO('!','='): return !!r;

  case TWO('!','>'):
  case TWO('<','='): return (r<=0);
  case '<': return (r<0);

  case TWO('!','<'):
  case TWO('>','='): return (r>=0);
  case '>': return (r>0);
  }

  // not going to work for 7 etc...
  char* a= str(da);
  char* b= str(db);
  
  switch (TWO(cmp[0], cmp[1])) {
  case TWO('i', 'n'): expected("not implemented in");

  case TWO('i','l'): return like(a, b, 1);
  case TWO('l','i'): return like(a, b, 0);
    
  //case TWO('~','='): return !strcasecmp(a, b);

  default:
    printf("OP= %s\n", cmp);
    expected("legal opoerator");
  }
  return -666;
}

// returns "extended boolean"
#define LFALSE (-1)
#define LFAIL 0
#define LTRUE 1
// NOTE: -LFALSE==LTRUE !!

int comparison() {
  char op[NAMELEN]= {};
  int a= expr();
  if (!a) return 0;
  if (!comparator(op)) expected("op compare");
  int b= expr();
  if (!b) return 0;
  // TODO: optimzie for type if known

  //if (isnum(a) && isnum(b))
  //r= dcmp(op, a.d, b.d)?LTRUE:LFALSE;
  //else
  //r= scmp(op, a, b)?LTRUE:LFALSE;
  // don't even care type now!
  return dcmp(op, a, b);
}

int logical();

int logsimple() {
  // TODO: WHERE (1)=1    lol
  // ==extended expr & use val?
  if (gotc('(')) {
      int r= logical();
      if (!gotc(')')) expected(")");
      return r;
  }
  if (got("not")) {
    // TODO: 
    expected("NOT not implemented");
    return -logsimple();
  }
  // TODO "or here"?
  // but need lower prio than not
  return comparison();
}

// returns boolean of evaluation
int logical() {
  spcs();
  int r= LTRUE, n;
  while((n= logsimple())) {
    r= ((r==LTRUE) && (n==LTRUE))?LTRUE:LFALSE;
    if (got("and")) {
      continue;
    } else if (!got("or")) {
      return r;
    } else {
      // OR
      while((n= logsimple())) {
	if (n==LTRUE) r= n;
	printf("OL or\n");
	if (!got("or")) break;
      }
      if (!got("and")) return r;
    }
  }
  return r;
}

int after_where(char* selexpr) {
  // GROUP BY and HAVING
  if (got("group by")) {
    // ? implies sorting, unless hash
    // first variant could just sort on
    // the group by columns (must be mentioned in the select?)
    error("GROUP BY: not yet implemented");
  }
  if (got("having")) {
    // to test after aggregation of group
    // can "invalidate" rows by changin
    // col1 to DELETED (or all cols)
    error("NOT HAVING fun yet!");
  }

  if (got("order by")) {
    // will order on groups already generated... valuies order by might be aggregate
    double d;
    // TODO: or column name...
    //   now -4 also works as "ASC" lol
    int db= parse_num();
    if (!db) expected("order by COL");
    int col= db;
    got("ASC") || got("ASCENDING");
    if (got("DESC") || got("DESCENDING"))
      col= -col;
    // TODO: generate OL

    // TODO: several columns...
    //if (debug) printf("ORDER BY %d\n", col);
  }

  if (selexpr) return !!print_header(selexpr, 1);
  return 1;
}

int where(char* selexpr) {
  if (got("where")) (void)logical();
  return after_where(selexpr);
}

// called to do next table
int from_list(char* selexpr, int is_join);

int next_from_list(char* selexpr) {
  char* backtrack= ps;
  if (got("join")) {
    return from_list(selexpr, 1);
  } else if (gotc(','))
    return from_list(selexpr, 0);
  else
    return where(selexpr);
  ps= backtrack;
}
		   
// schema("type,name,param,select,impl",
//   type, name, params, select, impl);


int VIEW(char* table, char* selexpr) {
  int nvars= varcount;
  char* backtrack= ps;

  val type={}, name={}, params={}, select={}, impl={};
  linkval(table, "type", &type);
  linkval(table, "name", &name);
  linkval(table, "params", &params);
  linkval(table, "select", &select);
  linkval(table, "impl", &impl);

  //dbobj* l= objs;
  lineno= -2;
  /*
  while(l) {
    lineno++;

    setstrconst(&type, l->type);
    setstrconst(&name, l->name);
    setstrconst(&params, l->params);
    setnum(&select, l->select);
    setstrconst(&impl, l->impl);

    ps= backtrack;
    //print_expr_list(selexpr);
    next_from_list(selexpr);
    l= l->next;
  }
  */
  ps= backtrack;
  varcount= nvars;
  return 1;
}

// sqlite3: virtual tables
// - https://www.sqlite.org/vtablist.html

// dir(): CREATE TABLE dir(path) FROM popen("ls -Q -l --time-style=long-iso path |")
//    or --full-time

// DuckDB:
//   CREATE TABLE t1(x FLOAT, two_x AS (2 * x))
//   CREATE TABLE t1(x FLOAT, two_x FLOAT GENERATED ALWAYS AS (2 * x) VIRTUAL)
//   https://duckdb.org/docs/sql/statements/create_view


// TODO: add (start,stop,STEP)
//   https://duckdb.org/docs/sql/statements/create_sequence
int INT(char* selexpr) {
  int nvars= varcount;

  char name[NAMELEN]= {};
  double start= 0, stop= 0, step= 1;
  // TODO: generalize, use functions?
  if (gotc('(')) {
    int dstart= parse_num();
    if (!dstart) expected("number");
    start= dstart;
    if (!gotc(',')) expected(",");
    int dstop= parse_num();
    if (!dstop) expected("number");
    stop= dstop;
    if (!gotc(')')) expected(")");
    stop+= 0.5;
    spcs();
    expectname(name, NULL);
  } else expected("(");

  int n= OL3("i", start, stop);
  defvar("int", name, n);

  next_from_list(selexpr);

  return n;
}

#include "index.c"

// header, if given, is free:d
int TABCSV(FILE* f, char* table, int t, char* header, int fn, char* selexpr) {
  printf("OL scan %d", t);

  char* line= header? header: csvgetline(f, ',');
  int r;
  char name[NAMELEN]= {};
  char delim= decidedelim(line);
  char* p= line;
  while((r= sreadCSV(&p, name, NAMELEN, NULL, delim))) {
    //printf("COL: %s\n", name);
    // even numbers will be in name
    int n= defvar(table, name, 0);
    printf(" -%d", n);
  }
  free(line);

  printf("\n");

  return t;
}

// Parses a column list (COL, COL..)
//
// Returns: strdup:ed string, to be free:d

char* getcollist() {
  if (!gotc('(')) return NULL;

  char* start= ps;
  spcs();
  while(!end()) {
    char col[NAMELEN]= {0};
    expectname(col, "colname");
    spcs();
    if (gotc(')')) break;
    if (!gotc(',')) expected("colname list");
  }
  return strndup(start, ps-start-1);
}

// tries to find it in ./ and ./Test/
FILE* openfile(char* spec) {
  if (!spec || !*spec) expected("filename");

  FILE* f= NULL;

  // popen? see if ends with '|'
  int len= strlen(spec);
  if (spec[len-1]=='|') {
    if (security) expected2("Security doesn't allow POPEN style queries/tables", spec);
    spec[strlen(spec)-1]= 0;
    if (debug) printf(" { POPEN: %s }\n", spec);
    f= popen(spec, "r");
  } else {

    // try open actual FILENAME
    // TODO: make a fopen_debug
    if (debug) printf(" [trying %s]\n", spec);
    f= fopen(spec, "r");

    // try open Temp/FILENAME
    if (!f) {
      char fname[NAMELEN]= {0};
      snprintf(fname, sizeof(fname), "Test/%s", spec);
      if (debug) printf(" [trying %s]\n", fname);
      f= fopen(fname, "r");
    }
  }

  nfiles++;
  return f;
}

FILE* expectfile(char* spec) {
  FILE* f= openfile(spec);
  if (!f) expected2("File not exist", spec);
  return f;
}

// opens a file:
// - type .sql - call ./sql on it...
// - type .csv just read it
FILE* _magicopen(char* spec) {
  // TODO:maybe not needed?
  spec= strdup(spec);
  // handle foo.sql script -> popen!
  if (endsWith(spec, ".sql")) {
    char fname[NAMELEN]= {0};
    snprintf(fname, sizeof(fname), "./minisquel --batch --init %s |", spec);
    free(spec);
    spec= strdup(fname);
  }
  if (debug) printf(" [trying %s]\n", spec);
  FILE* f= openfile(spec);
  free(spec);
  return f;
}
  

// opens a magic file
// - if "foobar |" run it and read output
// - try it as given
// - try open.csv if not exist
// - try open.sql and run it
// - fail+exit (not return) if fail
// - guaranteed to return file descriptor
FILE* magicfile(char* spec) {
  if (!spec || !*spec) expected("filename");
  spec= strdup(spec); // haha

  FILE* f= _magicopen(spec);
  if (!f && spec[strlen(spec)-1]!='|') {
    if (!f) { // .csv ?
      spec= realloc(spec, strlen(spec)+1+4);
      strcat(spec, ".csv");
      f= _magicopen(spec);
    }
    if (!f) { // .sql ?
      strcpy(spec+strlen(spec)-4, ".sql");
      f= _magicopen(spec);
    }
  }
  if (!f) expected2("File not exist (tried X X.csv X.sql", spec);
  if (debug && f) printf(" [found %s]\n", spec);
  free(spec);
  return f;
}

int from_list(char* selexpr, int is_join) {
  char* backtrack= ps;

  // filename.csv or "filename.csv"
  char spec[NAMELEN]= {0};
  expectsymbol(spec, "from-iterator");

  // TODO: how to do fulltest search/filter
  //   like sqlite3 MATCH?
  //   - https://www.sqlite.org/fts5.html
  //   - https://www.ibm.com/docs/en/db2/11.1?topic=indexes-full-text-search-methods
  //  my thinking, "and grep it!"
  // FROM  "foo.csv" MATCH "+must +have" ftab
  // FROM GREP("mail.csv" from:jsk subject:fish) myfish
  
  // dispatch to named iterator
  if (0==strcmp("$view", spec)) {
    char table[NAMELEN]= {0};
    expectname(table, "table alias name");
    VIEW(table, selexpr);
  } else if (0==strcmp("int", spec)) {
    INT(selexpr);
    // TODO: is_join?
  } else {
    // - foo.csv(COL, COL..) foo
    char* header= getcollist();

    // - foo.csv ... TABALIAS
    char table[NAMELEN]= {0};
    expectname(table, "table alias name");
    
    // - JOIN foo.csv ... tab ON
    char joincol[NAMELEN]= {0};
    val* joinval= NULL;
    if (is_join) {
      if (!got("on")) expected("on joincol");
      // ON "foo"
      expectname(joincol, "join column name");
      // TODO: get "last" table here?
      joinval= findvar(NULL, joincol);
    }
    
    int fn= defstr(spec);
    //OL(defvar("$file", table, fn);

    int t= defvar("$table", table, 0);

    printf("OL file -%d %d\n", t, fn);
    
    FILE* f= magicfile(spec);
    TABCSV(f, table, t, header, fn, selexpr);
    fclose(f);

    next_from_list(selexpr);
  }

  ps= backtrack;
  return 1;
}

int from(char* selexpr) {
  if (!got("from")) {
    where(selexpr);
    return 0;
  } else {
    from_list(selexpr, 0);
    return 1;
  }
}

// --- JOIN
// - https://dev.mysql.com/doc/refman/5.7/en/join.html
//   FROM tab (|AS t)
//   (|LEFT|RIGHT)
//   (|INNER|OUTER|NATURAL|CROSS)
//   JOIN tab (|AS t)
//   (ON (a=b AND ...) | USING (col, col, col))
// simplify:
//   from a (|MERGE) JOIN b USING(col, ...)
//   from a NATURAL JOIN b 
//
// 1. implemnt as nested loop
//    a. create index
//    b. lookup from outer loop
//    c. read matching/lines
// 2. merge join if sorted
//    a. analyze file -> create link if sorted:
//        FILTAB.index:a,b,c
//    b. merge-join
//    c. sort files/create index


// just an aggregator!
int sqlselect() {
  strcpy(format, globalformat);
  if (got("format") && !getname(format)) expected("format name");
  if (!got("select")) return 0;
  char* expr= ps;
  // "skip" (dummies) to get beyond
  parse_only= 1;
  char* end= print_header(expr, 0);
  if (end) ps= end;

  from(expr);
  return 1;
}


int sqlcreateview() {
  return 1;
}


// CREATE FUNCTION sum(a,b) RETURN a+b;
int sqlcreate_function() {
  return 1;
}

// TODO: make resident in mem! LOL
int create_index() {
  char name[NAMELEN]= {0};
  expectname(name, "index name");

  if (!got("on")) expected("ON");

  char table[NAMELEN]= {0};
  expectsymbol(table, "table namex");

  if (!gotc('(')) expected("(");

  char col[NAMELEN]= {0};
  // TODO: computed expressions?
  // or ... AS SELECT ...
  expectname(col, "column name");

  if (!gotc(')')) expected(")");
  
  // TODO: registerobj

  FILE* f= magicfile(table);
  error("create index ...OL");
  //TABCSV(f, table, NULL, 1, col, selexpr);
  fclose(f);

  return 1;
}

// CREATE VIEW name [ ( 2bar, ... ) ] AS ...
// CREATE VIEW files AS "ls -1|"(name)
// CREATE VIEW files(@dir) AS concat("ls -1 ", @dir, "|")(name)
// CREATE VIEW ont2ten AS SELECT i FROM int(1,10) i
// CREATE VIEW iota(@a,@b) AS SELECT i FROM int(@a,@b) i
int create_view() {
  char name[NAMELEN]= {0};
  expectname(name, "index name");

  // parameters
  char* params= NULL;
  if (gotc('(')) {
    params= params;
    while(!gotc(')'));
    if (!*ps) expected(")");
  }

  if (!got("as")) expected("AS");

  spcs();
  char* impl= ps;
  //regobj("view", name, params, impl, got("select"));
  return 1;
}

int sqlcreate() {
  if (!got("create")) return 0;
  int (*creator)()= NULL;
  
  if (got("index")) return create_index();
  if (got("view")) return create_view();
  //  if (got("function")) return create_function();
  return 0;
}


// TODO: DECLARE var [= 3+4]
//   - if in file, forget after load!
int setvarexp() {
  // set a = 42
  // TODO?: set (a,b,c) = (42,"foo",99)
  if (!got("set")) return 0;
  char name[NAMELEN]= {0};
  expectname(name, "variable");
  spcs();
  if (!gotc('=')) expected("=");
  int d= expr();
  if (!d) expected("expression");
  val v= {};
  setnum(&v, d);
  setvar("global", name, &v);
  return 1;
}



int sql() {
  spcs();
  if (end()) return 1;

  int r= sqlselect() ||
    setvarexp() ||
    sqlcreate() ;

  return r;
}

 void testread() {
  // not crash for either file
  //FILE* f= fopen("foo.csv", "r");
  FILE* f= fopen("happy.csv", "r");
  //FILE* f= fopen("err.csv", "r");
  //FILE* f= fopen("fil.csv", "r");
  if (!f) error("NOFILE");
  char s[10240];
  int r= 0;
  double d= 0;
  while((r=freadCSV(f, s, sizeof(s), &d, ','))) {
    if (r==RNEWLINE) printf("\n");
    else if (r==RNUM) printf("=> %3d  >%lg<\n", r, d);
    else if (r==RNULL) printf("=> %3d  NULL\n", r);
    else printf("=> %3d  >%s<\n", r, s);
  }
  printf("=> %3d  %s\n", r, s);
  fclose(f);
}


// TODO: this doesn't feel minimal, lol

// TODO: first call (sql, "start", NULL...)
//   after query (sql, "end", "error", "msg", readrows, rows, ms)
void sqllog(char* sql, char* state, char* err, char* msg, long readrows, long rows, long ms) {
  static FILE* f= NULL;
  if (!f) {
    f= fopen("sql.log.csv", "a+");
    setlinebuf(f);
  }
  if (!f) error("can't open/create logfile");
  fseek(f, 0, SEEK_END);
  long pos= ftell(f);
  // if new file, create header
  // ISO-TIME,"query"
  if (!pos) {
    fprintf(f, "time,state,sql,error,msg,readrows,rows,ms,nfiles,nalloc,nfree,leak,nbytes\n");
  }

  if (sql) {
    fprintf(f, "%s,%s,", isotime(), state);
    fprintquoted(f, sql, 7, '"', 0);
  }

  if (err || msg || readrows || rows || ms) {
    fputc(',', f);
    fprintquoted(f, err?err:"", 7, '"', 0);
    fputc(',', f);
    fprintquoted(f, msg?msg:"", 7, '"', 0);
    fputc(',', f);
    fprintf(f, "%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
      readrows, rows, ms, nfiles, nalloc, nfree, nalloc-nfree, nbytes);
  } else {
    fprintf(f, ",,,,,,,");
  }

  fputc('\n', f);

  // close is kind of optional, as "\a+" will flush?
}

void runquery(char* cmd) {

  if (echo) printf("SQL> %s\n", cmd);
  if (!*cmd) return;
  
  mallocsreset();

  long ms= cpums();
  parse(cmd);
  int r= sql();
  ms= cpums()-ms;
  
  // lineno= -2 when starts
  // lineno= 0 when printed printed header just beore printint one row
  // if no rows, no header printed so -1

  // just shows FROM ... 
  //printf("AT> '%s'\n", ps);

  if (stats && lineno >= -1) {
    // remove "jitter"
    if (ms<1) ms= 1;
    if (ms>1000) ms= (ms+5)/10*10;
    printf("\n%ld rows in %ld ms (read %ld lines)\n", lineno<0 ? 0 : lineno, ms, readrows);
    fprintmallocs(stdout);
  }

  // TODO: clean stats!

  // TODO: catch/report parse errors
  if (r!=1) printf("\n%% Couldn't parse that\n");

  // TODO: print leftover
  //if (ps && *ps) printf("%%UNPARSED>%s<\n", ps);
  printf("\n");
}

void process_file(FILE* in, int prompt);

// returns number of args processed (1 or 2)
int process_arg(char* arg, char* next) {
  int gotit =
    optstr("format", globalformat, sizeof(globalformat), arg) ||
    optint("debug", &debug, arg) ||
    optint("echo", &echo, arg) ||
    optint("security", &security, arg) ||
    optint("stats", &stats, arg) ||
    optint("force", &force, arg) ||
    optint("verbose", &verbose, arg) ||
    optint("-v", &verbose, arg) ||
    optint("interactive", &interactive, arg) ||
    optint("-t", &interactive, arg) ||
    optint("browse", &browse, arg) ||
    0;

  int foo=0;
  if (gotit) {
    return 1;
  } else if (optint("csv", NULL, arg)) {
    strcpy(format, "csv");
  } else if (optint("foo", &foo, arg)) {
    printf("FOO %d\n", foo);
  } else if (0==strncmp("--batch", arg, 7)) {
    // LOL, need to do manual to suppress
    optmsg= NULL;
    optint("batch", &batch, arg);
    if (batch) {
      strcpy(globalformat, "csv");
      echo= 0; stats= 0; interactive= 0;
      optmsg= NULL;
    } else {
      echo= 1; stats= 1; interactive= 1;
      optmsg= optmessage;
    }

  } else if (optint("init", NULL, arg)) {
    char* fn= NULL;
    if (optstr("init", &fn, -1, arg)) next= NULL;
    FILE* f= expectfile(fn ? fn : next);
    free(fn);
    process_file(f, 0);
    fclose(f);
    if (next) return 2;

  } else if (arg==strstr(arg, "--")) {
    // -- Unknown option
    printf("Usage: ./sql ( [OPTIONS] [SQL] ) ... \n\
\n\
[OPTIONS]\n\
--batch\n\
	(== --csv --no-echo --no-stats --no-interactive)\n\
--csv\n\
--debug\n\
	gives some inner working info\n\
	like detailed index info\n\
--echo\n\
--force\n\
--format=csv|bar|tab	(tab is default)\n\
--init FILENAME\n\
	Loads and runs SQL from file.\n\
--interactive | -t\n\
--security\n\
	Disables potentially dangerious operations.\n\
	popen: select name from \"ls -1 |\"(name) file\n\
--stats\n\
	= 1 (default) gives timing\n\
	= 2 gives more details\n\
--verbose | -v\n\
--verbose\n\
\n\
[SQL]	'select 42'\n\
	'select \"foo\"'\n\
\n\
	If queries are run command line, it'll exit after.\n\
	To change; add--interactive.\n\
\n\
\n");
    printf("Unknown option: %s\n", arg);
    fprintf(stderr, "Unknown option: %s\n", arg);

    error("Unkonwn option");

  } else {
    //  no option match: assume sql
    runquery(arg);

    interactive= 0;
  }

  return 1;
}

void process_file(FILE* in, int prompt) {
  char* line= NULL;
  size_t l= 0;
  do {
    if (prompt) printf("SQL> ");
    if (getline(&line, &l, in)==EOF) break;
    int len= strlen(line);
    if (line[len-1]=='\n') line[len-1]= 0;
    // haha, process as if argument!
    if (prompt) echo= 0;
    (void)process_arg(line, NULL);
    echo= globalecho;
  } while(strcmp("exit", line) && strcmp("quit", line));
  if (prompt) printf("\n");
}

// speed "sql' 1.6 M/s
// nn=30M ops 4.55 M/s in 6600 ms -c
// nn=30 Mops 6.12 M/s in 4903 ms -O3
// nn=30 Mops 9.58 M/s in 3132 ms const
// nn=30 Mops 9.65 M/s in 3108 ms !stat
// nn=30 Mops 9.99 M/s in 3003 ms !clr
// nn=30 Mops 5.14 M/s in 5842 ms findf

// - findfunc overhead (/ 6.12 5.14)
//    1.19 x == it's going to inc!!!
//
// TODO: could cache at parsepos! LOL

// - clear overhead (/ 9.99 9.65)
//    1.03 x == ok...

// - updatestats overhead (/ 9.65 9.58)
//    1.007 x == free!

// const: (using setstrconst)
// - mall/free overhead (/ 9.58 4.05)
//    2.4 x

// - sql overhead (/ 6.12 1.6)
//    3.8 x


void speedtest() {
  int N= 10*1000*1000;
  val n4={},n5={},n6={};
  val a={},b={},c={},d={};
  val foo={},bar={},fie={};
  val fbf={},r={},l={};
  int n= 0;
  long ms= cpums();
  for(int i=0; i<N; i++) {
    if(0){
    typeof(concat) *_concat;  _concat= findfunc("concat")->f;
    typeof(left) *_left;  _left= findfunc("left")->f;
    typeof(right) *_right; _right= findfunc("right")->f;
    if (_concat) n++;
    if (_left) n++;
    if (_right) n++;
    }
    setnum(&n4, 4);
    setnum(&n5, 5);
    setnum(&n6, 6);

    setstr(&foo, "foo");
    setstr(&bar, "bar");
    setstr(&fie, "fie");
    concat(&fbf, 3, (void*)&(val[]){foo,bar,fie});
    right(&r, 2, &fbf, &n6);
    left(&l, 2, &r, &n4);
    n+= 1;

    if (1) {
      clearval(&n4);
      clearval(&n5);
      clearval(&n6);
      clearval(&foo);
      clearval(&bar);
      clearval(&fbf);
      clearval(&r);
      clearval(&l);
    }
  }
  ms= cpums()-ms;
  int nn= N*3;
  float calls= (0.0+nn)/ms/1000;
  printf("nn=%d Mops %.2f M/s in %ld ms\n", nn/1000000, calls, ms);
  printf("n=%d\n", n);
  exit(0);
}


void print_at_error(char* msg, char* param) {
  printf("\nquery:\n%s\n", query);
  for(int i=0; i< ps-query; i++) putchar(' '); printf("|------>\n");
  if (msg) printf("\nWANTED(?): %s\n", msg);
  if (param) printf("\t%s\n", param);
  printf("\nAT: '%s'\n\n", ps);
}

void bang() {
  printf("%% SIGFAULT, or something...\n\n");
  install_signalhandlers(bang);
  
  if (interactive) {
    printf("%% Stacktrace? (Y/n/d/q) >"); fflush(stdout);
    char key= tolower(getchar());
    switch(key) {
    case 'n': break;
    case 'q': exit(77); break;
    case 'd': debugger(); break;
    case 'y':
    default:  print_stacktrace(); break;
    }
    process_file(stdin, 1);
  }
  //else exit(77);
}

int main(int argc, char** argv) {
  // --- DO NOT change this!
  //
  // this code is written under these
  // assumptions!
  assert(sizeof(long)==8);
  assert(sizeof(double)==8);
  // pointers on x64 linux is 51 bits
  // - https://stackoverflow.com/questions/9249619/is-the-pointer-guaranteed-to-be-a-certain-value
  
  // WRONG! on android it's negative
  // hibyte == b400000....
  {
    // stack
    long lomem= 1l << (10*3); // 1 GB
    long himem= 1l << (48+3); // 2 PB !
    long mask= ~0xff00000000000000l;
    long a= ((long)&argc) & mask;
    if (debug) printf("stack= %16p %ld\n", &argc,a);
    assert(labs(a) > lomem);
    assert(labs(a) < himem);

    // heap
    char* foo= strdup("bar");
    long f= ((long)foo) & mask;
    if (debug) printf(" heap= %16p %ld\n", foo,f);
    if (debug) printf(" lome= %16lx %ld\n", lomem,lomem);
    assert(labs(f) > lomem);
    assert(labs(f) < himem);
    // lowest 3 bits are 0 (align 8)
    assert((((long)foo) & 0x07)==0);
    assert((((long)foo) & 0x0f)==0); // 16

    free(foo);
  }
  // END assumptions



  // carry on!
  print_exit_info= print_at_error;
  
  // crash! (test to catch)
  //char* null= NULL; *null= 42;
  
  register_funcs();
  
  //speedtest();
  // testread(); exit(0);
  
  // -- main stuff
  char* arg0= *argv;
 
  int n=1;
  while (*((argv+=n)) && (argc+=n)>0)
    n= process_arg(argv[0], argv[1]);

  if (!batch) install_signalhandlers(bang);
  if (interactive) process_file(stdin, 1);

  return 0;
}
