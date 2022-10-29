// MAP macros
// inspired from
// - https://stackoverflow.com/questions/1872220/is-it-possible-to-iterate-over-arguments-in-variadic-macros

// Make a FOREACH macro
#define FE_0(WHAT)
#define FE_1(WHAT, X) WHAT(X)
#define FE_2(WHAT, X, ...) WHAT(X);FE_1(WHAT, __VA_ARGS__)
#define FE_3(WHAT, X, ...) WHAT(X);FE_2(WHAT, __VA_ARGS__)
#define FE_4(WHAT, X, ...) WHAT(X);FE_3(WHAT, __VA_ARGS__)
#define FE_5(WHAT, X, ...) WHAT(X);FE_4(WHAT, __VA_ARGS__)
#define FE_6(WHAT, X, ...) WHAT(X);FE_5(WHAT, __VA_ARGS__)
#define FE_7(WHAT, X, ...) WHAT(X);FE_6(WHAT, __VA_ARGS__)
#define FE_8(WHAT, X, ...) WHAT(X);FE_7(WHAT, __VA_ARGS__)
#define FE_9(WHAT, X, ...) WHAT(X);FE_8(WHAT, __VA_ARGS__)
//... repeat as needed

#define GET_MACRO(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME 
#define MAP(action,...) \
  GET_MACRO(_0,__VA_ARGS__,FE_9,FE_8,FE_7,FE_6,FE_5,FE_4,FE_3,FE_2,FE_1,FE_0)(action,__VA_ARGS__)

// Make a FOREACH macro
#define LFE_0(WHAT)
#define LFE_1(WHAT, X) WHAT(X) 
#define LFE_2(WHAT, X, ...) WHAT(X),LFE_1(WHAT, __VA_ARGS__)
#define LFE_3(WHAT, X, ...) WHAT(X),LFE_2(WHAT, __VA_ARGS__)
#define LFE_4(WHAT, X, ...) WHAT(X),LFE_3(WHAT, __VA_ARGS__)
#define LFE_5(WHAT, X, ...) WHAT(X),LFE_4(WHAT, __VA_ARGS__)
//... repeat as needed
#define LFE_6(WHAT, X, ...) WHAT(X),LFE_5(WHAT, __VA_ARGS__)
#define LFE_7(WHAT, X, ...) WHAT(X),LFE_6(WHAT, __VA_ARGS__)
#define LFE_8(WHAT, X, ...) WHAT(X),LFE_7(WHAT, __VA_ARGS__)
#define LFE_9(WHAT, X, ...) WHAT(X),LFE_8(WHAT, __VA_ARGS__)
//... repeat as needed

#define LGET_MACRO(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME 
#define ARGMAP(action,...) \
  GET_MACRO(_0,__VA_ARGS__,LFE_9,LFE_8,LFE_7,LFE_6,LFE_5,LFE_4,LFE_3,LFE_2,LFE_1,LFE_0)(action,__VA_ARGS__)

// Make a FOREACH macro
#define SFE_0(WHAT)
#define SFE_1(WHAT, X) WHAT(X) 
#define SFE_2(WHAT, X, ...) WHAT(X)SFE_1(WHAT, __VA_ARGS__)
#define SFE_3(WHAT, X, ...) WHAT(X)SFE_2(WHAT, __VA_ARGS__)
#define SFE_4(WHAT, X, ...) WHAT(X)SFE_3(WHAT, __VA_ARGS__)
#define SFE_5(WHAT, X, ...) WHAT(X)SFE_4(WHAT, __VA_ARGS__)
//... repeat as needed
#define SFE_6(WHAT, X, ...) WHAT(X)SFE_5(WHAT, __VA_ARGS__)
#define SFE_7(WHAT, X, ...) WHAT(X)SFE_6(WHAT, __VA_ARGS__)
#define SFE_8(WHAT, X, ...) WHAT(X)SFE_7(WHAT, __VA_ARGS__)
#define SFE_9(WHAT, X, ...) WHAT(X)SFE_8(WHAT, __VA_ARGS__)
//... repeat as needed

#define GET_MACRO(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,NAME,...) NAME 
#define STRMAP(action,...) \
  GET_MACRO(_0,__VA_ARGS__,SFE_9,SFE_8,SFE_7,SFE_6,SFE_5,SFE_4,SFE_3,SFE_2,SFE_1,SFE_0)(action,__VA_ARGS__)

#include <stdio.h>

//void schema(char* name, char* typ, char* param, ...) {
void schema(void* record, char* name, char* typ, char* typs, char* param, int* offsets) {
  printf("\n\n-------------------\n");
  printf("name: %s\n", name);
  printf("%s\n", typ);

  int* o= offsets;
  char* p= param;
  char* t= typs;
  while(p && *p) {
    printf("  %-9s @ %3d : %c = ", p, *o, *t);
    void* v= ((char*)record) + *o;
    switch(*t) {
    case 's': printf("\"%s\"", *(char**)v); break;
    case 'd': printf("%lf", *(double*)v); break;
    case 'i': printf("%d", *(int*)v); break;
    case 'l': printf("%ld", *(long*)v); break;
    case 'c': printf("'%c'", *(char*)v); break;
    }
    printf("\n");
    p+= strlen(p)+1;
    o++; t++;
  }
  printf("SIZE=%d\n", *o);
}

typedef struct foo{
  char* type;
  char* name;
  char* param;
  char* impl;
  int select;
  char c;
  double num;
} foo;

int main(void) {
#define STR(a) #a
#define XXSTR(a) STR(a)"\0"

#define BAR(f) f
#define FOO(typ) (int)(long)&((typ)0)->BAR
#define TOFFSET(typ) FOO(struct typ*)

  
#define SCHEMA(record, name, typ, typs, ...)		\
  schema(record, name, #typ, typs, \
    STRMAP(XXSTR, __VA_ARGS__) "\0",\
	 (int*)&(int[]){ARGMAP(TOFFSET(typ), __VA_ARGS__),sizeof(struct typ),0})

  foo r= (foo){"TYPE", "NAME", "PARAM", "IMPL", 42, 'q', 3.141592654};

  SCHEMA(&r, "view", foo, "ssiscd", name, param, select, impl, c, num);
  int a= 45;
  typeof(a) b=77;
}

//int main(void) {
//  // FOO(a);FOO(b);FOO(c);
//  MAP(FOO, a,b,c);
//  // foo(FOO(a), FOO(b), FOO(c));
//  foo(ARGSMAP(FOO, a,b,c));
////}
