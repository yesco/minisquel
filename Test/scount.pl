sub doit {
    my ($it) = @_;
    return "1\t$it" if $it =~ /:(spc|dot|comma|qm|up|down|digit|newline)/;
    return "2\t$it" if $it =~ /:funny:.*/;
    return "2\t$it" if $it =~ /:repeat:(\d+).*/;
    if ($it =~ /:(\d+):(\w+)/) {
	return "2\t$1:$2" if $d<32;
	return "3\t$1:$2";
    }
    return ">>>$it<<<";;
}

while(<>) {
    s/<(.*?)>/"\n".doit($1)."\n"/smge;
    s/{(.*?)}/"\n".doit($1)."\n"/smge;
    s/^([a-z]+)$/"\n".length($1)."\t$1"/smge;
    s|^(\w+)$|join("\n", split(//, $1))|smge;
    s|^(\w)$|1\t$1\n|smg;
    print;
}
