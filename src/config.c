#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "tofi.h"
#include "color.h"
#include "config.h"
#include "log.h"
#include "nelem.h"
#include "xmalloc.h"

/* Maximum number of config file errors before we give up */
#define MAX_ERRORS 5

/* Anyone with a 10M config file is doing something very wrong */
#define MAX_CONFIG_SIZE (10*1024*1024)

struct uint32_percent {
	uint32_t value;
	bool percent;
};

static char *strip(const char *str);
static bool parse_option(struct tofi *tofi, const char *filename, size_t lineno, const char *option, const char *value);
static char *get_config_path(void);
static uint32_t fixup_percentage(uint32_t value, uint32_t base, bool is_percent, uint32_t scale, bool use_scale);

static uint32_t parse_anchor(const char *filename, size_t lineno, const char *str, bool *err);
static bool parse_bool(const char *filename, size_t lineno, const char *str, bool *err);
static struct color parse_color(const char *filename, size_t lineno, const char *str, bool *err);
static uint32_t parse_uint32(const char *filename, size_t lineno, const char *str, bool *err);
static int32_t parse_int32(const char *filename, size_t lineno, const char *str, bool *err);
static struct uint32_percent parse_uint32_percent(const char *filename, size_t lineno, const char *str, bool *err);

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
			char *line_stripped = strip(line);
			if (!line_stripped) {
				/* Skip blank lines */
				continue;
			}
			char first_char = line_stripped[0];
			free(line_stripped);
			/*
			 * Comment characters.
			 * N.B. treating section headers as comments for now.
			 */
			switch (first_char) {
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
	if (strcasecmp(option, "anchor") == 0) {
		tofi->anchor = parse_anchor(filename, lineno, value, &err);
	} else if (strcasecmp(option, "background-color") == 0) {
		tofi->window.entry.background_color = parse_color(filename, lineno, value, &err);
	} else if (strcasecmp(option, "corner-radius") == 0) {
		tofi->window.entry.corner_radius = parse_uint32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "output") == 0) {
		snprintf(tofi->target_output_name, N_ELEM(tofi->target_output_name), "%s", value);
	} else if (strcasecmp(option, "font") == 0) {
		snprintf(tofi->window.entry.font_name, N_ELEM(tofi->window.entry.font_name), "%s", value);
	} else if (strcasecmp(option, "font-size") == 0) {
		tofi->window.entry.font_size = parse_uint32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "num-results") == 0) {
		tofi->window.entry.num_results = parse_uint32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "outline-width") == 0) {
		tofi->window.entry.outline_width = parse_uint32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "outline-color") == 0) {
		tofi->window.entry.outline_color = parse_color(filename, lineno, value, &err);
	} else if (strcasecmp(option, "prompt-text") == 0) {
		snprintf(tofi->window.entry.prompt_text, N_ELEM(tofi->window.entry.prompt_text), "%s", value);
	} else if (strcasecmp(option, "min-input-width") == 0) {
		tofi->window.entry.input_width = parse_uint32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "result-spacing") == 0) {
		tofi->window.entry.result_spacing = parse_int32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "border-width") == 0) {
		tofi->window.entry.border_width = parse_uint32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "border-color") == 0) {
		tofi->window.entry.border_color = parse_color(filename, lineno, value, &err);
	} else if (strcasecmp(option, "text-color") == 0) {
		tofi->window.entry.foreground_color = parse_color(filename, lineno, value, &err);
	} else if (strcasecmp(option, "selection-color") == 0) {
		tofi->window.entry.selection_foreground_color = parse_color(filename, lineno, value, &err);
	} else if (strcasecmp(option, "selection-match-color") == 0) {
		tofi->window.entry.selection_highlight_color = parse_color(filename, lineno, value, &err);
	} else if (strcasecmp(option, "selection-padding") == 0) {
		tofi->window.entry.selection_background_padding = parse_int32(filename, lineno, value, &err);
	} else if (strcasecmp(option, "selection-background") == 0) {
		tofi->window.entry.selection_background_color = parse_color(filename, lineno, value, &err);
	} else if (strcasecmp(option, "width") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.width = percent.value;
		tofi->window.width_is_percent = percent.percent;
	} else if (strcasecmp(option, "height") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.height = percent.value;
		tofi->window.height_is_percent = percent.percent;
	} else if (strcasecmp(option, "margin-top") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.margin_top = percent.value;
		tofi->window.margin_top_is_percent = percent.percent;
	} else if (strcasecmp(option, "margin-bottom") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.margin_bottom = percent.value;
		tofi->window.margin_bottom_is_percent = percent.percent;
	} else if (strcasecmp(option, "margin-left") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.margin_left = percent.value;
		tofi->window.margin_left_is_percent = percent.percent;
	} else if (strcasecmp(option, "margin-right") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.margin_right = percent.value;
		tofi->window.margin_right_is_percent = percent.percent;
	} else if (strcasecmp(option, "padding-top") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.entry.padding_top = percent.value;
		tofi->window.entry.padding_top_is_percent = percent.percent;
	} else if (strcasecmp(option, "padding-bottom") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.entry.padding_bottom = percent.value;
		tofi->window.entry.padding_bottom_is_percent = percent.percent;
	} else if (strcasecmp(option, "padding-left") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.entry.padding_left = percent.value;
		tofi->window.entry.padding_left_is_percent = percent.percent;
	} else if (strcasecmp(option, "padding-right") == 0) {
		percent = parse_uint32_percent(filename, lineno, value, &err);
		tofi->window.entry.padding_right = percent.value;
		tofi->window.entry.padding_right_is_percent = percent.percent;
	} else if (strcasecmp(option, "horizontal") == 0) {
		tofi->window.entry.horizontal = parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "hide-cursor") == 0) {
		tofi->hide_cursor = parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "history") == 0) {
		tofi->use_history = parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "fuzzy-match") == 0) {
		tofi->fuzzy_match = parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "require-match") == 0) {
		tofi->require_match = parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "drun-launch") == 0) {
		tofi->drun_launch = parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "drun-print-exec") == 0) {
		log_warning("drun-print-exec is deprecated, as it is now always true.\n"
				"           This option may be removed in a future version of tofi.\n");
	} else if (strcasecmp(option, "hint-font") == 0) {
		tofi->window.entry.harfbuzz.disable_hinting = !parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "late-keyboard-init") == 0) {
		tofi->late_keyboard_init = parse_bool(filename, lineno, value, &err);
	} else if (strcasecmp(option, "output") == 0) {
		snprintf(tofi->target_output_name, N_ELEM(tofi->target_output_name), "%s", value);
	} else if (strcasecmp(option, "scale") == 0) {
		tofi->use_scale = parse_bool(filename, lineno, value, &err);
	} else {
		PARSE_ERROR(filename, lineno, "Unknown option \"%s\"\n", option);
		err = true;
	}
	return !err;
}

