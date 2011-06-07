cmake_minimum_required (VERSION 2.6)
project (astro)

set(ASTRO_SRC aa_hadec.c airmass.c auxil.c circum.c deep.c eq_ecl.c
helio.c mjd.c nutation.c plans.c refract.c sphcart.c utc_gst.c
aberration.c anomaly.c chap95.c comet.c deltat.c eq_gal.c libration.c
moon.c obliq.c precess.c riset.c sdp4.c sun.c vsop87.c actan.c ap_as.c 
chap95_data.c dbfmt.c earthsat.c formats.c misc.c mooncolong.c parallax.c 
reduce.c riset_cir.c sgp4.c thetag.c vsop87_data.c)
 
add_library(astro SHARED ${ASTRO_SRC})

install (TARGETS astro DESTINATION lib)
