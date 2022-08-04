#include <gio/gio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "string_vec.h"
#include "xmalloc.h"

const char *legacy_icon_dir = ".icons/";
const char *pixmap_dir = "/usr/share/pixmaps";

[[nodiscard("memory leaked")]]
static struct string_vec get_icon_paths() {
	char *base_paths = NULL;
	const char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
	if (xdg_data_dirs == NULL) {
		xdg_data_dirs = "/usr/local/share/:/usr/share/";
	}
	const char *home = getenv("HOME");
	if (home == NULL) {
		log_error("Couldn't retrieve HOME from environment.\n");
		exit(EXIT_FAILURE);
	}

	{
		size_t len = strlen(home) + 1
			+ strlen(legacy_icon_dir) + 1
			+ strlen(xdg_data_dirs) + 1
			+ strlen(pixmap_dir) + 1;
		base_paths = xmalloc(len);
		snprintf(
				base_paths,
				len,
				"%s/%s:%s:%s",
				home,
				legacy_icon_dir,
				xdg_data_dirs,
				pixmap_dir);
	}


	/* Append /icons/hicolor/ to each entry. */
	struct string_vec paths = string_vec_create();
	char *saveptr = NULL;
	char *path_entry = strtok_r(base_paths, ":", &saveptr);
	while (path_entry != NULL) {
		const char *subdir = "icons/hicolor/";
		size_t len = strlen(path_entry) + 1 + strlen(subdir) + 1;
		char *dirs = xmalloc(len);
		snprintf(dirs, len, "%s/%s", path_entry, subdir);
		string_vec_add(&paths, dirs);
		free(dirs);
		path_entry = strtok_r(NULL, ":", &saveptr);
	}
	free(base_paths);

	return paths;
}

/* This isn't standards-compliant, but we need speed. */
char *find_icon(const char *name, uint32_t size, uint32_t scale)
{
	char *ret = NULL;
	struct string_vec theme_paths = get_icon_paths();
	for (size_t i = 0; i < theme_paths.count; i++) {
		const char *entry = theme_paths.buf[i].string;
		size_t index_len = strlen(entry) + 1 + strlen("index.theme") + 1;
		char *index = xmalloc(index_len);
		snprintf(index, index_len, "%s/%s", entry, "index.theme");
		GKeyFile *file = g_key_file_new();
		if (!g_key_file_load_from_file(file, index, G_KEY_FILE_NONE, NULL)) {
			g_key_file_unref(file);
			free(index);
			continue;
		}
		log_debug("Looking for icons in %s.\n", index);

		char *subdirs_string = g_key_file_get_string(
				file,
				"Icon Theme",
				"Directories",
				NULL);
		if (subdirs_string == NULL) {
			log_error("No 'Directories' entry in %s.\n", index);
			g_key_file_unref(file);
			free(index);
			continue;
		}

		struct string_vec subdirs = string_vec_create();
		char *saveptr = NULL;
		char *subdir = strtok_r(subdirs_string, ",", &saveptr);
		while (subdir != NULL) {
			if (strstr(subdir, "apps") != NULL) {
				string_vec_add(&subdirs, subdir);
			}
			subdir = strtok_r(NULL, ",", &saveptr);
		}
		free(subdirs_string);
		for (size_t i = 0; i < subdirs.count; i++) {
			printf("%s\n", subdirs.buf[i].string);
		}


		g_key_file_unref(file);
		free(index);
	}
	string_vec_destroy(&theme_paths);

	return ret;
}
