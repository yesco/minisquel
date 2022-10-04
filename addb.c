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
  while(!end() && (isalnum(*ps) || *ps=='$'|| *ps=='.')) {
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

// returns one of these or 0 at EOF
#define RNEWLINE 10
#define RNULL 20
#define RNUM 30
#define RSTRING 40 // -RSTRING if truncated

//CSV: 2, 3x, 4, 5y , 6 , 7y => n,s,n,s,n,s
//CSV: "foo", foo, "fooo""bar", "foo^Mbar"

int readfield(FILE* f, char* s, int max, double* d) {
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
  return typ?typ:RNULL;
}

// TODO: 70 lines too much, maybe not parse num?
int readfield2(FILE* f, char* r, int max) {
  // TODO: try use fscanf instead?
  // TODO: maybe use malloced strings?
  int c= fgetc(f);
;
  int typ= RNULL;
  *r= 0;
  if (isnewline(c)) return RNEWLINE;
  ungetc(c, f);
  // terminator
  max--;
  while((c=fgetc(f)) != EOF) {
    *r= 0;
    if (c==',') return typ;
    if (isnewline(c)) {
      ungetc(c, f);
      return typ;
    }
    if (isspace(c)) continue;
    // TODO: 'foo'
    if (c=='"') {
      typ= RSTRING;
      while((c=fgetc(f)) != EOF) {
	if (c=='"') {
	  if((c=fgetc(f)) == EOF) return typ;
	  while(isspace(c) && c!=EOF) c=fgetc(f);
	  if (isnewline(c)) {
	    ungetc(c, f);
	    return typ;
	  }
	  if (c==EOF || c==',') return typ;
	  // TODO: unexpected chars after quoted string, error?
	  return typ;
	}
	// newline is fine!
	// TODO: handle quote
	if (max==0) typ= -RSTRING;
	if (max>0) {
	  *r++= c;
	  *r= 0;
	}
	max--;
      }
      return -RSTRING;
    }
    if (isdigit(c) || c=='.' || c=='-' || c=='+') {
      // TODO: merge with unquoted string detect if number
      typ= RNUM;
      do {
	if (max==0) typ= -RSTRING;
	*r++= c;
	*r= 0;
	max--;
	if ((c=fgetc(f)) == EOF) return typ;
	if (isnewline(c)) {
	  ungetc(c, f);
	  return typ;
	}
      } while(isdigit(c) || c=='.' || c=='-' || c=='+' || c=='e' || c== 'E');
      while(isspace(c) && c!=EOF) c=fgetc(f);
      if (c=='\n') ungetc(c, f);
      if (c!=',') {
	ungetc(c, f);
	// hopefully jumps to unquoted string
	// TODO: 24" would be problem...
	typ= 0;
	continue;
      }
      return typ;
    }
    // unquoted string
    typ= RSTRING;
    do {
	if (c==',') return typ;
	if (isnewline(c)) {
	  ungetc(c, f);
	  return typ;
	}
	// TODO: handle quote
	if (max==0) typ= -RSTRING;
	*r++= c;
	*r= 0;
	max--;
    } while((c=fgetc(f)) != EOF);
    // nothing wrong
    return RSTRING;
  }
  return 0;
}

// TODO: take delimiter as argument?
int TABCSV(FILE* f, char* expression) {
  // parse header col names
  char cols[MAXCOLS][NAMELEN]= {};
  char head[NAMELEN*MAXCOLS]= {};
  fgets(&head[0], sizeof(head), f);
  //printf("HEAD=%s<\n", head);
  int col= 0, row= 0;
  char* h= &head[0];
  int i= 0;
  // prefix with table name?
  while(*h) {
    while(*h && isspace(*h)) h++;
    // TODO: quoted strings?
    // TODO: name w space/funny char?
    if (*h==',' || *h=='\t') {
      i= 0;
      col++;
    } else {
      cols[col][i++]= *h;
    }
    h++;
  }

  val vals[MAXCOLS]={};
  for(int i=0; i<=col; i++) {
    vals[i].d= i;
    vals[i].not_null = 1;
    linkval(cols[i], &vals[i]);
  }

  col= 0;
  while(!feof(f)) {
    int c= fgetc(f);

    // TODO: move to readval function
    // TODO: readtill('"')
    if (c=='\n' || c=='\r') {
      where(expression);
      ZERO(vals);
      if (col) row++;
      col= 0;
      vals[col].d= 0;
      vals[col].not_null= 0;
      printf("\n");
    } else if (isspace(c))
      ;
    else if (c==',' || c=='\t') {
      col++;
      vals[col].d= 0;
      vals[col].not_null= 0;
    } else if (isdigit(c)) {
      // TODO: parse myself as number?
      // TODO: how to handle "24h" "74-11-x"
      ungetc(c, f);
      if (1==fscanf(f, "%lg", &vals[col].d)) {
	printf("  %d.%s\tNUM=%g\n", col, cols[col], vals[col].d);
	vals[col].not_null= 1;
      }
    } else if (c=='"') {
      int i= 0;
      printf("  %d.%s\tSTRING=", col, cols[col]);
      while((c= fgetc(f)) != EOF) {
	if (c=='"') break;
	printf("%c", c);
	// TODO: i limit?
      }

      // TODO: store the string!
      vals[col].s= "STRING-TODO";
      vals[col].not_null= 1;
      if (c=='"') printf("<");
      printf("\n");
    } else {
      // TODO: prepend to "string"
      if (vals[col].not_null)
	printf("%% trail text after number\n");

      // unquoted string
      ungetc(c, f);
      printf("  %d.%s\tUNQSTRING=", col, cols[col]);
      while((c= fgetc(f)) != EOF) {
	if (c==',' || c=='\n' || c=='\r') break;
	printf("%c", c);
	// TODO: i limit?
      }
      ungetc(c, f);
      
      // TODO: store the string!
      vals[col].s= "STRING-TODO";
      vals[col].not_null= 1;
      printf("<\n");
    }

    // TODO: print value detected
  }
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
  while((r=readfield(f, s, sizeof(s), &d))) {
    if (r==RNEWLINE) printf("\n");
    else if (r==RNUM) printf("=> %3d  >%lg<\n", r, d);
    else if (r==RNULL) printf("=> %3d  NULL\n", r);
    else printf("=> %3d  >%s<\n", r, s);
  }
  printf("=> %3d  %s\n", r, s);
  fclose(f);
}

int main(int argc, char** argv) {
  testread();
  exit(0);
  
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
