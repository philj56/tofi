#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "tofi.h"
#include "color.h"
#include "config.h"
#include "log.h"
#include "nelem.h"
#include "scale.h"
#include "unicode.h"
#include "xmalloc.h"

/* Maximum number of config file errors before we give up */
#define MAX_ERRORS 5

/* Maximum inclusion recursion depth before we give up */
#define MAX_RECURSION 32

/* Anyone with a 10M config file is doing something very wrong */
#define MAX_CONFIG_SIZE (10*1024*1024)

/* Convenience macros for anchor combinations */
#define ANCHOR_TOP_LEFT (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT \
		)
#define ANCHOR_TOP (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT \
		)
#define ANCHOR_TOP_RIGHT (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT \
		)
#define ANCHOR_RIGHT (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM \
		)
#define ANCHOR_BOTTOM_RIGHT (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT \
		)
#define ANCHOR_BOTTOM (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT \
		)
#define ANCHOR_BOTTOM_LEFT (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT \
		)
#define ANCHOR_LEFT (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM \
		)
#define ANCHOR_CENTER (\
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT \
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT \
		)

struct uint32_percent {
	uint32_t value;
	bool percent;
};

static char *strip(const char *str);
static bool parse_option(struct tofi *tofi, const char *filename, size_t lineno, const char *option, const char *value);
static char *get_config_path(void);
static uint32_t fixup_percentage(uint32_t value, uint32_t base, bool is_percent);

static uint32_t parse_anchor(const char *filename, size_t lineno, const char *str, bool *err);
static enum cursor_style parse_cursor_style(const char *filename, size_t lineno, const char *str, bool *err);
static enum matching_algorithm parse_matching_algorithm(const char *filename, size_t lineno, const char *str, bool *err);

static bool parse_bool(const char *filename, size_t lineno, const char *str, bool *err);
static uint32_t parse_char(const char *filename, size_t lineno, const char *str, bool *err);
static struct color parse_color(const char *filename, size_t lineno, const char *str, bool *err);
static uint32_t parse_uint32(const char *filename, size_t lineno, const char *str, bool *err);
static int32_t parse_int32(const char *filename, size_t lineno, const char *str, bool *err);
static struct uint32_percent parse_uint32_percent(const char *filename, size_t lineno, const char *str, bool *err);
static struct directional parse_directional(const char *filename, size_t lineno, const char *str, bool *err);

/*
 * Function-like macro. Yuck.
 */
#define PARSE_ERROR_NO_ARGS(filename, lineno, fmt) \
	if ((lineno) > 0) {\
		log_error("%s: line %zu: ", (filename), (lineno));\
		log_append_error((fmt)); \
	} else {\
		log_error((fmt)); \
	}

#define PARSE_ERROR(filename, lineno, fmt, ...) \
	if ((lineno) > 0) {\
		log_error("%s: line %zu: ", (filename), (lineno));\
		log_append_error((fmt), __VA_ARGS__); \
	} else {\
		log_error((fmt), __VA_ARGS__); \
	}

