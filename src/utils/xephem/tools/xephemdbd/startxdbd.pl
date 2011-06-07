#!/usr/bin/perl

$dbdir = '/usr/local/xephem/catalogs';
$lkfile = 'xephemdbd.pid';
$infifo = 'xephemdbd.in';
$logfn = "xephemdbd.log";
system "mkfifo $infifo" if (! -e $infifo);
system "./xephemdbd_static -vtlic 0 $lkfile $infifo $dbdir >> $logfn 2>&1";
