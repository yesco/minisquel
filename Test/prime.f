\ 
\ http://www.lysator.liu.se/~ptp/pub/prime.f

: postpone  word must-find compile-word ; immediate
: constant  word define  code ,  compile-literal  postpone EXIT ;
: ,c"    BEGIN key dup [ char " compile-literal ] <> WHILE c, REPEAT drop ;
: ,"     here 0 c, here ,c" here swap - swap c! ;
: count  dup 1+ swap c@ ;

2124680 constant pp

here ," is not prime says " constant _
: .is_not_prime_says  _ count type ;

: prime
   pp 1- BEGIN dup 2 >= WHILE
      pp over mod 0= IF pp . .is_not_prime_says dup . cr THEN
   1- REPEAT drop
;

: go
   2 BEGIN dup 100 <= WHILE
      prime flush
   1+ REPEAT drop
;

go bye
