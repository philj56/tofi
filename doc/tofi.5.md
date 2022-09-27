# NAME

tofi - configuration file

# DESCRIPTION

The config file format is basic .ini/.cfg style. Options are set one per
line, with the syntax:

> option = value

Whitespace is ignored. Values starting or ending with whitespace can be
given by enclosing them in double quotes like so:

> option = " value "

Lines beginning with \# or ; are treated as comments. Section headers of
the form \[header\] are currently ignored. All options and values are
case-insensitive, except where not possible (e.g. paths). Later options
override earlier options, and command line options override config file
options.

# SPECIAL OPTIONS

**include**=*path*

> Include the contents of another config file. If *path* is a relative
> path, it is interpreted as relative to this config file's path (or the
> current directory if **--include** is passed on the command line).
> Inclusion happens immediately, before the rest of the current file's
> contents are parsed.

# BEHAVIOUR OPTIONS

**hide-cursor**=*true\|false*

> Hide the cursor.
>
> Default: false

**history**=*true\|false*

> Sort results by number of usages in run and drun modes.
>
> Default: true

**fuzzy-match**=*true\|false*

> If true, searching is performed via a simple fuzzy matching algorithm.
> If false, substring matching is used, weighted to favour matches
> closer to the beginning of the string.
>
> Default: false

**require-match**=*true\|false*

> If true, require a match to allow a selection to be made. If false,
> making a selection with no matches will print input to stdout. In drun
> mode, this is always true.
>
> Default: true

**drun-launch**=*true\|false*

> If true, directly launch applications on selection when in drun mode.
> Otherwise, just print the path of the .desktop file to stdout.
>
> Default: false

**drun-print-exec**=*true\|false*

> **WARNING**: In the current version of tofi, this option has changed
> to always be true and has no effect, as it should have been from the
> start. It may be removed in a future version of tofi.
>
> Default: true.

**late-keyboard-init**=*true\|false*

> Delay keyboard initialisation until after the first draw to screen.
> This option is experimental, and will cause tofi to miss keypresses
> for a short time after launch. The only reason to use this option is
> performance on slow systems.
>
> Default: false

# STYLE OPTIONS

**font**=*font*

> Font to use. If *font* is a path to a font file, **tofi** will not
> have to use Pango or Fontconfig. This greatly speeds up startup, but
> any characters not in the chosen font will fail to render.
>
> If a path is not given, *font* is interpreted as a font name in Pango
> format.
>
> Default: "Sans"

**font-size**=*pt*

> Point size of text.
>
> Default: 24

**background-color**=*color*

> Color of the background. See **COLORS** for more information.
>
> Default: \#1B1D1E

**outline-width**=*px*

> Width of the border outlines.
>
> Default: 4

**outline-color**=*color*

> Color of the border outlines. See **COLORS** for more information.
>
> Default: \#080800

**border-width**=*px*

> Width of the border.
>
> Default: 12

**border-color**=*color*

> Color of the border. See **COLORS** for more information.
>
> Default: \#F92672

**text-color**=*color*

> Color of text. See **COLORS** for more information.
>
> Default: \#FFFFFF

**prompt-text**=*string*

> Prompt text.
>
> Default: "run: "

**prompt-padding**=*px*

> Extra horizontal padding between prompt and input.
>
> Default: 0

**num-results**=*n*

> Maximum number of results to display. If *n* = 0, tofi will draw as
> many results as it can fit in the window.
>
> Default: 0

**selection-color**=*color*

> Color of selected result. See **COLORS** for more information.
>
> Default: \#F92672

**selection-match-color**=*color*

> Color of the matching portion of the selected result. This will not
> always be shown if the **fuzzy-match** option is set to true. Any
> color that is fully transparent (alpha = 0) will disable this
> highlighting. See **COLORS** for more information.
>
> Default: \#00000000

**selection-padding**=*px*

> Extra horizontal padding of the selection background. If *px* = -1,
> the padding will fill the whole window width.
>
> Default: 0

**selection-background**=*color*

> Background color of selected result. See **COLORS** for more
> information.
>
> Default: \#00000000

**result-spacing**=*px*

> Spacing between results. Can be negative.
>
> Default: 0

**min-input-width**=*px*

> Minimum width of input in horizontal mode.
>
> Default: 0

**width**=*px\|%*

> Width of the window. See **PERCENTAGE VALUES** for more information.
>
> Default: 1280

**height**=*px\|%*

> Height of the window. See **PERCENTAGE VALUES** for more information.
>
> Default: 720

**corner-radius**=*px*

> Radius of the window corners.
>
> Default: 0

**anchor**=*position*

> Location on screen to anchor the window. Supported values are
> *top-left*, *top*, *top-right*, *right*, *bottom-right*, *bottom*,
> *bottom-left*, *left*, and *center*.
>
> Default: center

**output**=*name*

> The name of the output to appear on, if multiple outputs are present.
> If empty, the compositor will choose which output to display the
> window on (usually the currently focused output).
>
> Default: ""

**scale**=*true\|false*

> Scale the window by the output's scale factor.
>
> **WARNING**: In the current version of tofi, the default value has
> changed to true, so you may need to update your config. Additionally,
> font scaling will no longer occur when this is set to *false*.
>
> Default: true

**margin-top**=*px\|%*

> Offset from top of screen. See **PERCENTAGE VALUES** for more
> information. Only has an effect when anchored to the top of the
> screen.
>
> Default: 0

**margin-bottom**=*px\|%*

> Offset from bottom of screen. See **PERCENTAGE VALUES** for more
> information. Only has an effect when anchored to the bottom of the
> screen.
>
> Default: 0

**margin-left**=*px\|%*

> Offset from left of screen. See **PERCENTAGE VALUES** for more
> information. Only has an effect when anchored to the left of the
> screen.
>
> Default: 0

**margin-right**=*px\|%*

> Offset from right of screen. See **PERCENTAGE VALUES** for more
> information. Only has an effect when anchored to the right of the
> screen.
>
> Default: 0

**padding-top**=*px\|%*

> Padding between top border and text. See **PERCENTAGE VALUES** for
> more information.
>
> Default: 8

**padding-bottom**=*px\|%*

> Padding between bottom border and text. See **PERCENTAGE VALUES** for
> more information.
>
> Default: 8

**padding-left**=*px\|%*

> Padding between left border and text. See **PERCENTAGE VALUES** for
> more information.
>
> Default: 8

**padding-right**=*px\|%*

> Padding between right border and text. See **PERCENTAGE VALUES** for
> more information.
>
> Default: 8

**horizontal**=*true\|false*

> List results horizontally.
>
> Default: false

**hint-font**=*true\|false*

> Perform font hinting. Only applies when a path to a font has been
> specified via **font**. Disabling font hinting speeds up text
> rendering appreciably, but will likely look poor at small font pixel
> sizes.
>
> Default: true

# COLORS

Colors can be specified in the form *RGB*, *RGBA*, *RRGGBB* or
*RRGGBBAA*, optionally prefixed with a hash (#).

# PERCENTAGE VALUES

Some pixel values can optionally have a % suffix, like so:

> width = 50%

This will be interpreted as a percentage of the screen resolution in the
relevant direction.

# AUTHORS

Philip Jones \<philj56@gmail.com\>

# SEE ALSO

**tofi**(5), **dmenu**(1) **rofi**(1)
