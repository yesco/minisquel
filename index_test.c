#include "malloc-count.c"

#include "index.c"

// ENDWCOUNT

// - wget https://raw.githubusercontent.com/openethereum/wordlist/master/res/wordlist.txt
// - wget https://download.weakpass.com/wordlists/1239/1.1million%20word%20list.txt.gz

#include <assert.h>

#include "mytime.c"

long rbytes= 0;

void readwords(memindex* i, char* filename) {
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
    sixadd(i, w, ftell(f));
  }
  fclose(f);
  free(w);
  ms= timems()-ms;
  printf("read %ld words from %s in %ld ms\n", n, filename, ms);
}

int main(int argc, char** argv) {
  memindex* ix= newindex("test", 0);
  
  assert(sizeof(keyoffset)==16);
  printf("keyoffset bytes=%lu\n", sizeof(keyoffset));

  keyoffset ko= {.type= 77, .o=4711};
  printf("%-16s %d %d\n", ko.val.s, ko.type, ko.o);
  strncpy(ko.val.s, "ABCDEFGHIJKLMNO", 12);
  ko.type= 0;
  printf("%-16s %d %d\n", ko.val.s, ko.type, ko.o);
  (void)dixadd(ix, -99, 23);
  (void)sixadd(ix, "foo", 2);
  (void)sixadd(ix, "bar", 3);
  (void)sixadd(ix, "fie", 5);
  (void)sixadd(ix, "fum", 7);
  (void)sixadd(ix, "foobar", 11);
  (void)sixadd(ix,"abba", 13);
  (void)dixadd(ix, 111, 17);
  (void)dixadd(ix, 22, 19);
  (void)sixadd(ix, "", 23);
  printix(ix);

  sortix(ix);
  
  // search
  {
    char *f= "fum";
    printf("\nFIND '%s' \n  ", f);
    keyoffset* kf= sfindix(ix, f);
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
    freeindex(ix);
    fprintmallocs(stderr);
    exit(0);
  }
  
  // words
  ix->n= 0;
  if (0) readwords(ix, "wordlist-1.1M.txt");
  else if (1)
    readwords(ix, "1.1million word list.txt");
  else readwords(ix, "wordlist-8K.txt");

  printf("\n==WORDS! %d\n\n", ix->n);

  // on already sorted 67ms
  // on 1.1mil.. 499ms
  long sortms= timems();
  printf("sorting...\n");
  sortix(ix);
  printf("  sorting took %ld ms\n", timems()-sortms);


  printf("\n==WORDS! %d\n\n", ix->n);

  //printix();
  
  if (1) { // < 'abba' xproduct!
    keyoffset* abba= sfindix(ix, "abba");
    if (!abba) exit(11);

    long n= 0;
    keyoffset* a= ix->kos;
    while(a <= abba) {
      keyoffset* b= ix->kos;
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

    freeindex(ix);
    fprintmallocs(stderr);
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
    keyoffset* kf= sfindix(ix, "yoyo");
    if (!kf) kf= sfindix(ix, "york");
    kf= sfindix(ix, "ABOVEGROUND-BULLETINS");
    printf("FISH: ");
    printko(kf);
    if (!kf) { printf("%%: ERROR no yoyo/york!\n"); exit(33); }

    long f= 0;
    //for(int i=0; i<10000000; i++) {
    //for(int i=0; i<100000; i++) {
    int n= 0;
    for(int i=0; i<10; i++) {
      keyoffset* end= ix->kos + ix->n;

      keyoffset* p= ix->kos;
      //fprintf(stderr, ".");
      while(p<end) {
	n++;
	n+= i;
	if (0==cmpko(kf, p)) break;
	//if (eqko(kf, p)) break;
	//if (0==strcmp(kf, p)) break;
	p++;
      }
      if (p >= end) p= NULL;
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
	keyoffset* kf= sfindix(ix, f);
	if (!kf) { printf("ERROR"); exit(3); }
	//if (kf) printko(kf);
	//else printf("NOT FOUND!\n");
      }
  }

  // Statistics/estamtes mem-usage
  //
  // 8 is the average waste in malloc ?
  printf("nstrdup=%ld bstrdup=%ld\n", nstrdup, bstrdup);
  printf("BYTES: %ld\n", bstrdup + ix->n*sizeof(keyoffset) + nstrdup*8);
  printf("ARRAY: %ld\n", rbytes + ix->n*(sizeof(char*)+sizeof(int)+8));

  freeindex(ix);
  fprintmallocs(stderr);
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
