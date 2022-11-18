// 1.432s ! only 100% slower than C

p= 2124679;
p= 2124680;

for(i=2; i<=100; i++) {
    x= p-1;
    for(d=2; d<=x; d++) {
	y= p % d;
	if (y==0)
	    console.log(p, " not prime says", d);
    }
}
