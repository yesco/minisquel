// Utils / String ? Math
// ======================
// (c) 2022 jsk@yesco.org

#ifndef JSK_INCLUDED_UTILS

#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

#define ZERO(z) memset(&z, 0, sizeof(z))

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

void* memdup(void* m, size_t size) {
  void* r= malloc(size);
  if (r && m) memcpy(r, m, size);
  return r;
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
  printf("\n\n---ENTERING DEBUGGER---\n\n");
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

void nl() { putchar('\n'); }
void indent(int n) {
  printf("%*s", n, ""); // lol
}

// Deletes trailing white space
char* rtrim(char* s) {
  if (!s) return s;
  int len= strlen(s);
  while (isspace(s[--len]))
    s[len]= 0;
  return s;
}

// Return pointer *INTO* string skipping whit espace
//
// BEWARE: if malloced...
char* ltrim(char* s) {
  return s? s+strspn(s, " \t\n\r") : s;
}

// Returns pointer *INTO* string, after truncating whitespace at end.
//
// BEWARE: if malloced...
char* trim(char* s) {
  return rtrim(ltrim(s));
}

void optmessage(char* name, char* s, int n) {
  if (s)
    printf("Variable '%s' set to '%s'\n", name, s);
  else 
    printf("Variable '%s' set to %d\n", name, n);
}

// Print to FILE a STRING with surrounding QUOTE char and quote DELIMITER
// quot>0 quote it, ignore width
// quot<0 => no surrounding quot, width respected
// delimiter>0 quote c==delim
// delimiter<=0 ignore width
// but will backslash \ -quote|quot
void fprintquoted(FILE* f, char* s, int width, int quot, int delim) {
  if (!s && delim==',') return; // csv: null
  if (!s) return (void)printf("NULL");
  if (!*s) return; // NULL
  if (debug>3) printf("[Q%dD%dW%d]", quot, delim, width);
  if (quot>0) { width-= 2; fputc(quot, f); }
  while(*s && (quot>0 || width-- > 1 || delim<=0)) {
    // TODO:corner case where one extra
    if (*s==quot) { width--; fputc(abs(quot), f); }
    if (*s==abs(delim)) { width--; fputc('\\', f); }
    else if (*s=='\\') {width--; fputc(*s, f); }
    // TODO: newline etc?
    fputc(*s, f);
    s++;
  }
  // print last char only or '*' for truncation
  if (quot<=0 && *s) fputc(s[1]?'*':*s, f);
  if (quot>0) fputc(quot, f);
}

// Arguments ala Program Options
// -----------------------------

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

long utime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_usec + 1000000*tv.tv_sec;
}

long mstime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_usec/1000 + 1000*tv.tv_sec;
}

long cpums() {
  return clock()*1000/CLOCKS_PER_SEC;
  // really unreliable?
  //struct timespec t;
  //clock_gettime(CLOCK_MONOTONIC, &t);
  //return t.tv_sec*1000 + t.tv_nsec/1000;
}

// https://en.m.wikipedia.org/wiki/Metric_prefix
char isoprefix[]= "yzafpum kMGTPEZY";
char isoprefixzero= 7;

// human print call hprint!
int hprint_hlp(double d, char* unit, int width) {
  // TODO: add width? now assumes 8
  if (fabs(d)>1e30) return 0;

  // prefix?
  int i= -1;
  char* bi= "";

  // try binary prefix
  double bp= log(fabs(d))/log(1024);
  i= (int)bp;
  double dd= d/pow(1024, i);
  // is it a nice whole number?
  if (abs(i)>0 && fabs(dd)>1 && (int)dd == dd) {
    bi= "i";
    width--;
    d= dd;
    i+= isoprefixzero;
  } else {
    i= -1; 
  }

  // decimals
  if (i==-1) {
    i= isoprefixzero;
    
    if (fabs(d)<0.0001)
      while(fabs(d)>0 && fabs(d)<1 && i>0) {
	d*= 1000;
	i--;
      }

    if (fabs(d)>1000*10)
      while(fabs(d)>1000 && i<strlen(isoprefix)+1) {
	d/= 1000;
	i++;
      }
  }

  // TODO: error!
//  if (i<0 || i>=strlen(isoprefix))
//    i= isoprefixzero;

  char c= isoprefix[i];
  // indicate using ≈ ??
  if (c==' ')
    printf("%*.5lg%s", width+1, d, unit);
  else
    printf("%*.4lg%c%s%s",
      width, d, isoprefix[i], bi, unit);
  return 1;
}

