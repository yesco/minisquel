#include <stdio.h>

// fib := [0, 1, -1.fib, -2.fib]

typedef struct fibiter {
  int n;
  long prev, cur;
} fibiter;

int FIB1(fibiter *fib) {
  int prev= fib->prev;
  int cur= fib->cur;
  switch(fib->n++) {
  case 0:  fib->cur= 0; break;
  case 1:  fib->cur= 1; break;
  default: fib->cur= prev + cur;
  }
  fib->prev= cur;
  return 1;
}

int FIB2(fibiter* f) {
  *f= (fibiter){f->n,
    f->cur,
    f->n==1 ? 1 :
      f->prev + f->cur};
  return 1;
}

fibiter FIB3(fibiter f) {
  return (fibiter){f.n,
    f.cur,
    f.n==1? 1 :
      f.prev + f.cur};
}

int FIB4(long* n, long* prev, long* cur) {
  int nprev= *cur;
  switch((*n)++) {
  case 0:  *cur= 0; break;
  case 1:  *cur= 1; break;
  default: *cur= *prev + *cur; break;
  }
  *prev = nprev;
  return 1;
}

int FIB5(long* n, long *next, long* cur) {
  if (*n==1) *next= 1;
  long prev= *cur;
  *cur= *next;
  *next= *cur + prev;
  return 1;
}

//////////////////////////////

typedef struct doubleiter1 {
  long n;
  double cur, next, end, step;
} doubleiter1;

int DOUBLE1(doubleiter1* f) {
  *f= (doubleiter1){
    .cur=  f->next,
    .next= f->next + f->step,
    .end=  f->end,
    .step= f->step};
  return f->cur <= f->end;
}

int DOUBLE1b(doubleiter1* f) {
  *f= (doubleiter1){f->n,
    f->next,
    f->next + f->step,
    f->end,
    f->step};
  return f->cur <= f->end;
}

typedef struct doubleiter2 {
  long n;
  double cur, end, step;
} doubleiter2;

int DOUBLE2(doubleiter2* f) {
  f->cur= f->n==0 ? f->cur :
    f->cur + f->step;
  return f->cur <= f->end;
}

int DOUBLE2x(doubleiter2* f) {
  f->cur= (f->n==5 || f->n==3 || 
	   f->n==0) ? f->cur :
    f->cur + f->step;
  f->n++;
  return f->cur <= f->end;
}

int DOUBLE3(double *next, double *cur, double* end, double* step) {
  *cur= *next;
  if (*cur > *end) return 0;
  *next+= *step;
  return 1;
}

int DOUBLE3x(long* n, double *next, double *cur, double* end, double* step) {
  *cur= *next;
  if (*cur > *end) return 0;
  if (*n==3) return 1; // douplicate
  *next+= *step;
  return 1;
}


int JOIN2(double v, double start, double end, double step) {
  double cur;
  while(DOUBLE3(&start, &cur, &end, &step)) {
    if (cur==v) printf("JOIN %lg\n", v);
  }
  return 1;
}

int JOIN1(double start, double end, double step) {
  double cur;
  while(DOUBLE3(&start, &cur, &end, &step)) {
    JOIN2(cur, 3, 9, 0.5);
  }
  return 1;
}

void JOIN() {
  JOIN1(1,10,1);
}

// if equal and duplicates, it does cross product for that section!
void MJOIN() {
  long an= 0, bn= 0, bsaved_n= 0;
  double as=1, ae=10, at=1, acur= -1;
  double bs=3, be=9, bt=0.5, bcur= -2;
  
  if (!DOUBLE3x(&an, &as, &acur, &ae, &at)) return;
  if (!DOUBLE3x(&bn, &bs, &bcur, &be, &bt)) return;
  an++; bn++;
  
  double bsaved= 0, bsaved_s, bsaved_e, bsaved_t, bsaved_cur;
  while(1) {
    printf("(%lg, %lg)\n", acur, bcur);
    if (acur==bcur) {
      printf("-->RESULT: %lg\n", acur);
      if (!bsaved) {
	printf("\t\tSaving b\n");
	// save
	bsaved= 1;
	bsaved_n= bn;
	bsaved_s= bs;
	bsaved_cur= bcur;
	bsaved_e= be;
	bsaved_t= bt;
      }
    }
    
    if (acur<bcur) {
      printf("\tadvance A\n");
      int pcur= acur;
      if (!DOUBLE3x(&an, &as, &acur, &ae, &at)) return;
      an++;
      // only restore if a not change
      if (bsaved) {
	bsaved= 0;
	if (pcur==acur) {
	  printf("\t\tRestoring b\n");
	  // restore
	  bn= bsaved_n;
	  bs= bsaved_s;
	  bcur= bsaved_cur;
	  be= bsaved_e;
	  bt= bsaved_t;
	}
      }
      // >= inner right join
      // TODO: doesn't handle duplicate in a!
    } else if (acur>=bcur) {
      printf("\tadvance B\n");
      if (!DOUBLE3x(&bn, &bs, &bcur, &be, &bt)) return;
      bn++;
    }
  }
}

