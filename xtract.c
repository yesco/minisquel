// Extract anything from anything
//
// Extract foo/bar[3]/fie => fum
//
//   xml:  <foo>..<bar>...<bar>...<fie>fum</fie>
//   json: { foo: { bar: { fuu: .. } ... bar: .. fie: "fum" } }

// TODO: [1,2,3,foo:bar] - nodejs...

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

char* xnext(char* s) {
  return strpbrk(s, " \t\n\r()[]{,;}</>=:\\\"\'");
}

// parses one level of "expression"
char* xscan(char* s, char t) {
  printf("\t\t\t%s\n", s);
  char *last= s; int n= 1;
  while(*s && (s= xnext(s))) {
    if (t=='[' && n==3) printf("---ARR:\n");
    printf("\t>%.*s<\t%c\n", (int)(s-last), last, *s);
    
    int q= 0; char* start= 0;
    switch(*s) {

    case ' ': case '\t': case '\r': case '\n': break;
      
    case ')': case '}': case ']': return s;
      
    case '\"': q++;
    case '\'': q++;
      last= s; s++;
      while(*s && *s!=" \'\""[q]) {
	if (*s=='\\') s++;
	s++;
      }
      if (*s) s++;
      printf("---STRING: %.*s\n", (int)(s-last), last);
      break;
      
    // JSON
    case '[': q++;
    case '{': q++;
    case '(': q++;
      s= xscan(s+1, *s);
      assert(*s==" ({["[q]); break;

    case ':': case '=':
      printf("--- '%.*s'\n", (int)(s-last), last);
      // TODO: match path to name
      s= xscan(s+1, ':');
      break;

    // array
    case ',': case ';': n++;
      printf("--- [%d]\n", n); break;
      
    default: printf("default>%.*s< '%c'\n", (int)(last-s), s, *s);
      
    }
    s++;
    last= s;
  }
  return 0;
}

void scan(char* s) {
  printf("\n??? %s\n", s);
  xscan(s, 0);
}

int main(int argc, char** argv) {
  scan("{foo: { bar: [1,2,{ fie: \"fum\" }] } }");
  exit(1);
  scan("<foo>...<bar></bar><bar></bar><bar>...<fie>fum</fie></bar></foo>");
  // TODO: ?
  scan("(foo (bar 1 bar 2 bar (fie \"fum\")))");
  scan("((foo (bar . 1) (bar . 2) ( bar fie . \"fum\" )))");
}

