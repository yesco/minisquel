// Symbols - Aka hashstrings, atoms, ...
// -------------------------------------
//       "There can be only one!"
//
// (>) 20222 jsk@yesco.org
//

#include <stdio.h>

#include "utils.c"


// replace by dynamic arrays
#define SYMS_MAX 1024*1024
#define SYMS_BYTES SYMS_MAX*72 // lol


// don't reset as it may be lazy allocated
char* syms[SYMS_MAX] = {NULL,""};
int symscount= 2;
//char symsheapcharsyms[SYMS_BYTES];

// ordered syms
struct {
  int sym;
  int i;
} foo;

// map from osym->sym
int osyms[SYMS_MAX] = {0, 1};
// map from sym->osym
int xsyms[SYMS_MAX] = {0, 1};
int osyms_ordered= 1;

char* symstr(int s) {
  if (s>= symscount) {
    printf("Illegal sym: %d\n", s);
    fprintf(stderr, "Illegal sym: %d\n", s);
    error("Illegal sym");
  }
  return syms[s];
}

// safe nullstrcmp!
int nullstrcmp(char* a, char* b) {
  if (a==b) return 0;
  if (!a) return -1;
  if (!b) return +1;
  return strcmp(a, b);
}

// compare symbols
int symcmp(int a, int b) {
  if (a==b) return 0;
  if (!a) return -1;
  if (!b) return +1;
  return nullstrcmp(symstr(a), symstr(b));
}

// TODO: idea use negative numbers to indicate it's "ordered", that way can detect "mixing"
int sym_owned(char* owned);

// looup STRING, or add a *copy*.
// 
// Returns: sym number
int sym(char* s) {
  return sym_owned(strdup(s));
}

int osym(char* s);
char* osymstr(int o);

// lookup OWNED string, if need adds
// the pointer, otherwise free:s it.
//
// Returrns: sym number
int sym_owned(char* owned) {
  char*s = owned;
  // find previous
  if (!s) return 0;
  // TODO: improve w hash?
  // or just keep in osyms order?
  for(int i=1; i<symscount; i++)
    if (0==strcmp(s, syms[i])) {
      free(owned);
      return i;
    }
  // not found - add
  if (symscount>SYMS_MAX)
    error("Broke the SYMS_MAX limit!");
  syms[symscount]= owned;
  // TODO: do insertion sort? above!
  osyms[symscount]= symscount;
  // if adding in higher than existing...
  if (osyms_ordered &&
      strcmp(syms[osyms[symscount-1]], s)<0) {
    ;
  } else {
    // do insertion sort to keep ordered!
    // TODO: binary search?
    //printf("---------INSERT: '%s'\n", s);
    int i;
    for(i=0; i<symscount; i++)
      if (nullstrcmp(osymstr(i), s)>0)
	break;
    //printf("iiiiiiiii=%d %d '%s'\n\n", i, symscount, osymstr(i));
    if (i<=symscount) {
      for(int j=symscount; j>i; j--)
 	osyms[j]= osyms[j-1];
      osyms[i]= symscount;
    }
  }
  return symscount++;
}

int osym(char* s) {
  int d= sym(s);
  if (!osyms_ordered)
    error("OSYMS broken/needs update\n");
  return xsyms[d];
}

char* osymstr(int o) {
  if (o>=symscount) return NULL; // TODO:
  return symstr(osyms[o]);
}


// xtra fast symcmp if have ordered
int xsymcmp(int a, int b) {
  if (a==b) return 0;
  if (!a) return -1;
  if (!b) return +1;
  int oa= xsyms[a], ob= xsyms[b];
  if (osyms_ordered) {
    return (oa > ob) - (ob > oa);
  } else
    error("OSYMS xsymcmp index borken!\n");
  return strcmp(symstr(a), symstr(b));
}

// sorting

int symsortcmp(char** a, char** b) {
  return nullstrcmp(*a, *b);
}

int psymsortcmp(char** a, char** b) {
  printf("osym cmp '%s' '%s'", *a, *b);
  int r= symsortcmp(a, b);
  printf(" => %d\n", r);
  return r;
}

// WARNING: this will change all symnums!
// could have "ordered syms" - osym?
// auto sort if changed, lol
// and have mapping table
void symssort() {
  qsort(syms, symscount, sizeof(char*), (void*)symsortcmp);
  osyms_ordered= 1;
}


int osymsortcmp(int* a, int* b) {
  //char* sa= symstr(osyms[*a]);
  //char* sb= symstr(osyms[*b]);
  char* sa= osymstr(*a);
  char* sb= osymstr(*b);
  return psymsortcmp(&sa, &sb);
}

// if didn't reuse any numbers....? and had gap? could then detect "old" numbers...
void osymssort() {
  printf("Doesn't friggin' work!\n");
  exit(1);
  
  //if (osyms_ordered) return;

  // TODO: neede? all numbers there
  if (0)
  for(int i=0; i<symscount; i++)
    osyms[i]= i;

  qsort(osyms, symscount, sizeof(int), (void*)osymsortcmp);
  // create mapping sym->osym
  for(int i=0; i<symscount; i++) {
    xsyms[osyms[i]]= i;
    printf("%d %d '%s'\n", i, osyms[i], symstr(osyms[i]));
  }
  osyms_ordered= 1;
}

void test(int s) {
  char* str= symstr(s);
  printf("%d '%s' -> %d\n", s, str, sym(str));
}
       
void dump() {
  printf("\nDump\n----\n");

  for(int s=0; s<symscount; s++)
    printf("%3d %3d %3d  '%s'\n", s, osyms[s], xsyms[s], symstr(s));

  printf("%s\n", osyms_ordered?"ORDERED":"MESSY");
  for(int s=0; s<symscount; s++)
    printf("%3d %3d %3d  '%s'\n", s, osyms[s], xsyms[s], osymstr(s));

  printf("\n");
}

void simple() {
  sym("zero");
  sym("one");
  sym("two");
  sym("three");
  sym("four");
  sym("five");
  sym("six");
  sym("seven");
  sym("eight");
  sym("nine");
  return;
      
  printf("\nSimple\n-----\n");
  int c= sym("circus");
  int a= sym("a");
  int b= sym("abba");
  test(sym(NULL));
  test(sym(""));
  test(a);
  test(b);
  test(c);
  test(sym(NULL));
  test(sym(""));
  dump();
}

int main(int argc, char** argv) {
  simple(); dump();
  printf("AAAA\n");
  //symssort();
  //osymssort();
  printf("BBBB\n");
  simple(); dump();
  printf("CCCCC\n");
}