void config_load(struct tofi *tofi, const char *filename)
{
	char *default_filename = NULL;
	if (!filename) {
		default_filename = get_config_path();
		if (!default_filename) {
			return;
		}
		filename = default_filename;
	}
	/*
	 * Track and limit recursion depth, so we don't overflow the stack if
	 * a config file loop is created.
	 */
	static uint8_t recursion_depth = 0;
	recursion_depth++;
	if (recursion_depth > MAX_RECURSION) {
		log_error("Refusing to load %s, recursion too deep (>%u layers).\n", filename, MAX_RECURSION);
		recursion_depth--;
		return;
	}
	char *config;
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		if (!default_filename || errno != ENOENT) {
			log_error("Failed to open config file %s: %s\n", filename, strerror(errno));
		}
		goto CLEANUP_FILENAME;
	}
	if (fseek(fp, 0, SEEK_END)) {
		log_error("Failed to seek in config file: %s\n", strerror(errno));
		fclose(fp);
		goto CLEANUP_FILENAME;
	}
	size_t size;
	{
		long ssize = ftell(fp);
		if (ssize < 0) {
			log_error("Failed to determine config file size: %s\n", strerror(errno));
			fclose(fp);
			goto CLEANUP_FILENAME;
		}
		if (ssize > MAX_CONFIG_SIZE) {
			log_error("Config file too big (> %d MiB)! Are you sure it's a file?\n", MAX_CONFIG_SIZE / 1024 / 1024);
			fclose(fp);
			goto CLEANUP_FILENAME;
		}
		size = (size_t)ssize;
	}
	config = xmalloc(size + 1);
	if (!config) {
		log_error("Failed to malloc buffer for config file.\n");
		fclose(fp);
		goto CLEANUP_FILENAME;
	}
	rewind(fp);
	if (fread(config, 1, size, fp) != size) {
		log_error("Failed to read config file: %s\n", strerror(errno));
		fclose(fp);
		goto CLEANUP_CONFIG;
	}
	fclose(fp);
	config[size] = '\0';

	char *config_copy = xstrdup(config);
	if (!config_copy) {
		log_error("Failed to malloc second buffer for config file.\n");
		goto CLEANUP_ALL;
	}

	log_debug("Loading config file %s.\n", filename);

	char *saveptr1 = NULL;
	char *saveptr2 = NULL;

	char *copy_pos = config_copy;
	size_t lineno = 1;
	size_t num_errs = 0;
	for (char *str1 = config; ; str1 = NULL, saveptr2 = NULL) {
		if (num_errs > MAX_ERRORS) {
			log_error("Too many config file errors (>%u), giving up.\n", MAX_ERRORS);
			break;
		}
		char *line = strtok_r(str1, "\r\n", &saveptr1);
		if (!line) {
			/* We're done here */
			break;
		}
		while ((copy_pos - config_copy) < (line - config)) {
			if (*copy_pos == '\n') {
				lineno++;
			}
			copy_pos++;
		}
		{
			/* Grab first non-space character on the line. */
			char c = '\0';
			for (char *tmp = line; *tmp != '\0'; tmp++) {
				c = *tmp;
				if (!isspace(c)) {
					break;
				}
			}
			/*
			 * Comment characters.
			 * N.B. treating section headers as comments for now.
			 */
			switch (c) {
				case '#':
				case ';':
				case '[':
					continue;
			}
		}
		if (line[0] == '=') {
			PARSE_ERROR_NO_ARGS(filename, lineno, "Missing option.\n");
			num_errs++;
			continue;
		}
		char *option = strtok_r(line, "=", &saveptr2);
		if (!option) {
			char *tmp = strip(line);
			PARSE_ERROR(filename, lineno, "Config option \"%s\" missing value.\n", tmp);
			num_errs++;
			free(tmp);
			continue;
		}
		char *option_stripped = strip(option);
		if (!option_stripped) {
			PARSE_ERROR_NO_ARGS(filename, lineno, "Missing option.\n");
			num_errs++;
			continue;
		}
		char *value = strtok_r(NULL, "\r\n", &saveptr2);
		if (!value) {
			PARSE_ERROR(filename, lineno, "Config option \"%s\" missing value.\n", option_stripped);
			num_errs++;
			free(option_stripped);
			continue;
		}
		char *value_stripped = strip(value);
		if (!value_stripped) {
			PARSE_ERROR(filename, lineno, "Config option \"%s\" missing value.\n", option_stripped);
			num_errs++;
			free(option_stripped);
			continue;
		}
		if (!parse_option(tofi, filename, lineno, option_stripped, value_stripped)) {
			num_errs++;
		}

		/* Cleanup */
		free(value_stripped);
		free(option_stripped);
	}

CLEANUP_ALL:
	free(config_copy);
CLEANUP_CONFIG:
	free(config);
CLEANUP_FILENAME:
	if (default_filename) {
		free(default_filename);
	}
	recursion_depth--;
}

char *strip(const char *str)
{
	size_t start = 0;
	size_t end = strlen(str);
	while (start <= end && isspace(str[start])) {
		start++;
	}
	if (start == end) {
		return NULL;
	}
	while (end > start && (isspace(str[end]) || str[end] == '\0')) {
		end--;
	}
	if (end < start) {
		return NULL;
	}
	if (str[start] == '"' && str[end] == '"' && end > start) {
		start++;
		end--;
	}
	size_t len = end - start + 1;
	char *buf = xcalloc(len + 1, 1);
	strncpy(buf, str + start, len);
	buf[len] = '\0';
	return buf;
}

