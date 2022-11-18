# 50.916s

p= 2124679
p= 2124680

for i in range(2,101):
    for d in range(2,p):
        y= p % d
        if y==0:
            print(p, " not prime says", d);
