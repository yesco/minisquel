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

// TODO: use???
char* xnext(char* s) {
  return strpbrk(s, " \t\n\r()[]{,;}</>=:\\\"\'");
}

char* spc(char* s) {
  while(s && isspace(*s)) s++;
  return s;
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

char* xatm(char* s) {
  // TODO: unicode use stopchars
  while(*s && isalnum(*s)) s++;
  return s;
}

char* xsym(char* s) {
  s= xatm(s);
  assert(*s==':');
  return s+1;
}

void result(char* p, char* r, int n) {
  if (n==0 || !*r) printf("---NORESULT\n");
  else printf("===> %.*s\n", n, r);
}


// TODO: handle comments?

char* xxml(char* s, char* p, int level) {
  if (!s || !*s) return s;
  if (p && !*p) { result(p, s, strlen(s)); return 0; }

  // extract next path name needed
  char* pn= p;
  while(pn && *pn && strchr("./", *pn)) pn++;
  char* e= pn? strpbrk(pn, "/.["): 0;
  int l= !pn? 0: e? e-pn: strlen(pn);

  // TODO: skip spaces inside tag parsing...

  //printf("XXX: %*s%d %s    %.10s\n", level, "", level, p, s);
 next: while(*s) {
    if (*s=='<') {
      s++;
      // end tag
      if (*s=='/') return s-1;
      //printf("%*s<--- %.10s\n", level-2, "", s);

      // start tag
      //printf("%*s---> %.10s\n", level, "", s);
      char* a= s;
      s= xatm(s);
      int match= (0==strncmp(pn, a, l));
      // TODO: do correctly @attr
      while(*s && *s!='>')
	if (*s++=='/') { s++; goto next; }

      char* r= ++s;
      s= xxml(s, match? e: p, level+2);
      if (match && !e)
	result(e, r, (int)(s-r));
      
      assert(*s=='<');
      while(*s && *s!='>') s++;

    } else { // ignore all else text
      s++;
    }
  }
  return s;
}

char* xtract(char* s, char* p, int xml) {
  //if (xml) return xxml(s, p, xml);
  if (xml) return xxml(s, p, xml);
  const static char bracks[] = "(){}[]";

  s= spc(s);
  if (!s || !*s) return s;
  if (p && !*p) { result(p, s, strlen(s)); return 0; }

  // extract next path name needed
  char* pn= p;
  while(pn && *pn && strchr("./", *pn)) pn++;
  char* e= pn? strpbrk(pn, "/.["): 0;
  int l= !pn? 0: e? e-pn: strlen(pn);

  switch(*s) {
  case ',': return xtract(s+1, p, xml); // TODO: remove?
      
  case '(': case '{': case '[': {
    char q= bracks[strchr(bracks, *s)-bracks+1];
    s++; int n= 1;
    do {
      s= spc(s);
      if (*s==',') s= spc(s+1);
      char* a= s;
      s= xtract(s, p, xml);
      if (0==strncmp(pn, a, l)) {
	char* r= s= spc(s);
	s= xtract(s, e, xml);
	if (!e) result(e, r, s-r);
      }

      //printf("> %.*s\n", (int)(s-a), a);
      s= spc(s);
    } while(*s && *s!=q);
    assert(*s==q);
    return s+1; }
  case '"': case '\'': return xstr(s);
  case '0'...'9': case '-': case '.': return xnum(s);
  default: return xsym(s);
  }
}

void scan(char* s) {
  printf("\n??? %s\n", s);
  int xml = (*s=='<');
  xtract(s, "", xml);
  printf("\n\n");
  xtract(s, "/foo/bar/fie", xml);
  //xtract(s, "foo/bar/3/fie");
}

int main(int argc, char** argv) {
  scan("{foo: { bar: [1,2,{fie: \"fum\" },{fie: \"FUM\"}] fie: \"NOT\" }}");
  scan("<foo>...<bar></bar><bar></bar><bar>...<fie>fum</fie></bar><fie>NOT</fie><bar><fie>FUM</fie></bar></foo>");
  exit(1);
  // TODO: ?
  scan("(foo (bar 1 bar 2 bar (fie \"fum\")))");
  scan("((foo (bar . 1) (bar . 2) ( bar fie . \"fum\" )))");
}

