/* these two structs are used to form lists of polygon vertices and lists
 *   of such lists. they are used to form maps.
 */

typedef struct {
    short lg, lt;	/* longitude and latitude, degs (*100 for earth) */
} MCoord;

typedef struct {
    char *rname;	/* region name */
    MCoord *mcp;	/* list of MCoords */
    int nmcp;		/* number of entries in mcp[] */
} MRegion;

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: map.h,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $
 */
