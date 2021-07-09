/* some handy string functions */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strops.h"

#ifndef PI
#define PI 3.14159265358979323846
#endif

char *basenm(str) char *str;
{
    char *base;
    char c;

    for (base = str; (c = *str++) != '\0';)
        if (c == '/')
            base = str;

    return (base);
}

/* convert string s to all lower-case IN PLACE */
void strtolower(s) char *s;
{
    char c;

    for (; (c = *s) != '\0'; s++)
        if (isupper(c))
            *s = tolower(c);
}

/* convert string s to all upper-case IN PLACE */
void strtoupper(s) char *s;
{
    char c;

    for (; (c = *s) != '\0'; s++)
        if (islower(c))
            *s = toupper(c);
}

/* like strcmp() but ignores case and whitespace */
int strcwcmp(char *s1, char *s2)
{
    char c1, c2;

    do
    {
        do
            c1 = *s1++;
        while (isspace(c1));
        if (isupper(c1))
            c1 = tolower(c1);
        do
            c2 = *s2++;
        while (isspace(c2));
        if (isupper(c2))
            c2 = tolower(c2);
    } while (c1 == c2 && c1 != '\0' && c2 != '\0');

    return (c1 - c2);
}

#if 0
/* like strstr() but ignores case */
char *
strcasestr (char *s1, char *s2)
{
	char *s1cpy = strcpy(malloc(strlen(s1)+1),s1);
	char *s2cpy = strcpy(malloc(strlen(s2)+1),s2);
	char *ss;

	strtolower (s1cpy);
	strtolower (s2cpy);
	ss = strstr (s1cpy, s2cpy);

	free (s1cpy);
	free (s2cpy);

	return (ss);
}
#endif

/* given a month number, 1-12, return pointer to a static string of its
 * 3-char name.
 */
char *monthName(int monthno)
{
    static char *mname[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };

    if (monthno < 1 || monthno > 12)
        return ("?");
    return (mname[monthno - 1]);
}

/* given an angle in rads E of N, return pointer to a static string of 1-3
 * chars (plus \0) naming its cardinal direction.
 */
char *cardDirName(double a)
{
    static char sectors[][4] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",
    };

    a *= 180.0 / PI;
    a += 11.25;
    a -= 360.0 * floor(a / 360.0);

    return (sectors[(int)(a / 22.5)]);
}

/* given an angle in rads E of N, return pointer to a static string of words
 * describing its cardinal direction.
 */
char *cardDirLName(double a)
{
    static char *sectors[] = {
        "north",      "north north east", "north east", "east north east",  "east",       "east south east",
        "south east", "south south east", "south",      "south south west", "south west", "west south west",
        "west",       "west north west",  "north west", "north north west",
    };

    a *= 180.0 / PI;
    a += 11.25;
    a -= 360.0 * floor(a / 360.0);

    return (sectors[(int)(a / 22.5)]);
}
