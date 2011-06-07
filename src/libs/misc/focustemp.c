
/************************************************************\
			Focus interpolation functions
			
			S. Ohmert 10-10-2002
	
	Maintain a database of focus positions recorded at various
	temperatures, for each filter.
	Given a current temperature, interpolate (or extrapolate)
	from the recorded data and produce a calculated result.
	
	The interpolation is linear.  The algorithm used to
	produce the result will first find the positions for
	the two nearest	recorded temperatures on either side
	of the target temperature, and produce a interpolated
	result from this.  It will then repeat the procedure
	"moving outward" to the next two nearest values, and
	so on, until all the data for that filter has been
	examined.  The median of these values is chosen as the
	returned result.
	
	The number of cycles can be controlled with a configuration
	setting in focus.cfg: MAXINTERP.  By setting this value
	to 1, you would limit the recursion to effectively produce a
	linear interpolation between the two nearest recorded temperatures.
	A large value for MAXINTERP is likely better where there are
	likely to be deviations from perfect positioning in the recorded data,
	whereas a small value (such as 1) would be better if the focus offset
	with temperature is not linear.
	If MAXINTERP is not defined in focus.cfg, it will default to 100.
	(this is set when config is read -- see telescoped focus.c)
	
	Used by XOBS AUTOFOCUS and the "autofocus" function of telescoped
	Also used by the command-line "focustemp" utility.
	
	CVS History:
		$Id: focustemp.c,v 1.6 2007/06/09 10:07:38 steve Exp $
		$Log: focustemp.c,v $
		Revision 1.6  2007/06/09 10:07:38  steve
		fixed bugs with save/restore in focustemp
		
		Revision 1.5  2006/11/27 04:15:30  steve
		fixes for autofocus
		
		Revision 1.4  2003/01/15 04:48:18  steve
		fixed bug in selecting proper filter and in choosing min/max values
		
		Revision 1.3  2002/11/05 18:11:06  steve
		Fixed problem with not extrapolating values 'outside the box'
		
		Revision 1.2  2002/10/27 04:58:03  steve
		fixed bug with looking for maximum interpolation pairs when not that much data existed
		
		Revision 1.1  2002/10/23 21:48:34  steve
		support for better interpolated focus
		
			
\***************************************************************/	

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "telenv.h"
#include "focustemp.h"

////////////

// turn on for tracing
#define DBTRACE 0

#if DBTRACE
#define TRACE fprintf(stderr,
void dumptable(void);
void dumprlist(double *rlist, int n);
#endif

static char *focusDataFile = "archive/config/FocusTemp.dat";
static FocPosEntry *focusPositionTable;
static int focusPositionEntries;

// configuration controlled variable (focus.cfg)
static int maxInterpolationPairs = 100;

// Get the pointer to the focus position table
FocPosEntry *getFocusPositionTable(void)
{ return focusPositionTable; }

// Get the number of entries in the focus position table
int getFocusPositionEntries(void)
{ return focusPositionEntries; }

// Set the maximumn number of interpolation pairs
void focusPositionSetMaxInterp(int max)
{
	maxInterpolationPairs = max;
}

// Read the focus position data into the table
int focusPositionReadData(void)
{
	FILE *fp;
	char f;
	double t,p;
	int entries = 0;
	
	fp = telfopen(focusDataFile,"r");
	if(!fp) {
		fprintf(stderr, "Unable to open focus data file $TELHOME/%s: %s\n",focusDataFile,strerror(errno));
		return -1;
	}
		
	focusPositionClearTable();
	while(1) {
		int rt = fscanf(fp,"%c %lf %lf\n",&f,&t,&p);
		if(rt == EOF) break;
		if(rt != 3) {
			fprintf(stderr,"Error in format for focus data entry #%d. Aborting read.\n",entries+1);
			fclose(fp);
			return -1;
		} else {
			focusPositionAdd(f,t,p);
			entries++;
		}
	}
	fclose(fp);
	focusPositionEntries = entries;
	return entries; // number of entries read in
}

