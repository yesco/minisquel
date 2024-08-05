#include <stdio.h>
#include <stdlib.h>

// "Minimal" Iterator
// ======================
// (c) 2022 jsk@yesco.org
//

// vtab type information
typedef struct iter_type {
  char* name;
  size_t size;
  // we use void* to avoid casting
  void* next;    // required
  void* dup;     // optional
  void* release; // optional
} iter_type;

// must be head of every iterator/copy it
typedef struct iterator {
  iter_type* type;
  long n;
} iterator;

typedef int (*nextf)(void* it);
typedef void* (*dupf)(void* it, void* to);
typedef void (*releasef)(void* it);

// general iterator
//
// returns 1 if have result
//   0 if none
int iter(void* it) {
  iterator* i= it;
  iter_type* t= i->type;
  nextf f= t->next;
  int r= f(i);
  if (r>0) i->n++;
  return r;
}

// call by specific constructor
void* iternew(iter_type* type) {
  iterator* to= calloc(1, type->size);
  to->type= type;
  return to;
}
  
// Duplicate ITerator storing in TO if
// provided, or allocate it.
void* iterdup(void* it, void* to) {
  iterator* i= it;
  iter_type* t= i->type;
  dupf dup= t->dup;
  // alloc if needed
  if (!to) to= iternew(t);
  if (!to) return NULL;
  // default impl: copy
  memcpy(to, it, t->size);
  if (!dup) return to;
  return dup? dup(it, to): NULL;
}

// use for stack allocations
void iterrelease(void* it) {
  if (!it) return;
  iterator* i= it;
  releasef release= i->type->release;
  if (release) release(it);
}

// use for allocated generators
void iterfree(void* it) {
  iterrelease(it);
  free(it);
}

  

// - double iterator example
typedef struct double_iter {
  iter_type* type;
  long n;

  double cur, end, step;
} double_iter;

// simple ierator
int double_next(double_iter* it) {
  it->cur= it->n==0 ? it->cur :
    it->cur + it->step;
  return it->cur <= it->end;
}
		
iter_type* DOUBLES= &(iter_type){"doubles", sizeof(double_iter), double_next, NULL, NULL};
  
// simple constructor
double_iter* doubles(double start, double end, double step) {
  double_iter* it= iternew(DOUBLES);
  if (!it) return it;
  it->cur= start;
  it->end= end;
  it->step= step;
  return it;
}

// 1 1.33333 1.66667 2 2.33333 2.66667 3
void example_doubles() {
  double_iter* it= doubles(1.0, 3.0, 1.0/3);
  while(iter(it)) {
    printf("%lg ", it->cur);
  }
  iterfree(it);
}

// - complex filelines iterator example
// Can be dup:ed to store position
typedef struct flines_iter {
  iter_type* type;
  long n;
  
  char* fname;
  FILE* f; // can't be shared
  long offset; // where to read

  char* cur; // can't be shared
  size_t len;
} flines_iter;

int flines_next(flines_iter* it) {
  long o= ftell(it->f); // no cost
  // reset if "rewinded"
  //printf("FTELL= %ld OFFSET=%ld\n", o, it->offset);
  if (it->offset != o)
    if (fseek(it->f, it->offset, SEEK_SET) < 0)
      return 0;
  if (getline(&it->cur, &it->len, it->f) < 0)
    return 0;
  it->offset= ftell(it->f);
  return 1;
}
		
flines_iter* flines_dup(flines_iter* it, flines_iter *to) {
  //printf("DUP: %p\n", it->f);
  // can't share allocations...
  // TODO: potentially really expensive...
  // TODO: lazily open in _next!!!

  // 2.5x slower
  // to->f= fopen(to->fname, "r");

  to->cur= NULL; to->len= 0;
  return to;
}

void flines_release(flines_iter* it) {
  //fclose(it->f);
  // 2.5x slower
  // free(it->cur);
}

iter_type* FLINES= &(iter_type){"flines", sizeof(flines_iter), flines_next, flines_dup, flines_release};

flines_iter* flines(char* filename) {
  FILE* f= fopen(filename, "r");
  if (!f) return NULL;
  // resetable!
  flines_iter* it= iternew(FLINES);
  if (!it) { fclose(f); return NULL; }
  it->fname= filename;
  it->f= f;
  return it;
}

// 1 a/2 b/3 c/4 d/5 e/5 f/6 g/7 h/8 i
void example_flines() {
  flines_iter* it= flines("count.txt");
  flines_iter* clone= NULL;
  int n= 20;
  while(n-- && iter(it)) {
    printf("%2ld: %s", it->n, it->cur);
    if (it->n == 4) {
      printf("save\n");
      clone= iterdup(it, NULL);
    }
    if (it->n == 6 && clone) {
      printf("restore\n");
      iterfree(it);
      it= clone; clone= NULL;
    }
  }
  iterfree(clone);
  iterfree(it);
}

