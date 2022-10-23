// Minisquel Indices Experiments
// -----------------------------
//    2022 (c) jsk@yesco.org
//

// an idea for in-mmeory index storage
// - keyoffset entries: string/double
// - double inline in 8 bytes + type
// - null is an empty string = 0...0
// - strings <11 chars inline
// - longer strings, is pointed to
//
// - hashstrings/atoms for longer strs
// - reference counting

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

#define IX_MAX 15*1024*1024
keyoffset ix[IX_MAX]= {0};
int nix= 0;

void printko(keyoffset* ko) {
  if (!ko) return;
  switch(ko->type) {
  case 0: // string
    if (*ko->s)
      printf("IX> '%-16s' %3d  @%5d\n", ko->s, ko->type, ko->o);
    else printf("IX> NULL\n");
    break;
  case 16: { // double
    dkeyoffset* kd= (void*)ko;
    printf("IX> %18lg %3d  @%5d\n", kd->d, kd->type, kd->o); break; }
  default: printf("IX> Unknown type=%d\n", ko->type);
  }
}

keyoffset* addix() {
  keyoffset* ko= ix + nix++;
  if (nix >= IX_MAX) {
    fprintf(stderr, "%% ERROR: INDEX FULL\n");
    exit(66);
  }
  if (strlen(ko->s) > 11) {
    // TODO:
    fprintf(stderr, "%% ERROR: INDEX: str too long\n");
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
  printf("--- %ld\n", nix);
}

char* strix(keyoffset* a) {
  // TODO: handle different strings
  return a;
}

long ncmpko= 0;
// NULL, '' <<< double <<< string
int cmpko(const void* a, const void* b) {
  //printf("CMD "); printko(a); printf("     "); printko(b); printf("\n");
  ncmpko++;
  // TODO: too complicated!
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

long neqko= 0;
int eqko(keyoffset* a, keyoffset* b) {
  neqko++;
  if (a==b) return 1;
  unsigned long la= *(unsigned long*)a;
  unsigned long lb= *(unsigned long*)b;
  //if (a<b) return -1;
  //if (a>b) return +1;
  if (a!=b) return 0; // sure
  // still not sure equal
  return 0==cmpko(a, b);
}

// returns NULL or found keyoffset
keyoffset* findix(char* s) {
  keyoffset ks= {0};
  strncpy(ks.s, s, 12);
  ks.type= 0;
  return bsearch(&ks, ix, nix, sizeof(keyoffset), cmpko);
}

// return -1, or index position where >= s
// TODO: this only finds exact match!
keyoffset* searchix(char* s) {
  return NULL;
}

double drand(double min, double max) {
  double range = (max - min); 
  double div = RAND_MAX / range;
  return min + (rand() / div);
}

#include "mytime.c"

void readwords(char* filename) {
  long ms= timems();
  FILE* f= fopen(filename, "r");
  if (!f) exit(66);
  char* w= NULL;
  size_t l= 0;
  long n= 0;
  while(getline(&w, &l, f)!=EOF) {
    n++;
    int l= strlen(w);
    //printf("c=%d\n", w[l-1]);
    // stupid!
    if (l && w[l-1]==10) w[l---1]= 0;
    if (l && w[l-1]==13) w[l---1]= 0;
    sadd(w, ftell(f));
  }
  fclose(f);
  free(w);
  ms= timems()-ms;
  printf("read %ld words from %s in %ld ms\n", n, filename, ms);
}

// - wget https://raw.githubusercontent.com/openethereum/wordlist/master/res/wordlist.txt
// - wget https://download.weakpass.com/wordlists/1239/1.1million%20word%20list.txt.gz

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

  qsort(ix, nix, sizeof(keyoffset), cmpko);
  printix();

  // search
  {
    char *f= "fum";
    printf("\nFIND '%s' \n  ", f);
    keyoffset* kf= findix(f);
    if (kf) printko(kf);
    else printf("NOT FOUND!\n");
  }

  // words
  nix= 0;
  if (1) readwords("wordlist-1.1M.txt");
  else if (1) readwords("1.1million word list.txt");
  else readwords("wordlist-8K.txt");

  printf("\n==WORDS! %ld\n\n", nix);

  //printix();
  
  if (1) {
    // linear

    // 11.83s 100K linear 7.7K words
    // ^ cmpok
    // 3.34s 100K eqko !
    // 3.00s 100K strcmp (break f ptr)
    // = 100K * 7.7K= 770M tests
    //
    // 1.1M words
    // - 5.67s 1000x linear cmpko
    //   -0#: 6.34s
    // - 7.2s 10K linear eqko
    //   (63s if no opt 3!)
    // - 32.8s          strcmp
    keyoffset* kf= findix("yoyo");
    if (!kf) kf= findix("york");
    if (!kf) { printf("%%: ERROR no yoyo/york!\n"); exit(33); }

    long f= 0;
    //for(int i=0; i<10000000; i++) {
    //for(int i=0; i<100000; i++) {
    int n= 0;
    for(int i=0; i<1000; i++) {
      keyoffset* end= ix+nix;
      keyoffset* p= ix;
      //fprintf(stderr, ".");
      while(p<end) {
	n++;
	n+= i;
	if (0==cmpko(kf, p)) break;
	//if (eqko(kf, p)) break;
	//if (0==strcmp(kf, p)) break;
	p++;
      }
      if (ix>=end) p= NULL;
      if (!p) { printf("ERROR\n"); exit(33); }
      f++;
    }
    printf("Linear found %ld\n", f);
    printf("ncmpko=%ld\n", ncmpko);
    printf("neqko=%ld\n", neqko);
    printf("n=%ld\n", n);
  } else {
  // search
  // 10M times == 2.27s (8K words)
  for(int i=0; i<10000000; i++)
  {
    char *f= "yoyo";
    //printf("\nFIND '%s' \n  ", f);
    keyoffset* kf= findix(f);
    if (!kf) { printf("ERROR"); exit(3); }
    //if (kf) printko(kf);
    //else printf("NOT FOUND!\n");
  }
  }
}