// Save the focus position data from the table into the database (file)
int focusPositionWriteData(void)
{
	FILE *fp;
	FocPosEntry *pt = focusPositionTable;
	int entries = focusPositionEntries;
		
	fp = telfopen(focusDataFile,"w");
	if(!fp) {
		fprintf(stderr, "Unable to open focus data file $TELHOME/%s: %s\n",focusDataFile,strerror(errno));
		return -1;
	}
	if(pt) while(entries--) {		
		fprintf(fp, "%c %.2lf %.2lf\n",pt->filter,pt->tmp,pt->position);
		pt++;
	}
	fclose(fp);
	
	return 0;
}

// Clear any current focusPositionTable
void focusPositionClearTable(void)
{
#if DBTRACE
	TRACE "Clearing focus table\n");
#endif
	if(focusPositionTable) free(focusPositionTable);
	focusPositionTable = NULL;
	focusPositionEntries = 0;
}

// Add new data to the focusPositionTable
// Sort by temperature upon entry
// note that this may CHANGE the pointer for focusPositionTable!
void focusPositionAdd(char filter, double tmp, double position)
{
	FocPosEntry *pt;
	int i,entry;
	
#define FPT_CHUNK_SIZE 10

	if((focusPositionEntries % FPT_CHUNK_SIZE) == 0) {
		focusPositionTable = realloc(focusPositionTable, (focusPositionEntries+2)*FPT_CHUNK_SIZE*sizeof(FocPosEntry));
	}
	if(!focusPositionTable) {
		fprintf(stderr,"Allocation error with focusPositionTable at %d entries\n",focusPositionEntries);
		return;
	}	
#if DBTRACE	
	TRACE "++++++++++++++++++++\n");
#endif	
	// find a spot in current table where the temperature is higher than this
	pt = focusPositionTable;
	entry = 0;
	while(entry < focusPositionEntries) {
		if(pt[entry].tmp > tmp) break;
		entry++;
	}
#if DBTRACE	
	dumptable();
	TRACE "broke at entry %d of %d for %.2lf\n",entry,focusPositionEntries,tmp);
#endif	
	// move everything up one slot from here to end of list
	for(i=focusPositionEntries; i>entry; i--) {
		pt[i] = pt[i-1];
	}
#if DBTRACE	
	dumptable();
#endif	
	// add our entry here
	pt[entry].filter = filter;
	pt[entry].tmp = tmp;
	pt[entry].position = position;
	focusPositionEntries++;
#if DBTRACE	
	dumptable();
	TRACE "^^^^^^^^^^^^^^^^^^^^^^\n");
	TRACE "...adding %c %.2lf %2lf as entry #%d\n",filter,tmp,position,focusPositionEntries);
#endif
	focusPositionWriteData();	// Save after adding new value
#undef FPT_CHUNK_SIZE	
}

#if DBTRACE
void dumptable()
{
	FocPosEntry *pt = focusPositionTable;
	int n = focusPositionEntries;

	TRACE "========\n");	
	while(n--) {
		TRACE "%c %.2lf %.2lf\n",pt->filter,pt->tmp,pt->position);
		pt++;
	}
	TRACE "========\n");	
}

void dumprlist(double *rlist, int n)
{
	while(n--) {
		fprintf(stdout,"%.2lf\n",*rlist++);
	};
}
#endif

// Given the current temperature for a filter,
// interpolate a position for the focus, and return this.
// Do this by finding the two data points nearest the
// given temperature, and compute a linear interpolation from this
// and add this value to a sorted list.
// Then, "move outwards" to the next two nearest data points and repeat
// until all the data has been examined.
// Return the median of the values in the list in outPosition.
// return 0 on success, or non-zero on error.

int focusPositionFind(char filter, double tmp, double *outPosition)
{
	double *rlist;
	double lt, ht;
	FocPosEntry *pt, *pht, *plt, *phtH, *pltL;
	int entry;
	int rlistEntries = 0;
	int traceCnt;
	int lowfound,highfound;
	int rt;
	
#if DBTRACE	
	TRACE "finding focus position...\n");
