/* surround declarations with P_() to include prototypes if it looks likes
 * compiler should handle them.
 */
#ifndef P_
#if defined(__STDC__) || defined(__cplusplus) || NeedFunctionPrototypes
#define P_(s) s
#else
#define P_(s) ()
#endif
#endif /* P_ */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: P_.h,v $ $Date: 2001/04/19 21:12:13 $ $Revision: 1.1.1.1 $ $Name:  $
 */
