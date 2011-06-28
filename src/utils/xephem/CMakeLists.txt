cmake_minimum_required (VERSION 2.6)
project (xephem)

set(XEPHEM_SRC
        aavso.c         
        atlas.c         
        bdl.c           
        broadcast.c     
        calmenu.c       
        closemenu.c     
        compiler.c      
        constel.c       
        datamenu.c      
        db.c            
        dbmenu.c        
        earthmap.c      
        earthmenu.c     
        explodegif.c    
        fallbacks.c     
        fits.c          
        formats.c       
        fsmenu.c        
        glance.c        
        gsc.c           
        gscnet.c        
        helpmenu.c      
        homeio.c        
        hznmenu.c       
        jupmenu.c       
        listmenu.c      
        mainmenu.c      
        marsmenu.c      
        marsmmenu.c     
        moonmenu.c      
        moonnf.c        
        msgmenu.c       
        netmenu.c       
        objmenu.c       
        patchlevel.c    
        plot_aux.c      
        plotmenu.c      
        preferences.c   
        progress.c      
        ps.c            
        query.c         
        rotated.c       
        satmenu.c       
        saveres.c       
        setiathome.c    
        sites.c         
        skyeyep.c       
        skyfiltmenu.c   
        skyfits.c       
        skyhist.c       
        skylist.c       
        skytoolbar.c    
        skyviewmenu.c   
        solsysmenu.c    
        srchmenu.c      
        time.c          
        tips.c          
        trailmenu.c     
        uranusmenu.c    
        usno.c          
        versionmenu.c   
        wcs.c           
        webtlemenu.c    
        xe2.c           
        xephem.c        
        xmisc.c
        )

include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/fits")
#include_directories ("${CORE_LIBS_DIR}/fs")
#include_directories ("${CORE_LIBS_DIR}/misc")
#include_directories ("${CORE_LIBS_DIR}/xmisc")
#include_directories ("${CORE_LIBS_DIR}/wcs")

add_executable(xephem ${XEPHEM_SRC})

target_link_libraries (xephem astro m Xm Xt)

install (TARGETS xephem DESTINATION bin)
install(DIRECTORY auxil DESTINATION ${CORE_INSTALL_DIR}/xephem)
install(DIRECTORY fifos DESTINATION ${CORE_INSTALL_DIR}/xephem)
