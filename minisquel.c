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

// stats
long lineno= -2, readrows= 0, nfiles= 0;

#define NAMELEN 64
#define VARCOUNT 256
#define MAXCOLS 32


#include "utils.c"

// global flag: skip some evals/sideeffects as printing during a "parse"/skip-only phase... (hack)
// TODO: change to eparse_num(disable_call, disable_print, print_header)
//   to handle header name def/print
//   to handle aggregates (not print every row, only last (in group))
int parse_only= 0;

char *query=NULL, *ps= NULL;
long foffset= 0;

// TODO: generalize output formats:
//
//   maybe use external formatter?
//     popen("| formacmd");
//
//   - sqlite3(ascii box column csv html insert json line list markdown qbox quote table tabs tcl)
//   - DuckDB(ascii, box, column, csv, (no)header, html, json, line, list, markdown, newline SEP, quote, separator SEP, table)

// -- Options

int batch= 0, force= 0, browse= 0;
int verbose= 0, interactive= 1;

int globalecho= 1, echo= 1;

char globalformat[30]= {0};
char format[30]= {0};


char formatdelim() {
  if (!*format) return '\t';
  if (0==strcmp("csv", format)) return ',';
  if (0==strcmp("tab", format)) return '\t';
  if (0==strcmp("bar", format)) return '|';
  return '\t';
}

