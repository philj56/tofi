## NAME

tofi - Tiny dynamic menu for Wayland, inspired by **rofi**(1) and
**dmenu**(1).

## SYNOPSIS

**tofi** \[options...\]

**tofi-run** \[options...\]

**tofi-drun** \[options...\]

## DESCRIPTION

**tofi** is a tiny dynamic menu for Wayland compositors supporting the
layer-shell protocol. It reads newline-separated items from stdin, and
displays a graphical selection menu. When a selection is made, it is
printed to stdout.

When invoked via the name **tofi-run**, **tofi** will not accept items
on stdin, instead presenting a list of executables in the user's \$PATH.

When invoked via the name **tofi-drun**, **tofi** will not accept items
on stdin, and will generate a list of applications from desktop files as
described in the Desktop Entry Specification.

## OPTIONS

**-h, --help**

> Print help and exit.

**-c, --config** \<path\>

> Specify path to custom config file.

All config file options described in **tofi**(5) are also accepted, in
the form **--key=value**.

## KEYS

\<Up\> \| \<Left\> \| \<Ctrl\>-k \| \<Ctrl\>-p \| \<Ctrl\>-b \|
\<Alt\>-k \| \<Alt\>-p \| \<Alt\>-h \| \<Shift\>-\<Tab\>

> Move the selection back one entry.

\<Down\> \| \<Right\> \| \<Ctrl\>-j \| \<Ctrl\>-n \| \<Ctrl\>-f \|
\<Alt\>-j \| \<Alt\>-n \| \<Alt\>-l \| \<Tab\>

> Move the selection forward one entry.

\<Page Up\>

> Move the selection back one page.

\<Page Down\>

> Move the selection forward one page.

\<Backspace\> \| \<Ctrl\>-h

> Delete character.

\<Ctrl\>-u

> Delete line.

\<Ctrl\>-w \| \<Ctrl\>-\<Backspace\>

> Delete word.

\<Enter\> \| \<Ctrl\>-m

> Confirm the current selection and quit.

\<Escape\> \| \<Ctrl\>-c \| \<Ctrl\>-g \| \<Ctrl\>-\[

> Quit without making a selection.

## FILES

*/etc/xdg/tofi/config*

> Example configuration file.

*\$XDG_CONFIG_HOME/tofi/config*

> The default configuration file location.

*\$XDG_CACHE_HOME/tofi-compgen*

> Cached list of executables under \$PATH, regenerated as necessary.

*\$XDG_CACHE_HOME/tofi-drun*

> Cached list of desktop applications, regenerated as necessary.

*\$XDG_STATE_HOME/tofi-history*

> Numeric count of commands selected in **tofi-run**, to enable sorting
> results by run count.

*\$XDG_STATE_HOME/tofi-drun-history*

> Numeric count of commands selected in **tofi-drun**, to enable sorting
> results by run count.

## EXIT STATUS

**tofi** exits with one of the following values:

0

> Success; a selection was made, or **tofi** was invoked with the **-h**
> option.

1

> An error occurred, or the user exited without making a selection.

## AUTHORS

Philip Jones \<<philj56@gmail.com>\>

## SEE ALSO

**tofi**(5), **dmenu**(1), **rofi**(1)
