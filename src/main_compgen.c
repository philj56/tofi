#include <stdio.h>
#include "compgen.h"
#include "string_vec.h"

int main()
{
	struct string_vec commands = compgen();
	for (size_t i = 0; i < commands.count; i++) {
		printf("%s\n", commands.buf[i]);
	}
	string_vec_destroy(&commands);
}
