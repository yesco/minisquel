// hash.c
// ----------------------
// (>) 2022 jsk@yesco.org

// This file contains:
// - dyanmic hashtable
// - dynamic arena memory allocator
// - atoms (string interning)/symbols

// --- Dynamically growing hashtable.

// If there is morethan 10 eleemnt/slot.
// It'll resize automatically (3x).

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "utils.c"


// --- Arena memory allocater

// An arena is a simple allocator that
// putses out data from a contigious
// memory pool. There is typically no
// deallocator for individual allocations.
//
// This arena returns an offset, that
// can be used to get a temporory pointer.
// This pointer should not be stored.
// Any other adds to the arena may
// move the memory to other addrees.
//
// Use and story only the index returned.

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

long arenaoptimize(arena* a) {
  // more like "truncate" waste
  long bytes= a->size - a->top;
  long newsize= a->align * ((a->top+a->align-1)/a->align);
  a->mem= realloc(a->mem, a->top);
  a->size= a->top;
  return bytes;
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

// WARNING: Do NOT store this pointer;
// it may cahnge after any new adds!
void* arenaptr(arena*a, int i) {
  if (!a) return NULL;
  return &a->mem[i];
}

// string add
int saddarena(arena* a, char* s) {
  if (!s) return -1;
  int len= strlen(s);
  return addarena(a, s, len+1);
}

long printarena(arena* a, int details) {
  if (!a) { putchar('\n'); return 0; }
  printf("--- arena (size=%d top=%d)\n", a->size, a->top);
  if (!details) return a->size;
  char* c= a->mem;
  for(int i=0; i<a->top; i++) {
    if (!*c) printf("\\0");
    else putchar(*c);
    c++;
  }
  printf("<<<\n");
  return a->size;
}


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
  int count; // optional?
  // int; for alignment
} hashentry;

struct hashtab;

typedef int(*hasheq)(struct hashtab*,void*, void*);

typedef struct hashtab {
  hashentry** arr;
  int size;
  int n;
  hasheq eq;
  int freedata; // free hashentry.data
  arena* arena;
  arena* ars;
} hashtab;


hashtab* newhash(int size, hasheq eq, int freedata) {
  size= size<=0 ? HASH_DEFAULT : size;
  hashtab *ht = calloc(1, sizeof(*ht));
  if (!ht) return NULL;
  ht->arr= calloc(size, sizeof(void*));
  ht->size= size;
  ht->eq= eq;
  ht->freedata= freedata;
  return ht;
}

void resizehash(hashtab* ht, int size) {
  //fprintf(stderr, "{Rrrr %d => ", ht->size);
  if (size<=0) size= 67;
  hashentry** arr= calloc(size, sizeof(void*));
  for(int i=0; i<ht->size; i++) {
    hashentry* e= *(ht->arr + i);
    while(e) {
      hashentry* next= e->next;
      e->next= NULL;
      int i= e->h % size;
      
      hashentry* o= arr[i];
      arr[i]= e;
      e->next= o;
		  
      e= next;
    }
  }
  free(ht->arr);
  ht->arr= arr;
  ht->size= size;
  //fprintf(stderr, "%d rrrr}", size);
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
    // now is equal hash
    if (!ht->eq || ht->eq(ht, e->data, s)) break; // FOUND
    e= e->next;
  }
  return e;
}