int parse(char* s) {
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
  while(gotcs(" \t\n\t")) n++;;
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

int parse_num(double* d) {
  spcs();
  char* end= NULL;
  *d= hstrtod(ps, &end);
  if (end<=ps) return 0;
  ps= end;
  return 1;
}

// mallocates string to be free:d
// if it returns true
int parse_str(char** s) {
  spcs();
  char q= gotcs("\"'");
  if (!q) return 0;
  *s= ps;
  while(!end()) {
    if (*ps==q && *++ps!=q) break; // 'foo''bar'
    if (*ps=='\\') ps++; // 'foo\'bar'
    ps++;
  }
  ps--;
  if (*ps!=q) return 0;
  char* a= strndup(*s, ps-*s);
  *s= a;
  ps++;
  // fix quoted \' and 'foo''bar'
  while(*a) {
    if (*a==q || *a=='\\')
      strcpy(a, a+1);
    a++;
  }
  return 1;
}

#include "csv.c"
#include "vals.c"

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
  char* s= NULL;
  spcs();
  if (*ps=='"' && parse_str(&s)) { // "foo bar"
    strncpy(name, s, NAMELEN);
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
  if (s) free(s);
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

int expr(val* v);

// TODO: use to call "sql" with params?
// EXEC updateAddress @city = 'Calgary'
// TODO:
//   updateAddress(@city=5, @foo=7);
//

// 
int call(val* r, char* name) {
  #define MAXPARAMS 10
  val params[MAXPARAMS]= {0};
  int pcount= 0;
  func* f= findfunc(name);
  if (!f) expected2("not found func", name);
  while(!gotc(')')) {
    if (!expr(&params[pcount++])) expected("expression");
    spcs();
    if (*ps!=')' && !gotc(',')) expected("comma");
  } 

  int rv= 0;
  if (!parse_only) {
    // caller cleans up in C, so this is safe!
    rv= ((int(*)(val*,int,val[],val*,val*))f->f)(r, pcount, params, params+1, params+2);

  }

  // cleanup
  for(int i=0; i<pcount; i++)
    clearval(&params[i]);

  // done
  if (parse_only || rv>0) return 1;

  // - wrong set of parameters
  char msg[NAMELEN];
  sprintf(msg, "%d parameters, got %d", -rv, pcount);
  expected2(name, msg);
  return 0;

  #undef MAXPARAMS
}

// parse var name and get value
// WARNING: if not found=>NULL
// and always return true
int var(val* v) {
  char name[NAMELEN]= {};
  // TODO:? "variables) in prepared stmsts
  if (getname(name)) {
    if (gotc('(')) return call(v, name);
    char* dot= strchr(name, '.');
    if (dot) *dot= 0;
    char* column= dot?dot+1:name;
    char* table= dot?name:NULL;
    getval(table, column, v);
    return 1;
  }
  // TODO: maybe not needed here?
  // not found == null
  ZERO(*v);
  return 1; // "NULL" "hmm"
}

int prim(val* v) {
  spcs();
  char* s= NULL;
  if (gotc('(')) {
    if (!expr(v)) expected("expr");
    spcs();
    if (!gotc(')')) expected("')'");
    return 1;
  }
  if (parse_num(&v->d)) { v->not_null= 1; return 1; }
  if (parse_str(&s)) { setstrfree(v, s); return 1; }
  // only if has name
  if (isid(*ps) && var(v)) return 1;
  return 0;
}

int mult(val* v) {
  if (!prim(v)) return 0;
  char op;
  while ((op= gotcs("*/%"))) {
    val vv= {};
    if (prim(&vv)) {
      // TODO: type checking?
      if (op == '*') {
	v->d= v->d * vv.d;
      } else if (op == '/') {
	v->d= v->d / vv.d;
      } else if (op == '%') {
	v->d= ((long)v->d) % ((long)vv.d);
      } else error("Undefined operator!");
      v->not_null &= vv.not_null;
    }
  }
  return 1;
}

int expr(val* v) {
  spcs();
  ZERO(*v);
  int neg= gotc('-')? -1 : +1;
  if (!mult(v)) return 0;
  v->d= neg * v->d;
  char op;
  while ((op= gotcs("+-"))) {
    val vv= {};
    if (mult(&vv)) {
      // TODO: type checking?
      if (op == '+') {
	v->d= v->d + vv.d;
      } else if (op == '-') {
	v->d= v->d - vv.d;
      }
      v->not_null &= vv.not_null;
    }
  }
  return 1;
}

// TODO: pass around dbval
#include "dbval.c"
#include "table.c"

table* results= NULL;

void do_result_action(char* name, val* v, long row, int col) {
  if (debug>1) {
    printf("\nRESULT: '%s' ", name); printval(v, '"', 0); printf(" %ld, %d  ", row, col);
    val dummy= {.not_null=1, .s= name};
    dbval dbv= val2dbval(v?v:&dummy);
    printf(">>>"); dbprinth(dbv, 0, 1);
    printf("<<< ps=>>%s<", ps);
    putchar('\n');
  }

  if (results) {
    if (col > results->cols) results->cols= col;
    if (col==1 && v) results->count++;

    val dummy= {.not_null=1, .s= name};
    dbval dbv= val2tdbval(results, v?v:&dummy);
    if (debug>1) {
      printf("===> "); tdbprinth(results, dbv, 8, 1); putchar('\n');
    }
    addarena(results->data, &dbv, sizeof(dbv));
  }
}

void (*result_action)(char* name, val* v, long row, int col)= do_result_action;
  

void print_colname(char* name, int col, char* start, char* ps, int delim) {
  if (!name[0]) {
    // make a name from expr
    if (delim!='\t' || delim<=0) {
      // use whole
      int n= ps-start;
      if (n>=NAMELEN-1) n= NAMELEN;
      strncpy(name, start, n);
    } else {
      // truncate - make 7 char wide
      size_t end= strcspn(start, " ,;");
      if (!end) end= 20;
      strncpy(name, start, end);
      if (end>7) strcpy(name+5, "..");
    }
    rtrim(name);
  }

  if (parse_only<0)
    fprintquoted(stdout, name, 7, abs(delim)==','?'\"':0, delim);
  if (result_action && parse_only==-1)
    result_action(name, NULL, lineno+1, col);
}

char* print_header(char* e) {
  char* old_ps= ps;
  ps= e;
  
  int delim= formatdelim();
  int col= 0;
  val v= {};
  int more= 0;
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
	if (like(varname, name, 0)) {
	  if (parse_only<0 && col) putchar(abs(delim));
	  col++;
	  print_colname(varname, col, NULL, NULL, delim);
	}
      }
      spcs(); more= gotc(',');
    } else if (expr(&v)) {
      // select 42 AS foo
      if (got("as")) expectsymbol(name, "alias name");
      spcs(); more= gotc(',');
      if (parse_only<0 && col) putchar(abs(delim));
      col++;
      print_colname(name, col, start, ps-(more?1:0), more?delim:-delim);
    } else expected("expression");

    clearval(&v);
  } while(more);
  if (parse_only<0) putchar('\n');

  // To distinguish if print/noprint
  lineno++; // haha!

  // pretty underline header
  if (parse_only<0) {
    if (abs(delim)=='|')
      printf("==============\n");
    else if (abs(delim)=='\t') {
      for(int i=col; i; i--)
	printf("======= ");
      putchar('\n');
    }
  }

  e= ps;
  ps= old_ps;
  return e;
}

