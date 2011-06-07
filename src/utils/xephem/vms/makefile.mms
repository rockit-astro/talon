# A MMS (or the FreeWare MMS clone by Matt Madison, MMK) Description for xephem
#
# For an Alpha with DEC C,
#    use the following command line execution (the default):
#
#   MMS /Description = Makefile.mms Install
#
# For a VAX using DEC C, use the following command line execution:
#
#   MMS /Description = Makefile.mms /Macro = (VAX=1) Install
#
#       These both assume you are using Motif v1.2-xx.
#
#
#*************** NOTE  XEphem 3.0 does not build with VAX C anymore ***********
# For a VAX with VAX C, use the following command line execution (the default):
#
#       MMS /Description = Makefile.mms /Macro = VAXC=1
#
#       This assumes you are using Motif v1.1.  Change the OPTS file to the
#       DECC version below if you have Motif v1.2 with VAX C.  Don't use the
#       Macro method to select the XEphem_DECC.opt file.
#*******************************************************************************
#
#   Rick Dyson  (rick-dyson@uiowa.edu)
#   Created: 25-MAY-1995
#   Last Modified: 13-AUG-1995 for XEphem v2.7
#                  31-DEC-1995 for XEphem v2.8
#                  14-MAY-1996 for XEphem v2.9  "/Standard = VAXC" became
#                                               required with TCP/IP functions
#                                               CC /DECC is needed for those
#                                               with both VAX C and DEC C
#                   2-JUL-1996 for XEphem v2.9.1
#
#   Edited (without testing!) 9-FEB-1997 for XEphem v3.0 by Elwood Downey
#                  10-APR-1997 for XEphem v3.0  RLD
#
# Modify the destination directory to your local tastes...
Install_Dir = XEphem_Dir

OPTIMIZE = /Optimize
DEBUG = /NoDebug

OPTS = Sys$Disk:[]XEphem_DECC.opt
STD = /Standard = VAXC
CC = CC /DECC

.ifdef VAX
DEFS = /Define = VAX
.endif

CFLAGS = $(CFLAGS) $(STD) $(DEFS) $(OPTIMIZE) $(DEBUG)
LINKFLAGS = $(LINKFLAGS) $(DEBUG)

OBJS =	aa_hadec.obj aberration.obj anomaly.obj broadcast.obj calmenu.obj chap95.obj chap95_data.obj \
	circum.obj closemenu.obj comet.obj compiler.obj constel.obj datamenu.obj db.obj \
	dbmenu.obj deltat.obj earthmap.obj earthmenu.obj earthsat.obj eq_ecl.obj eq_gal.obj fits.obj \
	formats.obj fsmenu.obj gsc.obj gscnet.obj helpmenu.obj homeio.obj jupmenu.obj \
	libration.obj listmenu.obj mainmenu.obj marsmenu.obj misc.obj mjd.obj moon.obj \
	mooncolong.obj moonmenu.obj moonnf.obj msgmenu.obj nutation.obj objmenu.obj \
	obliq.obj parallax.obj patchlevel.obj plans.obj plot_aux.obj plotmenu.obj ppm.obj \
	precess.obj preferences.obj progress.obj ps.obj query.obj reduce.obj refract.obj \
	riset.obj riset_cir.obj rotated.obj satmenu.obj sites.obj skyhist.obj skylist.obj \
	skyfiltmenu.obj skyfits.obj skyviewmenu.obj solsysmenu.obj sphcart.obj \
	srchmenu.obj sun.obj time.obj tips.obj trailmenu.obj utc_gst.obj versionmenu.obj \
	xephem.obj vsop87.obj vsop87_data.obj wcs.obj xmisc.obj

