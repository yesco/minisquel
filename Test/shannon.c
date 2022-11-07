// generate shannon codes from input sorted on
//
//    freq	word/symbol
//
//    5         ' '
//    9         'e'
//    1         'the'

#include <stdio.h>
#include <stdlib.h>

#define MAXSYMS 1024*1024
long  freq[MAXSYMS]= {};
char* sym[MAXSYMS]= {};
int narr= 0;

// Note: prefix bits are reversed!

void findmidpoint(int start, int end, long prefix, int depth) {
  if (start>end) return;
  if (start==end) {

    printf("MID: ");
    for(int i=0; i<start; i++) putchar(' ');
    printf("*  - ");
    for(int i=0; i<depth; i++) {
      putchar('0'+(prefix&1));
      prefix>>= 1;
    }
    putchar('\n');
    
    for(int i=0; i<depth; i++) {
      putchar('0'+(prefix&1));
      prefix>>= 1;
    }
    printf("\ti=%3d ", start);
    printf(" (%2d) %s # %ld OUT\n", depth, sym[start], freq[start]);
    return;
  }

  printf("= ");
  for(int i=0; i<depth; i++) putchar(' ');
  printf("%2d %2d (%d)\n", start, end, depth);
  long suma= 0, sumb= 0;
  int s= start, e= end;
  printf("\n");
  while(e-s>0) {
    //printf("  %4d %4d %8ld %8ld\n", s, e, suma, sumb);
    while(suma<=sumb && s<e) {
      suma+= freq[s++];
    }
    while(suma>=sumb && s<e) {
      sumb+= freq[e--];
    }
  }
  int mid= s;
  //printf("= %4d %4d %8ld %8ld\n", s, e, suma, sumb);
  depth++;
  prefix<<= 1;
  if (end-start>0) {
    if (!(start>=mid || mid>=end)) {
    printf("MID: ");
    for(int i=0; i<start; i++) putchar(' ');
    for(int i=start; i<mid; i++) putchar('0');
    for(int i=mid; i<end; i++) putchar('1');
    printf("\n");
  }

    // must be a bug here somewhere? lol
    // code isn't unique, shouldn't happen
    findmidpoint(start, mid-1, prefix+0, depth);
    findmidpoint(mid, end, prefix+1, depth);
  }
}

int main(void) {
  size_t ln= 0;
  long f; char s[1024]= {0};
  while(!feof(stdin)) {
    // insecure, lol
    if (2!= fscanf(stdin, " %ld %[^\n]ms", &f, (char*)&s)) {
      char* err= NULL;
      size_t ln= 0;
      getline(&err, &ln, stdin);
      printf("%% IGNORING: %s", err);
      free(err);
      continue;
    }
    // TODO: check it is sorted incr
    printf("- %ld\t%s\n", f, s);
    if (narr>MAXSYMS) exit(1);
    freq[narr]= f;
    sym[narr++]= strdup(s);
  }

  findmidpoint(0, narr-1, 0, 0);
}
