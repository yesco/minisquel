// Variable storege for MiniSQuel
// ==============================
// (>) 2022 jsk@yesco.org

// VALUES
// ======
// This file contains a simple value type for
// MiniSQuel. It basically captures only:
//
//   - numbers (doubles + int53)
//   - strings
//   - and it also keep track of nulls
//
// It handles shared strings and keeps track
// of when to deallcoate.
//
// All variables, when set, update a bunch
// of stats for that "variable". These updates
// barely cost anyhthing as measured (1%!).
// It allows automatic compuation/aggregation
// of count/sum/min/max etc.
//
// Possibly, its cheap because of parsing
// overhead. LOL


// VARIABLES
// =========
// It also keeps a growing "stack" of named
// variables "table"."column" that stores
// values loaded, computed, and named (AS).
//
// Variables are searched backwards, latest
// first.
//
// you can either "set"(declare) a variable
// to create but BEWARE, as it will be removed
// once you "exit" your scope...
//
// Bascically, a table loader will declare
// the table.columns adding them linking
// to locally held values on the stack,
// then fill in values for each row and call
// the next FROM-table. This way, WHERE and
// JOINS already have all the values. When
// the table is finished, it backs up, removing
// all defined variables since it started.
// (just plainly restoring nvars...)
//
// So "GLOBAL" variables must be "DECLARED"
// (set at top level) to be retained.
//
// NOTES:
//   * This is not a storage efficent
//     But it's only used for single in-
//     memory instances, like one row/table.
//  * 
typedef struct val {
  char* s;
  char* dealloc; // if set (to s) deallocate
  double d;
  int not_null; // haha: "have val"
  // aggregations/statistics
  // TODO: cost too much?
  double min, max, sum, sqsum, last;
  int n, nnull, nstr, not_asc, not_desc;
} val;
  
// keeps stats
void clearval(val* v) {
  if (v->dealloc) free(v->dealloc);
  v->dealloc= v->s= NULL;
  v->d= 0;
  v->not_null= 0;
}

// quot<0 => no surrounding quot
// but will quote -quote|quot
void fprintquoted(FILE* f, char* s, int quot, int delim) {
  if (!s && delim==',') return; // csv
  if (!s) return (void)printf("NULL");
  if (!*s) return; // NULL
  if (quot>0) fputc(quot, f);
  while(*s) {
    if (*s==quot) fputc(abs(quot), f);
    if (*s==abs(delim)) fputc('\\', f);
    else if (*s=='\\') fputc(*s, f);
    fputc(*s, f);
    s++;
  }
  if (quot>0) fputc(quot, f);
}

// quot: see fprintquoted
void printval(val* v, int quot, int delim) {
  if (!v && delim==',') return; // csv
  if (!v) { printf("(null)"); return; }
  if (!v->not_null && delim==',') return; // csv
  if (!v->not_null) printf("NULL");
  else if (v->s) fprintquoted(stdout, v->s, quot, delim);
  else {
    if (delim==',') {
      printf("%.15lg", v->d);
    } else if (v->d > 0.000001 && v->d < 1e7) {
      printf("%7.7lg", v->d);
    } else if (delim!=',' && hprint(v->d, "")) {
      ; // if hprint is 0 try other
    } else if (v->d > 0 && v->d < 1e10) {
      char s[NAMELEN]= {0};
      sprintf(s, "%7.4lg", v->d);
      // remove +0: 1.2e+05 -> 1.234e5 !
      if (s[6]=='+' && s[7]=='0')
	strcpy(s+6, s+8); // safe?
      printf("%s", s);
    } else {
      // TODO: negatives!
      printf("%7.2lg", v->d);
    }
  }
}

// no measurable overhead!
void updatestats(val *v) {
  // TODO: capute number of findvar?
  
  if (v->s) {
    // TODO: string values, asc/desc?
    //   can compare last value if not clean!
    //   maybe set .not_value=1 even if str
    v->nstr++;
  } else if (!v->not_null) {
    v->nnull++;
  } else {
    v->sum+= v->d;
    v->sqsum+= v->d*v->d;
    if (!v->n || v->d < v->min) v->min= v->d;
    if (!v->n || v->d > v->max) v->max= v->d;
    if (v->n) {
      if (v->last > v->d) v->not_asc= 1;
      if (v->last < v->d) v->not_desc= 1;
    }
    v->last= v->d;
    v->n++;
  }
}

