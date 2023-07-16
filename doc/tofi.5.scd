tofi(5)

# NAME

tofi - configuration file

# DESCRIPTION

The config file format is basic .ini/.cfg style. Options are set one per line,
with the syntax:

	option = value

Whitespace is ignored. Values starting or ending with whitespace can be given
by enclosing them in double quotes like so:

	option = " value "

Lines beginning with # or ; are treated as comments. Section headers of the
form [header] are currently ignored. All options and values are
case-insensitive, except where not possible (e.g. paths). Later options
override earlier options, and command line options override config file
options.

# SPECIAL OPTIONS

*include*=_path_
	Include the contents of another config file. If _path_ is a relative
	path, it is interpreted as relative to this config file's path (or the
	current directory if *--include* is passed on the command line).
	Inclusion happens immediately, before the rest of the current file's
	contents are parsed.

# BEHAVIOUR OPTIONS

*hide-cursor*=_true|false_
	Hide the mouse cursor.

	Default: false

*text-cursor*=_true|false_
	Show a text cursor in the input field.

	Default: false

*history*=_true|false_
	Sort results by number of usages. By default, this is only effective in
	the run and drun modes - see the *history-file* option for more
	information.

	Default: true

*history-file*=_path_
	Specify an alternate file to read and store history information from /
	to. This shouldn't normally be needed, and is intended to facilitate
	the creation of custom modes. The default value depends on the current
	mode.

	Defaults:
		- tofi:      None (no history file)
		- tofi-run:  _$XDG_STATE_HOME/tofi-history_
		- tofi-drun: _$XDG_STATE_HOME/tofi-drun-history_

*matching-algorithm*=_normal|prefix|fuzzy_
	Select the matching algorithm used.
	If _normal_, substring matching is used, weighted to favour matches
	closer to the beginning of the string.
	If _prefix_, only substrings at the beginning of the string are matched.
	If _fuzzy_, searching is performed via a simple fuzzy matching
	algorithm.

	Default: normal

*fuzzy-match*=_true|false_
	*WARNING*: This option is deprecated, and may be removed in a future
	version of tofi. You should use the *matching-algorithm*
	option instead.

	If true, searching is performed via a simple fuzzy matching algorithm.
	If false, substring matching is used, weighted to favour matches closer
	to the beginning of the string.

	Default: false

*require-match*=_true|false_
	If true, require a match to allow a selection to be made. If false,
	making a selection with no matches will print input to stdout.
	In drun mode, this is always true.

	Default: true

*auto-accept-single*=_true|false_
	If true, automatically accept a result if it is the only one remaining.
	If there's only one result on startup, window creation is skipped
	altogether.

	Default: false

*hide-input*=_true|false_
	If true, typed input will be hidden, and what is displayed (if
	anything) is determined by the *hidden-character* option.

	Default: false

*hidden-character*=_char_
	Replace displayed input characters with _char_. If _char_ is set to the
	empty string, input will be completely hidden.
	This option only has an effect when *hide-input* is set to true.

	Default: \*

*physical-keybindings*=_true|false_
	If true, use physical keys for shortcuts, regardless of the current
	keyboard layout. If false, use the current layout's keys.

	Default: true

*print-index*=_true|false_
	Instead of printing the selected entry, print the 1-based index of the
	selection. This option has no effect in run or drun mode. If
	*require-match* is set to false, non-matching input will still result in
	the input being printed.

	Default: false

*drun-launch*=_true|false_
	If true, directly launch applications on selection when in drun mode.
	Otherwise, just print the Exec line of the .desktop file to stdout.

	Default: false

*terminal*=_command_
	The terminal to run terminal programs in when in drun mode. _command_
	will be prepended to the the application's command line.
	This option has no effect if *drun-launch* is set to true.

	Default: the value of the TERMINAL environment variable

*drun-print-exec*=_true|false_
	*WARNING*: This option does nothing, and may be removed in a future
	version of tofi.

	Default: true

*late-keyboard-init*=_true|false_
	Delay keyboard initialisation until after the first draw to screen.
	This option is experimental, and will cause tofi to miss keypresses
	for a short time after launch. The only reason to use this option is
	performance on slow systems.

	Default: false

*multi-instance*=_true|false_
	If true, allow multiple simultaneous processes.
	If false, create a lock file on startup to prevent multiple instances
	from running simultaneously.

	Default: false

*ascii-input*=_true|false_
	Assume input is plain ASCII, and disable some Unicode handling
	functions. This is faster, but means e.g. a search for "e" will not
	match "é".

	Default: false

# STYLE OPTIONS

*font*=_font_
	Font to use. If _font_ is a path to a font file, *tofi* will not have
	to use Pango or Fontconfig. This greatly speeds up startup, but any
	characters not in the chosen font will fail to render.

	If a path is not given, _font_ is interpreted as a font name in Pango
	format.

	Default: "Sans"

