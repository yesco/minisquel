#include "darr.c"

void dprarr(darr* d) {
  printf("\n\ndaray[%d]={ first=%d next=%d max=%d\n", dlen(d), d->first, d->next, d->max);
  for(int i=0; i<d->max; i++) {
    dint p = dget(d, i);
    if (p) 
      printf("%d: %ld\n", i, p);
    else 
      putchar('.');
  }
  printf("\n");
}

int main(void) {
  dint p;

  printf("\n======== array: put get\n");
  darr* d= NULL; //darray(NULL, 0);
  d= dput(d, 0, 100);
  d= dput(d, 1, 101);
  p = dget(d, 1); printf("[1] %ld == 101?\n", p);
  d= dput(d, 2, 102);
  d= dput(d, 3, 103);
  p = dget(d, 4); printf("emty [4] %ld\n", p);
  d= dput(d, 4, 104);

  dprarr(d);

  printf("\n======== stack: push + pop\n");
  d= dpush(d, 105);
  d= dpush(d, 106);

  dprarr(d);

  p = dpop(d); printf("pop %ld\n", p);
  p = dpop(d); printf("pop %ld\n", p);
  p = dpop(d); printf("pop %ld\n", p);
  p = dpop(d); printf("pop %ld\n", p);
  p = dpop(d); printf("pop %ld\n", p);
  p = dpop(d); printf("pop %ld\n", p);
  p = dpop(d); printf("pop %ld\n", p);
  p = dpop(d); printf("over pop %ld (poop?)\n", p);
  p = dpop(d); printf("over pop %ld (poop?)\n", p);

  dprarr(d);

  printf("\n======== expand down\n");
  d= dunshift(d, 200);
  dprarr(d);

  printf("\n======== queue: push + shift\n");
  d= dpush(d, 300);
  d= dpush(d, 301);
  d= dpush(d, 302);
  p = dshift(d); printf("shift %ld\n", p);
  p = dshift(d); printf("shift %ld\n", p);
  d = dpush(d, 303);
  dprarr(d);

  p = dshift(d); printf("shift %ld\n", p);
  p = dshift(d); printf("shift %ld\n", p);
  p = dshift(d); printf("shift %ld\n", p);
  
  return 0;
}

