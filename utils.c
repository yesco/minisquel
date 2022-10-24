// Utils / String ? Math
// ======================
// (c) 2022 jsk@yesco.org

#ifndef JSK_INCLUDED_UTILS

#include <ctype.h>
#include <stdlib.h>
#include <math.h>

int min(int a, int b) { return a<b?a:b; }
int max(int a, int b) { return a>b?a:b; }

double drand(double min, double max) {
  double range = (max - min); 
  double div = RAND_MAX / range;
  return min + (rand() / div);
}



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
    realloc(s?s:strdup(""), l+1+(s?strlen(s):1)),
    add?add:"", l);
}


// -- Error handling/fatal, exit

void error(char* msg) {
  fprintf(stdout, "Error: %s\n", msg);
  fprintf(stderr, "Error: %s\n", msg);
  // TODO: return 1 for shortcut?
  //fprintf(stderr, "At>%s<\n", ps);
  exit(1);
}

// TODO: I had a errorf?

void expected2(char* msg, char* param) {
  fprintf(stdout, "Error: expected %s\n", msg);
  fprintf(stderr, "Error: expected %s\n", msg);
  if (param) printf("  %s\n", param);
  //fprintf(stderr, "At>%s<\n", ps);
  exit(1);
}

void expected(char* msg) {
  expected2(msg, NULL);
}

// TODO: warningf


#define JSK_INCLUDED_UTILS

#endif
