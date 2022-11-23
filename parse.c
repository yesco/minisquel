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

char* OLnames= NULL;

#define OLcmp(f, a, b) (printf("OL \"%s\" %d %d 0\n", f, (int)a, (int)b))

#define OL3(f, a, b) (printf("OL : -666\nOL %s -%d %d %d 0\n", f, nextvarnum, (int)a, (int)b), nextvarnum++)

#define OL4(f, a, b, c) (printf("OL : -666\nOL %s -%d %d %d %d 0\n", f, nextvarnum, (int)a, (int)b, (int)c), nextvarnum++)

#define ROLcmp(f, a, b) return OLcmp(f, a, b)

int varfind(char* table, char* col) {
  val* vp= findvar(table, col);
  //assert(vp);
  return vp ? vp->d : 0;
}

int defvar(char* table, char* col, int d) {
  // TODO: leak
  int old= nextvarnum;
  val v= {};
  d= d ? d : nextvarnum++;
  setnum(&v, d);
  setvar(table, col, &v);
  // allocate?
  if (nextvarnum!=old) {
    if (debug) fprintf(stderr, "@%d", nextvarnum);
    if (debug) fprintf(stderr, "\t{OL@%d %s %s}\n", d, table, col);
    printf("OL : -666\n");
  } else {
    if (debug) fprintf(stderr, "\t{OL@%d %s %s}\n", d, table, col);
  }
  if (0){
  if (varfind(table, col)==d)
    printf("SAMEAVAR %d\n", d);
  else
    printf("BADVAR\n");
  }
  return d;
}

int defstr(char* s) {
  int n= varfind("$const", s);
  if (n) return n;
  
  if (debug) fprintf(stderr, "@%d ", nextvarnum);
  defvar("$const", s, nextvarnum);
  printf("OL : ");
  if (s && *s)
    fprintquoted(stdout, s, -1, '"', 0);
  else
    printf("''");
  printf("\n");
  return nextvarnum++;
}

int defnum(double d) {
  char name[NAMELEN]= {};
  snprintf(name, NAMELEN, "%.15lg", d);

  int n= varfind("$const", name);
  if (n) return n;

  if (debug) fprintf(stderr, "@%d ", nextvarnum);
  defvar("$const", name, nextvarnum);
  return (printf("OL %s %s\n", ":", name), nextvarnum++);
}

int defint(long d) {
  return defnum(d);
}

int findOLname(char olname[NAMELEN], char* name) {
  // --- look up ObjectLog name
  snprintf(olname, NAMELEN, "\t%s", name);
  char* s= strcasestr(OLnames, olname);
  char* p= s;
  if (!p) {
    // assume it's an UDF!
    snprintf(olname, NAMELEN, "@%s", name);
    return 1;
    // TODO: verify "OL" UDF
    return 0;
  }
  while(*p!='\n' && p>OLnames) p--;
  strncpy(olname, p+1, s-p);
  olname[s-p-1]= 0;
  return 1;
}

// -- Options

int batch= 0, force= 0, browse= 0;
int verbose= 0, interactive= 1;

int globalecho= 1, echo= 1;

char globalformat[30]= {0};
char format[30]= {0};

//////////////////////////////
// Parser
// ======

// In general, parsing functions that
// generate ObjectLog will output OL
// predicates as side-effect, while
// returning an int representing the
// variable number where the result
// was stored. This represents the value
// and is used by the caller when
// generating predicates to refer to it.
//
// A negative number means operation is
// setting the value. Any usage is pos.

// Example (in principle):
//
// 33 + 44 * 55
//
//  : -1 33
//  : -2 44
//  : -3 55
//  * -4 2 3
//  + -5 1 4

// global flag: skip some evals/sideeffects as printing during a "parse"/skip-only phase... (hack)
// TODO: change to eparse_num(disable_call, disable_print, print_header)
//   to handle header name def/print
//   to handle aggregates (not print every row, only last (in group))
int parse_only= 0;

// ps points at parse position into query
char *query=NULL, *ps= NULL;