double stats_stddev(val *v) {
  // rapid calculation method
  // - https://en.m.wikipedia.org/wiki/Standard_deviation
  // s1==sum
  // s2==sqsum
  int N= v->n;
  return sqrt((N*v->sqsum - v->sum*v->sum)/N/(N-1));
}

double stats_avg(val *v) {
  return v->sum/v->n;
}

void setnum(val* v, double d) {
  clearval(v);
  v->not_null= 1;
  v->d= d;
  updatestats(v);
}

void setnull(val* v) {
  clearval(v);
  updatestats(v);
}

void setstrfree(val* v, char* s) {
  clearval(v);
  v->not_null= !!s;
  if (s) v->dealloc= v->s= s;
  updatestats(v);
}

void setstr(val* v, char* s) {
  clearval(v);
  v->not_null= !!s;
  if (s) v->dealloc= v->s= strdup(s);
  updatestats(v);
}

void setstrconst(val* v, char* s) {
  clearval(v);
  v->not_null= !!s;
  v->s= s;
  updatestats(v);
}

#include "csv.c"

void setval(val* v, int r, double d, char* s) {
  clearval(v);
  v->not_null= (r != RNULL);
  switch(r) {
  case RNULL: break;
  case RNUM: v->d= d; break;
  case RSTRING: v->dealloc= v->s= strdup(s); break;
  default: error("Unknown freadCSV ret val");
  }
  updatestats(v);
}

// TODO: make struct/linked list?
char* tablenames[VARCOUNT]= {0};
char* varnames[VARCOUNT]= {0};
val* varvals[VARCOUNT]= {0};
int varcount= 0;

val* linkval(char* table, char* name, val* v) {
  if (varcount>=VARCOUNT) error("out of vars");
  if (debug) printf(" {linkval %s.%s}\n", table, name);
  tablenames[varcount]= table;
  varnames[varcount]= name;
  varvals[varcount]= v;
  varcount++;
  return v;
}

val* findvar(char* table, char* name) {
  // Search names from last to first
  //   would work better w recursive func
  for(int i=varcount-1; i>=0; i--)
    if (0==strcmp(name, varnames[i]))
      if (!table || 0==strcmp(table, tablenames[i])) return varvals[i];
  return NULL;
}

int matchvars(char* table, char* match) {
  // search names in defined order
  // works better for column names
  int n= 0;
  for(int i=0; i<=varcount; i++) {
    printf("\t\t- %s.%s\n", tablenames[i], varnames[i]);
    if ((!table || 0==strcmp(table, tablenames[i]))
	&&  like(varnames[i], match)) {
      n++;
      printf("\tMATCH: %s.%s\n", table, varnames[i]);
    }
  }
  return n;
}

// TODO:setnum/setstr?
// returns variable's &val
val* setvar(char* table, char* name, val* s) {
  val* v= findvar(table, name);
  // TODO: deallocate duped name,val...
  //   - or not, only "alloc once"
  //   memory-leak
  if (!v) v= linkval(table, strdup(name), calloc(1, sizeof(*v)));
  // copy only value (not stats)
  clearval(v);
  v->s= s->s;
  // it's not the owner! I think
  //v->dealloc= s->dealloc;
  v->d= s->d;
  v->not_null= s->not_null;
  updatestats(v);
  return v;
}

int getval(char* table, char* name, val* v) {
  // special names
  if (*name=='$') {
    if (0==strcmp("$lineno", name)) {
      v->d= lineno+1;
      v->not_null= 1;
      return 1;
    }
    if (0==strcmp("$foffset", name)) {
      v->d= foffset;
      v->not_null= 1;
      return 1;
    }
    if (0==strcmp("$time", name)) {
      v->d = 42; // TODO: utime?
      v->not_null= 1;
      return 1;
    }
  }
  // lookup variables
  val* f= findvar(table, name);
  if (f) {
    *v= *f;
    // It doesn't own the string...
    // (for this to be safe: copy from table)
    v->dealloc= NULL;
    return 1;
  }
  // failed, null
  ZERO(*v);
  return 0;
}