// - complex slines iterator example
// Can be dup:ed to store position
typedef struct slines_iter {
  iter_type* type;
  long n;
  
  char* str;
  char* cur;
} slines_iter;

int slines_next(slines_iter* it) {
  static long max= 0;
  if (!*it->str) return 0;
  //printf("%d\n", strlen(it->str));
  if (1) {
    char* s= it->str;
    while(*s && *s!='\n') s++;
    long n= s - it->str + 1;
    if (max<n || !it->cur) {
      max= n+10;
      it->cur= realloc(it->cur, max);
    } 
    strncpy(it->cur, it->str, n);
    *(it->cur+n)= 0;

    it->str+= n;
  } else {
    int rt= sscanf(it->str, "%m[^\n]s ", &it->cur);
    printf("sscanf.rt=%d\n", rt);
    if (rt!=1) return 0;
    it->str+= strlen(it->cur)+1;
  }
  return 1;
}
		
void slines_release(slines_iter* it) {
  free(it->cur);
}

iter_type* SLINES= &(iter_type){"fSines", sizeof(slines_iter), slines_next, NULL, NULL};

slines_iter* slines(char* str) {
  slines_iter* it= iternew(SLINES);
  if (!it) return NULL;
  it->str= str;
  return it;
}



// 1 a/2 b/3 c/4 d/5 e/5 f/6 g/7 h/8 i
void example_slines() {
  slines_iter* it= slines("1 a\n2 b\n3 c\n4 d\n5 e\n5 F\n6 g");
  slines_iter* clone= NULL;
  int n= 20;
  while(n-- && iter(it)) {
    printf("%2ld: %s", it->n, it->cur);
    if (it->n == 4) {
      printf("save\n");
      clone= iterdup(it, NULL);
    }
    if (it->n == 6 && clone) {
      printf("restore\n");
      iterfree(it);
      it= clone; clone= NULL;
    }
  }
  iterfree(clone);
  iterfree(it);
}
// end iterator

// 1 a/2 b/3 c/4 d/5 e/5 f/6 g/7 h/8 i
void speed_flines() {
  //flines_iter* it= flines("count.txt");
  flines_iter* it= flines("happy.csv");
  flines_iter* clone= NULL;
  int n= 1000000;
  int nfopen= 0;
  int iii= -1;
  while(n-- && ((iii=iter(it)))) {
    //printf("%2ld: %s", it->n, it->cur);
    if (it->n==6 && clone) {
      //printf("restore\n");
      iterfree(it);
      it= clone; clone= NULL;
      //printf("f%p %ld\n", it->f, it->offset);
    }
    //printf(" %2ld: %s", it->n, it->cur);
    if (it->n==4) {
      iterfree(clone);
      //printf("save\n");
      clone= iterdup(it, NULL);
      nfopen++;
    }
    //printf(" %2ld offset=%ld %s", it->n, it->offset, it->cur);
  }
  printf("\n\nii===%d\n", iii);
  iterfree(clone);
  iterfree(it);
  printf("work left= %d nfopen=%d\n", n, nfopen);
}

void speed_slines() {
  slines_iter* it= slines("1 a\n2 b\n3 c\n4 d\n5 e\n5 F\n6 g");
  slines_iter* clone= NULL;
  int n= 1000000;
  int nfopen= 0;
  int iii= -1;
  while(n-- && ((iii=iter(it)))) {
    //printf("%2ld: %s", it->n, it->cur);
    if (it->n==6 && clone) {
      //printf("restore\n");
      iterfree(it);
      it= clone; clone= NULL;
      //printf("f%p %ld\n", it->f, it->offset);
    }
    //printf(" %2ld: %s", it->n, it->cur);
    if (it->n==4) {
      iterfree(clone);
      //printf("save\n");
      clone= iterdup(it, NULL);
    }
    //printf(" %2ld offset=%ld %s", it->n, it->offset, it->cur);
  }
  printf("\n\nii===%d\n", iii);
  iterfree(clone);
  iterfree(it);
  printf("work left= %d nfopen=%d\n", n, nfopen);
}

