// hash.c
// ----------------------
// (>) 2022 jsk@yesco.org

// repurposed from:
// - https://github.com/yesco/esp-lisp/blob/master/symbols.c

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// not power of 2 as ascii is regular
#define HASH_DEFAULT 63


#define LARSONS_SALT 0

static unsigned long larsons_hash(const char* s, int len) {
  unsigned long h = LARSONS_SALT;
  while (len-- > 0 && *s)
    h = h * 101 + *(unsigned char*)s++;
  return h;
}

// http://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
//static unsigned int int_hash(unsigned int x) {
//    x = ((x >> 16) ^ x) * 0x45d9f3b;
//    x = ((x >> 16) ^ x) * 0x45d9f3b;
//    x = ((x >> 16) ^ x);
//    return x;
//}

typedef uint64_t hashval;

typedef struct hashentry {
  struct hashentry* next;
  hashval h;
  void* data;
} hashentry;

struct arena;
struct hashtab;

typedef int(*hasheq)(struct hashtab*,void*, void*);

typedef struct hashtab {
  hashentry** arr;
  int size;
  int n;
  hasheq eq;
  int freedata; // free hashentry.data
  struct arena* arena;
} hashtab;


hashtab* newhash(int size, hasheq eq, int freedata) {
  // TODO: resize?
  size= size<=0 ? HASH_DEFAULT : size;
  hashtab *ht = calloc(1, sizeof(*ht));
  if (!ht) return NULL;
  ht->arr= calloc(size, sizeof(void*));
  ht->size= size;
  ht->eq= eq;
  ht->freedata= freedata;
  return ht;
}

void freehash(hashtab* ht) {
  if (!ht) return;
  for(int i=0; i<ht->size; i++) {
    hashentry* e= ht->arr[i];
    while(e) {
      hashentry* next= e->next;
      if (ht->freedata && e->data) free(e->data);
      free(e);
      e= next;
    }
  }
  free(ht->arena);
  free(ht);
}

hashentry* findhash(hashtab* ht, char* s) {
  int len= strlen(s?s:"");
  hashval h= s?larsons_hash(s, len):0;
  int i= h % ht->size;
  
  hashentry *e= ht->arr[i];
  while(e) {
    while(e && e->h!=h) e= e->next;
    if (!e) return NULL;
    // equal hash
    if (!ht->eq || ht->eq(ht, e->data, s)) break;
    e= e->next;
  }
  return e;
}

// if found, replace data
hashentry* addhash(hashtab* ht, char* s, void* data) {
  int len= strlen(s?s:"");
  hashval h= s?larsons_hash(s, len):0;
  int i= h % ht->size;
  
  hashentry *e= ht->arr[i];
  while(e) {
    while(e && e->h!=h) e= e->next;
    if (!e) break;
    // equal hash
    if (!ht->eq || ht->eq(ht, e->data, s)) break;
    e= e->next;
  }

  if (!e) {
    e= calloc(1, sizeof(*e));
    e->h= h;
    e->next= ht->arr[i];
    ht->arr[i]= e;
    ht->n++;
  } else if (ht->freedata && e->data)
    free(e->data);
  
  e->data= data;
  return e;
}

// print the slots
void printhash(hashtab* ht) {
  if (!ht) return;
  printf("\n----- hashtab (%d items)\n", ht->n);
  int n= 0;
  for(int i=0; i<ht->size; i++) {
    hashentry* e= *(ht->arr + i);
    if (e) {
      n++;
      printf("%3d : ", i);
      int nn = 0;
      while(e) {
	nn++;
	e= e->next;
      }
      if (nn) printf(" --- #%d\n", nn);
    }
  }
  printf("=== %d slots used\n", n);
}

typedef struct arena {
  char* mem;
  int size;
  int top;
  int align;
} arena;

arena* newarena(int size, int align) {
  if (align<=0) align= 1;
  if (size<=0) size= 10*1024;
  arena* a= calloc(1, sizeof(*a));
  if (!a) return NULL;

  a->mem= malloc(size);
  if (!a->mem) { free(a); return NULL; }
  a->size= size;
  a->top= 0;
  a->align= align;

  return a;
}

