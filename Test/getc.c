#include <stdio.h>

char* nextcsv(char** line, size_t* len, FILE* f, int state) {
  if (!line) return NULL;
  char* in= *line;
  if (!in || !*in) {
    if (getline(line, len, f)==EOF) return NULL;
    else nextcsv(line, len, f, 1);
  }
  // get rid of old value
  int ln= strlen(in);
  memmov(in, in+ln+1, len-ln);
  while(*in && *in!=',') in++;
  if (!*in) return *line;
  if (*in==',') { *in= 0; return *line; }
  printf("FOO\n");
  return NULL;
}

int main(int argc, char** argv) {
  //  FILE* f= fopen("happy.csv", "r");
  FILE* f= fopen("fil10M.tsv", "r");
  int n= 0;
  //for(int i=0; i<100; i++) {
  //  rewind(f);
  setlinebuf(f);
  char buf[1024*1024];
  //setbuffer(f, (char*)buf, sizeof(buf));

  if (1) {
    int c;
    while((c= fgetc(f)) !=EOF) {
      //putchar(c);
    }
  } else {
    char* s= NULL;
    size_t l= 0;
    while(getline(&s, &l, f) >= 0) {
      n++;
      //puts(s);
    }
    free(s);
  }
  //}
fclose(f);
printf("n=%d\n", n);
}
