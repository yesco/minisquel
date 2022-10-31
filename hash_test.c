#include "hash.c"

void testarena() {
  arena* a= newarena(0,1);
  int foo= saddarena(a, "foo");
  int bar= saddarena(a, "bar");
  int fiefum= saddarena(a, "fiefum");
  printarena(a, 1);
  for(int i=0; i<10000; i++) {
    char s[32];
    snprintf(s, sizeof(s), "FOO-%d-BAR", i);
    int ix= saddarena(a, s);
    printf("%d @ %d %s\n", i, ix, (char*)arenaptr(a, ix));
  }
  printarena(a, 1);
  printf("BAR: %s\n", (char*)arenaptr(a, bar));
  exit(0);
}



void readdict() {
  FILE* f= fopen("duplicates.txt", "r");
  //  FILE* f= fopen("wordlist-1.1M.txt", "r");
  //FILE* f= fopen("Test/count.txt", "r");
  char* word= NULL;
  size_t len= 0;
  int n= 0;
  while(getline(&word, &len, f)!=EOF) {
    int l= strlen(word);
    if (word[l-1]=='\r') word[l-1]= 0, l--;
    if (word[l-1]=='\n') word[l-1]= 0, l--;
    if (word[l-1]=='\r') word[l-1]= 0, l--;
    n++;

    // store one int/file offset
    int o= 4711;
    int s= atomappend(word, &o, sizeof(o));
    //printf("  ---> %d '%s'\n", s, atomstr(s));
    //int s= atom(word);

    if (n%1000 == 0) fputc('.', stderr);
  }
  printf("# %d\n", n);
  fclose(f);

  printatoms(atoms);
}

void testatoms() {
  readdict(); printhash(atoms, 1); exit(0);
  
  int data=4711;
  
  //  int a= atom("foo");  int b= atom("bar");  int c= atom("foo"); int bb= atom("bar");
  //int a= atomappend("foo", &data, sizeof(data));  int b= atomappend("bar", &data, sizeof(data));  int c= atomappend("foo", &data, sizeof(data)); int bb= atomappend("bar", &data, sizeof(data));
  char cc= '1';
  int a= atomappend("foo", &cc, 1);  int b= atomappend("bar", &cc, 1);  int c= atomappend("foo", &cc, 1); int bb= atomappend("bar", &cc, 1);

  printf("%d %d %d %d\n", a,b,c,bb);

  printf("%s %s %s %s\n", atomstr(a),atomstr(b),atomstr(c),atomstr(bb));

  printhash(atoms, 1);

  printf("%s %s %s %s\n", atomstr(a),atomstr(b),atomstr(c),atomstr(bb));

  printhash(atoms, 1);
  
  //readdict();

  printhash(atoms, 0);

  //dumpatoms(atoms);

  //printarena(atoms->arena, 1);
  freehash(atoms);
  
  printatoms(atoms);
  exit(0);
}


int main(void) {
  testatoms();
  testarena();
  
  hashtab *h= newhash(10, NULL, 0);
  printhash(h, 1);

  hashentry* f= addhash(h, "foo", NULL);
  hashentry* b= addhash(h, "bar", NULL);
  printhash(h, 1);

  freehash(h);
}
