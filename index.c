// Minisquel Indices Experiments
// -----------------------------
//    2022 (c) jsk@yesco.org
//

#include <stdio.h>
#include <stdlib.h>

typedef struct keyoffset {
  // TODO: make union so overlap w 
  char s[11];      // 11 chars
  // TDOO: handle long strings
  // type 0 inline string/zero terminate!
  //      1 pointer tostring
  //      2 offset to string
  //      3 double
  //    255 NULL 12 bytes 255!
  //  alt 0 NULL 12 bytes all null! (=="")
  char type;       // 1 type/zero terminator
  unsigned int o;  // 4 bytes
} keyoffset;

typedef struct dkeyoffset {
  double d;     // 8 bytes
  char x[3];    // unused 3 bytes
  char type;    // see keyoffset struct
  unsigned int o;  // 4 bytes
} dkeyoffset;

#define IX_MAX 1024
keyoffset ix[IX_MAX]= {0};
int nix= 0;

void printko(keyoffset* ko) {
  printf("IX> %-16s' %d %d\n", ko->s, ko->type, ko->o);
}

void add(char* s, int offset) {
  keyoffset* ko= ix + nix++;
  if (nix >= IX_MAX) {
    fprintf(stderr, "INDEX FULL\n");
    exit(66);
  }
  if (strlen(ko->s) > 11) {
    fprintf(stderr, "INDEX: str too long\n");
    exit(33);
  }
    
  strncpy(ko->s, s, 12); // fills w zeros
  ko->type= 0; // zeero-terminate/trunc
  ko->o= offset;
}
    
void printix() {
  printf("\n==========\n");
  for(int i=0; i<nix; i++)
    printko(ix + i);
}

int cmpkeyoffset(const void* a, const void* b) {
  // implement for all types!
  return memcmp(a, b, sizeof(((keyoffset*)0)->s));
}

// return -1, or index position where >= s
keyoffset* searchix(char* s) {
  return bsearch(s, ix, nix, sizeof(keyoffset), cmpkeyoffset);
}

int main(int argc, char** argv) {
  printf("pointer bytes=%lu\n", sizeof(argv));
  printf("keyoffset bytes=%lu\n", sizeof(keyoffset));
  printf("dkeyoffset bytes=%lu\n", sizeof(dkeyoffset));

  keyoffset ko= {.type= 77, .o=4711};
  printf("%-16s %d %d\n", ko.s, ko.type, ko.o);
  strncpy(ko.s, "ABCDEFGHIJKLMNO", 12);
  ko.typezero= 0;
  printf("%-16s %d %d\n", ko.s, ko.type, ko.o);
  add("foo", 2);
  add("bar", 3);
  add("fie", 5);
  add("fum", 7);
  add("foobar", 11);
  add("abba", 13);
  printix();

  //qsort(ix, sizeof(ix)/sizeof(*ix), sizeof(*ix), cmpkeyoffset);
  qsort(ix, nix, sizeof(keyoffset), cmpkeyoffset);
  printix();

  keyoffset* ks= searchix("fum");
  printko(ks);
}
