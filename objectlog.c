//  ObjectLog Interpreter
// ----------------------
// (>) 2022 jsk@yesco.org

#define MAXVARS 1024
#define MAXPLAN 2048

// all variables in a plan are numbered
// zero:th var isn't used.
// a zero index is a delimiter

dbval* var[MAXVARS]= {0};
char* name[MAXVARS]= {0};

// PLAN
//
// indices into vars[i] (names[i])

int* plan[MAXPLAN]= {0};

// Plan is a serialized array of indices
//
// Example:
//
//   set, -var, const, 0
//   fun, -ivar, ivar, ..., 0
//   fun, ivar, 0
//   fun, 0
//   fun, 0,
//   0

void run(int* p) {
  int* start= p;
  dbval* v= var;

#define N abs(*p++)
#define L(v) ((long)v)
  
#define R v[r]
#define A v[a]
#define B v[b]
#define C v[c]

#define Pr     r=N
#define Pra    Pr;a=N
#define Prab   Pra;b=N
#define Prabc  Prab;c=N

for(int f=N; f; !N) { switch(f) {
  case 0  : // I WIN;

  // set
  case ':': Pra;  R= A; break;

  // arith
  case '+': Prab; R= A+B; break;
  case '-': Prab; R= A-B; break;
  case '*': Prab; R= A*B; break;
  case '/': Prab; R= A/B; break;
  case '%': Prab; R= L(A)%L(B); break;

  // cmp
  case '=': Prab; R= A==B; break;
  case '<': Prab; R= A<B; break;
  case '>': Prab; R= A>B; break;
//case TWO('<', '>'):

  // logic
  case '!': Pra;  R= !A; break;
  case '&': Prab; R= A&&B; break;
  case '|': Prab; R= A||B; break;

  case 'i': Prab;
    for(R=A; R<=B; R++) run(p); break;
    
  case 'p': while(*p) {
    Pr; printf("%d \n", r); } break;
    
  }}

// cleanup down to start
 while(--p>=start) {
   if (*p < 0) {
     printf("\nclean: %d\n", *p);
     *p= 0;
   }
 }
}

int main(int argc, char** argv) {

  // read plan from arguments
  int* p= plan;
  while(--argc>0 && **++argv) {
    char* s= *argv++;
    *p++ = isdigit(*s) ? atoi(s) : *s+256*[s1];
  }
  *p++ = 0; // just to be sure!


  run(plan);
}
