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
  else printf("===> %.*s\n", n, r);
}

// TODO: not  working, looses the p...
char* xxml(char* s, char* p, int xml) {
  s= spc(s);
  if (!s || !*s) return s;
  if (p && !*p) { result(p, s, strlen(s)); return 0; }

  // extract next path name needed
  char* pn= p;
  while(pn && *pn && strchr("./", *pn)) pn++;
  // TODO: handle ':' separate (?)
  char* e= pn? strpbrk(pn, "/.["): 0;
  int l= !pn? 0: e? e-pn: strlen(pn);

  while(*s && *s!='<') s++;
  if (!*s) return s;
  assert(*s=='<');
  s= spc(s+1);
  
  if (*s!='/') { // start tag
    char* r= s= spc(s);
    s= xatm(s);
    if (0==strncmp(pn, r, l)) {
      printf("%*s---> %.*s %s\n", xml, "",(int)(s-r), r, e);
    }
    // TODO: handle attributes...
  } else { // end tag
    char* r= s+1;
    s= xatm(spc(r));
    xml-= 2;
    printf("%*s<--- %.*s %s\n", xml, "",(int)(s-r), r, e);
    s= spc(s);
    assert(*s=='>');
    //printf(">>>>> %s\n", s+1);
    // continue processing "same level"
    return xxml(s+1, p, xml);
  }
    
  s= spc(s);
  if (*s=='/') { // single tag
    // TODO: right?
    s= spc(s+1);
    assert(*s=='>');
    s++;
    return s;
  }

  // process content of tag
  s= spc(s);
  assert(*s=='>');
  //printf("\t\t\tXML: '%s'  '%s'\n", e, s+1);
  char* r= s+1;
  s= xxml(r, e, xml+2);
  if (!e) result(e, r, s-r);
  return s;
}

char* xxml2(char* s, char* p, int level) {
  if (!s || !*s) return s;
  if (p && !*p) { result(p, s, strlen(s)); return 0; }

  // extract next path name needed
  char* pn= p;
  while(pn && *pn && strchr("./", *pn)) pn++;
  char* e= pn? strpbrk(pn, "/.["): 0;
  int l= !pn? 0: e? e-pn: strlen(pn);

  // TODO: skip spaces inside tag parsing...

  //printf("XXX: %*s%d %s    %.10s\n", level, "", level, p, s);
  while(*s) {
    if (*s=='<') {
      s++;

      // TODO: handle comment?

      // end tag
      if (*s=='/') {
	printf("%*s<--- %.10s\n", level-2, "", s);
	return s-1;
      }

      // start tag
      printf("%*s---> %.10s\n", level, "", s);
      char* a= s;
      s= xatm(s);
      int match= (0==strncmp(pn, a, l));
      int single= 0;
      while(*s && *s!='>')
	if (*s++=='/') single= 1;

      // TODO: do correctly... skip attributes?
      // TODO: handle @attr name extract

      s++;
      if (!single) {
	char* r= s;
	s= xxml2(s, match? e: p, level+2);
	if (match && !e)
	  result(e, r, (int)(s-r));
	// skip </MATCH>
	assert(*s=='<');
	while(*s && *s!='>') s++;
      }
    } else { // ignore all else text
      s++;
    }
  }
  return s;
}

char* xscan3(char* s, char* p, int xml) {
  //if (xml) return xxml(s, p, xml);
  if (xml) return xxml2(s, p, xml);
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
  case ',': return xscan3(s+1, p, xml); // TODO: remove?
      
  case '(': case '{': case '[': {
    char q= bracks[strchr(bracks, *s)-bracks+1];
    s++; int n= 1;
    do {
      s= spc(s);
      if (*s==',') s= spc(s+1);
      char* a= s;
      s= xscan3(s, p, xml);
      if (0==strncmp(pn, a, l)) {
	char* r= s= spc(s);
	s= xscan3(s, e, xml);
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
  //xscan(s, 0);
  //xscan2(s);
  int xml = (*s=='<');
  xscan3(s, "", xml);
  printf("\n\n");
  xscan3(s, "/foo/bar/fie", xml);
  //xscan3(s, "foo/bar/3/fie");
}

int main(int argc, char** argv) {
  scan("{foo: { bar: [1,2,{fie: \"fum\" },{fie: \"FUM\"}] fie: \"NOT\" }}");
  scan("<foo>...<bar></bar><bar></bar><bar>...<fie>fum</fie></bar><fie>NOT</fie><bar><fie>FUM</fie></bar></foo>");
  exit(1);
  // TODO: ?
  scan("(foo (bar 1 bar 2 bar (fie \"fum\")))");
  scan("((foo (bar . 1) (bar . 2) ( bar fie . \"fum\" )))");
}

