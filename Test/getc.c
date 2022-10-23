#include <stdio.h>

int main(int argc, char** argv) {
  FILE* f= fopen("happy.csv", "r");
  int n= 0;
for(int i=0; i<100; i++) {
  rewind(f);
  setlinebuf(f);
  char buf[1024*1024];
  //setbuffer(f, (char*)buf, sizeof(buf));

  if (0) {
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
}
fclose(f);
printf("n=%d\n", n);
}
