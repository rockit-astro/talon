#!/usr/bin/perl
# read the gscd log and produce a list of connects, stars, DNS
# usage:
#   -r: reverse the DNS name components for easier domain sorting.
#   -i: process a syslog from inetd
#
# Elwood Downey
# v1.1:	17 Jun 96
# v1.2: 24 Jul 96: use IP if can not get name; add date to Totals + use stdout.

# check options
while (@ARGV > 0) {
    $rev = 1 if $ARGV[0] eq '-r';
    $inetd = 1 if $ARGV[0] eq '-i';
    shift;
}

# scan
while (<>) {
    if ($inetd) {
	next unless /gscd.*Sending/;
	($x,$x,$x,$x,$x,$ip,$x,$nstars) = split;
    } else {
	next unless /Sending/;
	($x,$x,$x,$ip,$x,$nstars) = split;
    }
    $ip =~ s/://;
    if ($names{$ip} eq "") {
	($a, $b, $c, $d) = split(/\./, $ip);
	$address = pack('C4', $a, $b, $c, $d);
	($name, $x) = gethostbyaddr($address, 2);
	$name =~ tr/A-Z/a-z/;
	$name = "$ip" if $name eq "";
	$names{$ip} = $name;
    } else {
	$name = $names{$ip};
    }
    &revdns ($name) if $rev;
    $connects{$name}++;
    $stars{$name} += $nstars;
}

# print connection info
foreach $name (sort keys(%connects)) {
    printf "%4d %8d %s\n", $connects{$name}, $stars{$name}, $name;
    $totcon += $connects{$name};
    $totstars += $stars{$name};
    $totsites++;
}

# add cummulatives
$dt = `date`;
chop($dt);
printf "$dt Totals: %d connects %d stars %d sites\n", $totcon, $totstars,
								    $totsites;

# function to reverse the dotted DNS entries in @_[0], IN PLACE
sub revdns {
    local (@flds) = split (/\./, @_[0]);
    local ($revdns) = "";
    foreach $a (@flds) {
	$revdns = "$a.$revdns";
    }
    chop ($revdns);
    @_[0] = $revdns;
}

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: gsclogstats.pl,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $
