#include <glib.h>
#include <stdbool.h>
#include "desktop_vec.h"
#include "matching.h"
#include "log.h"
#include "string_vec.h"
#include "unicode.h"
#include "xmalloc.h"

static bool match_current_desktop(char * const *desktop_list, gsize length);

[[nodiscard("memory leaked")]]
struct desktop_vec desktop_vec_create(void)
{
	struct desktop_vec vec = {
		.count = 0,
		.size = 128,
		.buf = xcalloc(128, sizeof(*vec.buf)),
	};
	return vec;
}

void desktop_vec_destroy(struct desktop_vec *restrict vec)
{
	for (size_t i = 0; i < vec->count; i++) {
		free(vec->buf[i].id);
		free(vec->buf[i].name);
		free(vec->buf[i].path);
		free(vec->buf[i].keywords);
	}
	free(vec->buf);
}

void desktop_vec_add(
		struct desktop_vec *restrict vec,
		const char *restrict id,
		const char *restrict name,
		const char *restrict path,
		const char *restrict keywords)
{
	if (vec->count == vec->size) {
		vec->size *= 2;
		vec->buf = xrealloc(vec->buf, vec->size * sizeof(vec->buf[0]));
	}
	vec->buf[vec->count].id = xstrdup(id);
	vec->buf[vec->count].name = utf8_normalize(name);
	if (vec->buf[vec->count].name == NULL) {
		vec->buf[vec->count].name = xstrdup(name);
	}
	vec->buf[vec->count].path = xstrdup(path);
	vec->buf[vec->count].keywords = xstrdup(keywords);
	vec->buf[vec->count].search_score = 0;
	vec->buf[vec->count].history_score = 0;
	vec->count++;
}

void desktop_vec_add_file(struct desktop_vec *vec, const char *id, const char *path)
{
	GKeyFile *file = g_key_file_new();
	if (!g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL)) {
		log_error("Failed to open %s.\n", path);
		return;
	}

	const char *group = "Desktop Entry";

	if (g_key_file_get_boolean(file, group, "Hidden", NULL)
			|| g_key_file_get_boolean(file, group, "NoDisplay", NULL)) {
		goto cleanup_file;
	}

	char *name = g_key_file_get_locale_string(file, group, "Name", NULL, NULL);
	if (name == NULL) {
		log_error("%s: No name found.\n", path);
		goto cleanup_file;
	}

	/*
	 * This is really a list rather than a string, but for the purposes of
	 * matching against user input it's easier to just keep it as a string.
	 */
	char *keywords = g_key_file_get_locale_string(file, group, "Keywords", NULL, NULL);
	if (keywords == NULL) {
		keywords = xmalloc(1);
		*keywords = '\0';
	}

	gsize length;
	gchar **list = g_key_file_get_string_list(file, group, "OnlyShowIn", &length, NULL);
	if (list) {
		bool match = match_current_desktop(list, length);
		g_strfreev(list);
		list = NULL;
		if (!match) {
			goto cleanup_all;
		}
	}

	list = g_key_file_get_string_list(file, group, "NotShowIn", &length, NULL);
	if (list) {
		bool match = match_current_desktop(list, length);
		g_strfreev(list);
		list = NULL;
		if (match) {
			goto cleanup_all;
		}
	}

	desktop_vec_add(vec, id, name, path, keywords);

cleanup_all:
	free(keywords);
	free(name);
cleanup_file:
	g_key_file_unref(file);
}

static int cmpdesktopp(const void *restrict a, const void *restrict b)
{
	struct desktop_entry *restrict d1 = (struct desktop_entry *)a;
	struct desktop_entry *restrict d2 = (struct desktop_entry *)b;
	return strcmp(d1->name, d2->name);
}

static int cmpscorep(const void *restrict a, const void *restrict b)
{
	struct scored_string *restrict str1 = (struct scored_string *)a;
	struct scored_string *restrict str2 = (struct scored_string *)b;

	int hist_diff = str2->history_score - str1->history_score;
	int search_diff = str2->search_score - str1->search_score;
	return hist_diff + search_diff;
}

void desktop_vec_sort(struct desktop_vec *restrict vec)
{
	qsort(vec->buf, vec->count, sizeof(vec->buf[0]), cmpdesktopp);
}

struct desktop_entry *desktop_vec_find_sorted(struct desktop_vec *restrict vec, const char *name)
{
	/*
	 * Explicitly cast away const-ness, as even though we won't modify the
	 * name, the compiler rightly complains that we might.
	 */
	struct desktop_entry tmp = { .name = (char *)name };
	return bsearch(&tmp, vec->buf, vec->count, sizeof(vec->buf[0]), cmpdesktopp);
}

struct string_ref_vec desktop_vec_filter(
		const struct desktop_vec *restrict vec,
		const char *restrict substr,
		enum matching_algorithm algorithm)
{
	struct string_ref_vec filt = string_ref_vec_create();
	for (size_t i = 0; i < vec->count; i++) {
		int32_t search_score;
		search_score = match_words(algorithm, substr, vec->buf[i].name);
		if (search_score != INT32_MIN) {
			string_ref_vec_add(&filt, vec->buf[i].name);
			/* Store the score of the match for later sorting. */
			filt.buf[filt.count - 1].search_score = search_score;
			filt.buf[filt.count - 1].history_score = vec->buf[i].history_score;
		} else {
			/* If we didn't match the name, check the keywords. */
			search_score = match_words(algorithm, substr, vec->buf[i].keywords);
			if (search_score != INT32_MIN) {
				string_ref_vec_add(&filt, vec->buf[i].name);
				/*
				 * Arbitrary score addition to make name
				 * matches preferred over keyword matches.
				 */
				filt.buf[filt.count - 1].search_score = search_score - 20;
				filt.buf[filt.count - 1].history_score = vec->buf[i].history_score;
			}
		}
	}
	/*
	 * Sort the results by this search_score. This moves matches at the beginnings
	 * of words to the front of the result list.
	 */
	qsort(filt.buf, filt.count, sizeof(filt.buf[0]), cmpscorep);
	return filt;
}

struct desktop_vec desktop_vec_load(FILE *file)
{
	struct desktop_vec vec = desktop_vec_create();
	if (file == NULL) {
		return vec;
	}

	ssize_t bytes_read;
	char *line = NULL;
	size_t len;
	while ((bytes_read = getline(&line, &len, file)) != -1) {
		if (line[bytes_read - 1] == '\n') {
			line[bytes_read - 1] = '\0';
		}
		char *id = line;
		size_t sublen = strlen(line);
		char *name = &line[sublen + 1];
		sublen = strlen(name);
		char *path = &name[sublen + 1];
		sublen = strlen(path);
		char *keywords = &path[sublen + 1];
		desktop_vec_add(&vec, id, name, path, keywords);
	}
	free(line);

	return vec;
}

void desktop_vec_save(struct desktop_vec *restrict vec, FILE *restrict file)
{
	/*
	 * Using null bytes for field separators is a bit odd, but it makes
	 * parsing very quick and easy.
	 */
	for (size_t i = 0; i < vec->count; i++) {
		fputs(vec->buf[i].id, file);
		fputc('\0', file);
		fputs(vec->buf[i].name, file);
		fputc('\0', file);
		fputs(vec->buf[i].path, file);
		fputc('\0', file);
		fputs(vec->buf[i].keywords, file);
		fputc('\n', file);
	}
}

bool match_current_desktop(char * const *desktop_list, gsize length)
{
	const char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
	if (xdg_current_desktop == NULL) {
		return false;
	}

	struct string_vec desktops = string_vec_create();

	char *saveptr = NULL;
	char *tmp = xstrdup(xdg_current_desktop);
 	char *desktop = strtok_r(tmp, ":", &saveptr);
 	while (desktop != NULL) {
		string_vec_add(&desktops, desktop);
 		desktop = strtok_r(NULL, ":", &saveptr);
 	}

	string_vec_sort(&desktops);
	for (gsize i = 0; i < length; i++) {
		if (string_vec_find_sorted(&desktops, desktop_list[i])) {
			return true;
		}
 	}

	string_vec_destroy(&desktops);
	free(tmp);
	return false;
}

/*
 * Checking-in commented-out code is generally bad practice, but this may be
 * needed in the near future. Using the various GKeyFile functions above
 * ensures correct behaviour, but is relatively slow (~3-4 ms for 60 desktop
 * files). Below are some quick and dirty replacement functions, which work
 * correctly except for name localisation, and are ~4x faster. If we go a while
 * without needing these, they should be deleted.
 */

