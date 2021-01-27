// code.c                                           Stan Eisenstat (08/08/19)
//
// Implementation of putBits() / flushBits() / getBits() described in code.h


#include <stdlib.h>
#include <string.h>
//#include "/c/cs323/Hwk4/code.h"
#include "code.h"

// Return the stage number from the environment variable STAGE (or 3 if STAGE
// does not exist or is not equal to 1 or 2).
int stage (void)
{
    char *s = getenv("STAGE");
    if (s && !strcmp (s, "1"))
	return 1;
    else if (s && !strcmp (s, "2"))
	return 2;
    else
	return 3;
}


// == PUTBITS MODULE =======================================================

// Information shared by putBits() and flushBits()
static int nExtra = 0;                  // #bits from previous byte(s)
static unsigned long extraBits = 0;     // Extra bits from previous byte(s)


// Write CODE (NBITS bits) to standard output
void putBits (int nBits, long code)
{
    if (nBits <= 0 || NBITS_MAX < nBits) {              // NBITS valid?
	fprintf (stderr, "invalid nBits = %d\n", nBits);
	return;
    }

    unsigned long maxCode = 1UL << (nBits-1);           // Compute maximum code
    maxCode = maxCode + (maxCode-1);
    if (code < 0 || maxCode < code) {                   // CODE representable?
	fprintf (stderr,
		 "nBits = %d, maxCode = %lu, code = %ld\n",
		  nBits,      maxCode,       code        );
	return;
    }

    if (stage() == 1) {                                 // Stage 1?
	printf ("%ld\n", code);
	return;

    } else if (stage() == 2) {                          // Stage 2?
	printf ("%d:%ld\n", nBits, code);
	return;
    }

    // If EXTRABITS << NBITS could overflow, output high-order CHAR_BIT bits
    if (nBits > NBITS_MAX - CHAR_BIT) {
	putBits (CHAR_BIT, code >> (nBits-CHAR_BIT));
	nBits -= CHAR_BIT;
    }

    nExtra += nBits;                                    // Add bits to EXTRABITS
    extraBits = (extraBits << nBits) | code;
    while (nExtra >= CHAR_BIT) {                        // Output whole chars &
	nExtra -= CHAR_BIT;                             //  save remaining bits
	unsigned long c = extraBits >> nExtra;
	putchar (c);
	extraBits ^= c << nExtra;
    }
}


// Flush remaining bits to standard output
void flushBits (void)
{
    if (stage() == 3) {
	if (nExtra != 0)
	    putchar (extraBits << (CHAR_BIT - nExtra));
    }
}


// == GETBITS MODULE =======================================================

// Return next code (#bits = NBITS) from input stream or EOF on end-of-file
long getBits (int nBits)
{
    if (nBits <= 0 || NBITS_MAX < nBits) {              // NBITS valid?
	fprintf (stderr, "invalid nBits = %d\n", nBits);
	return EOF;
    }

    unsigned long maxCode = 1 << (nBits-1);             // Maximum possible code
    maxCode = maxCode + (maxCode-1);

    if (stage() == 1) {
	long code;
	if (scanf ("%ld\n", &code) < 1){                // No code?
	    return EOF;

	} else if (code < 0 || maxCode < code) {        // Not within bounds?
	    fprintf (stderr, "nBits = %d, code = %ld\n",
			      nBits,      code        );
	    return EOF;

	} else {
	    return code;
	}

    } else if (stage() == 2) {
	long code;
	int nRcvd;
	if (scanf ("%d:%ld\n", &nRcvd, &code) < 2       // No NRCVD:CODE or
	      || nRcvd < nBits) {                       //  too few bits?
	    return EOF;

	} else if (nRcvd > nBits                        // Too many bits or
		     || code < 0                        //  not within bounds?
		     || maxCode < code) {
	    fprintf (stderr, "nBits = %d, nRcvd = %d, code = %ld\n",
			      nBits,      nRcvd,      code         );
	    return EOF;

	} else {
	    return code;
	}
    }

    static int nExtra = 0;              // #bits from previous byte(s)
    static unsigned long extraBits = 0; // Extra bits from previous byte(s)
									      
    long result = 0;                    // Value to return

    // If EXTRABITS << NBITS could overflow, read high-order CHAR_BIT bits
    if (nBits > NBITS_MAX - CHAR_BIT) {
	result = getBits (CHAR_BIT);
	if (result < 0)
	    return EOF;
	nBits -= CHAR_BIT;
    }

    // Read enough new bytes to have at least NBITS bits to extract code
    long c;
    while (nExtra < nBits) {
	if ((c = getchar()) == EOF)                     // Too few bits?
	    return EOF;
	nExtra += CHAR_BIT;
	extraBits = (extraBits << CHAR_BIT) | c;
    }
    // Extract remaining bits and save remainder
    nExtra -= nBits;
    c = extraBits >> nExtra;
    extraBits ^= c << nExtra;
    return (result << nBits) | c;
}
