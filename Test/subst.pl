@words= ();
open(IN, "Test/mywords.txt") or die "no file";
while(<IN>) {
    chop($_);
    push(@words, $_);
}
close(IN);

while(<>) {
    #print "ORIG: $_";
    
    # funny chars
    s/([<:>\{\}])/<:funny:>$1/msg;
    s/([0-9])/<:digit:>$1/msg;
#    print "QUOTED: $_";

    s/\n/<:newline:>/smg;

    #print "BEFORE: $_";
    
    # a least 4 ---> o<repeat:4>
    s/((.)\2\2\2+)/"$2\{:repeat:".length($1)."\}"/msge;

    #print "AFTER: $_";

    # up/lower
    s/^([a-z])/<:down>$1/msg;
    s/\n([a-z])/<:down>$1/msg;

    s/^([A-Z])/lc($1)/msge;
    s/\n([A-Z])/lc($1)/msge;
    
    # combined with . and or space
    s/\. ([A-Z])/"<:dotu>".lc($1)/msge;

    s/\b([A-Z]*{2.})/"<:spccaps>".lc($1)/msge;

    $i= 0;
    foreach $w (@words) {
	s/\b((?<!:)$w)\b/"<:$i:".lc($w).">"/imsge;
	$i++;
    }

    # funny chars
    s/(["'!#\$%&'\(\)\*\+\/;=\[\\\]\|@\`-])/<:funny:$1>/msg;

    s/ </</msg;
    s/> />/msg;

    s/\, /<:comma>/msg;
    s/ - /<:dash>/msg;
    
    s/ ([A-Z])/"<:spcu>".lc($1)/msge;

    s/([A-Z])/"{:up}".lc($1)/msge;

    # not worth it! foo -> fo<dup>
    # s/((.)\2)/"$2<dup>/msg;

    # not so common ??
    # s/? ([A-Z]/<qmU>$1/msg;
    
    s/ /<:spc>/sgm;
    s/\./<:dot>/sgm;
    s/,/<:comma>/sgm;
    s/!/<:bang>/sgm;
    s/\?/<:qm>/sgm;

    print;
}
    
# Ideas for compression
#
# 8 special "tokens"
# - space                      (5%)
# - delimiter 00 \0            (1%)
#             01 newline       (2%)
#             10 ,_            (1%)
#             11 ._U          (0.5%)
# - funny  12345 #%#(*&(*#$!(*#%
# - words  12345 32 most common words
# - endings 1234 16 different     
# - words  12-78 the rest "huffman"?
# - repeat 12345 4-36 repeat!
#
# - used alphabet
# 
# - ....
# - ....