// returns end pointer so can skip!
char* print_expr_list(char* e) {
  //if (parse_only) return print_header(e);
  if (lineno<0) {
    int saved= parse_only;
    if (!parse_only) parse_only= -1;
    char* x= print_header(e);
    parse_only= saved;
    if (lineno<0) return x;
  }

  char* old_ps= ps;
  ps= e;
  
  int delim= formatdelim();
  spcs();
  int col= 0;
  val v= {};
  int more= 0;

  int doprint= !browse;
  int printprogress= browse;
  do {
    spcs();
    char* start= ps;
    char name[NAMELEN]= {0};

    // [foo.*]
    char q;
    if ((q= gotcs("[*"))) {
      ps--;
      expectsymbol(name, "[] column name");
      // search names in defined order
      char varname[NAMELEN]= {};
      for(int i=0; i<varcount; i++) {
	// format name to test
	char* t= tablenames[i];
	snprintf(varname, sizeof(name), "%s.%s", t?t:"", varnames[i]);
	if (like(varname, name, 0)) {
	  getval(t, varnames[i], &v);
	  // print it
	  if (doprint && col) putchar(abs(delim));
	  col++;
	  // TODO: Don't truncate if last col!
	  // difficult to tell if will find more...
	  if (doprint)
	    printval(&v, delim==','?'\"':0, delim);
	  if (result_action)
	    result_action(NULL, &v, lineno+1, col);
	}
      }
      spcs(); more= gotc(',');
    } else if (expr(&v)) {
      // select 42 AS foo
      if (got("as")) {
	expectsymbol(name, NULL);
	setvar(NULL, name, &v);
      }

      // print it
      if (col) putchar(abs(delim));
      col++;
      // TODO: Don't truncate if last col!
      spcs(); more= gotc(',');
      if (doprint)
	printval(&v, delim==','?'\"':0, more?delim:-delim);
      if (result_action)
	result_action(NULL, &v, lineno+1, col);
    } else expected("expression");

    clearval(&v);
    if (printprogress && lineno%100==0)
      fprintf(stderr, "\r%ld rows produced\r", lineno);
  } while(more);
  if (doprint) putchar('\n');
  
  lineno++;
  
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

int dcmp(char* cmp, double a, double b) {
  // relative difference
  int eq= (a==b) || (fabs((a-b)/(fabs(a)+fabs(b))) < 1e-9);
  
  switch (TWO(cmp[0], cmp[1])) {
  case TWO('i', 'n'): expected("not implemented in");
    // lol

  case TWO('i','l'):  // ilike, like
  case TWO('l','i'):
  case TWO('=','='): return a==b;

  case '=':
  case TWO('~','='): return eq;

  case TWO('<','>'):
  case TWO('!','='): return !eq;

  case TWO('!','>'):
  case TWO('<','='): return (a<=b) || eq;
  case '<': return (a<b) && !eq;

  case TWO('!','<'):
  case TWO('>','='): return (a>=b) || eq;
  case '>': return (a>b) && !eq;
  default: expected("dcmp: comparator");
  }
  return 0;
}

int scmp(char* cmp, char* a, char* b) {
  // relative difference
  int r= strcmp(a, b);
  
  switch (TWO(cmp[0], cmp[1])) {
  case TWO('i', 'n'): expected("not implemented in");

  case TWO('i','l'): return like(a, b, 1);
  case TWO('l','i'): return like(a, b, 0);
    
  case TWO('~','='): return !strcasecmp(a, b);

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
  default: return 0; //expected("scmp: comparator");
  }
}

// returns "extended boolean"
#define LFALSE (-1)
#define LFAIL 0
#define LTRUE 1
// NOTE: -LFALSE==LTRUE !!

int comparison() {
  val a, b; char op[NAMELEN]= {};
  if (!(expr(&a))) return LFAIL;
  if (!comparator(op) || !expr(&b))
    expected("comparison");

  int r= LFALSE;
  if (!a.not_null || !b.not_null)
    r= LFALSE;
  if (a.s && b.s) {
    r= scmp(op, a.s, b.s)?LTRUE:LFALSE;
  } else if (!a.s && !b.s)
    r= dcmp(op, a.d, b.d)?LTRUE:LFALSE;

  clearval(&a); clearval(&b);
  return r;
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
  if (got("not")) return -logsimple();
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
	if (!got("or")) break;
      }
      if (!got("and")) return r;
    }
  }
  return r;
}