bool parse_option(struct tofi *tofi, const char *filename, size_t lineno, const char *option, const char *value)
{
	bool err = false;
	struct uint32_percent percent;
	if (strcasecmp(option, "include") == 0) {
		if (value[0] == '/') {
			config_load(tofi, value);
		} else {
			char *tmp = xstrdup(filename);
			char *dir = dirname(tmp);
			size_t len = strlen(dir) + 1 + strlen(value) + 1;
			char *config = xcalloc(len, 1);
			snprintf(config, len, "%s/%s", dir, value);
			config_load(tofi, config);
			free(config);
			free(tmp);
		}
	} else if (strcasecmp(option, "anchor") == 0) {
		uint32_t val = parse_anchor(filename, lineno, value, &err);
		if (!err) {
			tofi->anchor = val;
		}
	} else if (strcasecmp(option, "background-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.background_color = val;
		}
	} else if (strcasecmp(option, "corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.corner_radius = val;
		}
	} else if (strcasecmp(option, "output") == 0) {
		snprintf(tofi->target_output_name, N_ELEM(tofi->target_output_name), "%s", value);
	} else if (strcasecmp(option, "font") == 0) {
		if ((strlen(value) > 2) && (value[0] == '~') && (value[1] == '/')) {
			snprintf(tofi->window.entry.font_name, N_ELEM(tofi->window.entry.font_name), "%s%s", getenv("HOME"), &(value[1]));
		} else {
			snprintf(tofi->window.entry.font_name, N_ELEM(tofi->window.entry.font_name), "%s", value);
		}
	} else if (strcasecmp(option, "font-size") == 0) {
		uint32_t val =  parse_uint32(filename, lineno, value, &err);
		if (val == 0) {
			err = true;
			PARSE_ERROR(filename, lineno, "Option \"%s\" must be greater than 0.\n", option);
		} else {
			tofi->window.entry.font_size = val;
		}
	} else if (strcasecmp(option, "font-features") == 0) {
		snprintf(tofi->window.entry.font_features, N_ELEM(tofi->window.entry.font_features), "%s", value);
	} else if (strcasecmp(option, "font-variations") == 0) {
		snprintf(tofi->window.entry.font_variations, N_ELEM(tofi->window.entry.font_variations), "%s", value);
	} else if (strcasecmp(option, "num-results") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.num_results = val;
		}
	} else if (strcasecmp(option, "outline-width") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.outline_width = val;
		}
	} else if (strcasecmp(option, "outline-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.outline_color = val;
		}
	} else if (strcasecmp(option, "text-cursor") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.cursor_theme.show = val;
		}
	} else if (strcasecmp(option, "text-cursor-style") == 0) {
		enum cursor_style val = parse_cursor_style(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.cursor_theme.style = val;
		}
	} else if (strcasecmp(option, "text-cursor-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.cursor_theme.color = val;
			tofi->window.entry.cursor_theme.color_specified = true;
		}
	} else if (strcasecmp(option, "text-cursor-background") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.cursor_theme.text_color = val;
			tofi->window.entry.cursor_theme.text_color_specified = true;
		}
	} else if (strcasecmp(option, "text-cursor-corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.cursor_theme.corner_radius = val;
		}
	} else if (strcasecmp(option, "text-cursor-thickness") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.cursor_theme.thickness = val;
			tofi->window.entry.cursor_theme.thickness_specified = true;
		}
	} else if (strcasecmp(option, "prompt-text") == 0) {
		snprintf(tofi->window.entry.prompt_text, N_ELEM(tofi->window.entry.prompt_text), "%s", value);
	} else if (strcasecmp(option, "prompt-padding") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.prompt_padding = val;
		}
	} else if (strcasecmp(option, "prompt-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.prompt_theme.foreground_color = val;
			tofi->window.entry.prompt_theme.foreground_specified = true;
		}
	} else if (strcasecmp(option, "prompt-background") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.prompt_theme.background_color = val;
			tofi->window.entry.prompt_theme.background_specified = true;
		}
	} else if (strcasecmp(option, "prompt-background-padding") == 0) {
		struct directional val = parse_directional(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.prompt_theme.padding = val;
			tofi->window.entry.prompt_theme.padding_specified = true;
		}
	} else if (strcasecmp(option, "prompt-background-corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.prompt_theme.background_corner_radius = val;
			tofi->window.entry.prompt_theme.radius_specified = true;
		}
	} else if (strcasecmp(option, "placeholder-text") == 0) {
		snprintf(tofi->window.entry.placeholder_text, N_ELEM(tofi->window.entry.placeholder_text), "%s", value);
	} else if (strcasecmp(option, "placeholder-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.placeholder_theme.foreground_color = val;
			tofi->window.entry.placeholder_theme.foreground_specified = true;
		}
	} else if (strcasecmp(option, "placeholder-background") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.placeholder_theme.background_color = val;
			tofi->window.entry.placeholder_theme.background_specified = true;
		}
	} else if (strcasecmp(option, "placeholder-background-padding") == 0) {
		struct directional val = parse_directional(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.placeholder_theme.padding = val;
			tofi->window.entry.placeholder_theme.padding_specified = true;
		}
	} else if (strcasecmp(option, "placeholder-background-corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.placeholder_theme.background_corner_radius = val;
			tofi->window.entry.placeholder_theme.radius_specified = true;
		}
	} else if (strcasecmp(option, "input-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.input_theme.foreground_color = val;
			tofi->window.entry.input_theme.foreground_specified = true;
		}
	} else if (strcasecmp(option, "input-background") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.input_theme.background_color = val;
			tofi->window.entry.input_theme.background_specified = true;
		}
	} else if (strcasecmp(option, "input-background-padding") == 0) {
		struct directional val = parse_directional(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.input_theme.padding = val;
			tofi->window.entry.input_theme.padding_specified = true;
		}
	} else if (strcasecmp(option, "input-background-corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.input_theme.background_corner_radius = val;
			tofi->window.entry.input_theme.radius_specified = true;
		}
	} else if (strcasecmp(option, "default-result-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.default_result_theme.foreground_color = val;
			tofi->window.entry.default_result_theme.foreground_specified = true;
		}
	} else if (strcasecmp(option, "default-result-background") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.default_result_theme.background_color = val;
			tofi->window.entry.default_result_theme.background_specified = true;
		}
	} else if (strcasecmp(option, "default-result-background-padding") == 0) {
		struct directional val = parse_directional(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.default_result_theme.padding = val;
			tofi->window.entry.default_result_theme.padding_specified = true;
		}
	} else if (strcasecmp(option, "default-result-background-corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.default_result_theme.background_corner_radius = val;
			tofi->window.entry.default_result_theme.radius_specified = true;
		}
	} else if (strcasecmp(option, "alternate-result-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.alternate_result_theme.foreground_color = val;
			tofi->window.entry.alternate_result_theme.foreground_specified = true;
		}
	} else if (strcasecmp(option, "alternate-result-background") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.alternate_result_theme.background_color = val;
			tofi->window.entry.alternate_result_theme.background_specified = true;
		}
	} else if (strcasecmp(option, "alternate-result-background-padding") == 0) {
		struct directional val = parse_directional(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.alternate_result_theme.padding = val;
			tofi->window.entry.alternate_result_theme.padding_specified = true;
		}
	} else if (strcasecmp(option, "alternate-result-background-corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.alternate_result_theme.background_corner_radius = val;
			tofi->window.entry.alternate_result_theme.radius_specified = true;
		}
	} else if (strcasecmp(option, "min-input-width") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.input_width = val;
		}
	} else if (strcasecmp(option, "result-spacing") == 0) {
		int32_t val = parse_int32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.result_spacing = val;
		}
	} else if (strcasecmp(option, "border-width") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.border_width = val;
		}
	} else if (strcasecmp(option, "border-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.border_color = val;
		}
	} else if (strcasecmp(option, "text-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.foreground_color = val;
		}
	} else if (strcasecmp(option, "selection-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.selection_theme.foreground_color = val;
			tofi->window.entry.selection_theme.foreground_specified = true;
		}
	} else if (strcasecmp(option, "selection-match-color") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.selection_highlight_color = val;
		}
	} else if (strcasecmp(option, "selection-padding") == 0) {
		log_warning("The \"selection-padding\" option is deprecated, and will be removed in future. Please switch to \"selection-background-padding\".\n");
		int32_t val = parse_int32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.selection_theme.padding.left = val;
			tofi->window.entry.selection_theme.padding.right = val;
			tofi->window.entry.selection_theme.padding_specified = true;
		}
	} else if (strcasecmp(option, "selection-background") == 0) {
		struct color val = parse_color(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.selection_theme.background_color = val;
			tofi->window.entry.selection_theme.background_specified = true;
		}
	} else if (strcasecmp(option, "selection-background-padding") == 0) {
		struct directional val = parse_directional(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.selection_theme.padding = val;
			tofi->window.entry.selection_theme.padding_specified = true;
		}
	} else if (strcasecmp(option, "selection-background-corner-radius") == 0) {
		uint32_t val = parse_uint32(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.selection_theme.background_corner_radius = val;
			tofi->window.entry.selection_theme.radius_specified = true;
		}
	} else if (strcasecmp(option, "exclusive-zone") == 0) {
		if (strcmp(value, "-1") == 0) {
			tofi->window.exclusive_zone = -1;
		} else {
			percent = parse_uint32_percent(filename, lineno, value, &err);
			if (!err) {
				tofi->window.exclusive_zone = percent.value;
				tofi->window.exclusive_zone_is_percent = percent.percent;
			}
		}
	} else if (strcasecmp(option, "width") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.width = percent.value;
			tofi->window.width_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "height") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.height = percent.value;
			tofi->window.height_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "margin-top") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.margin_top = percent.value;
			tofi->window.margin_top_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "margin-bottom") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.margin_bottom = percent.value;
			tofi->window.margin_bottom_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "margin-left") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.margin_left = percent.value;
			tofi->window.margin_left_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "margin-right") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.margin_right = percent.value;
			tofi->window.margin_right_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "padding-top") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.padding_top = percent.value;
			tofi->window.entry.padding_top_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "padding-bottom") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.padding_bottom = percent.value;
			tofi->window.entry.padding_bottom_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "padding-left") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.padding_left = percent.value;
			tofi->window.entry.padding_left_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "padding-right") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.padding_right = percent.value;
			tofi->window.entry.padding_right_is_percent = percent.percent;
		}
	} else if (strcasecmp(option, "clip-to-padding") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.clip_to_padding = val;
		}
	} else if (strcasecmp(option, "horizontal") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.horizontal = val;
		}
	} else if (strcasecmp(option, "hide-cursor") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->hide_cursor = val;
		}
	} else if (strcasecmp(option, "history") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->use_history = val;
		}
	} else if (strcasecmp(option, "history-file") == 0) {
		snprintf(tofi->history_file, N_ELEM(tofi->history_file), "%s", value);
	} else if (strcasecmp(option, "matching-algorithm") == 0) {
		enum matching_algorithm val = parse_matching_algorithm(filename, lineno, value, &err);
		if (!err) {
			tofi->matching_algorithm= val;
		}
	} else if (strcasecmp(option, "fuzzy-match") == 0) {
		log_warning("The \"fuzzy-match\" option is deprecated, and may be removed in future. Please switch to \"matching-algorithm\".\n");
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			if (val) {
				tofi->matching_algorithm = MATCHING_ALGORITHM_FUZZY;
			}
		}
	} else if (strcasecmp(option, "require-match") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->require_match = val;
		}
	} else if (strcasecmp(option, "auto-accept-single") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->auto_accept_single = val;
		}
	} else if (strcasecmp(option, "print-index") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->print_index = val;
		}
	} else if (strcasecmp(option, "hide-input") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.hide_input = val;
		}
	} else if (strcasecmp(option, "hidden-character") == 0) {
		uint32_t ch = parse_char(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.hidden_character_utf8_length = 
				utf32_to_utf8(ch, tofi->window.entry.hidden_character_utf8);
		}
	} else if (strcasecmp(option, "physical-keybindings") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->physical_keybindings = val;
		}
	} else if (strcasecmp(option, "drun-launch") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->drun_launch = val;
		}
	} else if (strcasecmp(option, "drun-print-exec") == 0) {
		log_warning("drun-print-exec is deprecated, as it is now always true.\n"
				"           This option may be removed in a future version of tofi.\n");
	} else if (strcasecmp(option, "terminal") == 0) {
		snprintf(tofi->default_terminal, N_ELEM(tofi->default_terminal), "%s", value);
	} else if (strcasecmp(option, "hint-font") == 0) {
		bool val = !parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->window.entry.harfbuzz.disable_hinting = val;
		}
	} else if (strcasecmp(option, "multi-instance") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->multiple_instance = val;
		}
	} else if (strcasecmp(option, "ascii-input") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->ascii_input = val;
		}
	} else if (strcasecmp(option, "late-keyboard-init") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->late_keyboard_init = val;
		}
	} else if (strcasecmp(option, "output") == 0) {
		snprintf(tofi->target_output_name, N_ELEM(tofi->target_output_name), "%s", value);
	} else if (strcasecmp(option, "scale") == 0) {
		bool val = parse_bool(filename, lineno, value, &err);
		if (!err) {
			tofi->use_scale = val;
		}
	} else {
		PARSE_ERROR(filename, lineno, "Unknown option \"%s\"\n", option);
		err = true;
	}
	return !err;
}

bool config_apply(struct tofi *tofi, const char *option, const char *value)
{
	return parse_option(tofi, "", 0, option, value);
}

uint32_t fixup_percentage(uint32_t value, uint32_t base, bool is_percent)
{
	if (is_percent) {
		return value * base / 100;
	}
	return value;
}

void config_fixup_values(struct tofi *tofi)
{
	uint32_t base_width = tofi->output_width;
	uint32_t base_height = tofi->output_height;
	uint32_t scale;
	if (tofi->window.fractional_scale != 0) {
		scale = tofi->window.fractional_scale;
	} else {
		scale = tofi->window.scale * 120;
	}

	/*
	 * If we're going to be scaling these values in Cairo,
	 * we need to apply the inverse scale here.
	 */
	if (tofi->use_scale) {
		base_width = scale_apply_inverse(base_width, scale);
		base_height = scale_apply_inverse(base_height, scale);
	}

	tofi->window.margin_top = fixup_percentage(
			tofi->window.margin_top,
			base_height,
			tofi->window.margin_top_is_percent);
	tofi->window.margin_bottom = fixup_percentage(
			tofi->window.margin_bottom,
			base_height,
			tofi->window.margin_bottom_is_percent);
	tofi->window.margin_left = fixup_percentage(
			tofi->window.margin_left,
			base_width,
			tofi->window.margin_left_is_percent);
	tofi->window.margin_right = fixup_percentage(
			tofi->window.margin_right,
			base_width,
			tofi->window.margin_right_is_percent);
	tofi->window.entry.padding_top = fixup_percentage(
			tofi->window.entry.padding_top,
			base_height,
			tofi->window.entry.padding_top_is_percent);
	tofi->window.entry.padding_bottom = fixup_percentage(
			tofi->window.entry.padding_bottom,
			base_height,
			tofi->window.entry.padding_bottom_is_percent);
	tofi->window.entry.padding_left = fixup_percentage(
			tofi->window.entry.padding_left,
			base_width,
			tofi->window.entry.padding_left_is_percent);
	tofi->window.entry.padding_right = fixup_percentage(
			tofi->window.entry.padding_right,
			base_width,
			tofi->window.entry.padding_right_is_percent);

	/*
	 * Window width and height are a little special. We're only going to be
	 * using them to specify sizes to Wayland, which always wants scaled
	 * pixels, so always scale them here (unless we've directly specified a
	 * scaled size).
	 */
	tofi->window.width = fixup_percentage(
			tofi->window.width,
			tofi->output_width,
			tofi->window.width_is_percent);
	tofi->window.height = fixup_percentage(
			tofi->window.height,
			tofi->output_height,
			tofi->window.height_is_percent);
	if (tofi->window.width_is_percent || !tofi->use_scale) {
		tofi->window.width = scale_apply_inverse(tofi->window.width, scale);
	}
	if (tofi->window.height_is_percent || !tofi->use_scale) {
		tofi->window.height = scale_apply_inverse(tofi->window.height, scale);
	}

	/* Don't attempt percentage handling if exclusive_zone is set to -1. */
	if (tofi->window.exclusive_zone > 0) {
		/* Exclusive zone base depends on anchor. */
		switch (tofi->anchor) {
			case ANCHOR_TOP:
			case ANCHOR_BOTTOM:
				tofi->window.exclusive_zone = fixup_percentage(
						tofi->window.exclusive_zone,
						base_height,
						tofi->window.exclusive_zone_is_percent);
				break;
			case ANCHOR_LEFT:
			case ANCHOR_RIGHT:
				tofi->window.exclusive_zone = fixup_percentage(
						tofi->window.exclusive_zone,
						base_width,
						tofi->window.exclusive_zone_is_percent);
				break;
			default:
				/*
				 * Exclusive zone >0 is meaningless for other
				 * anchor positions.
				 */
				tofi->window.exclusive_zone =
					MIN(tofi->window.exclusive_zone, 0);
				break;
		}
	}
}

