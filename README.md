slstatus
========

**slstatus** is a suckless and lightweight status monitor for window managers which use WM_NAME as statusbar (e.g. DWM). It is written in pure C without any system() calls and only reads from files most of the time. It is meant as a better alternative to Bash scripts (inefficient) and Conky (bloated for this use).

The following information is included:

- wifi percentage
- battery percentage
- cpu usage in percent
- cpu temperature
- ram usage in percent
- alsa volume level in percent
- disk usage
- date
- time

Multiple entries (battery, wifi signal, ...) are supported and everything can be reordered and customized via a C header file (similar to DWM).

![screenshot](screenshot.png)

## Usage

### Installation

Before you continue, please be sure that a C compiler, `make` and `alsa-lib` are installed. Then compile the program once using `sudo make install`. After that you may change config.h to your needs and recompile it after any change! 

### Starting

Put the following code in your ~/.xinitrc (or similar):

```
while true; do
    slstatus
done &
```

The loop is needed that the program runs after suspend to ram.

## Contributing

In [TODO.md](TODO.md) there is a list of things that have to be done.

People who contributed are listed in [CONTRIBUTORS.md](CONTRIBUTORS.md).

For detailed information about coding style and restrictions see [CONTRIBUTING.md](CONTRIBUTING.md)

## License

See [LICENSE](LICENSE).