*font-size*=_pt_
	Point size of text.

	Default: 24

*font-features*=_features_
	Comma separated list of OpenType font feature settings to apply. The
	format is similar to the CSS "font-feature-settings" property.
	For example, "smcp, c2sc" will turn all text into small caps (if
	supported by the chosen font).

	Default: ""

*font-variations*=_variations_
	Comma separated list of OpenType font variation settings to apply. The
	format is similar to the CSS "font-variation-settings" property. For
	example, "wght 900" will set the weight of a variable font to 900 (if
	supported by the chosen font).

	Default: ""

*background-color*=_color_
	Color of the background. See *COLORS* for more information.

	Default: #1B1D1E

*outline-width*=_px_
	Width of the border outlines.

	Default: 4

*outline-color*=_color_
	Color of the border outlines. See *COLORS* for more information.

	Default: #080800

*border-width*=_px_
	Width of the border.

	Default: 12

*border-color*=_color_
	Color of the border. See *COLORS* for more information.

	Default: #F92672

*text-color*=_color_
	Color of text. See *COLORS* for more information.

	Default: #FFFFFF

*prompt-text*=_string_
	Prompt text.

	Default: "run: "

*prompt-padding*=_px_
	Extra horizontal padding between prompt and input.

	Default: 0

*prompt-color*=_color_
	Color of prompt text. See *COLORS* for more information.

	Default: Same as *text-color*

*prompt-background*=_color_
	Background color of prompt. See *COLORS* for more information.

	Default: #00000000

*prompt-background-padding*=_directional_
	Extra padding of the prompt background. See *DIRECTIONAL VALUES* for
	more information.

	Default: 0

*prompt-background-corner-radius*=_px_
	Corner radius of the prompt background.

	Default: 0

*placeholder-text*=_string_
	Placeholder input text.

	Default: ""

*placeholder-color*=_color_
	Color of placeholder input text. See *COLORS* for more information.

	Default: #FFFFFFA8

*placeholder-background*=_color_
	Background color of placeholder input text. See *COLORS* for more
	information.

	Default: #00000000

*placeholder-background-padding*=_directional_
	Extra padding of the placeholder input text background. See
	*DIRECTIONAL VALUES* for more information.

	Default: 0

*placeholder-background-corner-radius*=_px_
	Corner radius of the placeholder input text background.

	Default: 0

*input-color*=_color_
	Color of input text. See *COLORS* for more information.

	Default: Same as *text-color*

*input-background*=_color_
	Background color of input. See *COLORS* for more information.

	Default: #00000000

*input-background-padding*=_directional_
	Extra padding of the input background. See *DIRECTIONAL VALUES* for
	more information.

	Default: 0

*input-background-corner-radius*=_px_
	Corner radius of the input background.

	Default: 0

*text-cursor-style*=_bar|block|underscore_
	Style of the text cursor (if shown).

	Default: bar

*text-cursor-color*=_color_
	Color of the text cursor.

	Default: same as *input-color*

*text-cursor-background*=_color_
	Color of text behind the text cursor when *text-cursor-style*=block.

	Default: same as *background-color*

*text-cursor-corner-radius*=_px_
	Corner radius of the text cursor.

	Default: 0

*text-cursor-thickness*=_px_
	Thickness of the bar and underscore text cursors.

	Default: font-dependent when *text-cursor-style*=underscore, 2
	otherwise.

*default-result-color*=_color_
	Default color of result text. See *COLORS* for more information.

	Default: Same as *text-color*

*default-result-background*=_color_
	Default background color of results. See *COLORS* for more information.

	Default: #00000000

*default-result-background-padding*=_directional_
	Default extra padding of result backgrounds. See *DIRECTIONAL VALUES*
	for more information.

	Default: 0

*default-result-background-corner-radius*=_px_
	Default corner radius of result backgrounds.

	Default: 0

*alternate-result-color*=_color_
	Color of alternate (even-numbered) result text. See *COLORS* for more
	information.

	Default: same as *default-result-color*

*alternate-result-background*=_color_
	Background color of alternate (even-numbered) results. See *COLORS* for
	more information.

	Default: same as *default-result-background*

*alternate-result-background-padding*=_directional_
	Extra padding of alternate (even-numbered) result backgrounds. See
	*DIRECTIONAL VALUES* for more information.

	Default: same as *default-result-background-padding*

*alternate-result-background-corner-radius*=_px_
	Corner radius of alternate (even-numbered) result backgrounds.

	Default: same as *default-result-background-corner-radius*

*num-results*=_n_
	Maximum number of results to display. If _n_ = 0, tofi will draw as
	many results as it can fit in the window.

	Default: 0

*selection-color*=_color_
	Color of selected result. See *COLORS* for more information.

	Default: #F92672