char *get_config_path()
{
	char *base_dir = getenv("XDG_CONFIG_HOME");
	char *ext = "";
	size_t len = strlen("/tofi/config") + 1;
	if (!base_dir) {
		base_dir = getenv("HOME");
		ext = "/.config";
		if (!base_dir) {
			log_error("Couldn't find XDG_CONFIG_HOME or HOME envvars\n");
			return NULL;
		}
	}
	len += strlen(base_dir) + strlen(ext) + 2;
	char *name = xcalloc(len, sizeof(*name));
	snprintf(name, len, "%s%s%s", base_dir, ext, "/tofi/config");
	return name;
}

bool parse_bool(const char *filename, size_t lineno, const char *str, bool *err)
{
	if (strcasecmp(str, "true") == 0) {
		return true;
	} else if (strcasecmp(str, "false") == 0) {
		return false;
	}
	PARSE_ERROR(filename, lineno, "Invalid boolean value \"%s\".\n", str);
	if (err) {
		*err = true;
	}
	return false;
}

uint32_t parse_char(const char *filename, size_t lineno, const char *str, bool *err)
{
	uint32_t ch = U'\0';
	if (*str == '\0') {
		return ch;
	}
	if (!utf8_validate(str)) {
		PARSE_ERROR(filename, lineno, "Invalid UTF-8 string \"%s\".\n", str);
		if (err) {
			*err = true;
		}
		return ch;
	}
	char *tmp = utf8_compose(str);
	ch = utf8_to_utf32_validate(tmp);
	if (ch == (uint32_t)-2 || ch == (uint32_t)-1 || *utf8_next_char(tmp) != '\0') {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as a character.\n", str);
		if (err) {
			*err = true;
		}
	}
	free(tmp);

	return ch;
}

