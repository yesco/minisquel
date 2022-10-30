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

typedef int(*hasheq)(hashentry*, void*);

typedef struct hashtab {
  hashentry** arr;
  int size;
  int n;
  hasheq eq;
  char* heap;
} hashtab;


hashtab* newhash(int size, hasheq eq) {
  // TODO: resize?
  size= size<=0 ? HASH_DEFAULT : size;
  hashtab *ht = calloc(1, sizeof(*ht));
  if (!ht) return NULL;
  ht->arr= calloc(1, sizeof(*(ht->arr)));
  ht->size= size;
  ht->eq= eq;
  return ht;
}

void freehash(hashtab* ht) {
  if (!ht) return;
  for(int i=0; i<ht->size; i++) {
    hashentry* e= ht->arr[i];
    while(e) {
      hashentry* next= e->next;
      if (e->data) free(e->data);
      free(e);
      e= next;
    }
  }
  free(ht);
}

hashentry* findhash(hashtab* ht, char* s) {
  int len= strlen(s?s:"");
  hashval h= s?larsons_hash(s, len):0;
  int i= h % ht->size;
  
  hashentry *e= ht->arr[i];
  while(e) {
    while(e->h != h) e= e->next;
    if (!e) return NULL;
    // equal hash
    if (!ht->eq || ht->eq(e, s)) break;
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
    while(e->h != h) e= e->next;
    if (!e) return NULL;
    // equal hash
    if (!ht->eq || ht->eq(e, s)) break;
    e= e->next;
  }

  if (!e) {
    e= calloc(1, sizeof(*e));
    e->h= h;
    e->next= ht->arr[i];
    ht->arr[i]= e;
    ht->n++;
  } else if (e->data)
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

int main(void) {
  hashtab *h= newhash(10, NULL);
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