int after_where(char* selexpr) {
  // TODO: only call after LAST where!
  //   (now called every time... lol)
  parse_only= 0;

  // select * from groups;
  //   a,b,c
  //   1,2,3
  //   1,2,6
  //   2,2,7
  //   2,2,9
  //   1,3,3
  // CREATE FUNCTION groups AS VALUES (1,2,3), (1,2,6);, (2,2,7), (2,2,9), (1,3,3);
  
  // select 1000+sum(a) as col1,b,sum(c) from groups group by 2
  
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
    if (!parse_num(&d)) expected("order by COL");
    int col= d;
    got("ASC") || got("ASCENDING");
    if (got("DESC") || got("DESCENDING"))
      col= -col;
    // We're OK, before generating rows
    if (!results) results= newtable("result", 0, 0, NULL);
    results->sort_col= col;
    // TODO: several columns...
    //if (debug) printf("ORDER BY %d\n", col);
  }

  if (selexpr) return !!print_expr_list(selexpr);
  return 1;
}

int where(char* selexpr) {
  // ref - https://www.sqlite.org/lang_expr.html
  val v= {};
  char* saved= ps;
  if (got("where")) {
    int r= logical();
    //printf("WHERE => %d\n", r);
    v.not_null= (r==LTRUE);
  } else {
    v.not_null= 1;
  }
  
  if (v.not_null) {
    parse_only= 0;
    after_where(selexpr);
  }

  ps= saved;
  
  return 1;
}



