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

# OPTIONS

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

> Color of the matching portion of the selected result. Any color that
> is fully transparent (alpha = 0) will disable this highlighting. See
> **COLORS** for more information.
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

**hide-cursor**=*true\|false*

> Hide the cursor.
>
> Default: false

**horizontal**=*true\|false*

> List results horizontally.
>
> Default: false

**history**=*true\|false*

> Sort results by number of usages in run and drun modes.
>
> Default: true

**drun-launch**=*true\|false*

> If true, directly launch applications on selection when in drun mode.
> Otherwise, just print the path of the .desktop file to stdout.
>
> Default: false

**drun-print-exec**=*true\|false*

> If true, print an executable command line for the selected application
> when in drun mode. Otherwise, just print the path of the .desktop
> file.
>
> **WARNING**: In the next version of tofi, this will change to always
> be true, as it should have been from the start.
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
