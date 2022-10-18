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

// NOTES:
//   "foo""bar"","foo\"bar",'foo"bar' quoting
//   unquoted leading spaces removed
//   string that is (only) number becomes number
//   foo,,bar,"",'' gives 3 RNULLS
//
// WARNING: moedifies f
int sreadCSV(char** f, char* s, int max, double* d) {
  int c, q= 0, typ= 0;
  char* r= s;
  *r= 0; max--; // zero terminate 1 byte
  while((c= *(*f)++) &&
	(c!=',' || q>0) && (c!='\n' || q>0)) {
    if (c=='\r') continue; // DOS
    if (c==0) return 0;
    if (c==q) { q= -q; continue; } // "foo" bar, ok!
    if (c==-q) q= c;
    else if (!typ && !q && (c=='"' || c=='\''))
      { q= c; typ= RSTRING; continue; }
    if (!typ && isspace(c)) continue;
    if (c=='\\') if ('\n'==(c= *(*f)++)) {
	printf("QUOTE");
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




int min(int a, int b) { return a<b?a:b; }
int max(int a, int b) { return a>b?a:b; }

// To malloced string S
// append N characters from ADD.
//
// Returns: realloced string, non-NULL.
//
// inputs can be NULL
// n>=0 appends n chars
// n<0 appends whole ADD
char* strdupncat(char* s, int n, char* add) {
  int l= min(n>=0?n:INT_MAX, add?strlen(add):0);
  return strncat(
    realloc(s?s:strdup(""), s?strlen(s):0 + l+1),
    add?add:"", l);
}

char* csvgetline(FILE* f) {
  char* r= NULL;
  size_t l= 0;
  getline(&r, &l, f);

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
      if (s==r || (*s==',' && s++)) {
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
      r= strdupncat(r, -1, a);
      free(a);
    }
  } while (inquote);
  return r;
}

int main(int argc, char** argv) {
  if (0) {
    {
      char* s= strdupncat(strdup("foo"), -1, "bar");
      printf(">%s<\n", s);
      free(s);
      s= NULL;
    }
    {
      char* s= strdupncat(strdup("foo"), 2, "bar");
      printf(">%s<\n", s);
      free(s);
    }
    {
      char* s= strdupncat(NULL, 2, NULL);
      printf(">%s<  %p\n", s, s);
      free(s);
    }
  }

  {
    FILE* f= fopen("nl.csv", "r");
    char v[1024];
    double d;
    int r;
    while((r= freadCSV(f, v, sizeof(v), &d))) {
      if (r==RNULL) printf("NULL   ");
      if (r==RNUM) printf("%lg   ", d);
      if (r==RSTRING) printf(">%s<   ", v);
      if (r==RNEWLINE) printf("\n");
    }
  }

  {
    FILE* f= fopen("nl.csv", "r");
    char* s= csvgetline(f);
    printf("\n>>>%s<<<\n\n", s);

    char v[1024];
    double d;
    int r;
    char* ss= s;
    while((r= sreadCSV(&ss, v, sizeof(v), &d))) {
      if (r==RNULL) printf("NULL   ");
      if (r==RNUM) printf("%lg   ", d);
      if (r==RSTRING) printf(">%s<   ", v);
      if (r==RNEWLINE) printf("\n");
    }

    free(s);
  }
}