// TOOD: print where?
void printstats() {
  printf("----\n");
  printf("Stats\n");
  for(int i=0; i<varcount; i++) {
    val* v= varvals[i];
    // TODO: string values (nstr)
    // TODO: nulls (nnull)
    if (v->n) {
      char* t= tablenames[i];
      printf("  %s.%s %s%s %d#[%lg,%lg] u(%lg,%lg) S%lg\n",
        t?t:"", varnames[i], !v->not_desc?"DESC":"", !v->not_asc?"ASC":"",
        v->n, v->min, v->max, stats_avg(v), stats_stddev(v),
        v->sum
      );
    }
  }
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
		   
// - db objects
// TODO: generalize/hash from registerfun
typedef struct dbobj {
  struct dbobj* next;
  char* type;
  char* name;
  char* params;
  char* impl;
  int select;
} dbobj;
  
dbobj* objs= NULL;

void regobj(char* type, char* name, char* params, char* impl, int select) {
  dbobj* next= objs;
  objs= calloc(1, sizeof(dbobj));
  objs->next= next;

  objs->type= strdup(type); // not needed?
  objs->name= strdup(name);
  objs->params= params?strdup(params):NULL;
  objs->impl = strdup(impl);
  objs->select= select;
}

dbobj* findobj(char* name) {
  dbobj* o= objs;
  while(o) {
    if (0==strcmp(o->name, name))
      return o;
    o= o->next;
  }
  return NULL;
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

  dbobj* l= objs;
  lineno= -2;
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
  if (gotc('(') && parse_num(&start) && gotc(',')
      && parse_num(&stop) && gotc(')')) {
    stop+= 0.5;
    spcs();
    expectname(name, NULL);
  } else return 0;

  val v= {};
  linkval("int", name, &v);

  char* backtrack= ps;
  for(double i= start; i<stop; i+= step) {
    // handle coded setvar
    // TODO:setnum/setstr?
    setnum(&v, i);

    ps= backtrack;
    next_from_list(selexpr);
  }

  // restore "state"
  // ps= saved;
  varcount= nvars;

  return 1;
}

void hack_foffset(char* col, FILE** dataf, long* foffset, double d) {
  if (!col || strcmp(col, "$foffset")) return;

  // reading index special value
  // TODO: move more generic place
  *foffset= d;
  printf("FOFFSET=%ld\n", *foffset);
  if (!*dataf) *dataf= fopen("foo.csv", "r");
  if (*dataf) fseek(*dataf, *foffset, SEEK_SET);
  if (*dataf) {
    char* ln= NULL;
    size_t l= 0;
    getline(&ln, &l, *dataf);
    printf("\t%s\n", ln);
    if (ln) free(ln);
  }
}

#include "index.c"

typedef void(*action)(memindex* ix, char* table, char* joincol);

void process_result(int col, val* vals, int* row, char* parse_after, char* selexpr,
    action act, memindex* ix,  char* table, char* joincol) {
  if (!col) return;
  ps= parse_after;
  if (act) act(ix, table, joincol);
  // TODO:?
  if (selexpr)
    next_from_list(selexpr);
  // TODO: consider not clearing here
  //   can use current str as last!
  for(int i=0; i<col; i++)
    clearval(&vals[col]);
  (*row)++;
}

keyoffset* vixadd(memindex* ix, val* v, int offset) {
  // TODO: add null?
  //if (!v) return NULL;
  keyoffset* k= NULL;
  if (!v || !v->not_null) // NULL
    k= sixadd(ix, "", offset);
  else if (v->s)
    k= sixadd(ix, v->s, offset);
  else
    k= dixadd(ix, v->d, offset);
  return k;
}

void action_insert_into_index(memindex* ix, char* table, char* joincol) {
  //printf("\t(add %s.%s=", table, joincol);
  val* v= findvar(table, joincol);
  if (!vixadd(ix, v, foffset))
    fprintf(stderr, "\n%% Insert into index failed!\n");
}

// TODO: remove
int jdebug= 0;

int READFILE(FILE* f, char* table, char* header, int(*action)(char* table)) {
  return 0;
}  

// TODO: untangle read/indexing

// TODO: take delimiter as argument?
// TODO: too big!
//
// header, if given, is free:d
int TABCSV(FILE* f, char* table, char* header, int is_join, char* joincol, val* joinval, char* selexpr) {
  int nvars= varcount;

  char* parse_after= ps;
  
  action act= NULL;
  
  // - create index?

// TODO: hack for one index!
static
  memindex* ix= NULL;
static
  int index_complete= 0;
  // TODO: don't create index first time?
  //  (save nmatches, fantout,ms)
  //   1st - scan and analyze join variable
  //   2nd - create index
  // 2,3rd - use index, drop/block
  //            if too many seek / cpums
  // TODO: maybe don't need all cols
  //    maybe any "findvar" can create entry?
  //    then can decide if index covers?
  if (is_join && !index_complete) {
    char ixname[NAMELEN]= {0};
    snprintf(ixname, sizeof(ixname), "index.%s.%s", table, joincol);
    ix= newindex(ixname, 0);
    jdebug= debug;
    if (jdebug) fprintf(stderr, "! CREATE INDEX %s ON %s(%s)\n", ixname, table, joincol);
    act= action_insert_into_index;
  }
  // use index for JOIN!
  if (is_join && ix && index_complete && joinval) {
    // TODO: NULL?
    keyoffset* f= NULL;
    if (joinval->s)
      f= sfindix(ix, joinval->s);
    else
      f= dfindix(ix, joinval->d);
    
    // set column value
    // TODO: "joinval"? ok for eq
    val v= *joinval;

    // TODO;TODO;TODO;TODO;TODO;TODO;

    // TODO: cleanup!!!!
    linkval(table, joincol, &v);

    // TODO: interval
    keyoffset* cur= f;
    // TODO: range
    keyoffset* end= ix->kos + ix->n;
    if (jdebug) printf(">>>>JOIN match\n");
    while(cur && cur<end) {
      if (eqko(cur, f)) {
	if (jdebug) printko(f);

	int row= 0;
	process_result(
          -1, joinval, &row, parse_after, selexpr,
	  // no indexing!
          NULL, NULL, NULL, NULL);
      } else break;
      cur++;
    }
    if (jdebug) printf("<<<<<<\n\n");

    varcount= nvars;
    ;
    return 1;
  }
  
  // --- parse header col names
  char* cols[MAXCOLS]= {0};

  size_t hlen= 0;
  if (!header) {
    getline(&header, &hlen, f);
    if (!header) {
      perror("getline failed ");
      printf("ERRNO %d\n", errno);
    }
  }
  
  // Bad Hack for header: Remove '"'
  // TODO: use the damn CSV reader instead!
  while(1) {
    char* z = strchr(header, '"');
    if (!z) break;
    memcpy(z, z+1, strlen(z));
  }

  char delim= decidedelim(header);
  // TODO: read w freadCSV()
  // extract ','-delimited names
  // reads and modifies headers

  char* h= header;
  int col= 0, row= 0;
  cols[0]= h;
  while(*h && *h!='\n') {
    if (*h==' ') ; // TODO: lol \t?
    else if (*h==delim) {
      while(*h && isspace(h[1])) h++;
      *h= 0;
      cols[++col]= h+1;
    }
    h++;
  }
  *h= 0;

  //if (debug) printf("---TABCSV: %s\n", table);

  // link column as variables
  // TODO: dynamically allocate/use darr
  val vals[MAXCOLS]={0};
  for(int i=0; i<=col; i++){
    //if (debug) printf("%d---TABCSV.col: %s\n", getpid(), cols[i]);
    linkval(table, cols[i], &vals[i]);
  }
  foffset= 0; long fprev= ftell(f);
  FILE* dataf= NULL;

  // TODO: if we know we don't access any
  //   columns (for this table) can we
  //   avoid parsing? just count!
  //   happy.csv: 136ms csvgetline;
  //              450ms with freadCSV !

  double d;
  // TODO: WRONG, fix: too limited! dstr?
  char s[1024]= {0}; 
  int r;
  
  // TODO: use csvgetline !
  col= 0;
  while(1) {
    r= freadCSV(f, s, sizeof(s), &d, delim);
    if (debug>2) printf(" {CSV: %2d '%s' %lg} ", r, s, d);
    if (r==RNEWLINE || !r) {
      readrows++;
      if (debug && readrows%10000==0)
	{ printf("\rTABCSV: row=%ld", readrows); fflush(stdout); }

      // store offset of start of row
      // (ovehead? not measurable)
      foffset= fprev; fprev= ftell(f);

      process_result(
        col, vals, &row, parse_after, selexpr,
        act, ix, table, joincol);

      col= 0;
      if (!r) break; else continue;
    }

    // got new value
    setval(&vals[col], r, d, s);
    hack_foffset(cols[col], &dataf, &foffset, d);
    col++;
  }
  // end of table

  // post index revolution
  if (ix) {
    sortix(ix);
    index_complete= 1;
    if (jdebug || verbose || stats>1) printix(ix, jdebug);

    // TODO: retain somewhere!
    if (0) {
      free(ix); // pretty stupid...
      ix= NULL;
    }
  }
  
  // deallocate values
  // TODO: all??? correct ?
  for(int i=0; i<varcount; i++)
    clearval(varvals[i]);
  
  free(header); // column names
  fclose(f);

  //  if (1) printstats();

  // restore
  varcount= nvars;
  ps= parse_after;

  return 1;
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
      if (jdebug) printf("JOIN ON: %s = ", joincol);
      // TODO: get "last" table here?
      joinval= findvar(NULL, joincol);
      if (jdebug) {
	printval(joinval, 0, 0);
	printf("\n");
      }
    }
    
    FILE* f= magicfile(spec);

    TABCSV(f, table, header, is_join, joincol, joinval, selexpr);

    // TODO: json
    // TODO: xml
    // TODO: passwd styhle "foo:bar:fie"
    // (designate delimiter, use TABCSV)
    
    fclose(f);
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
  char* end= print_expr_list(expr);
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
  TABCSV(f, table, NULL, 1, col, NULL, NULL);
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
  regobj("view", name, params, impl, got("select"));
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
  val v= {0};
  if (!expr(&v)) expected("expression");
  if (debug) {
    printf(" [set '%s' to ", name);
    printval(&v, 0, 0); printf("]\n");
  }
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

  if (browse) {
    if (results) freetable(results);
    results= newtable("result", 0, 0, NULL);
  }

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

  if (results && results->count) {
    // It was Peter Boncz who said in his PhD
    // that: sorting is a function of the UI.
    // So... here we let the "UI" sort.
    if (results->sort_col)
      tablesort(results, results->sort_col, NULL);
    
    // Then launch the UI. Should be default?
    if (browse) {
      browsetable(results);
    } else if (0==strcmp(format, "csv")) {
      error("Not implemented for sorted table yet");
    } else {
      pretty_printtable(results, 0, -1, 1);
      printf("Use --browse to browse interactively\n");
    }
    if (debug) printtable(results, 0);
  }
  if (results) { freetable(results); results= NULL; }

  // TODO: print leftover
  //if (ps && *ps) printf("%%UNPARSED>%s<\n", ps);
  printf("\n");

  // log and time
  sqllog(cmd, "end", NULL, NULL, readrows, lineno-1, ms);
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
    if (results) freetable(results);

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
