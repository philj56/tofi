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
	Hide the cursor.

	Default: false

*history*=_true|false_
	Sort results by number of usages in run and drun modes.

	Default: true

*fuzzy-match*=_true|false_
	If true, searching is performed via a simple fuzzy matching algorithm.
	If false, substring matching is used, weighted to favour matches closer
	to the beginning of the string.

	Default: false

*require-match*=_true|false_
	If true, require a match to allow a selection to be made. If false,
	making a selection with no matches will print input to stdout.
	In drun mode, this is always true.

	Default: true

*hide-input*=_true|false_
	If true, typed input will be hidden, and what is displayed (if
	anything) is determined by the *hidden-character* option.

	Default: false

*hidden-character*=_char_
	Replace displayed input characters with _char_. If _char_ is set to the
	empty string, input will be completely hidden.
	This option only has an effect when *hide-input* is set to true.

	Default: \*

*drun-launch*=_true|false_
	If true, directly launch applications on selection when in drun mode.
	Otherwise, just print the path of the .desktop file to stdout.

	Default: false

*terminal*=_command_
	The terminal to run terminal programs in when in drun mode. _command_
	will be prepended to the the application's command line.
	This option has no effect if *drun-launch* is set to true.

	Default: the value of the TERMINAL environment variable

*drun-print-exec*=_true|false_
	*WARNING*: In the current version of tofi, this option has changed to
	always be true and has no effect, as it should have been from the
	start. It may be removed in a future version of tofi.

	Default: true

*late-keyboard-init*=_true|false_
	Delay keyboard initialisation until after the first draw to screen.
	This option is experimental, and will cause tofi to miss keypresses
	for a short time after launch. The only reason to use this option is
	performance on slow systems.

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
	Extra horizontal padding of the selection background. If _px_ = -1,
	the padding will fill the whole window width.

	Default: 0

*selection-background*=_color_
	Background color of selected result. See *COLORS* for more information.

	Default: #00000000

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
	Set the size of the exclusive zone. A value of -1 means ignore exclusive
	zones completely. A value of 0 will move tofi out of the way of other
	windows' exclusive zones. A value greater than 0 will set that much space as
	an exclusive zone. Values greater than 0 are only meaningful when tofi is
	anchored to a single edge.

	Default: -1

*output*=_name_
	The name of the output to appear on, if multiple outputs are present.
	If empty, the compositor will choose which output to display the window
	on (usually the currently focused output).

	Default: ""

*scale*=_true|false_
	Scale the window by the output's scale factor.

	*WARNING*: In the current version of tofi, the default value has
	changed to true, so you may need to update your config. Additionally,
	font scaling will no longer occur when this is set to _false_.

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

# AUTHORS

Philip Jones <philj56@gmail.com>

# SEE ALSO

*tofi*(5), *dmenu*(1) *rofi*(1)
