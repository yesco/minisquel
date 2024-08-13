// Poor mans path extractor (xmlpath, jsonpath, assoc, proplist)
//
// Extract using a path like "/foo/bar[3]/fie" => fum
//
// + 74 LOC
// + parse once
// + no internal memory alloctions, except for result
// + results as array of matches (offset,len)
// - results as occur in source string

// Path:
//   xmlpath-style:  $foo.bar[3].fie
//   jsonpath-style: /foo/bar/3/fie
//
//   xml:  <foo>..<bar>...<bar>...<fie>fum</fie>
//   json: { foo: { bar: { fuu: .. } ... bar: .. fie: "fum" } }

// TODO:  { "foo": { "bar": { "fuu": .. } ... "bar": .. "fie": "fum" } }


// TODO: toplevel: / $
// TODO: numbered match or index in array: [3]
// TODO: wildcard strict (/): * or // ..
//

// Related:
//   A simple C only parse library:
//     * 239 LOC
//     * querying an existing string
//     * no memory allocations
//     * returns number of matches
//     * must ask for match N
//     * constucts key for each match
//   - https://github.com/arisi/c-json-path

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

// TODO: use???
//char* xnext(char* s) {
//  return strpbrk(s, " \t\n\r()[]{,;}</>=:\\\"\'");
//}

char* spc(char* s) {
  while(s && isspace(*s)) s++;
  return s;
}

// TODO: remove?
char* xatm(char* s) {
  // TODO: unicode use stopchars?
  while(*s && isalnum(*s)) s++;
  return s;
}

// TODO: remove?
char* xsym(char* s) {
  s= xatm(s);
  assert(*s==':');
  return s+1;
}

// TODO: how to gather result, not just print...
//   idea: int* offset<<16+len ... 0
void result(char* p, char* r, int n) {
  if (n==0 || !*r) printf("---NORESULT\n");
  else printf("=> %.*s\n", n, r);
}


// Extract from xml using simple path
//   TODO: results... collection
//   21 LOC
char* xxml(char* s, char* p, int level) {
  if (!s || !*s) return s;
  if (p && !*p) { result(p, s, strlen(s)); return 0; }

  // extract next path name needed
  char* pn= p;
  while(pn && *pn && strchr("./", *pn)) pn++;
  char* e= pn? strpbrk(pn, "/.["): 0;
  int l= !pn? 0: e? e-pn: strlen(pn);

 next: while(*s) {
    if (*s++!='<') continue;

    // <!-- comment? -->   <? and... ?>
    if (*s=='!') { s= strstr(s, "-->"); continue; }
    if (*s=='?') { s= strstr(s, "?>"); continue; }

    // </tag end
    if (*s=='/') return s-1;

    // <tag - start
    char* a= s;  s= xatm(s);
    int match= (0==strncmp(pn, a, l));
    // TODO: do correctly @attr write xattr? (a=b c:d)
    while(*s && *s!='>')
      if (*s++=='/') { s++; goto next; } // <foo/> - no content

    char* r= ++s;  s= xxml(s, match? e: p, level+2);
    if (match && !e)
      result(e, r, (int)(s-r));
      
    // xxml returns just before </tag> - no check name...
    assert(*s=='<');
    while(*s && *s!='>') s++;
  }
  return s;
}

// Extract from STRING using PATH, if XML set 1
//
// Returns: a continuation pointer for parsing,
//   TODO: relatively useless, lol. used internally
//
// TODO: idea - return number of matches
// TODO: idea - ask for match N

//   TODO: results... collection
//   Quirk handles [1,2,3,foo:bar] - nodejs...
//   28 LOC
//   TODO: better choose xml/json/properties...
char* xtract(char* s, char* p, int xml) {
  if (xml) return xxml(s, p, xml);

  s= spc(s);
  if (!s || !*s) return s;
  if (p && !*p) { result(p, s, strlen(s)); return 0; }

  // extract current name (and len) + next path
  char* pn= p;
  while(pn && *pn && strchr("./", *pn)) pn++;
  char* e= pn? strpbrk(pn, "/.["): 0;
  int l= !pn? 0: e? e-pn: strlen(pn);

  switch(*s) {
  case ',': return xtract(s+1, p, xml);
      
  case '(': case '{': case '[': {
    s++; int n= 1;
    do {
      s= spc(s);
      if (*s==',') s= spc(s+1); // comma is optional...
      char* a= s;  s= xtract(s, p, xml);
      // TODO: if match string it's >"foo\nbar" ... need our own strncmp?
      // TODO: BUG: ["foo" "bar"] find "foo" will return "bar"...
      //   (It's right for assoc list ("foo" "bar") )
      if (0==strncmp(pn, a, l)) {
	char* r= s= spc(s);  s= xtract(s, e, xml);
	if (!e) result(e, r, s-r);
      }
      s= spc(s);
    } while(*s && *s!='}' && *s!=']' && *s!=')');
    return s+1; }
  case '"': case '\'': { char q= *s++;
      while(*s && *s!=q) s+= 1+(*s=='\\'); return s+1; }
  case '0'...'9': case '-': case '.':
    return s+strspn(s, "0123456789eE.+-");
  // TODO: not match sym with : but without
  default: return xsym(s);
  }
}

// ENDWCOUNT

#ifndef MAIN

void scan(char* s) {
  printf("\n---??? %s\n", s);
  int xml = (*s=='<');
  xtract(s, "", xml);
  printf("\n\n");
  xtract(s, "/foo/bar/fie", xml);
  //xtract(s, "foo/bar/3/fie");
}

int main(int argc, char** argv) {
  // TODO: test xml comments?
  scan("{foo: { bar: [1,2,{fie: \"fum\" },{fie: \"FUM\"}] fie: \"NOT\" }}");
  scan("{'foo': { 'bar': [1,2,{'fie': \"fum\" },{'fie': \"FUM\"}] 'fie': \"NOT\" }}");
  scan("<foo>...<bar></bar><bar></bar><bar>...<fie>fum</fie></bar><fie>NOT</fie><bar><fie>FUM</fie></bar></foo>");
  exit(1);
  // TODO: ?
  scan("(foo (bar 1 bar 2 bar (fie \"fum\")))");
  scan("((foo (bar . 1) (bar . 2) ( bar fie . \"fum\" )))");
}

#endif
