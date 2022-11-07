#include <stdio.h>
#include <ctype.h>
#include <math.h>

int alphabet(char* s) {
  long a[128/8]= {0};
  int c, n= 0;
  while((c= *s++)) {
    c= tolower(c); // mostly correct
    // TODO: CAP WRODS w prefix/shift
    int i= c/64;
    long m= 1l<<(c & 63);
    if (!(a[i] & m)) {
      n++;
      a[i] |= m;
    }
  }
  return n;
}

long compress(char* s) {
  printf("\n  DATA  %s\n", s);
  char* p;
  int n, c;

  char SPECIALS[]= " ,.-!\n";

  printf("\nDATA --- delta\n");
  int lowest= 255, highest= 0;
  int lowlowest= 255;
  int last= 0;
  p= s-1; n= 0;
  while((c= *++p)) {

    if (!strchr(SPECIALS, c)) {
      if (tolower(c) < lowlowest)
	lowlowest= tolower(c);
    }

    if (c!=' ' && c < lowest && !strchr(SPECIALS, c))
	lowest= c;
    if (c > highest) highest= c;

    if (c==' ') c= 'a'-1;
    printf("%c %3d\n", c, c-last);
    last= c;
  }

  printf("\nDATA --- range shift # %d [%d-%d]\n", highest-lowest, lowest, highest );
  lowest--; // space!
  p= s-1; n= 0;
  while((c= *++p)) {
    char d= c;
    if (c==' ') d= lowest; // !
    printf("%c %3d\n", c, d-lowest);
    n++;
  }
  printf("DATA = # %d outputs\n", n);



  lowest= lowlowest;
  lowest--; // " "   +0
  lowest--; // ". C" +1
  lowest--; // " C"  +2
  lowest--; // ", "  +3
  lowest--; // " - " +4
  lowest--; // "! C" +5
  lowest--; // "\n"  +6
  //lowest--; // ".\n"  +7
  
  printf("\nDATA --- '. C' ', ' %d [%d-%d]\n", highest-lowest, lowest, highest );
  lowest--; // space!
  p= s; n= 0;
  char* tofree= p= strdup(s);
  while((c= *p)) {
    char d= c;

    if (n==0) {
      if (c==toupper(c)) {
	d= tolower(c);
      } else {
	printf("%% Start with lowercase\n");
      }
    }

    // ---   ", "
    if (0==strncmp(", ", p, 2)) {
      printf("x %c %3d\n", c, +3); n++;
      p+= 2;
      continue;
    }
    // ---   ".\n"
    // no savings, not common!
    if (0) 
    if (0==strncmp(",\n", p, 2)) {
      printf("x %c %3d\n", c, +7); n++;
      p+= 2;
      continue;
    }
    // ---   ". C"
    if (c=='.' && p[1]==' ' && p[2]==toupper(p[2])) {
      printf("x %c %3d\n", c, +1); n++;
      p+= 2;
      c= *p;
      *p= tolower(c);
      continue;
    }
    // ---   "! C"
    if (c=='!' && p[1]==' ' && p[2]==toupper(p[2])) {
      printf("x %c %3d\n", c, +5); n++;
      p+= 2;
      c= *p;
      *p= tolower(c);
      continue;
    }
    // ---   " - "
    if (0==strncmp(" - ", p, 3)) {
      printf("x %c %3d\n", c, +4); n++;
      p+= 3;
      continue;
    }
    // ---   " C" (as in name)
    if (c==' ' && p[1]==toupper(p[1])) {
      printf("x %c %3d\n", c, +2); n++;
      p+= 1;
      c= *p;
      *p= tolower(c);
      continue;
    }
    if (c==' ') d= lowest+0;
    if (c=='\n') d= lowest+6;

    printf("  %c %3d\n", c, d-lowest); n++;
    p++;
  }

  int alpha= alphabet(s);
  alpha+= 5;
  int abits= log2f(alpha)/log2f(2)+0.9999;
  printf("DATA = alphabet %d alphabits=%d\n", alpha, abits);
  float nabits= n * abits;
  int nabytes= (int)(nabits+7)/8 + 8;
  printf("DATA n=%d bits=%d nbits=%d bytes= %d /%lu ALPHA\n", n, abits, (int)nabits, nabytes, strlen(s));  

  float apercent= 100.0-100*(nabytes+0.0)/strlen(s);
  printf("DATA = savings %4.3f %% ALPHA\n", apercent);

  printf("DATA = # %d outputs\n", n);
  int bits= log2f(highest-lowlowest)/log2f(2)+0.9999;
  float nbits= n * bits;
  printf("DATA n=%d bits=%d nbits=%d bytes= %d /%lu\n", n, bits, (int)nbits, (int)(nbits+7)/8, strlen(s));

  free(tofree);
  return nbits;
}

long report(char* s) {
  long nbits= compress(s);

  // -- report
  float percent= 100.0-100*(double)nbits/8/strlen(s);
  printf("DATA nbits=%ld bytes=%d (%lu)\n", (long)nbits, (int)(nbits/8), strlen(s));
  printf("DATA = savings %4.3f %%\n", percent);
  return (nbits+7)/8;
}

int main(int argc, char** argv) {
  if (argc>1) return report(argv[1]);

  char* str= NULL;
  size_t ln= 0;

  long bytes= 0, cbytes= 0;
  int len= 0;
  while((len=getline(&str, &ln, stdin))!=EOF) {
    bytes+= len;
    cbytes+= report(str);
    //printf("data ===== bytes=%ld cbytes=%ld\n" , bytes, cbytes);
  }
  free(str);

  float percent= 100.0-100*(double)cbytes/bytes;
  printf("DATA = savings %4.3f %%\n", percent);
}
