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

// In STRING count occurances of SUBSTR.
// ("foo bar boo", "oo") -> 2
// ("foooo", "oo") ->  3 !
int strcount(char* s, char* sub) {
  if (!s || !sub) return 0;
  int n= 0;
  while((s= strstr(s, sub))) {
    n++;
    s++;
  }
  return n;
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

long nlike= 0, nlike_last= 0;

int like_hlp(char* s, char* match, int ilike) {
  nlike_last++;
  if (debug>4) printf("\t\tlike '%s' '%s'\n", s, match);
  if (!s || !match) return 0;
  char c= *s, m= *match;
  if (!m) return !c;
  if (!c && *match && strchr("%*", m))
    return !match[1]; // '*' in last pos!
  if (ilike) {c=toupper(c);m=toupper(m);}
  if (c==m || strchr("?_", m))
    return like_hlp(s+1, match+1, ilike); // eq
  if (strchr("%*", m)) {
    if (!match[1]) return 1;
    if (1) {
      // fast non-stack-deep '*'
      char* sp= s;
      while(*sp)
	if (like_hlp(sp++, match+1, ilike)) return 1;
      return 0;
    } else {
      // exhausts stack...
      return like_hlp(s, match+1, ilike)
	||   like_hlp(s+1, match, ilike);
    }
  }
  // fail
  return 0;
}

// SQL LIKE function implementation
int like(char* s, char* match, int ilike) {
  nlike++;
  nlike_last= 0;
  int r= like_hlp(s, match, ilike);
  if (debug>3 || nlike_last>100000) {
    printf("\n{ nlike=%ld nlike_last=%ld '%s' '%s' }\n", nlike, nlike_last, s, match);
  }
  return r;
}

// -- Error handling/fatal, exit

void (*print_exit_info)(char*,char*);

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/prctl.h>

// - https://stackoverflow.com/questions/4636456/how-to-get-a-stack-trace-for-c-using-gcc-with-line-number-information/4732119#4732119
void print_stacktrace() {
  char pid_buf[30];
  sprintf(pid_buf, "%d", getpid());
  char name_buf[512];
  name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
  int child_pid = fork();
  if (!child_pid) {
    dup2(2,1); // redirect output to stderr - edit: unnecessary?
    execl("/usr/bin/gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
    abort(); /* If gdb failed to start */
  } else {
    waitpid(child_pid,NULL,0);
  }
}
 
void debugger() {
  char cmd[100]= {};
  snprintf(cmd, sizeof(cmd), "gdb -p %d -iex 'set pagination off' -n -ex thread -ex where", getpid());
  system(cmd);
}

// - https://stackoverflow.com/questions/4636456/how-to-get-a-stack-trace-for-c-using-gcc-with-line-number-information/4732119#4732119
void (*when_things_go_bang)()= NULL;

void install_signalhandlers(void* bang) {
  /* Install our signal handler */
  struct sigaction sa;

  when_things_go_bang= bang;
  
  sa.sa_handler = (bang?(void*)bang:(void*)print_stacktrace);
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);
  /* ... add any other signal here */
}

// TODO: I had a errorf?

void error(char* msg) {
  fprintf(stdout, "Error: %s\n", msg);
  fprintf(stderr, "Error: %s\n", msg);
  // TODO: return 1 for shortcut?
  //fprintf(stderr, "At>%s<\n", ps);
  printf("! EXIT\n");
  print_stacktrace();
  if (when_things_go_bang)
    when_things_go_bang();
  // else print_stacktrace();
  //char* null= NULL; *null= 42;
  // TODO: longjmp!
  exit(1);
}

void expected2(char* msg, char* param) {
  fprintf(stdout, "%% Error: expected %s\n", msg);
  fprintf(stderr, "%% Error: expected %s\n", msg);
  if (param) printf("  %s\n", param);
  if (print_exit_info) print_exit_info(msg, param);
  printf("! EXIT\n");
  if (when_things_go_bang)
    when_things_go_bang();
  //else print_stacktrace();
  //char* null= NULL; *null= 42;
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

// Time Functions

#include <time.h>

char* isotime() {
  // from: jml project
  static char iso[sizeof "2011-10-08T07:07:09Z"];
  time_t now;
  time(&now);
  strftime(iso, sizeof iso, "%FT%TZ", gmtime(&now));
  return iso;
}

long timems() {
  return clock()*1000/CLOCKS_PER_SEC;
  // really unreliable?
  //struct timespec t;
  //clock_gettime(CLOCK_MONOTONIC, &t);
  //return t.tv_sec*1000 + t.tv_nsec/1000;
}

// human print
int hprint(double d, char* unit) {
  // TODO: add width? now assumes 8
  // TODO: negative
  if (d<0 || d>1e20) return 0;

  char suffix[]= "afpum kMGTPE";
  int i=5;
  if (d<0.0001)
    while(d>0 && d<1 && i>0) {
      d*= 1000; i--;
    }
  if (d>1000*1000)
    while(d>1000 && i<strlen(suffix)+1) {
      d/= 1000; i++;
    }
  char c= suffix[i];
  // indicate using ≈ ??
  if (c==' ')
    printf("%7.5lg%s", d, unit);
  else
    printf("%6.4lg%c%s", d, suffix[i], unit);
  return 1;
}

#define JSK_INCLUDED_UTILS

#endif
