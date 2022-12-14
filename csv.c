#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>

// freadCSV reads from FILE a STRING of MAXLEN
// and sets a DOUBLE if it's a number.
//
// Returns one of these or 0 at EOF
#define RNEWLINE 10
#define RNULL 20
#define RNUM 30
#define RSTRING 40

extern int debug; // lol, you need define!

char decidedelim(char* line) {
  int comma= strcount(line, ",");
  int semi= strcount(line, ";");
  int colon= strcount(line, ":");
  int tab= strcount(line, "\t");
  int bar= strcount(line, "|");
  // TODO: ??
  int spcs= strcount(line, "  ");
  int bigbest= max(comma, max(semi, max(colon, max(tab, bar))));
  char delim= ','; // default
  if (comma==bigbest) delim= ',';
  if (semi ==bigbest) delim= ';';
  if (colon==bigbest) delim= ':';
  if (tab  ==bigbest) delim= '\t';
  if (bar  ==bigbest) delim= '|';
  //if (spcs ==bigbest) delim= "  "; //lol'\t';
  if (debug) {
    printf("decidedelim: '%c'\n", delim);
    printf("  DELIM: %d ,%d ;%d :%d \\t%d _%d\n", bigbest, comma, semi, colon, tab, spcs);
  }
  return delim;
}

int freadCSV(FILE* f, char* s, int max, double* d, char delim) {
  if (!delim) delim= ',';
  int c, q= 0, typ= 0;
  char* r= s;
  *r= 0; max--; // zero terminate 1 byte
  //if (debug) printf("{ CSV>");
  while((c= fgetc(f)) != EOF &&
	(c!=delim || q>0) && (c!='\n' || q>0)) {
    if (debug>3) printf("'%c'", c);
    if (c=='\r') continue; // DOS
    if (c==0) { // skip NL+NL*
      while(isspace((c= fgetc(f))));
      ungetc(c, f);
      return RNEWLINE;
    }
    if (c==q) { q= -q; continue; } // "foo" bar, ok!
    if (c==-q) q= c;
    else if (!typ && !q && (c=='"' || c=='\''))
      { q= c; typ= RSTRING; continue; }
    if (!typ && isspace(c)) continue;
    if (c=='\\') if ('\n'==(c= fgetc(f))) c= fgetc(f);
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
  if (d) *d= strtod(s, &end);
  if (d && end!=s) {
    // remove trailing spaces and reparse
    // but only if no other chars
    int l= strspn(end, " ");
    if (end+l==s+strlen(s)) *end=0;
    if (d) *d= strtod(s, &end);
    if (s+strlen(s)==end) return RNUM;
  }
  // Null if empty string
  if (0==strcmp("\\N", s) || 0==strcmp("null", s) || 0==strcmp("NULL", s)) return RNULL;
  return typ?typ:RNULL;
}

// NOTES:
//   "foo""bar"","foo\"bar",'foo"bar' quoting
//   unquoted leading spaces removed
//   string that is (only) number becomes number
//   foo,,bar,"",'' gives 3 RNULLS
//
// WARNING: moedifies f
int sreadCSV(char** f, char* s, int max, double* d, char delim) {
  if (!delim) delim= ',';
  int c, q= 0, typ= 0;
  char* r= s;
  *r= 0; max--; // zero terminate 1 byte
  while((c= *(*f)++) &&
	(c!=delim || q>0) && (c!='\n' || q>0)) {
    if (c=='\r') continue; // DOS
    if (c==0) return 0;
    if (c==q) { q= -q; continue; } // "foo" bar, ok!
    if (c==-q) q= c;
    else if (!typ && !q && (c=='"' || c=='\''))
      { q= c; typ= RSTRING; continue; }
    if (!typ && isspace(c)) continue;
    if (c=='\\') if ('\n'==(c= *(*f)++)) {
	//printf("QUOTE");
	c= *(*f)++;
      } 
    if (max>0) {
      *r++= c;
      *r= 0;
      max--;
      typ= RSTRING; // implicit if not quoted
    } else ; // TODO: indicate truncation?
  }
  // have value
  if (!c) if (!typ) return 0;
  // number?
  char* end;
  if (d) *d= strtod(s, &end);
  if (d && end!=s) {
    // remove trailing spaces and reparse
    // but only if no other chars
    int l= strspn(end, " ");
    if (end+l==s+strlen(s)) *end=0;
    if (d) *d= strtod(s, &end);
    if (s+strlen(s)==end) return RNUM;
  }
  // Null if empty string
  if (0==strcmp("\\N", s) || 0==strcmp("null", s) || 0==strcmp("NULL", s)) return RNULL;
  return typ?typ:RNULL;
}

char* csvgetline(FILE* f, char delim) {
  if (!delim) delim= ',';
  char* r= NULL;
  size_t l= 0;
  getline(&r, &l, f);
  //printf(">>>%s<<<\n", r);
  if (!r) return NULL;

  char inquote= 0;
  do {
    // inquote? - more to read!
    inquote= 0;
    char* s= r;
    while(*s) {
      if (*s=='\\') {
	switch(*++s) {
	case '\r': s++; // \n follows
	case '\n':
	  if (*++s) {++s; continue;}
	  else inquote= 1; // read more
	case 0: inquote= 1;
	default: continue;
	}
      }
      // start of field?
      if (s==r || (*s==delim && s++)) {
	while(*s && *s==' ') s++; // spc
	if (*s=='\\') continue;
	// is a start quote?
	if (*s && (*s=='\'' || *s=='"')) inquote = *s++;
	// find end quote
	if (*s && !inquote) s++;
	else while(*s && inquote)
	    if (*s++==inquote) {
	      if (*s==inquote) s++;
	      else inquote= 0;
	    }
      } else s++;
    }
    if (inquote) { // read more
      char* a= NULL;
      size_t ll= 0;
      getline(&a, &ll, f);
      //printf("\nREAD>%s< %lu\n", a, strlen(a));
      //printf("BEFORE>>>%s<<< %lu\n", r, strlen(r));
      r= strdupncat(r, -1, a);
      free(a);
      //printf("INQUOTE>>>%s<<< %lu\n\n", r, strlen(r));
    }
  } while (inquote);
  return r;
}

// Given a CURsor that points to a string
// variable, this function returns the
// next field as DELIMeted.
//
// CUR variable points at next field.
char* nextTSV(char** cur, char delim) {
  char* s= *cur;
  char* e= s;
  while(*e && *e!=delim) e++;
  *e++ = 0;
  *cur= e;
  return s;
}

// Given a CURsor that points to a string
// variable, this function returns the
// next field as DELIMeted.
//
// CUR variable points at next field.
char* nextCSV(char** cur, char delim) {
  char* s= *cur;

  while(*s==' '||*s=='\t') s++;

  // if quoted q is true
  char q= *s=='"' ? *s++ :
    (*s=='\'' ? *s++ : 0);

  char* e= s;
  while(*e && (q || *e!=delim)) {
    if (*e=='\n' || *e=='\r') break;
    if (*e==q && e[1]!=q) break;// ..""..
    if (*e=='\\') e++; // \"
    e++;
  }

  // truncate
  char endchar= *e;
  *e++ = 0;
  // fix the string
  char *t=s, *f=s;
  while(*f) {
    if (*f==q) f++; // ...""... keep one
    if (*f=='\\') f++;
    *t++ = *f++;
  }

  // "foo" ILLEGAL ,
  while(*e && *e!=delim) e++;

  *cur = *e ? e+1 : NULL;

  return s;
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
