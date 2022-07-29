tofi(1)

# NAME

tofi - Tiny dynamic menu for Wayland, inspired by *rofi*(1) and *dmenu*(1).

# SYNOPSIS

*tofi* [options...]

*tofi-run* [options...]

*tofi-drun* [options...]

*tofi-compgen*

# DESCRIPTION

*tofi* is a tiny dynamic menu for Wayland compositors supporting the
layer-shell protocol. It reads newline-separated items from stdin, and displays
a graphical selection menu. When a selection is made, it is printed to stdout.

When invoked via the name *tofi-run*, *tofi* will not accept items on stdin,
instead presenting a list of executables in the user's $PATH.

When invoked via the name *tofi-drun*, *tofi* will not accept items on stdin,
and will generate a list of applications from desktop files as described in the
Desktop Entry Specification.

*WARNING*:  Currently, when a selection is made in drun mode, the filename of
the selected desktop file is printed to stdout. In the next version of tofi,
this will change to an executable command line, to be passed to swaymsg exec or
similar. The new behaviour can be enabled now by passing --drun-print-exec=true
to tofi-drun.

*tofi-compgen* just prints the list of executables used by *tofi-run*.

# OPTIONS

*-h, --help*
	Print help and exit.

*-c, --config* <path>
	Specify path to custom config file.

*--output* <name>
	Select the output to appear on.

*--late-keyboard-init*
	Delay keyboard initialisation until after the first draw to screen.
	This option is experimental, and will cause tofi to miss keypresses
	for a short time after launch. The only reason to use this option is
	performance on slow systems.

All config file options described in *tofi*(5) are also accepted, in the form
*--key=value*.

# KEYS

<Up> | <Left> | <Ctrl>-k
	Move the selection back one entry.

<Down> | <Right> | <Ctrl>-j | <Tab>
	Move the selection forward one entry.

<Enter>
	Confirm the current selection and quit.

<Escape>
	Quit without making a selection.

# FILES

_/etc/xdg/tofi/config_
	Example configuration file.

_$XDG_CONFIG_HOME/tofi/config_
	The default configuration file location.

_$XDG_CACHE_HOME/tofi-compgen_
	Cached list of executables under $PATH, regenerated as necessary.

_$XDG_CACHE_HOME/tofi-drun_
	Cached list of desktop applications, regenerated as necessary.

_$XDG_STATE_HOME/tofi-history_
	Numeric count of commands selected in *tofi-run*, to enable sorting
	results by run count.

_$XDG_STATE_HOME/tofi-drun-history_
	Numeric count of commands selected in *tofi-drun*, to enable sorting
	results by run count.

# AUTHORS

Philip Jones <philj56@gmail.com>

# SEE ALSO

*tofi*(5), *dmenu*(1) *rofi*(1)
