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

# OPTIONS

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

*num-results*=_n_
	Maximum number of results to display. If _n_ = 0, tofi will draw as
	many results as it can fit in the window.

	Default: 0

*selection-color*=_color_
	Color of selected result. See *COLORS* for more information.

	Default: #F92672

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

*hide-cursor*=_true|false_
	Hide the cursor.

	Default: false

*horizontal*=_true|false_
	List results horizontally.

	Default: false

*history*=_true|false_
	Sort results by number of usages in run and drun modes.

	Default: true

*drun-launch*=_true|false_
	If true, directly launch applications on selection when in drun mode.
	Otherwise, just print the path of the .desktop file to stdout.

	Default: false

*drun-print-exec*=_true|false_
	If true, print an executable command line for the selected application
	when in drun mode. Otherwise, just print the path of the .desktop file.

	*WARNING*: In the next version of tofi, this will change to always be
	true, as it should have been from the start.

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
