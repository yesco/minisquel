// --- your funcs here!
// The functions can have two different signatures)

// Example: 2 argument function
// (max three direct params == +c)
int plus(val* r, int n, val* a, val* b) {
  if (n!=2) return -2;
  r->d= a->d + b->d;
  r->not_null= a->not_null && b->not_null;
  return 1;
}

// Example: "vararg" (max 10 params)
int add(val* r, int n, val params[]) {
  r->d= 0; r->not_null= 1;
  for(int i= 0; i<n; i++) {
    if (params[i].not_null)
      r->d+= params[i].d;
  }
  return 1;
}

// Math

int mod(val* r, int n, val* a, val* b) {
  if (n!=2) return -2;
  r->d= (long)a->d % (long)b->d;
  r->not_null= a->not_null && b->not_null;
  return 1;
}  

int _div(val* r, int n, val* a, val* b) {
  if (n!=2) return -2;
  r->d= (long)a->d / (long)b->d;
  r->not_null= a->not_null && b->not_null;
  return 1;
}  

// String
// - https://learn.microsoft.com/en-us/sql/t-sql/functions/string-functions-transact-sql?view=sql-server-ver16

// ASCII
int ascii(val* r, int n, val* a) {
  if (n!=1) return -1;
  r->not_null= a->not_null && a->s;
  if (!a->s) return 1;
  r->d= a->s[0];
  return 1;
}
// CHAR
int _char(val* r, int n, val* a) {
  if (n!=1) return -1;
  r->not_null= a->not_null && a->d;
  if (!a->d) return 1;
  r->dealloc= r->s= calloc(1, 2);
  r->s[0]= (int)a->d;
  return 1;
}
// CHARINDEX
// CONCAT
int concat(val* r, int n, val* v) {
  int len= 0;
  for(int i=0; i<n; i++)
    len+= (v[i].s? strlen(v[i].s): 5) + 1;

  char* s= calloc(1, len);
  for(int i=0; i<n; i++) {
    if (v[i].s) strcat(s, v[i].s);
  }
  
  setstr(r, s);
  return 1;
}
// CONCAT_WS
// DIFFERENCE
// FORMAT
// LEFT
int left(val* r, int n, val* s, val* len) {
  if (n!=2) return -2;
  setstrfree(r, s->s? strndup(s->s, len->d): NULL);
  return 1;
}
// LEN
// LOWER
int lower(val* r, int n, val* a) {
  if (n!=1) return -1;
  r->not_null= a->not_null && a->s;
  if (!a->s) return 1;
  r->dealloc= r->s= strdup(a->s);
  // TODO: utf-8?
  char* p= r->s;
  while(*p) {
    *p= tolower(*p);
    p++;
  }
  return 1;
}
// LTRIM
// NCHAR
// PATINDEX
// QUOTENAME
// REPLACE
// REPLICATE
// REVERSE
// RIGHT
int right(val* r, int n, val* s, val* len) {
  if (n!=2) return -2;
  if (!s->s) return 1;
  int ln= strlen(s->s);
  int rght= len->d;
  if (rght >= ln) rght= ln;
  setstrfree(r, strndup(s->s + ln-rght, rght));
  return 1;
}
// RTRIM
// SOUNDEX
// SPACE
// STR
// STRING_AGG
// STRING_ESCAPE
// STRING_SPLIT
// STUFF
// SUBSTRING
// TRANSLATE
// TRIM
// UNICODE
// UPPER
int upper(val* r, int n, val* a) {
  if (n!=1) return -1;
  r->not_null= a->not_null && a->s;
  if (!a->s) return 1;
  r->dealloc= r->s= strdup(a->s);
  // TODO: utf-8?
  char* p= r->s;
  while(*p) {
    *p= toupper(*p);
    p++;
  }
  return 1;
}  

// Time related
int timestamp(val* r, int n, val* a) {
  // TODO: normalize a if given?
  if (n) return 0;
  r->s= strdup(isotime());
  r->not_null= 1;
  return 1;
}

// Aggregators:

void agg_check(char* fun, val* r, val* a) {
  r->not_null= 1;

  int m= a->n + a->nstr + a->nnull;
  if (!m) expected2(fun, "currently only works on named variables");
}

int count(val* r, int n, val* a) {
  r->not_null= 1;
  if (n==0) {
    // select count() == count(*)
    r->d= lineno;
  } else if (n==1) {
    agg_check("count", r, a);
    r->d= a->n + a->nstr;
  } else
    return -1;
  return 1;
}

int sum(val* r, int n, val* a) {
  if (n!=1) return -1;
  agg_check("sum", r, a);
  r->d= a->sum;
  return 1;
}

int _min(val* r, int n, val* a) {
  if (n!=1) return -1;
  agg_check("min", r, a);
  r->d= a->min;
  return 1;
}

int _max(val* r, int n, val* a) {
  if (n!=1) return -1;
  agg_check("max", r, a);
  r->d= a->max;
  return 1;
}

int avg(val* r, int n, val* a) {
  if (n!=1) return -1;
  agg_check("avg", r, a);
  r->d= stats_avg(a);
  return 1;
}

int stdev(val* r, int n, val* a) {
  if (n!=1) return -1;
  agg_check("stdev", r, a);
  r->d= stats_stddev(a);
  return 1;
}

// system
int _type(val* r, int n, val* a) {
  if (n!=1) return -1;
  r->not_null= 1;
  if (!a->not_null) r->s= "none";
  else if (a->s) r->s= "str";
  // TODO: "date/time/json/xml etc?"
  else if (a->d==(long)a->d) r->s= "int";
  else r->s= "num";
  return 1;
}

// TODO: how to make boolean functions?

int in(val* r, int n, val params[]) {
  return -1;
}

// make SURE to add your function here
void register_funcs() {
  // examples
  registerfun("plus", plus);
  registerfun("add", add);

  // math
  registerfun("mod",  mod);
  registerfun("div",  _div);
  
  // strings
  registerfun("ascii", ascii);
  registerfun("char", _char);
  registerfun("concat", concat);
  registerfun("left", left);
  registerfun("lower", lower);
  registerfun("right", right);
  registerfun("upper", upper);
  
  // time
  registerfun("timestamp", timestamp);

  // aggregators
  registerfun("count", count);
  registerfun("sum", sum);
  registerfun("min", _min);
  registerfun("max", _max);
  registerfun("avg", avg);
  registerfun("stdev", stdev);

  // system
  registerfun("type", _type);
}

// -- end your funcs
