#include "syms.c"

void test(int s) {
  char* str= symstr(s);
  printf("%d '%s' -> %d\n", s, str, sym(str));
}
       
void simple() {
  sym("zero");
  sym("one");
  sym("two");
  sym("three");
  sym("four");
  sym("five");
  sym("six");
  sym("seven");
  sym("eight");
  sym("nine");
  return;
      
  printf("\nSimple\n-----\n");
  int c= sym("circus");
  int a= sym("a");
  int b= sym("abba");
  test(sym(NULL));
  test(sym(""));
  test(a);
  test(b);
  test(c);
  test(sym(NULL));
  test(sym(""));
  dump();
}

int main(int argc, char** argv) {
  simple(); dump();
  printf("AAAA\n");
  //symssort();
  //osymssort();
  printf("BBBB\n");
  simple(); dump();
  printf("CCCCC\n");
}