uint32_t parse_anchor(const char *filename, size_t lineno, const char *str, bool *err)
{
	if(strcasecmp(str, "top-left") == 0) {
		return ANCHOR_TOP_LEFT;
	}
	if (strcasecmp(str, "top") == 0) {
		return ANCHOR_TOP;
	}
	if (strcasecmp(str, "top-right") == 0) {
		return ANCHOR_TOP_RIGHT;
	}
	if (strcasecmp(str, "right") == 0) {
		return ANCHOR_RIGHT;
	}
	if (strcasecmp(str, "bottom-right") == 0) {
		return ANCHOR_BOTTOM_RIGHT;
	}
	if (strcasecmp(str, "bottom") == 0) {
		return ANCHOR_BOTTOM;
	}
	if (strcasecmp(str, "bottom-left") == 0) {
		return ANCHOR_BOTTOM_LEFT;
	}
	if (strcasecmp(str, "left") == 0) {
		return ANCHOR_LEFT;
	}
	if (strcasecmp(str, "center") == 0) {
		return ANCHOR_CENTER;
	}
	PARSE_ERROR(filename, lineno, "Invalid anchor \"%s\".\n", str);
	if (err) {
		*err = true;
	}
	return 0;
}

enum cursor_style parse_cursor_style(const char *filename, size_t lineno, const char *str, bool *err)
{
	if(strcasecmp(str, "bar") == 0) {
		return CURSOR_STYLE_BAR;
	}
	if(strcasecmp(str, "block") == 0) {
		return CURSOR_STYLE_BLOCK;
	}
	if(strcasecmp(str, "underscore") == 0) {
		return CURSOR_STYLE_UNDERSCORE;
	}
	PARSE_ERROR(filename, lineno, "Invalid cursor style \"%s\".\n", str);
	if (err) {
		*err = true;
	}
	return 0;
}

