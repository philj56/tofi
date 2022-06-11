#include <stdio.h>
#include "compgen.h"
#include "string_vec.h"

int main()
{
	struct string_vec commands = compgen_cached();
	string_vec_save(&commands, stdout);
	string_vec_destroy(&commands);
}
