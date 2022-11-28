#include <stdio.h>
#include <stdlib.h>
#include "compgen.h"
#include "string_vec.h"

int main()
{
	char *buf = compgen_cached();
	struct string_ref_vec commands = string_ref_vec_from_buffer(buf);
	for (size_t i = 0; i < commands.count; i++) {
		fputs(commands.buf[i].string, stdout);
		fputc('\n', stdout);
	}
	string_ref_vec_destroy(&commands);
	free(buf);
}
