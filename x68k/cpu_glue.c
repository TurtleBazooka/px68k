#include <assert.h>

#include "common.h"
#include "m68000.h"

int32_t m68000_ICountBk;

void
Error(const char *s)
{

	printf("%s\n", s);
	assert(0);
}
