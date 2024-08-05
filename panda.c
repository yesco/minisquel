#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>

int nval= 0;

void spc(char** p) {
  while(**p && isspace(**p)) (*p)++;
}

// TODO: reuse same constants...
int num(char** s) {
  spc(s);
  char* p= *s;
  if (!*p || !isdigit(*p) && *p!='-' && *p!='.') return 0;

  // we don't care if it's well-formed
  do {
    p++;
  } while(*p && (isdigit(*p) || *p=='.' || toupper(*p)=='e'));

  nval++;
  printf(": %.*s\n", (int)(p-*s), *s);
  *s= p;
  return nval;
}

// TODO: reuse same constants...
int str(char** s) {
  return 0;
}

int expr(char** p) {
  int r= num(p);
  if (!r) r= str(p);
  // TODO: foo:bar as http://ibm.com foo:3
  // TODO: paren expression
  // TODO: +-*/ math should use proper preceedence
  return r;
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
  argv++; // skip program name
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
