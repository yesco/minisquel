
$s= `cat ../smaz/resume.txt`;

#$s= "foo bar fie fum";

while($s =~ /(.)/g) {
    print "'$1'\n";
}
print "\n";

if (0) {
    while($s =~ /(..)(?{print "'$1'\n";})(*F)/g) {}
    print "\n";

    while($s =~ /(...)(?{print "'$1'\n";})(*F)/g) {}
    print "\n";
}


exit;


while($s =~ /\b(\w+)\b/g) {
    # print it as many times as it
    # is long to make it count
    # as much as it'd save!
    foreach (1..(length($1)+1)) {
	printf("'$1'\n");
    }
}
print "\n";

if (0) {
    while($s =~ /\b(\w+\W+\w+)\b(?{print "'$1'\n";})(*F)/g) {}
    print "\n";
}