enum matching_algorithm parse_matching_algorithm(const char *filename, size_t lineno, const char *str, bool *err)
{
	if(strcasecmp(str, "normal") == 0) {
		return MATCHING_ALGORITHM_NORMAL;
	}
	if(strcasecmp(str, "fuzzy") == 0) {
		return MATCHING_ALGORITHM_FUZZY;
	}
	if(strcasecmp(str, "prefix") == 0) {
		return MATCHING_ALGORITHM_PREFIX;
	}
	PARSE_ERROR(filename, lineno, "Invalid matching algorithm \"%s\".\n", str);
	if (err) {
		*err = true;
	}
	return 0;
}

struct color parse_color(const char *filename, size_t lineno, const char *str, bool *err)
{
	struct color color = hex_to_color(str);
	if (color.r == -1) {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as a color.\n", str);
		if (err) {
			*err = true;
		}
	}
	return color;
}

uint32_t parse_uint32(const char *filename, size_t lineno, const char *str, bool *err)
{
	errno = 0;
	char *endptr;
	int64_t ret = strtoull(str, &endptr, 0);
	if (endptr == str || *endptr != '\0') {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as unsigned int.\n", str);
		if (err) {
			*err = true;
		}
	} else if (errno || ret < 0 || ret > UINT32_MAX) {
		PARSE_ERROR(filename, lineno, "Unsigned int value \"%s\" out of range.\n", str);
		if (err) {
			*err = true;
		}
	}
	return ret;
}

