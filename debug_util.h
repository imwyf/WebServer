#include <assert.h>
#include <stdio.h>
#include <string>

#ifdef DEBUG
#define DBGprint(...) printf(__VA_ARGS__)
#else
#define DBGprint(...)
#endif
