#!/usr/bin/perl -w
# given a *.mp, produce a list of "size start function" by decreasing size.

open (S, "|sort -nr") or die "Can not start sort\n";

while(<>) {
    next unless /^       [\dA-F][\dA-F][\dA-F][\dA-F]/;
    ($val, $sym) = split();
    $val = oct "0x$val";
    printf (S "%5d 0x%04x %s\n", $val-$lval, $val, $lsym) if (defined($lval));
    $lval = $val;
    $lsym = $sym;
}
close (S);