int32_t parse_int32(const char *filename, size_t lineno, const char *str, bool *err)
{
	errno = 0;
	char *endptr;
	int64_t ret = strtoll(str, &endptr, 0);
	if (endptr == str || *endptr != '\0') {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as int.\n", str);
		if (err) {
			*err = true;
		}
	} else if (errno || ret < INT32_MIN || ret > INT32_MAX) {
		PARSE_ERROR(filename, lineno, "Int value \"%s\" out of range.\n", str);
		if (err) {
			*err = true;
		}
	}
	return ret;
}

struct uint32_percent parse_uint32_percent(const char *filename, size_t lineno, const char *str, bool *err)
{
	errno = 0;
	char *endptr;
	int64_t val = strtoull(str, &endptr, 0);
	bool percent = false;
	if (endptr == str || (*endptr != '\0' && *endptr != '%')) {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as unsigned int.\n", str);
		if (err) {
			*err = true;
		}
	} else if (errno || val < 0 || val > UINT32_MAX) {
		PARSE_ERROR(filename, lineno, "Unsigned int value \"%s\" out of range.\n", str);
		if (err) {
			*err = true;
		}
	}
	if (!err || !*err) {
		if (*endptr == '%') {
			percent = true;
		}
	}
	return (struct uint32_percent){ val, percent };
}

struct directional parse_directional(const char *filename, size_t lineno, const char *str, bool *err)
{
	/* One extra value to act as a sentinel for too many values in str. */
	int32_t values[5];
	char *saveptr = NULL;
	char *tmp = xstrdup(str);
	char *val = strtok_r(tmp, ",", &saveptr);

	size_t n;

	for (n = 0; n < N_ELEM(values) && val != NULL; n++) {
		values[n] = parse_int32(filename, lineno, val, err);
		if (err && *err) {
			break;
		}
		val = strtok_r(NULL, ",", &saveptr);
	}
	free(tmp);

	struct directional ret = {0};
	if (err && *err) {
		return ret;
	}

	switch (n) {
		case 0:
			break;
		case 1:
			ret = (struct directional) {
				.top = values[0],
				.right = values[0],
				.bottom = values[0],
				.left = values[0],
			};
			break;
		case 2:
			ret = (struct directional) {
				.top = values[0],
				.right = values[1],
				.bottom = values[0],
				.left = values[1],
			};
			break;
		case 3:
			ret = (struct directional) {
				.top = values[0],
				.right = values[1],
				.bottom = values[2],
				.left = values[1],
			};
			break;
		case 4:
			ret = (struct directional) {
				.top = values[0],
				.right = values[1],
				.bottom = values[2],
				.left = values[3],
			};
			break;
		default:
			PARSE_ERROR(filename, lineno, "Too many values in \"%s\" for directional.\n", str);
			if (err) {
				*err = true;
			}
			break;
	};

	return ret;
}
