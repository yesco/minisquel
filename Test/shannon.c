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

// Note: prefix are "aligned" on right
void findmidpoint(int start, int end, long prefix, int depth) {
  if (start>end) return;
  if (start==end) {

    char bits[depth+1];
    bits[depth]= 0;
    long p= prefix;
    for(int i=depth-1; i>=0; i--) {
      bits[i]= '0' + (p & 1);
      p>>= 1;
    }

    printf("%lx:%x\t", prefix, depth);
    printf("%6ld %-15s ", freq[start], sym[start]);
    printf("%s", bits);
    printf("(%2d) i=%3d OUT\n", depth, start);

    return;
  }

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