// not returning variable;
int parse(char* s) {
  // TODO: what about user vars!!!
  varcount= 0;

  query= ps= s;
  nextvarnum= 1;
  
  defstr(""); // 1
  defnum(0);  // 2
  defnum(1);  // 3
  defvar("$system","$header", 0); // 4
  int q= defstr(query);
  //printf("Q==%d\n", q);
  assert(q==5);
  defvar("$system", "sql", q); // 5
  assert(nextvarnum==6);

  return q;
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
  int alnum= 1;
  while(*p && *s && tolower(*p) == tolower(*s)) {
    alnum &= isalnum(*s);
    p++; s++;
  }
  if (*s) return 0;
  // got("f") not match "foo"
  if (alnum && isid(*p)) return 0;
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
int parse_str() {
  spcs();

  char* start= ps;
  if (debug>2) printf("--- PARSE_STR\n");

  char q= gotcs("\"'");
  if (!q) return 0; // fail
  char* s= ps;
  while(!end()) {
    if (*ps==q && *++ps!=q) break; // 'foo''bar'
    if (*ps=='\\') ps++; // 'foo\'bar'
    ps++;
  }
  ps--;
  if (*ps!=q) return 0; // fail

  // work on a copy "unquote"
  char* a= strndup(s, ps-s);
  s= a;
  ps++;
  // fix quoted \' and 'foo''bar'
  while(*a) {
    if (*a==q || *a=='\\')
      strcpy(a, a+1);
    a++;
  }

  int n= defstr(s);
  free(s);
  return n;
}

int parse_into_str(char name[NAMELEN]) {
  spcs();
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

// macros

#define EXPECTMSG(f, msg) ({typeof(f) v= f; if (!v) expected(msg); v;})
#define EXPECT(f) EXPECTMSG(f, #f)

#define OR(a, b) ({typeof(a) o= a; if (!o) o= b; o; })

#define IFRETURN(x) ({typeof(x) _x= x; if (_x) return _x;})


int expr();

int call(char* name) {
  #define MAXPARAMS 10

  char olname[NAMELEN]= {};
  if (!findOLname(olname, name)) {
    ps-= strlen(name)+1;
    expected2("Unknown func()", name);
  }
 
  if (debug) fprintf(stderr, "Call.OLNAME '%s' ... '%s'\n", name, olname);

  // -- get parameters
  val r= {};
  int params[MAXPARAMS]= {0};
  int pcount= 0;
  if (debug && !parse_only) printf("\n---CALL: %s\n", name);
  while(!gotc(')')) {
    int v= EXPECT(expr());

    params[pcount++]= v;
    if (debug && !parse_only) printf("\tARG %d: %d\n", pcount, v);

    spcs();
    if (*ps!=')') EXPECT(gotc(','));
  } 

  // output '' (null)
  //  we don't know return type
  printf("OL : \'\'\n");
  printf("OL %s %s -%d",
    olname[0]=='@' ? "call" : "",
    olname, nextvarnum);
  for(int i=0; i<pcount; i++)
    printf(" %d", params[i]);
  printf(" 0\n");
  return nextvarnum++;

  #undef MAXPARAMS
}

// parse var name and get varnum
// if not existing, create and assign
int var() {
  char name[NAMELEN]= {};
  // TODO:? "variables) in prepared stmsts
  if (getname(name)) {
    val v= {};
    if (gotc('(')) return call(name);

    // split into table . name
    char* dot= strchr(name, '.');
    if (dot) *dot= 0;
    char* column= dot?dot+1:name;
    char* table= dot?name:NULL;

    // find or define var
    int n= varfind(table, column);
    return n?n:defvar(table, name, 0);
  }
  return 0;
}

int prim() {
  int v= 0;
  spcs();
  if (gotc('(')) {
    v= EXPECT(expr());
    spcs();
    EXPECT(gotc(')'));
    return v;
  }

  IFRETURN(OR(parse_num(), parse_str()));

  // only if has name
  if (isid(*ps)) return var();
  return 0;
}

int mult() {
  int v= prim();
  if (!v) return 0;
  char op[2]= {0, 0};
  while ((op[0]= gotcs("*/%^"))) {
    int vv= EXPECT(prim());
    v= OL3(op, v, vv);
  }
  return v;
}

int expr() {
  spcs();
  int neg= gotc('-');
  int v= mult();
  if (neg) {
    int n0= defnum(0);
    v= OL3("-", n0, v);
  }

  char op[2]= {0, 0};
  while ((op[0]= gotcs("+-"))) {
    int vv= EXPECT(mult());
    v= OL3(op, v, vv);
  }
  return v;
}

char* select_exprs_and_header(char* e, int dodef) {
  if (!e) return NULL;
  char* old_ps= ps;
  ps= e;
  
  int col= 0;
  val v= {};
  int more= 0;

  // TODO: magic constants...
  char head[10240]= {0};
  int ol[100]= {0};
  int oln= 0;
  
  if (!dodef) parse_only= 1;
  do {
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
		// TODO: quote...
		strcat(head, " '");
		if (t) {
		  strcat(head, t);
		  strcat(head, ".");
		}
		strcat(head, varnames[i]);
		strcat(head, "'");

		ol[oln++]= d;
	      }
	    }
      }
      spcs(); more= gotc(',');
    } else {
      int d= EXPECT(expr());

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

      strcat(head, " '");
      strcat(head, name);
      strcat(head, "'");
      ol[oln++]= d;
    }

    clearval(&v);
  } while(more);
  if (!dodef) parse_only= 0;

  if (dodef) {
    printf("OL out");
    for(int i=0; i<=oln; i++)
      printf(" %d", ol[i]);
    printf("\n");

    printf("OL :: %s 0\n", head);
  }

  e= ps;
  ps= old_ps;
  // return pos after select ...
  return e;
}

