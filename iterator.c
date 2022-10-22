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
  if (!to) to= calloc(1, t->size);
  if (!to) return NULL;
  // default impl: copy
  if (!dup) return memcpy(it, to, t->size);
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
  if (it->offset != o)
    if (fseek(it->f, it->offset, SEEK_SET) < 0)
      return 0;
  if (getline(&it->cur, &it->len, it->f) < 0)
    return 0;
  it->offset= ftell(it->f);
  return 1;
}
		
flines_iter* flines_dup(flines_iter* it, flines_iter *to) {
  *to= *it;
  // can't share allocations...
  // TODO: potentially really expensive...
  // TODO: lazily open in _next!!!
  to->f= fopen(to->fname, "r");
  to->cur= NULL; to->len= 0;
  return to;
}

void flines_release(flines_iter* it) {
  fclose(it->f);
  free(it->cur);
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
    printf("%2d: %s", it->n, it->cur);
    if (it->n==4) {
      printf("save\n");
      clone= iterdup(it, NULL);
    }
    if (it->n==6 && clone) {
      printf("restore\n");
      iterfree(it);
      it= clone; clone= NULL;
    }
  }
  iterfree(clone);
  iterfree(it);
}
// end iterator




int main(int argc, char** argv) {
  example_doubles();
  putchar('\n');
  example_flines();
}
