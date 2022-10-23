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
  //      3 (str)
  //      4 (str)
  //     16 double
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
  if (!ko) return;
  switch(ko->type) {
  case 0: // string
    printf("IX> %-16s'  %d3  @%5d\n", ko->s, ko->type, ko->o); break;
  case 16: { // double
    dkeyoffset* kd= (void*)ko;
    printf("IX>  %16lg %3d  @%5d\n", kd->d, kd->type, kd->o); break; }
  default: printf("IX> Unknown type=%d\n", ko->type);
  }
}

keyoffset* addix() {
  keyoffset* ko= ix + nix++;
  if (nix >= IX_MAX) {
    fprintf(stderr, "INDEX FULL\n");
    exit(66);
  }
  if (strlen(ko->s) > 11) {
    fprintf(stderr, "INDEX: str too long\n");
    exit(33);
  }
  return ko;
}
  
void sadd(char* s, int offset) {
  keyoffset* ko= addix();
    
  strncpy(ko->s, s, 12); // fills w zeros
  ko->type= 0; // zeero-terminate/trunc
  ko->o= offset;
}
    
void dadd(double d, int offset) {
  dkeyoffset* kd= (void*) addix();

  kd->d= d;
  kd->type= 16; // double
  kd->o= offset;
}
    
void printix() {
  printf("\n==========\n");
  for(int i=0; i<nix; i++)
    printko(ix + i);
}

char* strix(keyoffset* a) {
  // TODO: handle different strings
  return a;
}

// NULL, '' <<< double <<< string
int cmpkeyoffset(const void* a, const void* b) {
  // TODO: too complicated
  keyoffset  *ka= a, *kb= b;
  int ta= ka->type, tb= kb->type;
  dkeyoffset *kda= a, *kdb= b;
  double da= kda->d, db= kdb->d;
  // null? - smallest
  if (!ta && !*(ka->s)) ta= 255;
  if (!tb && !*(kb->s)) tb= 255;
  // different strings
  if (ta < 4) ta= 0;
  if (tb < 4) tb= 0;
  // differnt type cmp typenumber!
  if (ta != tb) return (tb > ta) - (ta > tb);
  // same type/compat
  switch(ta) {
  case  0: return strncmp(strix(a), strix(b), 11);
  case 16: return (da > db) - (db > da);
  default: return 1;
  }
}

// returns NULL or found keyoffset
keyoffset* findix(char* s) {
  return bsearch(s, ix, nix, sizeof(keyoffset), cmpkeyoffset);
}

// return -1, or index position where >= s
// TODO: this only finds exact match!
keyoffset* searchix(char* s) {
  return NULL;
}

int main(int argc, char** argv) {
  printf("pointer bytes=%lu\n", sizeof(argv));
  printf("keyoffset bytes=%lu\n", sizeof(keyoffset));
  printf("dkeyoffset bytes=%lu\n", sizeof(dkeyoffset));

  keyoffset ko= {.type= 77, .o=4711};
  printf("%-16s %d %d\n", ko.s, ko.type, ko.o);
  strncpy(ko.s, "ABCDEFGHIJKLMNO", 12);
  ko.type= 0;
  printf("%-16s %d %d\n", ko.s, ko.type, ko.o);
  dadd(-99, 23);
  sadd("foo", 2);
  sadd("bar", 3);
  sadd("fie", 5);
  sadd("fum", 7);
  sadd("foobar", 11);
  sadd("abba", 13);
  dadd(111, 17);
  dadd(22, 19);
  sadd("", 23);
  printix();

  qsort(ix, nix, sizeof(keyoffset), cmpkeyoffset);
  printix();

  // search
  char *f= "fum";
  printf("\nFIND '%s' \n  ", f);
  keyoffset* ks= findix(f);
  if (ks) printko(ks);
  else printf("NOT FOUND!\n");
}