int addarena(arena* a, void* data, int size) {
  if (!a) return -1;
  if (a->top+size+a->align >= a->size) {
    int newsize= a->size*1.3;
    if (newsize - a->size < 1024) newsize+= 1024;
    char* mem= realloc(a->mem, newsize);
    if (!mem) return -1;
    a->mem= mem;
    a->size= newsize;
  }

  // allocate data
  int i= a->top;
  // align
  a->top+= size;
  while(a->top % a->align)
    a->mem[a->top++]= 0;

  memcpy(a->mem+i, data, size);
  return i;
}

void* arenaptr(arena*a, int i) {
  if (!a) return NULL;
  return &a->mem[i];
}

int saddarena(arena* a, char* s) {
  if (!s) return -1;
  int len= strlen(s);
  return addarena(a, s, len+1);
}

void printarena(arena* a) {
  printf("--- arena (size=%d top=%d)\n", a->size, a->top);
  char* c= a->mem;
  for(int i=0; i<a->top; i++) {
    if (!*c) printf("\\0");
    else putchar(*c);
    c++;
  }
  printf("<<<\n");
}

void testarena() {
  arena* a= newarena(0,1);
  int foo= saddarena(a, "foo");
  int bar= saddarena(a, "bar");
  int fiefum= saddarena(a, "fiefum");
  printarena(a);
  for(int i=0; i<10000; i++) {
    char s[32];
    snprintf(s, sizeof(s), "FOO-%d-BAR", i);
    int ix= saddarena(a, s);
    printf("%d @ %d %s\n", i, ix, (char*)arenaptr(a, ix));
  }
  printarena(a);
  printf("BAR: %s\n", (char*)arenaptr(a, bar));
  exit(0);
}

// atoms / hashstrings
static hashtab* atoms= NULL;

int hashstr_eq(hashtab* ht, void* a, void* b) {
  char* sa= arenaptr(ht->arena, *(long*)a);
  char* sb= b;
  return strcmp(sa, sb);
}

int atom(char* s) {
  if (!atoms) {
    atoms= newhash(700000, NULL, 0);
    atoms->arena= newarena(0, 1);
    atom(""); // take pos 0! lol
  }
  hashentry* e= findhash(atoms, s);
  if (e) return (long)(e->data);
  long i= saddarena(atoms->arena, s);
  e= addhash(atoms, s, (void*)i);
  return i;
}

char* atomstr(int a) {
  return arenaptr(atoms->arena, a);
}  

void readdict() {
  FILE* f= fopen("wordlist-1.1M.txt", "r");
  //FILE* f= fopen("Test/count.txt", "r");
  char* word= NULL;
  size_t len= 0;
  int n= 0;
  while(getline(&word, &len, f)!=EOF) {
    int l= strlen(word);
    if (word[l-1]=='\r') word[l-1]= 0, l--;
    if (word[l-1]=='\n') word[l-1]= 0, l--;
    if (word[l-1]=='\r') word[l-1]= 0, l--;
    n++;
    //printf("startADD: %s\n", word);
    int s= atom(word);
    //printf("endADD: %s\n====\n\n", symstr(s));
    if (n%1000 == 0) fputc('.', stderr);
    //if (n%1000 == 0) fputc('.', stdout);
  }
  printf("# %d\n", n);
  fclose(f);
}

void testatoms() {
  int a= atom("foo");
  int b= atom("bar");
  int c= atom("foo");

  printf("%d %d %d\n", a,b,c);

  printf("%s %s %s\n", atomstr(a),atomstr(b),atomstr(c));

  printhash(atoms);

  printf("%s %s %s\n", atomstr(a),atomstr(b
),atomstr(c));

  readdict();

  //printhash(atoms);
  //printarena(atoms->arena);
  freehash(atoms);
  
  exit(0);
}

int main(void) {
  testatoms();
  hashtab *h= newhash(10, NULL, 0);
  printhash(h);

  hashentry* f= addhash(h, "foo", NULL);
  hashentry* b= addhash(h, "bar", NULL);
  printhash(h);

  freehash(h);
}


// thinking for MiniSQueL
// ----------------------
// = probabilty of collsion
// values: 2 2^n
//
// fillness:
//
// 2^(n/2) =>      50% chance collision
//        /10 =>   1%
//            /4    0.1%
//
// 
// 32 bits, uniq 4M values: 2 clashes!
// 64 bits, uniq 2B values: 2 clashes!
//
// conclusion:
// - can use for group/by (is no groups!)
// - can't use for join
// REF - https://en.m.wikipedia.org/wiki/Birthday_problem

