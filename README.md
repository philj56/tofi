# greetd-mini-wl-greeter

An extremely simple raw Wayland greeter for greetd, inspired by
[lightdm-mini-greeter](https://github.com/prikhi/lightdm-mini-greeter).

![Screenshot](screenshot.png)

## Usage

Follow the same steps as for e.g. gtkgreet in the [greetd
wiki](https://man.sr.ht/~kennylevinsen/greetd/). See the man page for sway
config suggestions.

## Install

### Arch

greetd-mini-wl-greeter is available on the
[AUR](https://aur.archlinux.org/packages/greetd-mini-wl-greeter-git/):
```sh
paru -S greetd-mini-wl-greeter-git
```

### Source
```sh
meson build
ninja -C build
```

## Tips

It's quite entertaining to set the password character to one from the
[combining diacritical
marks] (https://en.wikipedia.org/wiki/Combining_Diacritical_Marks) page, e.g.
```sh
greetd-mini-wl-greeter -C ̣
```

![Combining diacritical screenshot](screenshot_vertical.png)
