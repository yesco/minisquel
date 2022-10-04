#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

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
  *ps= 0; // terminate string
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
  double d;
  int not_null;
} val;
  
void print_val(val* v) {
  if (!v->not_null) printf("NULL");
  else if (v->s) printf("\"%s\"", v->s);
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
  char* p= &name[0];
  while(!end() && (isalnum(*ps) || *ps=='_' || *ps=='$'|| *ps=='.')) {
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
  if (str(&v->s)) return 1;
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

int expr(val* v) {
  spcs();
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

// returns end pointer
char* print_expr_list(char* expression, int do_print) {
  char* old_ps= ps;
  ps= expression;
  
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
  
  expression= ps;
  ps= old_ps;
  return expression;
}

int where(char* expression) {
  val v= {};
  if (got("where")) {
    // TODO: test
  } else {
    v.not_null= 1;
  }
  
  if (v.not_null)
    print_expr_list(expression, 1);

  return 1;
}

int INT(char* expression) {
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

    where(expression);
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

// returns one of these or 0 at EOF
#define RNEWLINE 10
#define RNULL 20
#define RNUM 30
#define RSTRING 40 // -RSTRING if truncated
// TODO:
//#define RDATE 50 // bad idea?

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

int freadCSV(FILE* f, char* s, int max, double* d) {
  //printf("[ f=%p ]\n", f);
  int c, q= 0, typ= 0;;
  char* r= s;
  *r= 0;
  max--;
  // field ends: EOF/,/NL (if not in quote)
  while((c= fgetc(f))!=EOF &&
	(c!=',' || q>0) && (c!='\n' || q>0)) {
    if (c==0) return RNEWLINE;
    if (c==q) {q= -q; continue; } // "foo" bar, ok!
    if (c==-q) q= c;
    else if (!typ && !q && (c=='"' || c=='\'')) { q= c; continue; }
    if (!typ && isspace(c)) continue;
    if (c=='\\') c= fgetc(f);
    if (max>0) {
      *r++= c;
      *r= 0;
      max--;
      typ= RSTRING;
    }
  }
  // have value
  if (c=='\n') ungetc(0, f);
  if (c==EOF) return 0;
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
  // TODO: trailing spaces on unquoted str?
  // (standard disallows)
  return typ?typ:RNULL;
}

// TODO: take delimiter as argument?
int TABCSV(FILE* f, char* expression) {
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
  while((r= freadCSV(f, s, sizeof(s), &d))) {
    if (vals[col].s) free(vals[col].s);
    ZERO(vals[col]);

    if (r==RNEWLINE) {
      where(expression);
      ZERO(vals);
      if (col) row++;
      col= 0;
      continue;
    }

    // have col
    vals[col].not_null= (r != RNULL);
    if (r==RNULL) ;
    else if (r==RNUM) vals[col].d= d;
    else if (r==RSTRING) vals[col].s= strdup(s);
    else error("Unknown freadCSV ret val");

    col++;
  }

  // free strings
  for(int i=0; i<MAXCOLS; i++)
    if (vals[i].s) free(vals[i].s);

  free(header);
  fclose(f);
  return 1;
}

int from_list(char* expression) {
  char* start= ps;
  if (got("int")) INT(expression);
  else {
    // fallback, assume filename!
    char filnam[NAMELEN]= {0};
    if (!getname(filnam))
      error("Unknown from-iterator");
    FILE* f= fopen(filnam, "r");
    if (!f) error(filnam); // "no such file");

    // TODO: fil.csv("a,b,c") == header
    TABCSV(f, expression);
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

int from(char* expression) {
  if (!got("from")) {
    where(expression);
    return 0;
  } else {
    from_list(expression);
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
  char* expression= ps;
  // "skip" (dummies)
  char* end= print_expr_list(expression, 0);
  if (end) ps= end;

  from(expression);
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
