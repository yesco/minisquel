print "
// shannon encoder/decoder (generated)

typedef struct shannon {
  unsigned int code;
  char c;
} shannon;

shannon shan[]= {
";

while(<>) {
    if (/^(\w+):(\w+)\s+(\d+)\s+(\S+)\s+/) {
	my ($code, $len, $f, $str) = ($1,$2,$3,$4);
	$str= "' '" if $str eq "'";
	$str= "'\\''" if $str eq "'''";
	print sprintf("  {0x$code%02x, ", hex($len)), $str, "},\n"
    } else {
	print STDERR "%% GARBAGE? $_";
    }
}

print "
  {0, 0},
};

";