void config_apply(struct tofi *tofi, const char *option, const char *value)
{
	if (!parse_option(tofi, "", 0, option, value)) {
		exit(EXIT_FAILURE);
	};
}

uint32_t fixup_percentage(uint32_t value, uint32_t base, bool is_percent, uint32_t scale, bool use_scale)
{
	if (is_percent) {
		return value * base / 100;
	} else if (use_scale) {
		return value * scale;
	}
	return value;
}

void config_fixup_values(struct tofi *tofi)
{
	uint32_t scale = tofi->window.scale;

	/*
	 * Scale fonts to the correct size.
	 *
	 * TODO: In the next release (0.6.0), this should be moved within
	 * the use_scale conditional.
	 */
	tofi->window.entry.font_size *= scale;

	if (tofi->use_scale) {
		struct entry *entry = &tofi->window.entry;

		entry->corner_radius *= scale;
		entry->selection_background_padding *= scale;
		entry->result_spacing *= scale;
		entry->input_width *= scale;
		entry->outline_width *= scale;
		entry->border_width *= scale;
	}

	/* These values should only be scaled if they're not percentages. */
	tofi->window.width = fixup_percentage(
			tofi->window.width,
			tofi->output_width,
			tofi->window.width_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.height = fixup_percentage(
			tofi->window.height,
			tofi->output_height,
			tofi->window.height_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.margin_top = fixup_percentage(
			tofi->window.margin_top,
			tofi->output_height,
			tofi->window.margin_top_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.margin_bottom = fixup_percentage(
			tofi->window.margin_bottom,
			tofi->output_height,
			tofi->window.margin_bottom_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.margin_left = fixup_percentage(
			tofi->window.margin_left,
			tofi->output_width,
			tofi->window.margin_left_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.margin_right = fixup_percentage(
			tofi->window.margin_right,
			tofi->output_width,
			tofi->window.margin_right_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.entry.padding_top = fixup_percentage(
			tofi->window.entry.padding_top,
			tofi->output_height,
			tofi->window.entry.padding_top_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.entry.padding_bottom = fixup_percentage(
			tofi->window.entry.padding_bottom,
			tofi->output_height,
			tofi->window.entry.padding_bottom_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.entry.padding_left = fixup_percentage(
			tofi->window.entry.padding_left,
			tofi->output_width,
			tofi->window.entry.padding_left_is_percent,
			tofi->window.scale,
			tofi->use_scale);
	tofi->window.entry.padding_right = fixup_percentage(
			tofi->window.entry.padding_right,
			tofi->output_width,
			tofi->window.entry.padding_right_is_percent,
			tofi->window.scale,
			tofi->use_scale);
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

uint32_t parse_anchor(const char *filename, size_t lineno, const char *str, bool *err)
{
	if(strcasecmp(str, "top-left") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	}
	if (strcasecmp(str, "top") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	}
	if (strcasecmp(str, "top-right") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	}
	if (strcasecmp(str, "right") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	}
	if (strcasecmp(str, "bottom-right") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	}
	if (strcasecmp(str, "bottom") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	}
	if (strcasecmp(str, "bottom-left") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
	}
	if (strcasecmp(str, "left") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	}
	if (strcasecmp(str, "center") == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	}
	PARSE_ERROR(filename, lineno, "Invalid anchor \"%s\".\n", str);
	*err = true;
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
	int32_t ret = strtoul(str, &endptr, 0);
	if (endptr == str) {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as unsigned int.\n", str);
		if (err) {
			*err = true;
		}
	} else if (errno || ret < 0) {
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
	int32_t ret = strtol(str, &endptr, 0);
	if (endptr == str) {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as int.\n", str);
		if (err) {
			*err = true;
		}
	} else if (errno) {
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
	int32_t val = strtoul(str, &endptr, 0);
	bool percent = false;
	if (endptr == str) {
		PARSE_ERROR(filename, lineno, "Failed to parse \"%s\" as unsigned int.\n", str);
		if (err) {
			*err = true;
		}
	} else if (errno || val < 0) {
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