*selection-match-color*=_color_
	Color of the matching portion of the selected result. This will not
	always be shown if the *fuzzy-match* option is set to true. Any color
	that is fully transparent (alpha = 0) will disable this highlighting.
	See *COLORS* for more information.

	Default: #00000000

*selection-padding*=_px_
	*WARNING*: This option is deprecated, and will be removed in a future
	version of tofi. You should use the *selection-background-padding*
	option instead.

	Extra horizontal padding of the selection background. If _px_ = -1,
	the padding will fill the whole window width.

	Default: 0

*selection-background*=_color_
	Background color of selected result. See *COLORS* for more information.

	Default: #00000000

*selection-background-padding*=_directional_
	Extra padding of the selected result background. See *DIRECTIONAL
	VALUES* for more information.

	Default: 0

*selection-background-corner-radius*=_px_
	Corner radius of the selected result background.
	Default: 0

*result-spacing*=_px_
	Spacing between results. Can be negative.

	Default: 0

*min-input-width*=_px_
	Minimum width of input in horizontal mode.

	Default: 0

*width*=_px|%_
	Width of the window. See *PERCENTAGE VALUES* for more information.

	Default: 1280

*height*=_px|%_
	Height of the window. See *PERCENTAGE VALUES* for more information.

	Default: 720

*corner-radius*=_px_
	Radius of the window corners.

	Default: 0

*anchor*=_position_
	Location on screen to anchor the window. Supported values are
	_top-left_, _top_, _top-right_, _right_, _bottom-right_, _bottom_,
	_bottom-left_, _left_, and _center_.

	Default: center

*exclusive-zone*=_-1|px|%_
	Set the size of the exclusive zone. A value of -1 means ignore
	exclusive zones completely. A value of 0 will move tofi out of the way
	of other windows' exclusive zones. A value greater than 0 will set that
	much space as an exclusive zone. Values greater than 0 are only
	meaningful when tofi is anchored to a single edge.

	Default: -1

*output*=_name_
	The name of the output to appear on, if multiple outputs are present.
	If empty, the compositor will choose which output to display the window
	on (usually the currently focused output).

	Default: ""

*scale*=_true|false_
	Scale the window by the output's scale factor.

	Default: true

*margin-top*=_px|%_
	Offset from top of screen. See *PERCENTAGE VALUES* for more
	information. Only has an effect when anchored to the top of the screen.

	Default: 0

*margin-bottom*=_px|%_
	Offset from bottom of screen. See *PERCENTAGE VALUES* for more
	information. Only has an effect when anchored to the bottom of the
	screen.

	Default: 0

*margin-left*=_px|%_
	Offset from left of screen. See *PERCENTAGE VALUES* for more
	information. Only has an effect when anchored to the left of the
	screen.

	Default: 0

*margin-right*=_px|%_
	Offset from right of screen. See *PERCENTAGE VALUES* for more
	information. Only has an effect when anchored to the right of the
	screen.

	Default: 0

*padding-top*=_px|%_
	Padding between top border and text. See *PERCENTAGE VALUES* for more
	information.

	Default: 8

*padding-bottom*=_px|%_
	Padding between bottom border and text. See *PERCENTAGE VALUES* for
	more information.

	Default: 8

*padding-left*=_px|%_
	Padding between left border and text. See *PERCENTAGE VALUES* for more
	information.

	Default: 8

*padding-right*=_px|%_
	Padding between right border and text. See *PERCENTAGE VALUES* for more
	information.

	Default: 8

*clip-to-padding*=_true|false_
	Whether to clip text drawing to be within the specified padding. This
	is mostly important for allowing text to be inset from the border,
	while still allowing text backgrounds to reach right to the edge.

	Default: true

*horizontal*=_true|false_
	List results horizontally.

	Default: false

*hint-font*=_true|false_
	Perform font hinting. Only applies when a path to a font has been
	specified via *font*. Disabling font hinting speeds up text
	rendering appreciably, but will likely look poor at small font pixel
	sizes.

	Default: true

# COLORS

Colors can be specified in the form _RGB_, _RGBA_, _RRGGBB_ or _RRGGBBAA_,
optionally prefixed with a hash (#).

# PERCENTAGE VALUES

Some pixel values can optionally have a % suffix, like so:

	width = 50%

This will be interpreted as a percentage of the screen resolution in the
relevant direction.

# DIRECTIONAL VALUES

The background box padding of a type of text can be specified by one to four
comma separated values, with meanings similar to the CSS padding property:

- One value sets all edges.
- Two values set (top & bottom), (left & right) edges.
- Three values set (top), (left & right), (bottom) edges.
- Four values set (top), (right), (bottom), (left) edges.

Specifying -1 for any of the values will pad as far as possible in that
direction.

# AUTHORS

Philip Jones <philj56@gmail.com>

# SEE ALSO

*tofi*(1), *dmenu*(1) *rofi*(1)
