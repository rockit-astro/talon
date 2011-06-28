/* define the order of the sections.
 * this works because this is linked first and so establishes the 
 * section names. the corresponding _end entries are in end.c.
 * N.B. make sure the last one is the same as used with NewHeap.
 */

        asm (".area text");
	asm ("__text_start::");
	asm (".area data");
	asm ("__data_start::");
	asm (".area bss");
	asm ("__bss_start::");

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: start.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
