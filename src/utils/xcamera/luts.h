/* interface to the various luts.
 * the black-white lut is just built in.
 */

/* used in State to code which lut was used to load xpixels[]. */
typedef enum {
    UNDEF_LUT=0, BW_LUT, HEAT_LUT, RAINBOW_LUT, STANDARD_LUT,
    N_LUT
} LutType;

/* the proportions of each color.
 */
typedef struct {
    float red, green, blue;
} LutDef;

#define	LUTLN	256		/* n entries in each LutDef array */

extern LutDef heat_lut[];
extern LutDef rainbow_lut[];
extern LutDef standard_lut[];

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: luts.h,v $ $Date: 2001/04/19 21:11:59 $ $Revision: 1.1.1.1 $ $Name:  $
 */
