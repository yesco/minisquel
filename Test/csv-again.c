#include <stdio.h>
#include <stdlib.h>

char* nextcsv(char** line, size_t* len, FILE* f) {
  if (!*line || **line=='\n') { //  || !(*line)[1]) {
    printf("\tGETLINE\n");
    if (getline(line, len, f)==EOF) {
      free(*line); *line= NULL; *len= 0;
      return NULL;
    }
    if (*len<strlen(*line)+1+1)
      *line= realloc(*line, *len+= 1);
    memmove(*line+1, *line, *len-1);
    **line= '0';
  }

  char c= **line;
  printf("\tCHAR: %c\n", c);
  if (c=='0') ;
  if (c=='d' || c=='N' || c=='-') {
    printf("\tDELETE\n");
    int ln= strlen(*line+1);
    char* p= *line+1;
    memmove(p, p+ln+1, *len-1-ln-1);
    if (c=='-') {
      //memmove(p, p+ln+1, *len-1-ln-1);
      **line= '0';
      return nextcsv(line, len, f);
    }
    if (c=='N') {
      **line= '\n';
      return nextcsv(line, len, f);
    }
  }
  if (c=='n') {
    printf("\tNEWLINE\n");
    **line= 'N';
    *(*line+1)= '\n';
    return *line+1;
  }

  char *at= *line+1;
  printf("\t1>%s<\n", at);
  if (*at==',') {
    printf("\tNULL\n");
    **line='-';
    *at= 0;
    return at;
  }
  while(*at && *at!=',' && *at!='\n') at++;
  if (*at=='\n') **line= 'n'; else **line= 'd';
  printf("\t2>%s<\n", *line+2);
  *at= 0;
  return *line+1;
}

int main(int argc, char** argv) {
  //  FILE* f= fopen("happy.csv", "r");
  //FILE* f= fopen("fil10M.tsv", "r");
  //FILE* f= fopen("Test/foo.csv", "r");
  FILE* f= fopen("Test/null.csv", "r");
  
  char* line= NULL;
  size_t len= 0;

  char* s= NULL;

  while((s= nextcsv(&line, &len, f))) {
    printf("---->%s<\n", s);
  }

  free(line);
  fclose(f);
}
