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
    else printf("IX> NULL           %3d  @%5d\n", ko->type, ko->o);
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
  int sorted;
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
  ix->sorted= 0;
  ix->kos= kos;
  return ix;
}

void cleanko(keyoffset* ko) {
  if (!ko) return;
  if (ko->type==1) {
    free(ko->val.lstr);
    ko->val.lstr= NULL;
  }
}

void freeindex(memindex* ix) {
  if (!ix) return;
  // clear all allocated strings
  keyoffset* kos= ix->kos;
  keyoffset* end= kos + ix->n;
  while(kos <end) {
    if (kos->type==1) cleanko(kos);
    kos++;
  }
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
  // TODO: update in each Xaddix
  //   if already ordered, no need sort!
  ix->sorted= 0;
  
  if (!ensurix(ix, 1)) return NULL;
  return ix->kos + ix->n++;
}
  
// TODO: make stats per index?
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

// is the string copied?
// TODO: already allocated?
//    take char** s !! set null!
keyoffset* sixadd(memindex* ix, char* s, int offset) {
  keyoffset* ko= addix(ix);
  if (!ko) return NULL;
  ko->o= offset;

  setstrko(ko, s);
  return ko;
}
    
keyoffset* dixadd(memindex* ix, double d, int offset) {
  keyoffset* ko= addix(ix);
  if (!ko) return NULL;
  ko->o= offset;

  ko->val.d= d;
  ko->type= 16; // double

  return ko;
}
    

long ncmpko= 0;
long neqko= 0;

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

// if equal, order in offset order!
// use for sort...
int cmpko_offset(const void* va, const void* vb) {
  int r= cmpko(va, vb);
  if (r) return r;
  const keyoffset *a= va, *b= vb;
  return (a->o > b->o) - (b->o > a->o);
}

void printix(memindex* ix) {
  printf("\n\n========== %s\n", ix->name);

  int groups= 0, same= 0;

  for(int i=0; i<ix->n; i++) {
    keyoffset* ko= ix->kos+i;

    // TODO: FAULTTY!
    //int eq= i && eqko(ko-1, ko);
    
    int eq= i && 0==cmpko(ko-1, ko);

    if (eq) same++;
    if (i==ix->n-1 || i && !eq && i && same>16) {
      // TODO: report last group!
      printf("...\n");
      printko(ko-1);
      groups++;
      printf("-- GROUP %d COUNT: %d ( %3.1f%% )\n\n", groups, same, 100.0*same/ix->n);
      same= 0;
    }

    if (same < 16) {
      printko(ko);
    } else {
      //printf(" @%d", ko->o);
    }
  }
  printf("------ %d entries %d groups\n", ix->n, groups);
  printf("nstrdup=%ld bstrdup=%ld\n", nstrdup, bstrdup);
  long bytes= bstrdup + ix->n*sizeof(keyoffset) + nstrdup*(16/2+4);
  printf("Bytes: %ld\n\n\n", bytes);
}

// returns NULL or found keyoffset
keyoffset* sfindix(memindex* ix, char* s) {
  keyoffset ko= {0};
  setstrko(&ko, s);

  keyoffset* r= bsearch(&ko, ix->kos, ix->n, sizeof(keyoffset), cmpko);

  cleanko(&ko);
  return r;
}

// return pos where s would be inserted
// TODO: implement
keyoffset* searchix(char* s) {
  fprintf(stderr, "searchix: not implemented\n"); exit(1);
  return NULL;
}

void sortix(memindex* ix) {
  if (!ix) return;
  if (ix->sorted) return;
  qsort(ix->kos, ix->n, sizeof(keyoffset), cmpko_offset);
  ix->sorted= 1;
}
