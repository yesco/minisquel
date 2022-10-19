// Minimal malloc/free counter!
//

#include <stdio.h>
#include <stdlib.h>

long nalloc= 0, nfree= 0, nbytes= 0;

char memory[256*1024*1024]= {0};
char* memptr= (char*)&memory;

// If malloc defined
// - update nalloc and nfree

void* malloc(size_t len) {
  nalloc++;
  nbytes+= len;
  // align on 16 byte boundary
  len= (len+15) & ~0x0fl;
  void* r= memptr;
  memptr+= len;
  if (memptr>memory+sizeof(memory))
    exit(66);
  return r;
}

void free(void* p) {
  if (!p) return;
  // TODO: reclaim?, LOL
  nfree++;
}

void mallocsreset() {
  nalloc= nfree= nbytes= 0;
}

void fprintmallocs(FILE* f) {
  fprintf(f, "%ld malloc %ld free (leak %+ld) %ld bytes\n", nalloc, nfree, nalloc-nfree, nbytes);
}

#ifdef MAIN

#include <string.h>

int main(int argc, char** argv) {
  char* s= strdup("foo");
  //char* s= NULL;
  free(s);
  
  printf("foo\n"); // 1? not free:d?

  fprintmallocs(stderr);
}

#endif
