// Minisquel Indices Experiments
// -----------------------------
//    2022 (c) jsk@yesco.org
//

// an idea for in-mmeory index storage
// - skeyoffset entries: string/double
// - double inline in 8 bytes + type
// - null is an empty string = 0...0
// - strings <11 chars inline
// - longer strings, is pointed to
//
// - hashstrings/atoms for longer strs
// - reference counting

#include <stdio.h>
#include <stdlib.h>

#define GROWIX 1000

typedef struct keyoffset {
  union choice {
    char s[8];   // it's "really s[12];
    char* lstr;  // to free
    double d;
  } val;

  // overwritten by type=0: string
  //   TODO: use for other types
  char x[3];

  // TODO: unify type names/constants?
  // type 0 & "\0" for inline string s[12]
  //      0 NULL If all s[] == 0
  //      1 pointer to string
  //    ( 2 reserved - str )
  //    ( 3 reserved - str )
  //    ( 4 reserved - str )
  //     16 double
  // (  255 NULL 12 bytes 255! )
  char type;
  int o;
} keyoffset;

const char* strix(const keyoffset* a) {
  if (!a->type) return (const char*)&a->val.s[0];
  if (a->type!=1) return "";
  // long string
  return a->val.lstr;
}

void printko(keyoffset* ko) {
  if (!ko) return;
  switch(ko->type) {
  case 1: // lstring
  case 0: { // string
    const char *s= strix(ko);
    if (*s) 
      printf("IX> '%-16s' %3d  @%5d\n", s, ko->type, ko->o);
    else printf("IX> NULL\n");
    break; }
  case 16: { // double
    printf("IX> %18lg %3d  @%5d\n", ko->val.d, ko->type, ko->o); break; }
  default: printf("IX> Unknown type=%d\n", ko->type);
  }
}

// Indexing
///////////

typedef struct memindex {
  char* name;
  int n;
  int max;
  keyoffset *kos;
} memindex;

memindex* newindex(char* name, int max) {
  if (max < 100) max= 100;
  keyoffset* kos= calloc(max, sizeof(keyoffset));
  if (!kos) return NULL;
  memindex* ix= calloc(1, sizeof(memindex));
  if (!ix) { free(kos); return NULL; }

  ix->name= name;
  ix->n= 0;
  ix->max= max;
  ix->kos= kos;
  return ix;
}

void freeindex(memindex* ix) {
  if (!ix) return;
  free(ix->kos); ix->kos= NULL;
  ix->max= 0;
  free(ix);
}

int ensurix(memindex* ix, int n) {
  if (!ix) return 0;
  if (ix->max - ix->n > n) return 1;
  int max= ix->max + n + GROWIX;
  // potentially big allocation, can fail!
  keyoffset* kos= realloc(ix->kos, max * sizeof(keyoffset));
  if (!kos) {
    fprintf(stderr, "%% WARNING: ensurix - Allocation failed!\n)"); return 0; }
  ix->kos= kos;
  ix->max= max;
  return 1;
}

keyoffset* addix(memindex* ix) {
  if (!ensurix(ix, 1)) return NULL;
  return ix->kos + ix->n++;
}
  
long nstrdup= 0, bstrdup= 0;

void setstrko(keyoffset* ko, char* s) {
  // 1.1M words
  // - 11 chars  99s  <--- !
  // -  7 chars 169s
  size_t len= strlen(s);
  if (len > 11) {
  //if (strlen(s) > 7) {
    nstrdup++;
    bstrdup+= len;

    ko->val.lstr= strdup(s);
    ko->type= 1;
  } else {
    // fills w zeros, overwrites x & type!
    strncpy(ko->val.s, s, 12);
    ko->type= 0; // zero-terminate/trunc
  }
}

void cleanko(keyoffset* ko) {
  if (!ko) return;
  if (ko->type==1) {
    free(ko->val.lstr);
    ko->val.lstr= NULL;
  }
}

// is the string copied?
// TODO: already allocated?
//    take char** s !! set null!
keyoffset* sixadd(memindex* i, char* s, int offset) {
  keyoffset* ko= addix(i);
  if (!ko) return NULL;
  ko->o= offset;

  setstrko(ko, s);
  return ko;
}
    
keyoffset* dixadd(memindex* i, double d, int offset) {
  keyoffset* ko= addix(i);
  if (!ko) return NULL;
  ko->o= offset;

  ko->val.d= d;
  ko->type= 16; // double

  return ko;
}
    
void printix(memindex* ix) {
  printf("\n==========\n");
  for(int i=0; i<ix->n; i++)
    printko(ix->kos + i);
  printf("--- %d\n", ix->n);
}

long ncmpko= 0;

// ORDER: null=='' <<< double <<< string
int cmpko(const void* va, const void* vb) {
  const keyoffset *a= va, *b= vb;
  ncmpko++;

  //printf("CMD "); printko(a); printf("     "); printko(b); printf("\n");

  // TODO: too complicated!

  // - TYPE comparisons
  int ta= a->type, tb= b->type;
  // null? - smallest
  if (!ta && !*(a->val.s)) ta= 255;
  if (!tb && !*(b->val.s)) tb= 255;
  // different string types
  if (ta < 4) ta= 0;
  if (tb < 4) tb= 0;
  // differnt type cmp typenumber!
  if (ta != tb) return (tb > ta) - (ta > tb);

  // same type/compat
  switch(ta) {
    // lstring and inline string
  case  0: return strncmp(strix(a), strix(b), 11);
  case 16: return (a->val.d > b->val.d) - (b->val.d > a->val.d);
  default: return 1; // TODO: ?
  }
}

long neqko= 0;

// very fast eq (dismissal), 1 if equal
int eqko(keyoffset* a, keyoffset* b) {
  neqko++;
  if (a==b) return 1; // lol
  unsigned long la= *(unsigned long*)a;
  unsigned long lb= *(unsigned long*)b;
  if (a!=b) return 0; // sure NE
  // still not sure equal
  return 0==cmpko(a, b);
}

// returns NULL or found keyoffset
keyoffset* sfindix(memindex* i, char* s) {
  keyoffset ko= {0};
  setstrko(&ko, s);

  keyoffset* r= bsearch(&ko, i->kos, i->n, sizeof(keyoffset), cmpko);

  cleanko(&ko);
  return r;
}

// return pos where s would be inserted
// TODO: implement
keyoffset* searchix(char* s) {
  fprintf(stderr, "searchix: not implemented\n"); exit(1);
  return NULL;
}

void sortix(memindex* i) {
  if (!i) return;
  qsort(i->kos, i->n, sizeof(keyoffset), cmpko);
}
