#!/bin/sed -f
# map S records addrs into range 0x8000 .. 0xbfff
s/^\(S1..\)0/\18/
s/^\(S1..\)1/\19/
s/^\(S1..\)2/\1a/
s/^\(S1..\)3/\1b/
s/^\(S1..\)4/\18/
s/^\(S1..\)5/\19/
s/^\(S1..\)6/\1a/
s/^\(S1..\)7/\1b/
s/^\(S1..\)[cC]/\18/
s/^\(S1..\)[dD]/\19/
s/^\(S1..\)[eE]/\1a/
s/^\(S1..\)[fF]/\1b/