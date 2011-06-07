# awk script to convert GPS almanac data into the representation
# used by xephem for earth satellite orbits.
# It was written quick and is very dirty.
# when used by: "awk -f alma.awk gps_almanac_file >xephem_file.edb" ,
# the file xephem.edb hopefully contains a correct description of the
# GPS satellite orbits. This has been  checked with only two different
# almanac data files , where the results of xephem  appeared to be ok.
# From: fischer@vs-ulm.dasa.de (Dr. Harald Fischer)
# Date: Tue, 30 Apr 1996 11:31:35 --100
BEGIN { nsats = 0;
        mu=3986005e8;	#earth's gravitational constant
        PI=3.1415926;
        radtodeg=180.0/PI;
        gps_leap = 11; # number of gps leap seconds as of January 1996.
                       # would need to be updated, but have only a small effect.
      }
/^ID:/                   { id=$2 ;}
/^Health:/               { health=$2;}
/^Eccentricity:/         { ecc = $2*1.0;}        
/^Time/                  { toa = 1.0*$4;}
/^Orbital/               { incl= $3*radtodeg;}
/^Rate/                 { roa=$5;}
/^SQRT/			{ sqrt_a= $3*1.0;}
/^Right/		{ ra = $5*1.0;} # (right ascension at ta) - (greenwich sidereal at t0) 
/^Argument/		{ aop = $4*radtodeg;}
/^Mean/			{ ma = $3*radtodeg;}
/^Af0/			{ af0=$2;}
/^Af1/			{ af1=$2;}
/^week:/                 { week=$2; nsats++;
# convert GPS time toa to julian:
   jd = 2444244.5+7.0*week+(toa-gps_leap)/(3600*24);
# and then to UT:
   a=int(jd+0.5);
   b=a+1537;
   c=int((b-122.1)/365.25);
   d=int(365.25*c);
   e=int((b-d)/30.6001);
   D=b-d-int(30.6001*e)+(jd+0.5)-int(jd+0.5);
   M=e-1-12*int(e/14.0);
   Y=c-4715-int((7+M)/10.0);
# we need the sidereal greenwich time at t0 = st0
# then the RAAN at toa is just the sum of st0+ra.
   jd0 = 2444244.5+7.0*week-gps_leap/(3600*24); # julian for beginning of gps week.
# the difference T to the standard epoch J2000.0 = JD 2451545.0
   T = (jd0 - 2451545.0)/36525;
   theta_0 = 24110.54841 + 8640184.812866 * T + 0.093104 *T*T - 6.2e-6*T*T*T; 
#polynomial for mean sidereal time expanded around J2000.0 (in seconds).
   theta_0 = theta_0/(24*3600);
# map RAAN into the range 0..1 days
   theta_0 = theta_0 - int(theta_0);
   if (theta_0 <0.0 ) theta_0 = theta_0 + 1.0;
# and convert to rad:
   st0 = theta_0*2*PI;
   ra = ra + st0;
   ra = radtodeg*ra;
# the mean motion as derived from the orbits semimajor axis:
   motion = sqrt(mu)/(sqrt_a*sqrt_a*sqrt_a)*3600*12/PI;
   printf("PRN-%d,E,%d/%f/%d,%f,%f,%f,%f,%f,%f,0.0,1\n",
   id, M,D,Y,incl,ra,ecc,aop,ma,motion); }

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: gps.awk,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $
