// code.h                                         Stan Eisenstat (08/08/19)
//
// Interface to putBits() / getBits() / flushBits()

#include <stdio.h>
#include <limits.h>

// Maximum allowed value of NBITS
#define NBITS_MAX (sizeof(long) * CHAR_BIT)

// Write the low-order NBITS bits of CODE to a bit stream on the standard
// output.  If NBITS <= 0, NBITS_MAX < NBITS, CODE < 0, or 1<<NBITS <= CODE,
// write an error message to the standard error instead.
//
// The bit stream is padded with up to CHAR_BIT-1 zero bits so that its
// length is a multiple of CHAR_BIT, and then is broken into a sequence of
// CHAR_BIT-bit characters that are written to the standard output.  To do so
// some low-order bits in CODE may have to be saved until the next call to
// putBits(), and the final call must be followed by a call to flushBits().
//
// When the environment variable STAGE exists and is equal to 1, CODE is
// written in ASCII using the format "%d\n".  When STAGE is equal to 2, NBITS
// and CODE are written using "%d:%d\n".

void putBits (int nBits, long code);


// If there are any bits in the bit stream being saved for the next call to
// putBits(), write to the standard output a character containing those bits
// padded with up to CHAR_BIT-1 zero bits.
//
// When the environment variable STAGE exists and is equal to 1 or 2, return
// instead.

void flushBits (void);


// Return an integer whose low-order bits are the next NBITS bits from a
// bit stream on the standard input and whose high-order bits are zero.
// Return EOF if there are fewer than NBITS remaining.  If NBITS <= 0,
// NBITS_MAX < NBITS or CODE < 0, write an error message to the standard
// error and return EOF instead.
//
// The bit stream is assumed to be padded with up to CHAR_BIT-1 zero bits
// so that its length is a multiple of CHAR_BIT and to be broken into
// CHAR_BIT-bit characters.
//
// When the environment variable STAGE exists and is equal to 1, the bits are
// read as an ASCII integer CODE using the format "%d\n".  When STAGE is equal
// to 2, the number of bits NRCVD and CODE are read using "%d:%d\n" and an
// error message is written to the standard error if NBITS < NRCVD.  In
// both cases write an error message to the standard error and return EOF
// if CODE < 0 or 1<<NBITS <= CODE.

long getBits (int nBits);


// In bash you can set STAGE by preceding the command by a local variable
// assignment like STAGE=2 or by using the built-in export command.
//
// A more permanent alternative is to set the value in main() by inserting
// the statement
//   setenv ("STAGE", "2", 1);
// before the first call to putBits() or getBits(); this also requires
//   #include <stdlib.h>
// at the start of the file.  But do not forget to change the second string
// and recompile when you want to change the value of STAGE, or to remove
// the statement when you reach STAGE=3.