int comparator(char cmp[NAMELEN]) {
  spcs();
  char* start= ps;
  // TODO: [not] BETWEEN ... AND
  // TODO: IS [not]
  if (got("like") || got("ilike") || got("in") || got("==") || got("<>") || got("!=") || got("<=") || got(">=") || got("!<") || got("!>") || gotcs("=<>")) {
    strncpy(cmp, start, ps-start);
    cmp[ps-start]= 0;
    // got eats trailing spaces...
    rtrim(cmp);
    spcs();
    return 1;
  }
  
  expected("Operator: LIKE ILIKE IN < <= = != <> > >=");
  return 0;
}

#define TWO(a, b) (a+16*b)
#define CASE(s) case TWO(s[0], s[1])

int comparison() {
  char op[NAMELEN]= {};
  int a= expr();
  if (!a) return 0;
  EXPECT(comparator(op));
  int b= EXPECT(expr());
  ROLcmp(op, a, b);
}

int logical();

int logsimple() {
  // TODO: WHERE (1)=1    lol
  // ==extended expr & use val?
  if (gotc('(')) {
    int r= EXPECT(logical());
    EXPECT(gotc(')'));
    return r;
  }
  if (got("not")) {
    int r= EXPECT(logical());
    error("NOT not implemented in ObjectLog");
    return 0;
  }
  return comparison();
}

int logical() {
  spcs();
  int n;
  while((n= logsimple())) {
    if (got("and")) {
      continue;
    } else if (!got("or")) {
      return n;
    } else {
      // OR
      error("OR not implemented in ObjectLog");
      while((n= logsimple())) {
	printf("OL or\n");
	if (!got("or")) break;
      }
      if (!got("and")) return n;
    }
  }
  return n;
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
    error("ORDER BY not implemented in ObjectLog");
    // will order on groups already generated... valuies order by might be aggregate
    // TODO: or column name...
    //   now -4 also works as "ASC" lol
    int db= EXPECTMSG(parse_num(), "order by column number");
    int col= db;
    got("ASC") || got("ASCENDING");
    if (got("DESC") || got("DESCENDING"))
      col= -col;
    // TODO: generate OL

    // TODO: several columns...
    //if (debug) printf("ORDER BY %d\n", col);
  }

  select_exprs_and_header(selexpr, 1);
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
  char name[NAMELEN]= {};
  double start= 0, stop= 0, step= 0;
  // TODO: generalize, use functions?
  if (gotc('(')) {
    start= EXPECT(expr());
    spcs();
    EXPECT(gotc(','));
    stop= EXPECT(expr());
    spcs();
    if (gotc(',')) {
      step= EXPECT(expr());
      spcs();
    }
    EXPECT(gotc(')'));
    spcs();
    expectname(name, NULL);
  } else expected("(");

  int n= step
    ? OL4("d", start, stop, step)
    : OL3("i", start, stop);
  defvar("num", name, n);

  next_from_list(selexpr);
  return n;
}

#include "index.c"

// header, if given, is free:d
int TABCSV(FILE* f, char* table, int t, char* header, int fn, char* selexpr) {
  int ol[64]= {};
  int oln= 0;

  char* line= header? header: csvgetline(f, ',');
  char delim= decidedelim(line);

  char* p= line;
  char name[NAMELEN]= {};
  int r;
  while((r= sreadCSV(&p, name, NAMELEN, NULL, delim))) {
    ol[oln++]= defvar(table, name, 0);
  }
  free(line);

  printf("OL l %d", t);
  for(int i=0; i<= oln; i++)
    printf(" %d", -ol[i]);
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
    EXPECTMSG(gotc(','), "colname list");
  }
  return strndup(start, ps-start-1);
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

