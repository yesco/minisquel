#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#define NAMELEN 64
#define VARCOUNT 256
#define MAXCOLS 32

char* ps= NULL;
int lineno= 0;

#define ZERO(z) memset(&z, 0, sizeof(z))

int parse(char* s) {
  ps= s;
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

// also removes starting/trailing spaces
int got(char* s) {
  if (end() || !s) return 0;
  spcs();
  char* p= ps;
  while(*p && *s && *p == *s) {
    p++; s++;
  }
  if (*s) return 0;
  ps= p;
  spcs();
  return 1;
}

int num(double* d) {
  spcs();
  int neg= gotc('-')? -1: 1;
  int i= strspn(ps, "0123456789.");
  if (!i) return 0;
  *d= atof(ps) * neg;
  ps+= i;
  spcs();
  return 1;
}

int str(char** s) {
  spcs();
  if (!gotc('"')) return 0;
  *s= ps;
  // TODO: handle \ quoted chars
  while(!end() && !gotc('"'))
    ps++;
  ps--;
  if (*ps != '"') return 0;
  char* a= strndup(*s, ps-*s);
  *s= a;
  ps++;
  return 1;
}

void error(char* msg) {
  fprintf(stdout, "Error: %s\n", msg);
  fprintf(stderr, "Error: %s\n", msg);
  // TODO: return 1 for shortcut?
  exit(1);
}

void expected(char* msg) {
  fprintf(stdout, "Error: expected %s\n", msg);
  fprintf(stderr, "Error: expected %s\n", msg);
  exit(1);
}

// app
typedef struct val {
  char* s;
  char* dealloc; // if set (to s) deallocate
  double d;
  int not_null;
} val;
  
void print_quoted(char* s, char quot) {
  if (!s) return (void)printf("NULL");
  putchar(quot);
  while(*s) {
    if (*s==quot) printf("\\%c", quot);
    else if (*s=='\\') printf("\\\\");
    else putchar(*s);
    s++;
  }
  putchar(quot);
}

void print_val(val* v) {
  if (!v->not_null) printf("NULL");
  else if (v->s) print_quoted(v->s, '\"');
  else printf("%.15lg", v->d);
}

char* varnames[VARCOUNT]= {0};
val* varvals[VARCOUNT]= {0};
int varcount= 0;

int linkval(char* name, val* v) {
  if (varcount>=VARCOUNT) error("out of vars");
  varnames[varcount]= name;
  varvals[varcount]= v;
  varcount++;
  return 1;
}

int getval(char name[NAMELEN], val* v) {
  // special names
  if (0==strcmp("$lineno", name)) {
    v->d= lineno;
    v->not_null= 1;
    return 1;
  }
  if (0==strcmp("$time", name)) {
    v->d = 42; // TODO: utime?
    v->not_null= 1;
    return 1;
  }
  // lookup variables
  for(int i=0; i<varcount; i++)
    if (0==strcmp(name, varnames[i])) {
      *v= *varvals[i];
      return 1;
    }
  // DO: v->not_null= 1;

  // TODO:

  // failed, null
  ZERO(*v);
  return 0;
}
	   
int getname(char name[NAMELEN]) {
  spcs();
  char *p= &name[0], *last= &name[NAMELEN-1];
  // first char already "verified"
  while(!end() && (p < last) &&
	(isalnum(*ps) || *ps=='_' || *ps=='$'|| *ps=='.')) {
    *p++= *ps;
    ps++;
  }
  return p!=&name[0];
}

int var(val* v) {
  char name[NAMELEN]= {};
  if (getname(name) && getval(name, v)) return 1;
  // not found == null
  ZERO(*v);
  return 1; // "NULL"
}

int expr(val* v);

int prim(val* v) {
  spcs();
  if (gotc('(')) {
    if (!expr(v)) expected("expr");
    spcs();
    if (!gotc(')')) expected("')'");
    return 1;
  }
  if (num(&v->d)) { v->not_null= 1; return 1; }
  if (str(&v->s)) { v->dealloc= v->s; v->not_null= 1; return 1; }
  if (var(v)) return 1;
  return 0;
}

int mult(val* v) {
  if (!prim(v)) return 0;
  char op;
  while ((op= gotcs("*/"))) {
    val vv= {};
    if (prim(&vv)) {
      // TODO: type checking?
      if (op == '*') {
	v->d= v->d * vv.d;
      } else if (op == '/') {
	v->d= v->d / vv.d;
      }
      v->not_null &= vv.not_null;
    }
  }
  return 1;
}

// Aggregators - probably not
// - https://learn.microsoft.com/en-us/sql/t-sql/functions/aggregate-functions-transact-sql?source=recommendations&view=sql-server-ver16

// TODO: functions
// - https://learn.microsoft.com/en-us/sql/t-sql/functions/date-and-time-data-types-and-functions-transact-sql?view=sql-server-2017
// - https://learn.microsoft.com/en-us/sql/t-sql/functions/string-functions-transact-sql?view=sql-server-2017

int expr(val* v) {
  spcs();
  ZERO(*v);
  if (!mult(v)) return 0;
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

int comparator(char cmp[NAMELEN]) {
  spcs();
  // TODO: not prefix?
  if (got("like")) { strcpy(cmp, "like"); return 1; }
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
  return 1;
}

#define TWO(a, b) (a+16*b)

int dcmp(char* cmp, double a, double b) {
  // relative difference
  int eq= (a==b) || (fabs((a-b)/(fabs(a)+fabs(b))) < 1e-9);
  
  switch (TWO(cmp[0], cmp[1])) {
  case TWO('i', 'n'): expected("not implemented in")
    // lol

  case TWO('i','l'):  // ilike
  case TWO('l', 'i'): // like
  case '=':
  case TWO('~','='):
  case TWO('=','='): return eq;

  case TWO('<','>'):
  case TWO('!','='): return !eq;

  case TWO('!','>'):
  case TWO('<','='): return (a<=b) || eq;
  case '<': return (a<b) && !eq;

  case TWO('!','<'):
  case TWO('>','='): return (a>=b) || eq;
  case '>': return (a>b) && !eq;
  default: expected("comparator");
  }
  return 0;
}

int scmp(char* cmp, char* a, char* b) {
  // relative difference
  int r= strcmp(a, b);
  
  switch (TWO(cmp[0], cmp[1])) {
  case TWO('i', 'n'): expected("not implemented in")

  case TWO('i','l'): // ilike
  case TWO('l', 'i'): // like
    return 0; // expected("implement like");
    
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
  default: return 0; //expected("comparator");
  }
}

// v.not_null if true
int comparison() {
  val a, b; char op[NAMELEN]= {};
  // TODO: not and or, priority?
  if (!(expr(&a) && comparator(op) && expr(&b)))
    expected("comparison");

  if (!a.not_null || !b.not_null)
    return 0;
  if (a.s && b.s)
    return scmp(op, a.s, b.s);
  else if (!a.s && !b.s)
    return dcmp(op, a.d, b.d);
  else
    return 0;
}

// returns end pointer
char* print_expr_list(char* e, int do_print) {
  char* old_ps= ps;
  ps= e;
  
  // TODO: alternative formats: csv,tab,TAB
  spcs();
  val v= {};
  do {
    if (expr(&v)) {
      if (do_print) print_val(&v);
    } else expected("expression");
    if (do_print) printf("\t");
  } while(gotc(','));
  printf("\n");
  lineno++;
  
  e= ps;
  ps= old_ps;
  return e;
}

int where(char* selexpr) {
  // ref - https://www.sqlite.org/lang_expr.html
  val v= {};
  if (got("where")) {
    // TODO: logocal expr w and/or
    v.not_null= comparison();
  } else {
    v.not_null= 1;
  }
  
  if (v.not_null)
    print_expr_list(selexpr, 1);

  return 1;
}

int INT(char* selexpr) {
  char name[NAMELEN]= {};
  double start= 0, stop= 0, step= 1;
  // TODO: generalize
  // TODO: make it use expression
  if (gotc('(') && num(&start) && gotc(',')
      && num(&stop) && gotc(')')) {
    stop+= 0.5;
    spcs();
    if (!getname(name)) expected("name");
  } else return 0;

  val v= {};
  linkval(name, &v);
  int old_count= varcount;
  for(double i= start; i<stop; i+= step) {
    v.d= i;
    v.not_null= 1;

    where(selexpr);
  }
  varcount= old_count;
  return 1;
}

int isnewline(int c) {
  return c=='\n' || c=='\n';
}

// TODO: https://en.m.wikipedia.org/wiki/Flat-file_database
// - flat-file
// - ; : TAB (modify below?)

//CSV: 2, 3x, 4, 5y , 6 , 7y => n,s,n,s,n,s
//CSV: "foo", foo, "foo^Mbar"
//CSV: "fooo""bar", "foo\"bar"

// TODO: separator specified in file
//   Sep=^    (excel)
//   - https://en.m.wikipedia.org/wiki/Comma-separated_values

// TODO: mime/csv
//   https://datatracker.ietf.org/doc/html/rfc4180

// TODO: extract cols/fields/rows from URL
//   http://example.com/data.csv#row=4-7
//   http://example.com/data.csv#col=2-*
//   http://example.com/data.csv#cell=4,1
//
//   - https://datatracker.ietf.org/doc/html/rfc7111
// TODO: null?
//   - https://docs.snowflake.com/en/user-guide/data-unload-considerations.html

// TODO:
//#define RDATE 50 // bad idea?

// freadCSV reads from FILE a STRING of MAXLEN
// and sets a DOUBLE if it's a number.
//
// Returns one of these or 0 at EOF
#define RNEWLINE 10
#define RNULL 20
#define RNUM 30
#define RSTRING 40

// NOTES:
//   "foo""bar"","foo\"bar",'foo"bar' quoting
//   unquoted leading spaces removed
//   string that is (only) number becomes number
//   foo,,bar,"",'' gives 3 RNULLS
int freadCSV(FILE* f, char* s, int max, double* d) {
  int c, q= 0, typ= 0;
  char* r= s;
  *r= 0; max--; // zero terminate 1 byte
  while((c= fgetc(f))!=EOF &&
	(c!=',' || q>0) && (c!='\n' || q>0)) {
    if (c=='\r') continue; // DOS
    if (c==0) return RNEWLINE;
    if (c==q) { q= -q; continue; } // "foo" bar, ok!
    if (c==-q) q= c;
    else if (!typ && !q && (c=='"' || c=='\''))
      { q= c; typ= RSTRING; continue; }
    if (!typ && isspace(c)) continue;
    if (c=='\\') c= fgetc(f);
    if (max>0) {
      *r++= c;
      *r= 0;
      max--;
      typ= RSTRING; // implicit if not quoted
    } else ; // TODO: indicate truncation?
  }
  // have value
  if (c=='\n') ungetc(0, f); // next: RNEWLINE
  if (c==EOF) if (!typ) return 0;
  // number?
  char* end;
  *d= strtod(s, &end);
  if (end!=s) {
    // remove trailing spaces and reparse
    // but only if no other chars
    int l= strspn(end, " ");
    if (end+l==s+strlen(s)) *end=0;
    *d= strtod(s, &end);
    if (s+strlen(s)==end) return RNUM;
  }
  // Null if empty string
  return typ?typ:RNULL;
}

// TODO: take delimiter as argument?
int TABCSV(FILE* f, char* selexpr) {
  char* cols[MAXCOLS]= {0};

  // parse header col names
  char* header= NULL;
  size_t hlen= 0;
  getline(&header, &hlen, f);

  char* h= header;
  // TODO: read w freadCSV()
  // TODO: prefix with table name?
  int col= 0, row= 0;
  cols[0]= h;
  while(*h && *h!='\n') {
    if (isspace(*h)) ;
    else if (*h==',' || *h=='\t') {
      *h= 0;
      cols[++col]= h+1;
    }
    h++;
  }
  *h= 0;

  // link column as variables
  val vals[MAXCOLS]={0};
  for(int i=0; i<=col; i++) {
    vals[i].d= i;
    vals[i].not_null = 1;
    //printf("===col %d %s\n", i, cols[i]);
    linkval(cols[i], &vals[i]);
  }

  double d;
  char s[1024]= {0};
  int r;

  col= 0;
  // TODO: consider caching whole line in order to not allocate small string fragments, then can point/modify that string
  while((r= freadCSV(f, s, sizeof(s), &d))) {
    if (vals[col].dealloc) free(vals[col].dealloc);
    ZERO(vals[col]);

    if (r==RNEWLINE) {
      where(selexpr);
      ZERO(vals);
      if (col) row++;
      col= 0;
      continue;
    }

    // have col
    vals[col].not_null= (r != RNULL);
    if (r==RNULL) ;
    else if (r==RNUM) vals[col].d= d;
    else if (r==RSTRING) vals[col].dealloc= vals[col].s= strdup(s);
    else error("Unknown freadCSV ret val");

    col++;
  }
  // no newline at end
  if (col) where(selexpr);

  // free strings
  for(int i=0; i<MAXCOLS; i++)
    if (vals[i].s) free(vals[i].s);

  free(header);
  fclose(f);
  return 1;
}

int from_list(char* selexpr) {
  char* start= ps;
  if (got("int")) INT(selexpr);
  else {
    // fallback, assume filename!
    char filnam[NAMELEN]= {};
    if (!getname(filnam))
      error("Unknown from-iterator");
    FILE* f= fopen(filnam, "r");
    if (!f) error(filnam); // "no such file");

    // TODO: fil.csv("a,b,c") == header
    TABCSV(f, selexpr);
    // TODO: json
    // TODO: xml
    // TODO: passwd styhle "foo:bar:fie"
    // (designate delimiter, use TABCSV)
    
    fclose(f);
  }
  // restore parse position!
  ps= start;
  return 1;
}

int from(char* selexpr) {
  if (!got("from")) {
    where(selexpr);
    return 0;
  } else {
    from_list(selexpr);
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
//   from (|MERGE) JOIN from USING(col, ...)
//
// 1. implemnt as nested loop
// 2. merge join if sorted
//    a. analyze file -> create link if sorted:
//        FILTAB.index:a,b,c
//    b. merge-join
//    c. sort files/create index


// just an aggregator!
int select() {
  if (!got("select")) return 0;
  char* expr= ps;
  // "skip" (dummies) to get beyond
  char* end= print_expr_list(expr, 0);
  if (end) ps= end;

  from(expr);
  return 1;
}

int sql() {
  int r= select();
  if (!r) return 0;
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
  while((r=freadCSV(f, s, sizeof(s), &d))) {
    if (r==RNEWLINE) printf("\n");
    else if (r==RNUM) printf("=> %3d  >%lg<\n", r, d);
    else if (r==RNULL) printf("=> %3d  NULL\n", r);
    else printf("=> %3d  >%s<\n", r, s);
  }
  printf("=> %3d  %s\n", r, s);
  fclose(f);
}

int main(int argc, char** argv) {
// testread(); exit(0);
 
  char* cmd= argv[1];
  printf("SQL> %s\n", cmd);

  parse(cmd);
  int r= sql();
  printf("---\nrows=%d\n", lineno-1);
  if (r!=1) printf("%%result=%d\n", r);
  if (ps && *ps) printf("%%UNPARSED>%s<\n", ps);
  printf("\n");


  return 0;
}
