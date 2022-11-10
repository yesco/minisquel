#include <stdio.h>
#include <ctype.h>
#include <math.h>

int main(int argc, char** argv) {
  long freq[256]= {0};
  
  char* str= NULL;
  size_t ln= 0;

  long bytes= 0, cbytes= 0;
  int len= 0;
  while((len=getline(&str, &ln, stdin))!=EOF) {
    bytes+= len;
    for(int i=0; i<len; i++)
      freq[toupper(str[i])]++;
  }
  free(str);

  for(int c=0; c<256; c++) {
    long n= freq[c];
    if (n) {
      printf("%8ld\t'%c'\n", n, c);
    }
  }
}
