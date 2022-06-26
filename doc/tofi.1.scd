tofi(1)

# NAME

tofi - Tiny dynamic menu for Wayland, inspired by *rofi*(1) and *dmenu*(1).

# SYNOPSIS

*tofi* [options...]

*tofi-run* [options...]

*tofi-compgen*

# DESCRIPTION

*tofi* is a tiny dynamic menu for Wayland compositors supporting the
layer-shell protocol. It reads newline-separated items from stdin, and displays
a graphical selection menu. When a selection is made, it is printed to stdout.

When invoked via the name *tofi-run*, *tofi* will not accept items on stdin,
instead presenting a list of executables in the user's $PATH.

*tofi-compgen* just prints the list of executables used by *tofi-run*.

# OPTIONS

*-h, --help*
	Print help and exit.

*-c, --config* <path>
	Specify path to custom config file.

*--late-keyboard-init*
	Delay keyboard initialisation until after the first draw to screen.
	This option is experimental, and will cause tofi to miss keypresses
	for a short time after launch. The only reason to use this option is
	performance on slow systems.

All config file options described in *tofi*(5) are also accepted, in the form
*--key=value*.

# KEYS

<Up> | <Left>
	Move the selection back one entry.

<Down> | <Right>
	Move the selection back forward entry.

<Enter>
	Confirm the current selection and quit.

<Escape>
	Quit without making a selection.

# FILES

_$XDG_CONFIG_HOME/tofi/config_
	The default configuration file.

_$XDG_CACHE_HOME/tofi-compgen_
	Cached list of executables under $PATH, regenerated as necessary.

_$XDG_STATE_HOME/tofi-history_
	Numeric count of commands selected in *tofi-run*, to enable sorting
	results by run count.

# AUTHORS

Philip Jones <philj56@gmail.com>

# SEE ALSO

*tofi*(5), *dmenu*(1) *rofi*(1)
