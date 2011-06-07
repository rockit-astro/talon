$!
$!      Makefile.com -- Command file to compile and link Xephem
$!
$!       Parameters:
$!          P1 = nothing, do a complete build and install
$!          P1 = INSTALL, just copy files to location pointed to by the
$!                        XEphem_Dir logical name.
$!
$!      A simple-minded command file to compile and link Xephem under
$!      OpenVMS VAX or Alpha and DECwindows MOTIF v1.1 or v1.2.
$!
$!      NOTE: You will need to choose one version of Motif to link with
$!      below by uncommenting the appropriate section of lines.
$!
$!      *** The default is for DEC C and Motif v1.2-3 ***
$!
$!      when            who                     what
$!
$!      2-feb-1992      Ethan VanMatre          original from unix makefile
$!      13-mar-1993     Max Calvani             updated for xephem v2.4.
$!      9-sep-1993      Elwood Downey           updated for xephem v2.5.
$!      9-jan-95        Bob Drucker             updated for new VMS compilers
$!      21-aug-95       Bob Drucker             updated for xephem 2.7.
$!      11-sep-95       Elwood Downey           updated for xephem 2.8.
$!      31-DEC-1995     Rick Dyson              added install options like MMS files
$!      21-MAY-1996     Rick Dyson              updated for xephem v2.9,
$!                                              Alpha DEC C, VAX DEC C ONLY
$!       9-FEB-97       Elwood Downey		Edited for 3.0(without testing!)
$!      11-APR-1997     Rick Dyson              Tested with v3.0
$!
$ On Control_Y Then GoTo FINISHED
$ On Error     Then GoTo FINISHED
$
$ If P1 .nes. "" Then GoTo 'P1'
$
$ Set Verify
$
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object aa_hadec
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object aberration
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object anomaly
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object broadcast
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object calmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object chap95
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object chap95_data
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object circum
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object closemenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object comet
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object compiler
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object constel
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object datamenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object db
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object dbmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object deltat
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object earthmap
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object earthmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object earthsat
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object eq_ecl
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object eq_gal
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object fits
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object formats
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object fsmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object gsc
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object gscnet
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object helpmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object homeio
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object jupmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object libration
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object listmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object mainmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object marsmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object misc
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object mjd
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object moon
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object mooncolong
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object moonmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object moonnf
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object msgmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object nutation
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object objmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object obliq
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object parallax
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object patchlevel
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object plans
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object plot_aux
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object plotmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object ppm
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object precess
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object preferences
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object progress
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object ps
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object query
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object reduce
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object refract
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object riset
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object riset_cir
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object rotated
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object satmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object sites
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object skyfiltmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object skyfits
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object skyhist
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object skylist
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object skyviewmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object solsysmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object sphcart
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object srchmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object sun
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object time
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object tips
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object trailmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object utc_gst
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object versionmenu
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object vsop87
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object vsop87_data
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object wcs
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object xephem
$ CC /DECC /Standard = VAXC /Optimize /NoDebug /NoList /Object xmisc
$
$LINK:
$   Link /NoMap /NoDebug xephem,   -
	    aa_hadec, -
	    aberration, -
	    anomaly, -
	    broadcast, -
	    calmenu, -
	    chap95, -
	    chap95_data, -
	    circum, -
	    closemenu, -
	    comet, -
	    compiler, -
	    constel, -
	    datamenu, -
	    db, -
	    dbmenu, -
	    deltat, -
	    earthmap, -
	    earthmenu, -
	    earthsat, -
	    eq_ecl, -
	    eq_gal, -
	    fits, -
	    formats, -
	    fsmenu, -
	    gsc, -
	    gscnet, -
	    helpmenu, -
	    homeio, -
	    jupmenu, -
	    libration, -
	    listmenu, -
	    mainmenu, -
	    marsmenu, -
	    misc, -
	    mjd, -
	    moon, -
	    mooncolong, -
	    moonmenu, -
	    moonnf, -
	    msgmenu, -
	    nutation, -
	    objmenu, -
	    obliq, -
	    parallax, -
	    patchlevel, -
	    plans, -
	    plot_aux, -
	    plotmenu, -
	    ppm, -
	    precess, -
	    preferences, -
	    progress, -
	    ps, -
	    query, -
	    reduce, -
	    refract, -
	    riset, -
	    riset_cir, -
	    rotated, -
	    satmenu, -
	    sites, -
	    skyfiltmenu, -
	    skyfits, -
	    skyhist, -
	    skylist, -
	    skyviewmenu, -
	    solsysmenu, -
	    sphcart, -
	    srchmenu, -
	    sun, -
	    time, -
	    tips, -
	    trailmenu, -
	    utc_gst, -
	    versionmenu, -
	    vsop87, -
	    vsop87_data, -
	    wcs, -
	    xmisc, -
$!
$!  Choose only ONE of the two sections below to uncomment.
$!
$!  Libraries for Motif version 1.1
$!
$!Sys$Input/Option
$!Sys$Share:DECW$DXMLibShr/Share
$!Sys$Share:DECW$XMLibShr/Share
$!Sys$Share:DECW$XLibShr/Share
$!
$!  Libraries for Motif version 1.2
$!
Sys$Input/Option
Sys$Share:DECW$DXMLibShr12.exe /Share
Sys$Share:DECW$XMLibShr12.exe /Share
Sys$Share:DECW$XTLibShrr5.exe /Share
Sys$Share:DECW$XLibShr.exe /Share
Sys$Library:VAXCRTL.olb /Library
$
$ Library /Help /Create XEphem.hlb XEphem.vms-hlp
$
$ Set NoVerify
$
$ Purge /NoLog /NoConfirm
$
$INSTALL:
$   Set NoOn
$   If F$TrnLnm ("XEphem_Dir") .nes. ""
$       Then
$           Copy XEphem.vms-ad DECW$System_Defaults:xephem.dat
$           Copy XEphem.exe,XEphem.hlb XEphem_Dir:
$           Copy [.edb]*.* XEphem_Dir:
$           Rename XEphem_Dir:readme. XEphem_Dir:readme.edb
$           Copy [.auxil]*.* /Exclude = *.dir XEphem_Dir:
$           Rename XEphem_Dir:readme. XEphem_Dir:readme.auxil
$           Set Protection = (Owner:RWE, World:RE) XEphem_Dir:*.*
$       Else
$           Write Sys$Output "You need to define XEphem_Dir and re-run with the INSTALL parameter!"
$   EndIf
$
$FINISHED:
$   Set NoVerify
$   Exit
