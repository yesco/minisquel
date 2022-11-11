// Minimal malloc/free counter
//   (works on CLANG)

#include <stdio.h>
#include <dlfcn.h>

/* AMD Memory Pointers!

In addition, the AMD specification
requires that

the most significant 16 bits of any
virtual address, bits 48 through 63, must
be copies of bit 47 (in manner akin to
sign extension).

(*---jsk: android isn't AMD!

- https://source.android.com/docs/security/test/tagged-pointers

        |47= 8 not set, but hibyte???
  645648v         /- 0 bit
  b400007a'82eb0050
  TxyyzzPP'PPPPPPPP
  
The T(op) 4 bits contain a key specific to
the memory region in question. 

So, *assuming* that xxyyzz are all zero,
to "shrink" a pointer to an allocated
object (to it store in a double) we can
store TPP_--PPPP which is 4+48-3=49 bits
as the lowest 3 are going to be all 0
(8B alignment).

 --*)


If this requirement is not met, the
processor will raise an
exception.[11]: 131  Addresses complying
with this rule are referred to as
"canonical form."[11]: 130 

Canonical form addresses run from:

  00000000'00000000   through
  00007FFF'FFFFFFFF,  and from

  FFFF8000'00000000   through
  FFFFFFFF'FFFFFFFF,  for a

total of 256 TB of usable virtual address
space. This isstill 65,536 times larger
than the virtual 4 GB address space of
32-bit machines.

*/

long nalloc= 0, nfree= 0, nbytes= 0;

void* malloc(size_t size) {
  static void* (*omalloc)(size_t)= NULL;
  if (!omalloc) omalloc= dlsym(RTLD_NEXT, "malloc");
  void* r= omalloc(size);
  if (r) {
    nalloc++;
    nbytes+= size;
    //fprintf(stderr, "malloc(%lu)\n", size);
    static long mask= -1;
    static long stored= -1;
    if (mask == -1) {
      //    0xb400007a82eb0050 = android
      mask= 0xffff000000000000;
      stored= mask & (long)r;
    } else if (1) {
      // no measurable cost of these tests
      
      // Assume the prefix is
      // "NOT changinmg")
      
      // See android note above,
      // if it used tagging it
      // should change?
      assert(((long)r & mask)==stored);

      // lowest 3 bits are 0 (align 8)
      assert((((long)r) & 0x07)==0);
      // clang && android NOT align 16 byte!!!
      // (but seems to be unusual)
      //assert((((long)r) & 0x0f)==0); // 16
    }
  }
  return r;
}

void free(void* p) {
  static void (*ofree)(void*)= NULL;
  if (!ofree) ofree= dlsym(RTLD_NEXT, "free");

  if (p) nfree++;
  ofree(p);
}  

void mallocsreset() {
  nalloc= nfree= nbytes= 0;
}

void fprintmallocs(FILE* f) {
  fprintf(f, "[%ld malloc %ld free (leak %+ld) %ld bytes]\n", nalloc, nfree, nalloc-nfree, nbytes);
}

#ifdef MAIN

int main(int argc, char** argv){
  char* s= strdup("foo");
  free(s);
  char* v= malloc(42);
  fprintmallocs(stderr);
}

#endif
