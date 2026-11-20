/* What src/zip.c hands a core for each zip named: its entry's name, size and
 * an FNV-1a hash of its bytes, or "refused". For test/zipmethods.py. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "diatom.h"

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		void *data = NULL;
		size_t len = 0;
		char name[512];
		uint32_t h = 2166136261u;

		if (!diatom_zip_load(argv[i], &data, &len, name, sizeof name)) {
			printf("refused\n");
			continue;
		}
		for (size_t k = 0; k < len; k++) h = (h ^ ((const unsigned char *)data)[k]) * 16777619u;
		printf("%s %zu %08x\n", name, len, (unsigned)h);
		free(data);
	}
	return 0;
}
