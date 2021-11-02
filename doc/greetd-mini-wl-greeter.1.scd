greetd-mini-wl-greeter(1)

# NAME

greetd-mini-wl-greeter - An extremely simple raw Wayland greeter for greetd

# SYNOPSIS

*greetd-mini-wl-greeter* -u user -c command [options]

# DESCRIPTION

greetd-mini-wl-greeter is a raw Wayland greeter for greetd, designed for a
single user. It requires a Wayland compositor to run, such as *cage*(1) or
*sway*(1).

# OPTIONS

All options which take a color expect a 3- or 6-digit hex code, e.g. F9A733.

*-u, --user* <username>
	The user to login as.

*-c, --command* <command>
	The command to run on login.

*-b, --background-image* <image>
	An image to use as the background. Currently only PNG images are supported.

*-B, --background-color* <color>
	Color of the background in the absence of an image.

*-o, --outline-width* <value>
	Width of the outlines around the border, in pixels.

*-O, --outline-color* <color>
	Color of the outlines around the border.

*-r, --border-width* <value>
	Width of the border in pixels.

*-R, --border-color* <color>
	Color of the border.

*-e, --entry-padding* <value>
	Padding around the password text in pixels.

*-E, --entry-color* <color>
	Color of the password entry box.

*-f, --font-name* <name>
	Font to use for the password entry.

*-F, --font-size* <value>
	Point size of the password text.

*-C, --password-character* <character>
	Character to use to hide the password. Defaults to full-stop.

*-n, --width-characters* <value>
	Make the password entry box big enough to fit this many characters.

*-w, --wide-layout*
	By default, the password entry box will be shrunk vertically to exactly fit
	the specified password character. Use this option if you'd rather that it got
	laid out like normal text, i.e. the box remains one full character tall.

*-H, --hide-cursor*
	Hide the cursor.

*-h, --help*
	Print help and exit.

# EXAMPLES

When using sway as the compositor for greetd-mini-greeter, something like the
following sway config lines produce a nice look:

```
# Remove any window borders
default_border none
hide_edge_borders both

# Add a background image, to avoid a flicker of default background color
output * background /path/to/image fill

# Use the same background image for the greeter
exec greetd-mini-wl-greeter -u username -c command -b /path/to/image
```

Fun can be had with the *-C* option, as it should support any character from
your locale, which is probably a UTF-8 one. For example, the Unicode "Combining
Dot Below" character ( ̣) produces a nice effect.

# AUTHORS

Philip Jones <philj56@gmail.com>

# SEE ALSO

*greetd*(1)