OBJLIST = aa_hadec,aberration,anomaly,broadcast,calmenu,chap95,chap95_data,\
	circum,closemenu,comet,compiler,constel,datamenu,db,\
	dbmenu,deltat,earthmap,earthmenu,earthsat,eq_ecl,eq_gal,fits,\
	formats,fsmenu,gsc,gscnet,helpmenu,homeio,jupmenu,\
	libration,listmenu,mainmenu,marsmenu,misc,mjd,moon,\
	mooncolong,moonmenu,moonnf,msgmenu,nutation,objmenu,\
	obliq,parallax,patchlevel,plans,plot_aux,plotmenu,ppm,\
	precess,preferences,progress,ps,query,reduce,refract,\
	riset,riset_cir,rotated,satmenu,sites,skyhist,skylist,\
	skyfiltmenu,skyfits,skyviewmenu,solsysmenu,sphcart,\
	srchmenu,sun,time,tips,trailmenu,utc_gst,versionmenu,\
	xephem,vsop87,vsop87_data,wcs,xmisc

.first
	@- Define /NoLog Sys DECC$Library_Include

all :   xephem help
        @ Write Sys$Output "Finished Building XEphem!!!"

xephem.exe : $(OBJS) $(OPTS)
	$(LINK) $(LINKFLAGS) $(OBJLIST),$(OPTS)/Option

xephem :	xephem.exe
        @ Continue

help :
	@ If F$Search ("xephem.hlb") .eqs. "" Then Library /Help /Create xephem.hlb xephem.vms-hlp

clean :
	@ Set Message /NoSeverity /NoFacility /NoIdentification /NoText
	@- Set Protection = Owner:RWED *.*;-1
	@- Purge /NoConfirm
	@ Set Message /Severity /Facility /Identification /Text

clobber : 	clean
        @ Set Message /NoSeverity /NoFacility /NoIdentification /NoText
        @- Set Protection = Owner:RWED *.exe;*,*.obj;*
	@- Delete /NoConfirm *.obj;*,*.hlb;,*.exe;
        @ Set Message /Severity /Facility /Identification /Text

install :	all
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Copy xephem.exe $(Install_Dir)
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Copy xephem.hlb $(Install_Dir)
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Copy xephem.VMS-ad DECW$System_Defaults:xephem.dat
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Copy [.edb]*.* $(Install_Dir)
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Rename $(Install_Dir):Readme. $(Install_Dir):Readme.edb
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Copy /Exclude = *.dir [.auxil]*.* $(Install_Dir)
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Rename $(Install_Dir):Readme. $(Install_Dir):Readme.auxil
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Set Protection = (Owner:RWE, World:RE) $(Install_Dir):*.*
        @- Write Sys$Output ""
	@- If (F$TrnLnm ("$(Install_Dir)") .eqs. "") Then Write Sys$Output \
        "You need to define XEphem_Dir to somewhere before trying to install it!!!"
	@- If (F$TrnLnm ("$(Install_Dir)") .nes. "") Then Write Sys$Output \
	"Finished installing XEphem in $(Install_Dir)"

#  especially worth having this dependency so the version stays current.
versionmenu.obj :	patchlevel.h

#
#  Build the linker options file for OpenVMS DEC C or VAX C.
#
Sys$Disk:[]XEphem_DECC.opt :
        @ Open /Write TMP XEphem_DECC.opt
        @ Write TMP "! XEphem (v3.0) Linker Options list for VMS DEC C"
        @ Write TMP "! This assumes Motif v1.2-3"
        @ Write TMP "Sys$Library:DECW$DXMLibShr12.exe /Share"
        @ Write TMP "Sys$Library:DECW$XMLibShr12.exe /Share"
        @ Write TMP "Sys$Library:DECW$XTLibShrR5.exe /Share"
        @ Write TMP "Sys$Library:DECW$XLibShr.exe /Share"
        @ Write TMP "Sys$Library:VAXCRTL.OLB /Library"
        @ Close TMP

#Sys$Disk:[]XEphem_VAXC.opt :
#        @ Open /Write TMP XEphem_VAXC.opt
#        @ Write TMP "! XEphem (v2.9) Linker Options list for VMS VAX C"
#        @ Write TMP "Sys$Library:DECW$DXMLibShr.exe /Share"
#        @ Write TMP "Sys$Library:DECW$XMLibShr.exe /Share"
#        @ Write TMP "Sys$Library:DECW$XLibShr.exe /Share"
#        @ Write TMP "Sys$Library:VAXCRTL.exe /Share"
#        @ Close TMP
