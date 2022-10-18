#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

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
	  if (*++s) continue;
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
    char* s= csvgetline(f);
    printf(">>>%s<<<\n", s);
    free(s);
  }
}
