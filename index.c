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




#define IX_MAX 15*1024*1024
keyoffset ix[IX_MAX]= {0};
int nix= 0;


keyoffset* addix() {
  keyoffset* ko= ix + nix++;
  if (nix >= IX_MAX) {
    fprintf(stderr, "%% ERROR: INDEX FULL\n");
    exit(66);
  }

  return ko;
}
  
long nstrdup= 0, bstrdup= 0;

void setstrko(keyoffset* ko, char* s) {
  // 1.1M word
  // - 11 chars  99s
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
keyoffset* sixadd(char* s, int offset) {
  keyoffset* ko= addix();
  if (!ko) return NULL;
  ko->o= offset;

  setstrko(ko, s);
  return ko;
}
    
keyoffset* dixadd(double d, int offset) {
  keyoffset* ko= (void*) addix();
  if (!ko) return NULL;
  ko->o= offset;

  ko->val.d= d;
  ko->type= 16; // double

  return ko;
}
    
void printix() {
  printf("\n==========\n");
  for(int i=0; i<nix; i++)
    printko(ix + i);
  printf("--- %d\n", nix);
}

long ncmpko= 0;

// ORDER: null=='' <<< double <<< string
int cmpko(const void* va, const void* vb) {
  //printf("CMD "); printko(a); printf("     "); printko(b); printf("\n");
  ncmpko++;
  // TODO: too complicated!
  const keyoffset *a= va, *b= vb;
  int ta= a->type, tb= b->type;

  // null? - smallest
  if (!ta && !*(a->val.s)) ta= 255;
  if (!tb && !*(b->val.s)) tb= 255;
  // different strings
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
keyoffset* sfindix(char* s) {
  keyoffset ko= {0};
  setstrko(&ko, s);

  keyoffset* r= bsearch(&ko, ix, nix, sizeof(keyoffset), cmpko);

  cleanko(&ko);
  return r;
}

// return pos where s would be inserted
// TODO: implement
keyoffset* searchix(char* s) {
  fprintf(stderr, "searchix: not implemented\n"); exit(1);
  return NULL;
}

// ENDWCOUNT

// - wget https://raw.githubusercontent.com/openethereum/wordlist/master/res/wordlist.txt
// - wget https://download.weakpass.com/wordlists/1239/1.1million%20word%20list.txt.gz

#include <assert.h>

#include "mytime.c"

long rbytes= 0;

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
    rbytes+= l;
    //printf("c=%d\n", w[l-1]);
    // stupid!
    if (l && w[l-1]==10) w[l---1]= 0;
    if (l && w[l-1]==13) w[l---1]= 0;
    sixadd(w, ftell(f));
  }
  fclose(f);
  free(w);
  ms= timems()-ms;
  printf("read %ld words from %s in %ld ms\n", n, filename, ms);
}

