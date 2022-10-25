#include "csv.c"

void readfile(char* fn) {
  FILE* f= fopen(fn, "r");
  char v[1024];
  double d;
  int r;
  while((r= freadCSV(f, v, sizeof(v), &d))) {
    printf("%2d: ", r);
    if (r==RNULL) printf("NULL\n");
    if (r==RNUM) printf("%lg\n", d);
    if (r==RSTRING) printf("\"%s\"\n", v);
    if (r==RNEWLINE) printf("NL\n");
  }
  fclose(f);
}

int main(int argc, char** argv) {

  readfile("Test/manynls.csv"); exit(0);

  if (0) {
    {
      char* s= strdupncat(strdup("foo"), -1, "bar");
      printf(">%s<\n", s);
      free(s);
      s= NULL;
    }
    {
      char* s= strdupncat(strdup("foo"), 2, "bar");
      printf(">%s<\n", s);
      free(s);
    }
    {
      char* s= strdupncat(NULL, 2, NULL);
      printf(">%s<  %p\n", s, s);
      free(s);
    }
    {
      char* s= strdupncat(strdup("fie\\\n"), -1, "fum");
      printf(">%s<  %p\n", s, s);
      free(s);
    }
    printf("\n==================\n\n\n");
  }

  if (0) {
    {
      FILE* f= fopen("Test/nl.csv", "r");
      char v[1024];
      double d;
      int r;
      while((r= freadCSV(f, v, sizeof(v), &d))) {
	if (r==RNULL) printf("NULL   ");
	if (r==RNUM) printf("%lg   ", d);
	if (r==RSTRING) printf(">%s<   ", v);
	if (r==RNEWLINE) printf("NL\n");
      }
      fclose(f);
    }

    if (0) {
      {
	FILE* f= fopen("Test/nl.csv", "r");
	char* s= csvgetline(f);
	printf("\n>>>%s<<<\n\n", s);

	char v[1024];
	double d;
	int r;
	char* ss= s;
	while((r= sreadCSV(&ss, v, sizeof(v), &d))) {
	  if (r==RNULL) printf("NULL   ");
	  if (r==RNUM) printf("%lg   ", d);
	  if (r==RSTRING) printf(">%s<   ", v);
	  if (r==RNEWLINE) printf("\n");
	}

	free(s);
	fclose(f);
      }
    }
  }

  if (1) {
    // reading csv lines and then parsing
    // values is faster than
    // setlinebuf, setbuffer and fgetc!
    // 
    // 0.45s for happy, getc.c 0.8s!
    //
    // 0.136s if remove sreadCSV!
    //
    
    FILE* f= fopen("Test/happy.csv", "r");

for(int i=10; i; i--){ rewind(f);

    int lines= 0;
    while(!feof(f)) {
      char* s= csvgetline(f);
      if (!s) break;
      lines++;
      //printf("\n>>>%s<<<\n\n", s);
      
      char v[1024];
      double d;
      int r;
      char* ss= s;
    if (0)
      while((r= sreadCSV(&ss, v, sizeof(v), &d))) {
	if (0) {
	  if (r==RNULL) printf("NULL   ");
	  if (r==RNUM) printf("%lg   ", d);
	  if (r==RSTRING) printf(">%s<   ", v);
	  if (r==RNEWLINE) printf("\n");
	}
      }

      free(s);
    }
    printf("lines=%d\n", lines);

}  // rewwind

    fclose(f);

  }    
}
