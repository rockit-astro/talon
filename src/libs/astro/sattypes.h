#ifndef __SATTYPES_H
#define __SATTYPES_H

typedef struct _Vec3
{
    double x, y, z;
} Vec3;

typedef struct _LookAngle
{
    double az;
    double el;
    double r;
} LookAngle;

typedef struct _Geoloc
{
    double lt;
    double ln;
    double h;
} GeoLoc;

#endif /* __SATTYPES_H */