int from_list(char* selexpr, int is_join) {
  char* backtrack= ps;

  // filename.csv or "filename.csv"
  char spec[NAMELEN]= {0};
  expectsymbol(spec, "from-iterator");

  // dispatch to named iterator
  if (0==strcmp("$view", spec)) {

    char table[NAMELEN]= {0};
    expectname(table, "table alias name");
    VIEW(table, selexpr);

  } else if (0==strcmp("int", spec) || 0==strcmp("num", spec)) {

    INT(selexpr);

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
      EXPECT(got("on"));
      expectname(joincol, "join column name");
      // TODO: get "last" table here?
      joinval= findvar(NULL, joincol);
    }
    
    int fn= defstr(spec);
    int t= defvar("$table", table, 0);
    printf("OL F -%d %d 0\n", t, fn);
    
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

int sqlselect() {
  int oldvarcount= varcount;

  strcpy(format, globalformat);
  if (got("format")) EXPECT(getname(format));
  if (!got("select")) return 0;
  spcs();
  
  // TODO: little confusing (expr will
  // be used to call select_+expr again
  char* expr= ps;

  // first we "skip over" since these expr
  // only should be evaluationed once from
  // where etc are finished.
  parse_only= 1;
  char* end= select_exprs_and_header(expr, 0);
  if (end) ps= end;

  from(expr);

  // TODO: clean vars? leak?
  varcount= oldvarcount;

  return 1;
}


int sqlcreateview() {
  return 1;
}


// CREATE FUNCTION sum(a,b) RETURN a+b;
int create_function() {
  int oldvarcount= varcount;

  char fname[NAMELEN]= {};
  expectname(fname, "function name");
  EXPECT(gotc('('));
  int ol[10]= {};
  int oln= 0;
  assert(nextvarnum==6); // TODO:
  do {
    char name[NAMELEN]= {};
    expectname(name, "parameter name");
    ol[oln++]= defvar(fname, name, 0);
    spcs();
  } while(gotc(','));
  EXPECT(gotc(')'));
  EXPECT(got("as"));
  // TODO: out only select-result
  //   potential mismatch copy back...
  // write: in, in, ..., out, out, out
  EXPECT(sqlselect());
  printf("OL ::: '%s'\n", fname);

  // TODO: cleanup?
  varcount= oldvarcount;
  return 1;
}

// TODO: make resident in mem! LOL
int create_index() {
  char name[NAMELEN]= {0};
  char table[NAMELEN]= {0};
  char col[NAMELEN]= {0};

  expectname(name, "index name");
  EXPECT(got("on"));
  expectsymbol(table, "table namex");
  EXPECT(gotc('('));
  // TODO: computed expressions?
  // or ... AS SELECT ...
  expectname(col, "column name");
  EXPECT(gotc(')'));
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

  // parameters (a,b,c)
  char* params= NULL;
  if (gotc('(')) {
    params= params; // TOOD: lol
    while(!gotc(')'));
    if (!*ps) expected(")");
  }

  EXPECT(got("as"));
  spcs();

  char* impl= ps;
  //regobj("view", name, params, impl, got("select"));
  return 1;
}

int sqlcreate() {
  if (!got("create")) return 0;
  int (*creator)()= NULL;
  
  if (got("index")) return create_index();
  if (got("view"))  return create_view();
  if (got("function")) return create_function();
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
  EXPECT(gotc('='));
  int d= EXPECT(expr());
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
  if (!*cmd) return;
  
  mallocsreset();

  parse(cmd);
  int r= sql();
  
  fprintmallocs(stdout);

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

void print_at_error(char* msg, char* param) {
  fprintf(stderr, "\nquery:\n%s\n", query);
  fprintf(stderr, "%*s|------>\n", (int)(ps-query), "");
  if (msg) fprintf(stderr, "\nEXPECTED: %s\n", msg);
  if (param) fprintf(stderr, "\t%s\n", param);
  fprintf(stderr, "\nAT: '%s'\n\n", ps);
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
  assert(sizeof(void*)==8);
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

  OLnames= readfile("objectlog.txt");

  // carry on!
  print_exit_info= print_at_error;
  
  // -- main stuff
  char* arg0= *argv;
 
  int n=1;
  while (*((argv+=n)) && (argc+=n)>0)
    n= process_arg(argv[0], argv[1]);

  if (!batch) install_signalhandlers(bang);
  if (interactive) process_file(stdin, 1);

  return 0;
}