// if equal and duplicates, it does cross product for that section!
void mjoin(doubleiter2* a, doubleiter2* b, doubleiter2* saved) {
  int bsaved= 0;
  
  if (!DOUBLE2x(a)) return;

  if (!DOUBLE2x(b)) return;
  
  while(1) {
    printf("(%lg, %lg)\n", a->cur, b->cur);
    if (a->cur==b->cur) {
      printf("-->RESULT: %lg\n", a->cur);
      if (!bsaved) {
	printf("\t\tSaving b\n");
	// save
	bsaved= 1;
	*saved= *b;
      }
    }
    
    if (a->cur < b->cur) {
      printf("\tadvance A\n");
      int pcur= a->cur;
      if (!DOUBLE2x(a)) break;
      // only restore if a not change
      if (bsaved) {
	bsaved= 0;
	if (pcur==a->cur) {
	  printf("\t\tRestoring b\n");
	  // restore
	  *b= *saved;
	}
      }
      // >= inner right join
      // TODO: doesn't handle duplicate in a!
    } else if (a->cur >= b->cur) {
      printf("\tadvance B\n");
      if (!DOUBLE2x(b)) break;
    }
  }
}

void MJOIN2() {
  doubleiter2 a= {0, 1, 10, 1};
  doubleiter2 b= {0, 3, 9, 0.5};

  doubleiter2 saved= {};

  mjoin(&a, &b, &saved);
}

int main(int argc, char** argv) {
  MJOIN(); 
  printf("--------------------22222222222222\n");
  MJOIN2(); exit(0);
  
  JOIN(); exit(0);
  
  struct foo{ int a, b; } f= {4,30};
  printf("f={%d, %d}\n", f.a, f.b);
  f = (struct foo){
    f.b+= (f.a+=200),
    ++f.a};
  printf("f={%d, %d}\n", f.a, f.b);
  
  switch(10) {

  case 10: {  // 1b
    doubleiter1 dit= {0, 0, 1, 10, 1};
    while(DOUBLE1b(&dit)) {
      dit.n++;
      printf("DBL %lg\n", dit.cur);
    }
    break; }
  case 11: {
    doubleiter1 dit= {0, 0, 1, 10, 1};
    while(DOUBLE1(&dit)) {
      dit.n++;
      printf("DBL %lg\n", dit.cur);
    }
    break; }
  case 12: {
    doubleiter2 dit= {0, 1, 10, 1};
    while(DOUBLE2(&dit)) {
      dit.n++;
      printf("DBL %lg\n", dit.cur);
    }
    break; }
  case 13: {
    double cur=0, next=1, end=10, step=1;
    while(DOUBLE3(&next, &cur, &end, &step)) {
      printf("DBL %lg\n", cur);
    }
    break; }


  case 01: {
    fibiter fib= {0};
    while(FIB1(&fib)) {
      fib.n++;
      printf("%ld\n", fib.cur);
    }
    break; }
  case 02: {
    fibiter fib= {0};
    while(FIB2(&fib)) {
      fib.n++;
      printf("%ld\n", fib.cur);
    }
    break; }
  case 03: {
    fibiter fib= {0};
    while((fib= FIB3(fib)), 1) {
      fib.n++;
      printf("%ld\n", fib.cur);
    }
    break; }
  case 04: {
    long n= 0, next= 0, cur= 0;
    while(FIB4(&n, &next, &cur)) {
      n++;
      printf("FIB %ld\n", cur);
    }
    break; }
  default: break;
  }
}

