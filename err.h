#pragma once

// /* wypisuje informacje o błędnym zakończeniu funkcji systemowej
// i kończy działanie */
// extern void syserr(const char* fmt, ...);

// /* wypisuje informacje o błędzie i kończy działanie */
// extern void fatal(const char* fmt, ...);

/* print system call error message and terminate */
extern void syserr(int bl, const char *fmt, ...);

/* print error message and terminate */
extern void fatal(const char *fmt, ...);