// reading happy takes               40-60ms
// scanning happy 10x takes 907ms   = 90ms
// 800ms 10x iterator NO COPY LINES = 80ms
// 560ms 10x scan char 22MB         = 56ms
// 650ms 10x counting newlines!     = 65ms
void speed_happy() {
  char* happy= NULL;
  if (1) {
    FILE* f= fopen("happy.csv", "r");
    const long sz= 22501526l;
    happy= calloc(1, sz+10);
    long rd= fread(happy, 1, sz+1, f);
    printf("rd=%ld\n", rd);
    fclose(f);
  } else {
    FILE* f= fopen("count.txt", "r");
    const long sz= 28;
    happy= calloc(1, sz+10);
    long rd= fread(happy, 1, sz+1, f);
    printf("rd=%ld\n", rd);
    fclose(f);
  }
  printf("len=%ld\n", strlen(happy));
  
  long n= 0;
for(int i=0; i<10; i++) {
  if (1)  { 
    for(char* s= happy; *s; s++) {
      if (*s=='\n') // 100ms more!!!
	n++;
    }
  } else {
    slines_iter* it= slines(happy);
    while(iter(it)) {
      n++;
      //fprintf(stderr, ".");
      //printf(">>>%s\n", it->cur);
    }
    iterfree(it);
  }
  printf("\ncount=%ld\n", n);
}
 
}

int debug=0, stats=0;

#include "utils.c"

void speed_cols(int m) {
  const int rows= 110000; // 110K rows
  char *chars= malloc(rows*sizeof(char));
  int *ints= malloc(rows*sizeof(int));
  long *longs= malloc(rows*sizeof(long));
  double *doubles= malloc(rows*sizeof(double));

  //for(int m=0; m<200; m++) {
  long n= 0;
  //exit(0); // 5ms

  long NN= 200;
  
  for(int nn=0; nn<NN; nn++)
    { 
  
    n=0;
  
  switch(m){

  case 1:
    // 377 ms 1000x

    // char processing more expensive!!!!
    // 12.935s 200x 1000x
    for(int ii=0; ii<1000; ii++) {
      char *v= chars;
      for(int i=0; i<rows; i++) {
	if (!*v++) n++; // 298ms ?
	//if (*v++ == 42) n++; // 366ms
      }
    }
    break;
  case 2:
    //  378 ms 1000x
    // 9.99s 200x 1000x
    for(int ii=0; ii<1000; ii++) {
      int *v= ints;
      for(int i=0; i<rows; i++) {
	if (!*v++) n++;
      }
    }
    break;
  case 3:
    // 370  ms 1000x
    // 9.39s 200x 1000x
    for(int ii=0; ii<1000; ii++) {
      long *v= longs;
      for(int i=0; i<rows; i++) {
	if (!*v++) n++;
      }
    }
    break;
  case 4:
    // 375ms 1000x
    // 8.55s 200x 1000x
    for(int ii=0; ii<1000; ii++) {
      double *v= doubles;
      for(int i=0; i<rows; i++) {
	if (!v++) n++; // 350ms
	//if (!doubles[i]) n++; // 395ms
      }
    }
    break;
  case 5:
    //  s 1000x
    for(int ii=0; ii<1000; ii++) {
      double *dend= doubles+rows;
      double *v= doubles;
      while(v<dend) {
	if (!*v++) n++;
      }
    }
    break;

  case 100: {
    // 110K^2 = 5.177s !!!
    // same as duckdb x product!
    long ms= cpums();
    int* aa= ints;
    for(int a=0; a<rows; a++) {
      int* bb= ints;
      for(int b=0; b<rows; b++) {
	//if (ints[a]==ints[b]) n++;
	if (*aa == *bb++) n++;
      }
      aa++;
    }
    printf("int 110K^2= %ld ms\n", cpums()-ms);
    break; }

  case 101: {
    // DOUBLES:
    // 110K^2 = 5.2s !@!!
    // - double same perf as int!
    long ms= cpums();
    double* aa= doubles;
    for(int a=0; a<rows; a++) {
      double* bb= doubles;
      for(int b=0; b<rows; b++) {
	//if (ints[a]==ints[b]) n++;
	if (*aa == *bb++) n++;
      }
      aa++;
    }
    printf("doubles 110K^2= %ld ms\n", cpums()-ms);
    break; }
}

  if (n) printf("%d %4d %lu\n", nn, m, n);
 }
  if (n) printf("%lu\n", n);
 
}

int main(int argc, char** argv) {
  //example_slines(); exit(0);

  if (1) {
    if (argc<2) {
      printf("Usage: ./a.out  M\n");
      exit(3);
    }
    int m= atoi(argv[1]);
    if (!m) m= 200;
    speed_cols(m);
  } else if (1)
    speed_happy();
  else if (1)
    speed_slines();
  else
    speed_flines();
  exit(0);

  example_doubles();
  putchar('\n');
  example_flines();
}
