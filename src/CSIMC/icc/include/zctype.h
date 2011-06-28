#ifndef MYCTYPE_H
#define MYCTYPE_H

/* checks for alphabetic characters */
#define iszalpha(x) ( ( (x) = (x),\
						(( (x) >= 'A') && ((x) <= 'Z')) || \
						(((x) >= 'a') && ((x) <= 'z'))) )

/* checks for numerals only */
#define iszdigit(x) ( ( (x) = (x), ((x) >= '0' ) && ( (x) <= '9' )) )  

/* checks for hexadecimal numbers, i,e, 0-9 A-Z & a-z */
#define iszxdigit(x) ( ( (x) = (x),\
						 (iszdigit(x)) || \
						 (( (x) >= 'a' ) && ( (x) <= 'f')) || \
						 (( (x) >= 'A') && ( (x) <= 'Z') ) ) )  

/* checks for white space characters */
#define iszspace(x) ( ( (x) = (x),\
						( (x) == ' ' ) || ( (x) == '\t' ) || \
						( (x) == '\r') || ( (x) == '\n') ) )  

/* checks for blank characters */
#define iszblank(x) ( ( (x) = (x), ((x) == ' ' ) || ( (x) == '\t' ) ) )

/* checks for ascii characters */
#define	iszascii(x) ( !( (x) & ~0177) )	/* If +ve & < 256 */

/* checks for alphanumeric characters */
#define iszalnum(x) ( (x) = (x), ( iszalpha(x) || iszdigit(x) ) ) 

/* checks for upper cases */
#define iszupper(x) ( (x) = (x), (( (x) >= 'A') && ((x) <= 'Z')) )  

/* checks for lower cases */
#define iszlower(x) ( (x) = (x), (( (x) >= 'a') && ((x) <= 'z')) )  

/* checks for any printable characters including space */
#define iszprint(x) ( (x) = (x), (( (x) >= ' ' ) && ((x) <= '~')) )

/* checks for any printable characters except space */
#define iszgraph(x) ( (x) = (x), (iszprint(x) && ( (x) != ' ')) )

/* checks forany printable characters which is not a space or 
   alphanumeric characters */
#define iszpunct(x) ( (x) = (x), ( (iszgraph(x)) && ( !(iszalnum(x) ))) )

/* checks for control ( non-printable ) characters */
#define iszcntrl(x) ( ( !(iszprint(x))) ) 

#define tozlower(x) ( ( (x) + 'a' - 'A' ) )	/* convert upper case to lower */
#define tozupper(x) ( ( (x) + 'A' - 'a' ) )	/* convert lower case to upper */
#define	tozascii(x) ( (x) & 0x7f ) 			/* Mask off high bit.  */

#endif

