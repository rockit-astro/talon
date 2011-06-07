
	/* scan left-to-right across top and bottom */
	n = 2*r + 1;
	p1 = &ip[-rw-r-1];	/* allow for preincrement in loop */
	p2 = &ip[rw-r-1];	/* allow for preincrement in loop */
	for (i = 0; i < n; i++) {
	    p = (double)(*++p1);
	    sum += p;
	    sum2 += p*p;
	    p = (double)(*++p2);
	    sum += p;
	    sum2 += p*p;
	}
	npix += 2*n;

	/* scan top-to-bottom down each side -- don't repeat corners! */
	n = 2*r - 1;
	p1 = &ip[-rw-r];	/* allow for preincrement in loop */
	p2 = &ip[-rw+r];	/* allow for preincrement in loop */
	for (i = 0; i < n; i++) {
	    p = (double)(*(p1+=w));
	    sum += p;
	    sum2 += p*p;
	    p = (double)(*(p2+=w));
	    sum += p;
	    sum2 += p*p;
	}
	npix += 2*n;

	/* compute stats */
	*mp = (int)floor(sum/npix + 0.5);
	*sp = (int)floor(sqrt(fabs((sum2 - sum*sum/npix)/(npix-1))));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: edgeStats.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