#endif	
	if(!focusPositionTable) {
		fprintf(stderr, "No focusPositionTable exists!\n");
		return -1;
	}
	
	// allocate a list to hold our median result
	rlist = (double *) malloc(((focusPositionEntries+1)/2) * sizeof(double)); // can't be any bigger than this
	if(!rlist) {
		fprintf(stderr, "Unable to allocate intermediate list for focus position calculation\n");
		return -1;
	}
	
	lt = ht = tmp;
	traceCnt = focusPositionEntries / 2;
    if(focusPositionEntries % 2) traceCnt++;
	if(traceCnt > maxInterpolationPairs) traceCnt = maxInterpolationPairs;
	lowfound = highfound = 0;
	while(traceCnt--) { // do recursively, moving outward
#if DBTRACE	
//		TRACE "finding brackets for %.2lf / %.2lf\n",lt,ht);
#endif
		// First, find the lowest and highest qualifying values as fallbacks
		pt = focusPositionTable;
		pht = plt = NULL;
		for(entry=0; entry<focusPositionEntries; entry++) {
			if(pt[entry].filter == filter) {
				plt = &pt[entry];
				break;
			}
		}
		for(entry=focusPositionEntries-1; entry>=0; entry--) {
			if(pt[entry].filter == filter) {
				pht = &pt[entry];
				break;
			}
		}
		
		pltL = plt;
		phtH = pht;
#if DBTRACE		
			TRACE "Min %c %.2lf %.2lf\n",plt->filter,plt->tmp,plt->position);
			TRACE "Max %c %.2lf %.2lf\n",pht->filter,pht->tmp,pht->position);
#endif			
			
		// Find low and high values that surround this range
		entry = 0;
		pt = focusPositionTable;
		while(entry < focusPositionEntries) {
#if DBTRACE		
			TRACE "Considering %c %.2lf %.2lf\n",pt->filter,pt->tmp,pt->position);
#endif			
			if(pt->filter == filter) {
				if(pt->tmp <= lt) {
					plt = pt;
					lowfound++;
#if DBTRACE
					TRACE "Found low @%c %.2lf %.2lf\n",pt->filter,pt->tmp,pt->position);
#endif					
				}
				if(pt->tmp >= ht) {
					pht = pt;
					highfound++;
#if DBTRACE
					TRACE "Found high @%c %.2lf %.2lf\n",pt->filter,pt->tmp,pt->position);
#endif					
					break; // once we find high, we're done
				}
			}
			pt++;
			entry++;
		}
		
		// handle cases where our data is "outside the box" and we must extrapolate
		if(!lowfound) {
			// use the lowest value in the data as our low side
			plt = pltL;
		}
		if(!highfound) {
			int hi;
			
			pht = phtH; // use highest value we have
			// the lowfound count is how far from the top we should look for the low value
			hi = focusPositionEntries-1-lowfound;
			if(hi < 0) hi = 0;
			plt = &focusPositionTable[hi];
		}
			
		// get these two values and interpolate a position from them; add to list
		if(plt && pht) {
			double trange = pht->tmp - plt->tmp;
			double prange = pht->position - plt->position;
			double tval = tmp - plt->tmp;
			double ival = trange ? ((tval/trange) * prange) + plt->position : plt->position;
			// insert into rlist
			int e,i;
			e = 0;
			while(e < rlistEntries) {
				if(rlist[e] > ival) break;
				e++;
			}
			for(i=rlistEntries; i>e; i--) {
				rlist[i] = rlist[i-1];
			}
			rlist[e] = ival;
			rlistEntries++;
			
#if DBTRACE			
			// debug output
			TRACE "%c @ %.2lfC:  %.2lf/%.2lf -> %.2lf/%.2lf ==> %.2lf\n",
							filter,tmp,
							plt->tmp,plt->position,
							pht->tmp,pht->position,
							ival);
#endif			
			// assign next values as being just a notch lower/higher than the current low/high
			lt = plt->tmp - 0.01;
			ht = pht->tmp + 0.01;
			
		} else {
			// couldn't find anything... we're done
			break;
		}
	}
	
#if DBTRACE	
	TRACE "Dumping rlist:\n");
	dumprlist(rlist,rlistEntries);	
	TRACE "==============\n");
#endif	
	if(rlistEntries) {
		// return the median of values in rlist
		if(outPosition) *outPosition = rlist[rlistEntries/2];
		rt = 0;
	} else {
		// We couldn't find a value at all!
		rt = -1;
	}
	if(rlist) free(rlist);
	return rt;

}