// if found, replace data
hashentry* addhash(hashtab* ht, char* s, void* data) {
  if (!ht) return NULL;
  if (ht->n / ht->size > 10)
    resizehash(ht, ht->size*3);
      
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
long printhashprinter(hashtab* ht, int details, long(*printer)(hashtab*,hashentry*)) {
  if (!ht) return 0;
  long bytes= 0;
  printf("\n----- hashtab (%d items/%d slots)\n", ht->n, ht->size);
  int n= 0;
  for(int i=0; i<ht->size; i++) {
    hashentry* e= *(ht->arr + i);
    if (e) {
      n++;
      if (details||printer) printf("%3d : ", i);
      int nn = 0;
      while(e) {
	if (printer) bytes+= printer(ht, e);
	nn++;
	e= e->next;
	bytes+= sizeof(hashentry);
      }
      //if (printer && nn) printf("\n");
      if ((details || printer) && nn) printf(" --- #%d\n", nn);
    }
  }
  bytes+= sizeof(hashtab);
  printf("=== %d slots used\n", n);
  printf("arena: "); bytes+= printarena(ht->arena, 0);
  printf("  ars: "); bytes+= printarena(ht->ars, 0);
  printf("BYTES: %ld\n", bytes);
  return bytes;
}

long printhash(hashtab* ht, int details) {
  return printhashprinter(ht, details, NULL);
}

// --- Atoms / Interned Strings (Pool)

// Use this to avoid storing duplicates.
// It'll return a string "number" that
// is unqiue (for its pool).
//
// Do not store pointer to the strings.

// atom(s) -> i
// atomstr(i)
//
// atomappend(s, data, len)
//
// atomarena(s) -> arena (for s)
//
// printatoms(hashentry)
//
// atomentry(s) -> hashentry

static hashtab* atoms= NULL;

typedef struct atomstorage {
  // must be long/8B and must be first!
  long i; // offset of atom string
  arena arena;
} atomstorage;

int hashstr_eq(hashtab* ht, long i, char* b) {
  //printf("EQ: %ld %s\n", i, b);
  if (i<0) {
    atomstorage* storage= arenaptr(ht->ars, -i);
    i= storage->i;
  }
  char* estr= arenaptr(ht->arena, i);
  //printf("EQ: %s %s\n", estr, b);
  return 0==strcmp(estr, b);
}

 
// will get existing or create
hashentry* atomentry(char* s) {
  if (!atoms) {
    atoms= newhash(0, (void*)hashstr_eq, 0);
    atoms->arena= newarena(0, 1);
    char* empty= "";
    // make sure not give offset 0, lol
    addarena(atoms->arena, empty, 1);
    // create an arena for arenas!
    // == macarena?
    atoms->ars= newarena(16*sizeof(atomstorage), sizeof(atomstorage));
    // make sure not give offset 0, lol
    addarena(atoms->ars, empty, 1);
  }

  hashentry* e= findhash(atoms, s);
  if (e) return e;

  long i= saddarena(atoms->arena, s);
  return addhash(atoms, s, (void*)i);
}

arena* atomarena(char* s) {
  hashentry* e= atomentry(s);
  if (!e) return NULL;

  // get storage and real i
  long i= (long)e->data;
  if (i>0) return NULL;

  atomstorage* storage= i>0 ? NULL : arenaptr(atoms->ars, -i);
  return &storage->arena;
}

long atomappend(char* s, void* data, int len) {
  hashentry* e= atomentry(s);
  e->count++;
  if (!e) return -1;

  // get storage and real i
  long i= (long)e->data;
  atomstorage* storage= i>0 ? NULL : arenaptr(atoms->ars, -i);
  if (storage) i= storage->i;
  assert(i>0);
  
  // no need store anything
  if (!data || len <= 0) return i;
  
  // we want to store - need storage
  if (!storage) {
    // allocate a new arena
    arena *a= newarena(16*sizeof(int), sizeof(int));

    // hacky, TODO: make initarena?
    atomstorage s= {};
    s.i= i;
    memcpy(&s.arena, a, sizeof(s.arena));
    long si= addarena(atoms->ars, &s, sizeof(s));
    // save this as as -offset
    e->data= (void*)-si;
    storage= arenaptr(atoms->ars, si);
  }

  // finally ready to add data
  long ai= addarena(&storage->arena, data, len);
  return i;
}

long atom(char* s) {
  hashentry* e= atomentry(s);
  if (!e) return -1;
  return (long)e->data;
}

char* atomstr(long a) {
  return arenaptr(atoms->arena, a);
}  

void dumpatoms(hashtab* ht) {
  error("more comples for neg i");
  for(int i=0; i<ht->size; i++) {
    hashentry* e= ht->arr[i];
    while(e) {
      long id= (long)e->data;
      printf("%s,%d\n", atomstr(id), e->count);
      e= e->next;
    }
  }
}

// return bytes
long atomprinter(hashtab* ht, hashentry* e) {
  long bytes= 0;
  if (!e) return 0;
  long i= (long)e->data;
  printf("\n    @%ld ", i);
  if (i<0) {
    atomstorage* storage= arenaptr(ht->ars, -i);
    i= storage->i;
  }
  printf("#%d ", e->count);
  printf("'%s' ", atomstr(i));

  arena* a= atomarena(atomstr(i));
  if (a) bytes+= printarena(a, 0);
  return bytes;
}

// return bytes
long printatoms(hashtab* ht) {
  return printhashprinter(atoms, 0, atomprinter);
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