// static  char *strip(const char *str)
// {
// 	size_t start = 0;
// 	size_t end = strlen(str);
// 	while (start <= end && isspace(str[start])) {
// 		start++;
// 	}
// 	if (start == end) {
// 		return NULL;
// 	}
// 	while (end > start && (isspace(str[end]) || str[end] == '\0')) {
// 		end--;
// 	}
// 	if (end < start) {
// 		return NULL;
// 	}
// 	if (str[start] == '"' && str[end] == '"' && end > start) {
// 		start++;
// 		end--;
// 	}
// 	size_t len = end - start + 1;
// 	char *buf = xcalloc(len + 1, 1);
// 	strncpy(buf, str + start, len);
// 	buf[len] = '\0';
// 	return buf;
// }
//
// static char *get_option(const char *line)
// {
// 	size_t index = 0;
// 	while (line[index] != '=' && index < strlen(line)) {
// 		index++;
// 	}
// 	if (index >= strlen(line)) {
// 		return NULL;
// 	}
// 	index++;
// 	while (isspace(line[index]) && index < strlen(line)) {
// 		index++;
// 	}
// 	if (index >= strlen(line)) {
// 		return NULL;
// 	}
// 	return strip(&line[index]);
// }
// static bool match_current_desktop2(const char *desktop_list)
// {
// 	const char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
// 	if (xdg_current_desktop == NULL) {
// 		return false;
// 	}
// 
// 	struct string_vec desktops = string_vec_create();
// 
// 	char *saveptr = NULL;
// 	char *tmp = xstrdup(xdg_current_desktop);
//  	char *desktop = strtok_r(tmp, ":", &saveptr);
//  	while (desktop != NULL) {
// 		string_vec_add(&desktops, desktop);
//  		desktop = strtok_r(NULL, ":", &saveptr);
//  	}
// 	free(tmp);
// 
// 	/*
// 	 * Technically this will fail if the desktop list contains an escaped
// 	 * \;, but I don't know of any desktops with semicolons in their names.
// 	 */
// 	saveptr = NULL;
// 	tmp = xstrdup(desktop_list);
//  	desktop = strtok_r(tmp, ";", &saveptr);
//  	while (desktop != NULL) {
// 		if (string_vec_find_sorted(&desktops, desktop)) {
// 			return true;
// 		}
//  		desktop = strtok_r(NULL, ";", &saveptr);
//  	}
// 	free(tmp);
// 
// 	string_vec_destroy(&desktops);
// 	return false;
// }
// 
// static void desktop_vec_add_file2(struct desktop_vec *desktop, const char *id, const char *path)
// {
// 	FILE *file = fopen(path, "rb");
// 	if (!file) {
// 		log_error("Failed to open %s.\n", path);
// 		return;
// 	}
// 
// 	char *line = NULL;
// 	size_t len;
// 	bool found = false;
// 	while(getline(&line, &len, file) > 0) {
// 		if (!strncmp(line, "[Desktop Entry]", strlen("[Desktop Entry]"))) {
// 			found = true;
// 			break;
// 		}
// 	}
// 	if (!found) {
// 		log_error("%s: No [Desktop Entry] section found.\n", path);
// 		goto cleanup_file;
// 	}
// 
// /* Please forgive the macro usage. */
// #define OPTION(key) (!strncmp(line, (key), strlen((key))))
// 	char *name = NULL;
// 	found = false;
// 	while(getline(&line, &len, file) > 0) {
// 		/* We've left the [Desktop Entry] section, stop parsing. */
// 		if (line[0] == '[') {
// 			break;
// 		}
// 		if (OPTION("Name")) {
// 			if (line[4] == ' ' || line[4] == '=') {
// 				found = true;
// 				name = get_option(line);
// 			}
// 		} else if (OPTION("Hidden")
// 				|| OPTION("NoDisplay")) {
// 			char *option = get_option(line);
// 			if (option != NULL) {
// 				bool match = !strcmp(option, "true");
// 				free(option);
// 				if (match) {
// 					goto cleanup_file;
// 				}
// 			}
// 		} else if (OPTION("OnlyShowIn")) {
// 			char *option = get_option(line);
// 			if (option != NULL) {
// 				bool match = match_current_desktop2(option);
// 				free(option);
// 				if (!match) {
// 					goto cleanup_file;
// 				}
// 			}
// 		} else if (OPTION("NotShowIn")) {
// 			char *option = get_option(line);
// 			if (option != NULL) {
// 				bool match = match_current_desktop2(option);
// 				free(option);
// 				if (match) {
// 					goto cleanup_file;
// 				}
// 			}
// 		}
// 	}
// 	if (!found) {
// 		log_error("%s: No name found.\n", path);
// 		goto cleanup_name;
// 	}
// 	if (name == NULL) {
// 		log_error("%s: Malformed name key.\n", path);
// 		goto cleanup_file;
// 	}
// 
// 	desktop_vec_add(desktop, id, name, path);
// 
// cleanup_name:
// 	free(name);
// cleanup_file:
// 	free(line);
// 	fclose(file);
// }