int main(int argc, char** argv) {
  assert(sizeof(keyoffset)==16);
  printf("newkeyoffset bytes=%lu\n", sizeof(keyoffset));

  keyoffset ko= {.type= 77, .o=4711};
  printf("%-16s %d %d\n", ko.val.s, ko.type, ko.o);
  strncpy(ko.val.s, "ABCDEFGHIJKLMNO", 12);
  ko.type= 0;
  printf("%-16s %d %d\n", ko.val.s, ko.type, ko.o);
  (void)dixadd(-99, 23);
  (void)sixadd("foo", 2);
  (void)sixadd("bar", 3);
  (void)sixadd("fie", 5);
  (void)sixadd("fum", 7);
  (void)sixadd("foobar", 11);
  (void)sixadd("abba", 13);
  (void)dixadd(111, 17);
  (void)dixadd(22, 19);
  (void)sixadd("", 23);
  printix();

  qsort(ix, nix, sizeof(keyoffset), cmpko);
  printix();

  // search
  {
    char *f= "fum";
    printf("\nFIND '%s' \n  ", f);
    keyoffset* kf= sfindix(f);
    if (kf) printko(kf);
    else printf("NOT FOUND!\n");
  }

  // join counting
  if (0) {
    const long N= 1100000; // 1.1M
    // const long N= 108000; // 108K

    int* arr= malloc(N*sizeof(*arr));
    for(long a=0; a<N; a++) {
      arr[a]= a;
    }

    long n= 0;
    long s= 0;
    for(long a= 0; a<N; a++) {
      for(long b=0; b<N; b++) {
	if (a==b) {
	  s+= arr[a]+arr[b];
	  n++;
	}
      }
    }
    printf("join %ld s=%ld\n", n, s);
    exit(0);
  }
  
  // words
  nix= 0;
  if (0) readwords("wordlist-1.1M.txt");
  else if (1)
readwords("1.1million word list.txt");
  else readwords("wordlist-8K.txt");

  printf("\n==WORDS! %d\n\n", nix);

  // on already sorted 67ms
  // on 1.1mil.. 499ms
  long sortms= timems();
  printf("sorting...\n");
  qsort(ix, nix, sizeof(keyoffset), cmpko);
  printf("  sorting took %ld ms\n", timems()-sortms);


  printf("\n==WORDS! %d\n\n", nix);

  //printix();
  
  if (1) { // < 'abba' xproduct!
    keyoffset* abba= sfindix("abba");
    if (!abba) exit(11);

    long n= 0;
    keyoffset* a= ix;
    while(a <= abba) {
      keyoffset* b= ix;
      while(b <= abba) {
	// index.c - 2m15
	//   join < abba === 8007892418
	//   real  99s user  99s sys 0.258s
	// DuckDB:
	//   join < abba === 8007260837
	//   real 119s user 299s sys 2s

	// - 99s
	// if (cmpko(a, b) < 0) { 
	// - 67s strcmp...
	//if (strcmp(strix(a), strix(b)) < 0) { 
	// - 0.73s !!!
	//   (DudkDB: 6.145s LOL)
	if (1) {
	// - 50s
	//if (strcmp(strix(a), strix(abba)) < 0) {
	  n++;
	}
	b++;
      }
      a++;
    }
    printf("join < abba === %ld\n", n);
    exit(1);
  } else if (1) {
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
    keyoffset* kf= sfindix("yoyo");
    if (!kf) kf= sfindix("york");
    kf= sfindix("ABOVEGROUND-BULLETINS");
    printf("FISH: ");
    printko(kf);
    if (!kf) { printf("%%: ERROR no yoyo/york!\n"); exit(33); }

    long f= 0;
    //for(int i=0; i<10000000; i++) {
    //for(int i=0; i<100000; i++) {
    int n= 0;
    for(int i=0; i<10; i++) {
      keyoffset* end= ix + nix;

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
      if (ix >= end) p= NULL;
      if (!p) { printf("ERROR\n"); exit(33); }
      f++;
    }
    printf("Linear found %ld\n", f);
    printf("ncmpko=%ld\n", ncmpko);
    printf("neqko=%ld\n", neqko);
    printf("n=%d\n", n);
  } else {
  // search
  // 10M times == 2.27s (8K words)
  for(int i=0; i<10000000; i++)
  {
    char *f= "yoyo";
    //printf("\nFIND '%s' \n  ", f);
    keyoffset* kf= sfindix(f);
    if (!kf) { printf("ERROR"); exit(3); }
    //if (kf) printko(kf);
    //else printf("NOT FOUND!\n");
  }
  }
  printf("nstrdup=%ld bstrdup=%ld\n", nstrdup, bstrdup);
  printf("BYTES: %ld\n", bstrdup + nix*sizeof(keyoffset) + nstrdup*8);
  printf("ARRAY: %ld\n", rbytes + nix*(sizeof(char*)+sizeof(int)+8));
  // 8 is the average waste in malloc
}

// 11 chars or 7 chars?
// 1111111111:
// -----------
// ==WORDS! 1049938
/*

read 1049938 words from 1.1million word list.txt in 453 ms

==WORDS! 1049938

sorting...
  sorting took 270 ms

==WORDS! 1049938

FISH: IX> 'ABOVEGROUND-MISC'   1  @8686040
Linear found 10
ncmpko=22319090
neqko=0
n=1575970
nstrdup=116029 bstrdup=1605328
BYTES: 19332568
ARRAY: 31388379

(/ 19.33 31.389) = 0.61   39% better!
*/


/* 777777777777777777

read 1049938 words from 1.1million word list.txt in 486 ms

==WORDS! 1049938

sorting...
  sorting took 584 ms

==WORDS! 1049938

FISH: IX> 'ABOVEGROUND-MISC'   1  @8686040
Linear found 10
ncmpko=22319090
neqko=0
n=1575970
nstrdup=535835 bstrdup=5371271
BYTES: 26456959
ARRAY: 31388379

(/ 26.46 31.39) = 0.84   16% better...

 */
