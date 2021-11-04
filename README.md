# greetd-mini-wl-greeter

An extremely simple raw Wayland greeter for
[greetd](https://sr.ht/~kennylevinsen/greetd/), inspired by
[lightdm-mini-greeter](https://github.com/prikhi/lightdm-mini-greeter).

![Screenshot](screenshot.png)

The aim is to do just what I want it to as quick as possible. On a 2015 Macbook
Pro, with no background image or font options, startup takes ~35ms. On a
Raspberry Pi Zero 2, it takes ~500ms, mostly waiting for EGL / OpenGL to
initialise.

## Usage

Follow the same steps as for e.g. gtkgreet in the [greetd
wiki](https://man.sr.ht/~kennylevinsen/greetd/). See the man page for
configuration options. All colors and sizes can be customised, and a PNG
background image can be displayed scaled and centered.

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
marks](https://en.wikipedia.org/wiki/Combining_Diacritical_Marks) page, e.g.
```sh
greetd-mini-wl-greeter -C ̣
```

![Combining diacritical screenshot](screenshot_vertical.png)
