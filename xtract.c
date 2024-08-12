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
      
    case ',':
    case ')': case '}': case ']':
      printf("RETURNING: %s\n", s);
      return s;
      
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
      printf("LIST: '%c'\n", *s);
      do {
	s= xscan(s+1, *s);
	printf("NEXT ELEMENT: %s\n", s);
      } while(*s==','); // TODO: space etc?
      printf("sss> '%s' %d '%c'\n", s, q, " )}]"[q]);
      assert(*s==" )}]"[q]); break;

    case ':': case '=':
      printf("--- '%.*s'\n", (int)(s-last), last);
      // TODO: match path to name
      s= xscan(s+1, ':');
      break;

    // array
      //    case ',': case ';': n++;
      //      printf("--- [%d]\n", n); break;
      
    default: printf("default>%.*s< '%c'\n", (int)(last-s), s, *s);
      
    }
    s++;
    last= s;
  }
  return 0;
}

char* xscan2(char* s);

char* spc(char* s) {
  while(s && isspace(*s)) s++;
  return s;
}

char* xarr(char* s) {
  do {
    s= spc(s);
    char* last= s;
    s= xscan2(s);
    s= spc(s);
    if (*s==',') s++;
  } while(*s!=']' && *s!=')');
  assert(*s==']' || *s==')');
  return s+1;
}

char* xsym(char* s);

char* xdic(char* s) {
  do {
    s= spc(s);
    char* key= s;
    s= xsym(s);
    if (key==s) break;
    s= spc(s);
    char* val= s= xscan2(s);
    s= spc(s);
    if (*s==',') s++;
  } while(*s!='}');
  assert(*s=='}');
  return s+1;
}

char* xstr(char* s) {
  char q= *s++;
  while(*s && *s!=q) {
    if (*s=='\\') s++;
    s++;
  }
  assert(*s==q);
  return s+1;
}

char* xnum(char* s) {
  while(*s && (isdigit(*s) || strchr("+-eE.", *s))) s++;
  return s;
}

char* xsym(char* s) {
  // TODO: unicode use stopchars
  while(*s && isalnum(*s)) 
    s++;
  assert(*s==':');
  return s+1;
}

char* xscan2(char* s) {
  s= spc(s);
  if (!s || !*s) return 0;
  switch(*s) {
  case '(':
  case '[': s= xarr(s+1); break;
  case '{': s= xdic(s+1); break;
  case '"': case '\'': s= xstr(s); break;
  case '0'...'9': case '-': case '.': s= xnum(s); break;
  default: s= xsym(s); break; }
  return s;
}

void result(char* p, char* r, int n) {
  if (n==0 || !*r) printf("---NORESULT\n");
  else printf("--- %s: %.*s\n", p, n, r);
}

char* xscan3(char* s, char* p) {
  const static char bracks[] = "(){}[]";
  s= spc(s);
  if (p && !*p) { result(p, s, strlen(s)); return 0; }
  if (!s || !*s) return s;
  char* pn= p;
  while(pn && *pn && strchr("./", *pn)) pn++;
  switch(*s) {
  case ',': return xscan3(s+1, p); // TODO: remove?
  case '(': case '{': case '[': {
    char q= bracks[strchr(bracks, *s)-bracks+1];
    s++; int n= 1;
    do {
      s= spc(s);
      if (*s==',') s= spc(s+1);
      char* a= s;
      s= xscan3(s, p);

      // TODO: handle ':' separate (?)
      char* e= strpbrk(pn, "/.[");
      int l= e? e-pn: strlen(pn);
      if (0==strncmp(pn, a, l)) {
	char* r= s= spc(s);
	s= xscan3(s, e);
	if (!e) result(e, r, s-r);
      }

      //printf("> %.*s\n", (int)(s-a), a);
      s= spc(s);
    } while(*s && *s!=q);
    //printf("ASSERT: q='%c' \"%s\"\n", q, s);
    assert(*s==q);
    return s+1; }
  case '"': case '\'': return xstr(s);
  case '0'...'9': case '-': case '.': return xnum(s);
  default: return xsym(s);
  }
}

void scan(char* s) {
  printf("\n??? %s\n", s);
  //xscan(s, 0);
  //xscan2(s);
  xscan3(s, "");
  printf("\n\n");
  xscan3(s, "/foo/bar/fie");
  //xscan3(s, "foo/bar/3/fie");
}

int main(int argc, char** argv) {
  scan("{foo: { bar: [1,2,{fie: \"fum\" },{fie: \"FUM\"}] fie: \"NOT\" }}");
  exit(1);
  scan("<foo>...<bar></bar><bar></bar><bar>...<fie>fum</fie></bar><fie>not</fie><bar><fie>FUM</fie></bar></foo>");
  // TODO: ?
  scan("(foo (bar 1 bar 2 bar (fie \"fum\")))");
  scan("((foo (bar . 1) (bar . 2) ( bar fie . \"fum\" )))");
}

