#include <assert.h>
#include <stdio.h>
#include <string>

#ifdef DEBUG
#define DBG(...) /
do {
    / fprintf(stdout, "[---DEBUG---]%s %s(Line %d): ", __FILE__, __FUNCTION__, __LINE__);
    / fprintf(stdout, __VA_ARGS__);
    /
} while (0)
#else
#define DBG(...)
#endif