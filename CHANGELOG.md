# Changelog

## [0.7.0] - 2022-11-01
### Added
- Added `include` option, allowing config files to include other files.
- Added `hide-input` and `hidden-character` options for sensitive input.
- Added `exclusive-zone` option, to control interaction with menu bars etc.
- Added `terminal` option to allow `tofi-drun` to launch terminal apps.
- Added a couple of extra keybindings.

### Changed
- Searching is now Unicode aware, so case-insensitive matching of non-Latin
	characters should work.
- Fuzzy matching will now use a simpler algorithm when matching lines more than
	100 characters in length to avoid slowdowns.
- By default, tofi will now refuse to start if another instance is already
	running, preventing accidental double launches. This can be changed with the
	`multi-instance` option.
- Tofi will now show up on top of fullscreen windows.

### Fixed
- Keyboard shortcuts are now bound to physical keys rather than characters, so
	should not change places when changing keyboard layouts.
- Fix crash when attempting to change the selection while no results are
	displayed.
- Fixed a rare issue where input could become out of sync with the display.


## [0.6.0] - 2022-09-08
### Warning - HiDPI config change
In the [0.5.0] release, the `scale` option was added to enable scaling of pixel
values by the display's scale factor. In this release, the default value of
`scale` has changed to be `true`, and fonts are no longer scaled if `scale` is
set to `false`. This makes tofi's behaviour match that of e.g. Sway, and makes
configs work more reliably on monitors with different scale factors.

If you use tofi on a HiDPI display, you may need to change your config's pixel
values to make things look right again.

### Added
- Added `require-match` option, to allow printing of input even when there are
	no matching results.
- Added `prompt-padding` option for more flexible spacing between the prompt
	and other text.
- Added a new example theme, dark-paper.

### Changed
- The `scale` option now defaults to `true`, as noted above. The example themes
	have been updated to account for this change.
- Spaces are now allowed as part of normal input. Similarly to dmenu, tofi will
	split the input into words, and only show results for which every word
	matches individually.
- Split `tofi(5)` manpage into behaviour and style options to make finding
	options easier.

### Fixed
- Fixed build failure when link-time optimisation is disabled.

## [0.5.0] - 2022-08-21
### Warning - HiDPI config change
In previous versions of tofi, pixel values were always treated as device
pixels, ignoring the display's scale factor. This allows pixel-perfect sizes,
but means you have to make different configs for differently scaled displays,
and isn't how e.g. Sway does things. Additionally, fonts currently *are* scaled
by the scale factor, making things a little complex.

This release adds a `scale` boolean option, which currently defaults to
`false`. Setting this to `true` will make pixel values scale with the display's
scale factor.

In the next version of tofi, `scale` will default to `true` (but still be
around if you want the old behaviour). Setting `scale` to `false` will also
start causing fonts to not be scaled by the scale factor.

If you use tofi on a HiDPI display, you should explicitly set `scale` to your
desired setting now (or at least be aware that you'll need to change some theme
dimensions in the next version).

### Added
- Fuzzy matching can now be enabled with the `fuzzy_match` option.
- Added `scale` option, as described above.
- Added Ctrl-u and Ctrl-w readline keybindings.
- Added this changelog.

### Changed
- Improved performance when neither `selection-match-color` or
  `selection-background-color` are specified.
- Improved performance on systems with Transparent HugePages enabled for shared
  memory (none that I know of for now, but may be relevant in the future).


## [0.4.0] - 2022-08-07
### Deprecated
In the [0.3.0] release, the `drun-print-exec` option was added to enable fixed
`tofi-drun` behaviour. This release changes this to be the default, as this is
how it should have been done from the start. Consequently, the
`drun-print-exec` option is now obsolete, and may be removed in the future, so
you can safely delete it from your configs.

### Added
- A full example config file is included in `doc/config`, and is installed to
  `/etc/xdg/tofi/config`
- Added `selection-padding` option, to make the selected item background wider.
- Added `selection-match-color` option, to highlight the matching portion of
  the selected result.
- Added key-repeat.
- Added result pagination.

### Removed
- `tofi-compgen` is no longer installed, as it's really just a debugging
  utility and not needed.

### Changed
- `drun-print-exec` is now always set to true, and the option is deprecated.
- The `output` and `late-keyboard-init` options are no longer command-line
  only, and so can be specified in the config file.

### Fixed
- Fixed slanted fonts being cut off if they extend back towards the prompt.
- The selection background now correctly wraps slanted fonts.
- Enable compilation with some older Wayland versions (specifically that found
  on Ubuntu 20.04).


## [0.3.1] - 2022-07-28
### Fixed
- Fix some program arguments not working in drun mode.


## [0.3.0] - 2022-07-27
### Deprecated
Previously, tofi-drun would print the filename of the selected .desktop file to
stdout. This could then be passed to `xargs swaymsg exec gio launch` to be
executed.

The problem is that this ends up defeating the purpose of passing the command
to swaymsg exec, and the workspace the command was selected on may not be the
one that it starts up on, if for example it takes a long time and the user
switches workspaces in the meantime.

The solution is to instead print the Exec= line from the .desktop file, and
pass that directly to `xargs swaymsg exec --` for execution.

To avoid too much breaking of configs for the few people who use tofi
currently, this release adds a new option, `--drun-print-exec`, to enable the
fixed behaviour. The next release will change this to be the default, as this
is how it should have been done from the start.

### Added
- Tofi will now automatically detect how many results can be displayed if
  `--num-results=0` is set (the new default).
- When running in drun mode, keywords will also be matched along with the name
  (so e.g. searching for "web" will return "firefox")
- Add `--drun-print-exec` option, as noted above.

### Fixed
- Fixed percentage values passed to margin options not behaving correctly when
  output scaling is enabled.
- Fix tofi not grabbing keyboard focus on River.
- Correct `--font` option name in man page.
- Fix percentage values on vertical monitors.
- Fix drun mode weirdness when history is enabled


## [0.2.0] - 2022-07-25
### Added
- `tofi-drun` mode for launching apps from `.desktop` files.
- Navigation keybindings for `Ctrl-j`, `Ctrl-k` and `Tab`.

### Changed
- Search results now prioritise matches early in a word, e.g. a search for
  `fire` yields `firefox` before `aafire`.

### Fixed
- Fixed input / display sometimes going out of sync.
- Add dependency on `librt` for systems that require that (from [@sktt](https://github.com/sktt)).
- Allow for a count of more than 128 program launches in the history file.


## [0.1.1] - 2022-06-28
### Fixed
- Fix typo in `meson.build`.

## [0.1.0] - 2022-06-27
Initial release. Good enough to use, but still some jank.

[0.7.0]: https://github.com/philj56/tofi/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/philj56/tofi/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/philj56/tofi/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/philj56/tofi/compare/v0.3.1...v0.4.0
[0.3.1]: https://github.com/philj56/tofi/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/philj56/tofi/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/philj56/tofi/compare/v0.2.0...v0.1.1
[0.1.1]: https://github.com/philj56/tofi/compare/v0.1.1...v0.1.0
[0.1.0]: https://github.com/philj56/tofi/releases/tag/v0.1.0
