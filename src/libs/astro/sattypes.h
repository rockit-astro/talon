#ifndef __SATTYPES_H
#define __SATTYPES_H

/* $Id: sattypes.h,v 1.1.1.1 2001/04/19 21:12:13 ocaas Exp $ */

typedef struct _Vec3 {
    double x, y, z;
} Vec3;


typedef struct _LookAngle {
    double az;
    double el;
    double r;
} LookAngle;


typedef struct _Geoloc {
    double lt;
    double ln;
    double h;
} GeoLoc;

#endif /* __SATTYPES_H */
