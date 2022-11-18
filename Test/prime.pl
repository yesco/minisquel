# 1m6.771s
# 66.771s

$p= 2124679;
$p= 2124680;

foreach $i (2..100) {
    $x= $p-1;
    foreach $d (2..$x) {
	$y= $p % $d;
	print "$p not prime says $d\n" if $y==0; 
    }
}
