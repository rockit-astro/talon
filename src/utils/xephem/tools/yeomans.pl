#!/usr/bin/perl
# convert elements posted by Don Yeomans to http://encke.jpl.nasa.gov/eph to
# xephem format.
# we need to scan the entire ephemeris so we can dig out the various values
#   which are sprinkled near the beginning and end of the file.
# (C) 1996 Elwood Charles Downey
# Dec 9, 1996. v1.0

# read the file and pick out the good stuff.
while (<>) {
    chop();
    # Capture the name from a title line -- example:
    #  Object: Comet C/1995 O1 (Hale-Bopp)
    if (/^ *Object: (.*)$/) {
	$_ = $1;
	s/Comet//;
	s/^ *//;
	s/ *$//;
	s/[()]//g;
	$name = $_;
	next;
    }

    # Capture the epoch of the "Corrected elements" from this line:
    #  Epoch  2450520.50000 = 1997 Mar 13.00000
    if (/^ *Epoch *[\d.]+ *= *([\d]+) *([a-zA-Z]+) *([\d.]+) *$/) {
	$eyr = $1;
	$emn = &monthno($2);
	$eda = $3 + 0;
	$epoch = "$emn/$eda/$eyr";
	next;
    }

    # Capture the "Corrected" elements:
    if (/^ *e *([\d.]+) *[\d.]+ *$/) {
	$E = $1;
	next;
    }
    if (/^ *q *([\d.]+) *[\d.]+ *$/) {
	$Q = $1;
	next;
    }
    if (/^ *Tp *[\d.]+ *[\d.]+ *([\d]+) *([a-zA-Z]+) *([\d.]+) *$/) {
	$YR = $1;
	$MN = &monthno($2);
	$DY = $3 + 0;
	$HR = 0;
	next;
    }
    if (/^ *Node *([\d.]+) *[\d.]+ *$/) {
	$LOMEGA = $1;
	next;
    }
    if (/^ *w *([\d.]+) *[\d.]+ *$/) {
	$SOMEGA = $1;
	next;
    }
    if (/^ *i *([\d.]+) *[\d.]+ *$/) {
	$I = $1;
	next;
    }


    # Capture magnitude model from line clear at the bottom -- example:
    # = 0.4 + 5.00*log(Delta) +  5.6*log(r)
    if (/^ *= *([\d.]+) *\+ *5\.00\*log\(Delta\) *\+ *([\d.]+)\*log\(r\)/) {
	$g = $1;
	$k = $2/2.5;
	next;
    }
}

# do a few sanity checks
length($name) > 0 || die "No name found.\n";
defined($YR) || die "No orbital elements found.\n";
$g > 0 || $k > 0 || warn "No magnitude model found.\n"; # not especially fatal.

# format output depending on orbital shape.
if ($E < 1) {
    # elliptical
    $i = $I;		# inclination
    $O = $LOMEGA;	# long of asc node
    $o = $SOMEGA;	# arg of peri
    $a = $Q/(1-$E);	# mean distance
    $n = 0;		# mean daily motion (derived)
    $e = $E;		# eccentricity
    $M = 0;		# mean anomaly
    $D = $epoch;	# date of i/O/o

    printf "%s,e,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%g/%.8g/%g,%s,g%g,%g\n",
	$name, $i, $O, $o, $a, $n, $e, $M, $MN, $DY+$HR/24, $YR, $D, $g, $k;
} else {
    die "Only elliptical format is supported at this time.\n";
}

# given a month abbrev, return month number
sub monthno {
    $_ = $_[0];
    return  1 if (/^Jan/i);
    return  2 if (/^Feb/i);
    return  3 if (/^Mar/i);
    return  4 if (/^Apr/i);
    return  5 if (/^May/i);
    return  6 if (/^Jun/i);
    return  7 if (/^Jul/i);
    return  8 if (/^Aug/i);
    return  9 if (/^Sep/i);
    return 10 if (/^Oct/i);
    return 11 if (/^Nov/i);
    return 12 if (/^Dec/i);
    die "Bad month name: $_\n";
}

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: yeomans.pl,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $
