#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

// PANDA LANGUAGE
// ==============
// pandalang.org is an online enviornment implementing
// the Panda Language.

// This is a C language implementation, of a simple parser,
// with a no-frills grammar. It even simplifies panda.

// An expression is a number or a string:
//
//     42
//     'foobar'
//
// A function calls is
//
//     42 sqrt
//
// A composite point-free style of function alls
//
//     1 to 10 sqr lt 55
//
// For all numbers from 1 to 10.
// Square them.
// Keep the ones less than 55.
// Then print it.
//
// 

int nval= 0;

void spc(char** p) {
  while(**p && isspace(**p)) (*p)++;
}

// TODO: reuse same constants...
int str(char** s) {
  return 0;
}

// TODO: scan and store all const num/'strings'/:foo
#define KMAX 1024
char* konst[KMAX];
int nk= 0, klen[KMAX];

void konstractor(char* p) {
  while(*p) {
    switch(*p) {
    case ' ': case '\t': case '\n': case '\r': p++; break;
    case '0'...'9': case '.': case '-': {
      konst[nk]= p;
      do {
	p++;
      } while (*p && (isdigit(*p) || *p=='.' || *p=='e'));
      klen[nk]= p-konst[nk];
      nk++;
      break; }
    case '"': case '\'': {
      konst[nk]= p;
      char e= *p;
      do {
	if (*p=='\\') p++;
	p++;
      } while(*p && *p!=e);
      if (*p) p++;
      klen[nk]= p-konst[nk];
      nk++;
      break; }
      //case ':': p++; r= str(&p, ' '); break;
    default: p++;
    }
  }
}

int ik= 0;

int expr(char** p) {
  spc(p);
  if (*p==konst[ik]) {
    *p+= klen[ik];
    return ik++;
  }
  // TODO: paren expression
  // TODO: +-*/ math should use proper preceedence
  return 0;
}

// a function has a preceeding argument O (or 0) lol
int fun(char** s, int o) {
  spc(s);
  char* endn= *s;

  // read name
  char* n= endn;
  while(*endn && isalnum(*endn))
    endn++;
  *s= endn;
  if (endn==n) return 0;

  #define NPAR 16
  int par[NPAR]= {0};
  int npar= 0;

  // keep reading args
  // TODO: allow fun(1 2 3) ?
  do {
    assert(npar<16);
    if (o) par[npar++]= o;
  } while((o= expr(s)));

  // result num
  int r= ++nval;

  printf("%.*s -%d", (int)(endn-n), n, r);
  for(int i=0; i<npar; i++)
    printf(" %d", par[i]);
  printf(" 0\n");
 
  return r;
}


int panda(char** p) {
  spc(p);
  int r= expr(p);
  while(**p) {
    int f= fun(p, r);
    if (f) r= f;
  }
  return r;
}

int main(int argc, char** argv) {
  argv++; argc--; // skip program name

  // Extract all constants
  char** a= argv;
  while(*a) konstractor(*a++);
  for(int i=0; i<nk; i++)
    printf(": %.*s\n", klen[i], konst[i]);

  nval= nk;
  
  while(*argv) {
    char* p = *argv++;
    // TODO: keep doing more expressions?
    int r= panda(&p);
    if (!r && *p) {
      printf("ERROR.panda: at >>>%s\n", p);
      exit(1);
    }

    printf("out %d 0\n", r);
  }
}