int hprint(double d, char* unit) {
  int width= (int)(6-strlen(unit)+strcount(unit, "\t"));
  int r= hprint_hlp(d, unit, width);
  if (r) return r;
  // fallback
  return printf("%*.5lg%s", width, d, unit);
}

// human strtod (1M, 2u)
double hstrtod(char* s, char** end) {
  char* x= s;
  double d= strtod(s, &x);
  if (end) *end= x;
  if (x==s) return d; else s= x;
  // got number - check suffix
  char* suffix= strchr(isoprefix, *s);
  int e= 0;
  if (!suffix || !*suffix) {
    // 1h=100 1d=0.1 1da=10
    if (*s== 'h') d*= 100;
    else if (*s== 'd')  // da or d
      d*= s[1]=='a' ? (++s, 10) : 0.1;
    else if (*s== 'c') d*= 0.01;
    else return d;
  } else
    e= suffix-isoprefix - isoprefixzero;

  ++s; // got one

  // binary prefixes
  // - https://en.m.wikipedia.org/wiki/Binary_prefix
  int base= 1000;
  if (*s=='i') { ++s; base= 1024; }

  if (end) *end= s;
  return d* pow(base, e);;
}


// magic files?
// tries to find it in ./ and ./Test/
FILE* openfile(char* spec) {
  if (!spec || !*spec) expected("filename");

  FILE* f= NULL;

  // popen? see if ends with '|'
  int len= strlen(spec);
  if (spec[len-1]=='|') {
    if (security) expected2("Security doesn't allow POPEN style queries/tables", spec);
    spec[strlen(spec)-1]= 0;
    if (debug) printf(" { POPEN: %s }\n", spec);
    f= popen(spec, "r");
  } else {

    // try open actual FILENAME
    // TODO: make a fopen_debug
    if (debug) printf(" [trying %s]\n", spec);
    f= fopen(spec, "r");

    // try open Temp/FILENAME
    if (!f) {
      char fname[NAMELEN]= {0};
      snprintf(fname, sizeof(fname), "Test/%s", spec);
      if (debug) printf(" [trying %s]\n", fname);
      f= fopen(fname, "r");
    }
  }

  nfiles++;
  return f;
}

FILE* expectfile(char* spec) {
  FILE* f= openfile(spec);
  if (!f) expected2("File not exist", spec);
  return f;
}

// opens a file:
// - type .sql - call ./sql on it...
// - type .csv just read it
FILE* _magicopen(char* spec) {
  // TODO:maybe not needed?
  spec= strdup(spec);
  // handle foo.sql script -> popen!
  if (endsWith(spec, ".sql")) {
    char fname[NAMELEN]= {0};
    snprintf(fname, sizeof(fname), "./minisquel --batch --init %s |", spec);
    free(spec);
    spec= strdup(fname);
  }
  if (debug) printf(" [trying %s]\n", spec);
  FILE* f= openfile(spec);
  free(spec);
  return f;
}
  

// opens a magic file
// - if "foobar |" run it and read output
// - try it as given
// - try open.csv if not exist
// - try open.sql and run it
// - fail+exit (not return) if fail
// - guaranteed to return file descriptor
FILE* magicfile(char* spec) {
  if (!spec || !*spec) expected("filename");
  spec= strdup(spec); // haha

  FILE* f= _magicopen(spec);
  if (!f && spec[strlen(spec)-1]!='|') {
    if (!f) { // .csv ?
      spec= realloc(spec, strlen(spec)+1+4);
      strcat(spec, ".csv");
      f= _magicopen(spec);
    }
    if (!f) { // .sql ?
      strcpy(spec+strlen(spec)-4, ".sql");
      f= _magicopen(spec);
    }
  }
  if (!f) expected2("File not exist (tried X X.csv X.sql", spec);
  if (debug && f) printf(" [found %s]\n", spec);
  free(spec);
  return f;
}

// reads till 0. i.e. whole TEXT file
// Returns NULL at error or malloced str
char* readfile(char* name) {
  FILE* f= fopen(name, "r");
  char* r= NULL;
  size_t len= 0;
  getdelim(&r, &len, 0, f);
  fclose(f);
  return r;
}



#define JSK_INCLUDED_UTILS

#endif
