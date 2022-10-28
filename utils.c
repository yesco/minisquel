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

char* endsWith(char *s, char *end) {
  if (!s || !end) return NULL;
  int sl= strlen(s), el= strlen(end);
  if (!sl || !el) return NULL;
  return (0==strcmp(s+sl-el, end))?
    s+sl : NULL;
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

//////////////////
// String routines

#include <strings.h>
#include <string.h>

void optmessage(char* name, char* s, int n) {
  if (s)
    printf("Variable '%s' set to '%s'\n", name, s);
  else 
    printf("Variable '%s' set to %d\n", name, n);
}

// variable, set to message reporter for variable changes
void (*optmsg)(char* name, char* s, int num) = optmessage;

// Extract number from argument
// --foo      -> 1
// --foo=on   -> 1
// --foo=off  -> 1
// --foo=37   -> 37
// --foo99    -> 99
// --no-foo   -> 0
int arg2int(char* arg) {
  if (strstr(arg, "=on")) return 1;
  if (strstr(arg, "=off")) return 0;
  if (strstr(arg, "--no-")) return 0;
  if (strstr(arg, "--no")) return 0; // ?
  size_t i= strcspn(arg, "0123456789");
  if (i==strlen(arg)) return 1;
  int neg= (i && arg[i-1]=='-')? -1: +1;
  return neg * atoi(arg+i);
}

// Return 1 if matched, set *v
// v is optional, can be NULL
int optint(char* name, int* v, char* arg) {
  if (*arg!='-' || !strstr(arg, name)) return 0;
  if (v) *v= arg2int(arg);
  if (v && optmsg) optmsg(name, NULL, *v);
  return 1;
}

// Look for NAMEd option, store it in STR copying MAX chars from ARG.
//
// ARG: "--format=csv"  => "csv"
//      "--format csv"  => "fish"
// ( NO "--csv"         => "csv" )
//
// 
// 
// If STR==NULL just return 1 if match.
// If MAX is negative, assume it's a pinter to a string pointer, deallocate/reallocate.
//
// If (MAX<0)
// - use existing storage (MAX>0)
// char filename[32];
// optstr("file", filename, sizeof(filename));

// - free/allocates new string (MAX<0)
// char* filename= NULL;
// optstr("file", &filename, -1);
//
// Return: 1 if matched
int optstr(char* name, void* s, int max, char* arg) {
  char* found= strstr(arg, name);
  if (*arg!='-' || !found) return 0;
  char* p= strchr(arg, '=');
  if (!p) p= strchr(arg, ' ');
  //if (!p && !*(found+strlen(name))) p= found-1;
  if (!s || !p)
    return 0;
  else if (max>0) strncpy(s, p+1, max);
  else {
    free(*(char**)s);
    *(char**)s= strdup(p+1);
  }
  if (s && p && optmsg)
    optmsg(name, max>0 ? (char*)s : *(char**)s, 0);
  return 1;
}


#define JSK_INCLUDED_UTILS

#endif
