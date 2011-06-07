
// Header for focustemp interpolation library functions
// (Part of libmisc)

typedef struct
{
	char	filter;
	double	tmp;
	double	position;
	
} FocPosEntry;

FocPosEntry *getFocusPositionTable(void);
int getFocusPositionEntries(void);
void focusPositionSetMaxInterp(int max);
int focusPositionReadData(void);
int focusPositionWriteData(void);
void focusPositionClearTable(void);
void focusPositionAdd(char filter, double tmp, double position);
int focusPositionFind(char filter, double tmp, double *outPosition);

