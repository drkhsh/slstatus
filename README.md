slstatus
========

**slstatus** is a suckless and lightweight status monitor for window managers which use WM_NAME as statusbar (e.g. DWM). It is written in pure C without any system() calls and only reads from files most of the time. It is meant as a better alternative to Bash scripts (inefficient) and Conky (bloated for this use).

If you write a bash script that shows system information in WM_NAME, it executes a huge amount of external command (top, free etc.) every few seconds. This results in high system resource usage. slstatus solves this problem by only using C libraries and/or reading from files in sysfs / procfs.

Looking at the LOC (lines of code) in the [Conky project](https://github.com/brndnmtthws/conky) is very interesting: *28.346 lines C++, 219 lines Python and 110 lines Lua*. slstatus currently has about **500 lines of clean, well commented C code** and even includes additional possibilities as it can be customized and extended very easily. Configuring it by editing config.h (a C header file) is very secure and fast as no config files are parsed at runtime.

The following information is included:

- battery percentage
- cpu usage (in percent)
- date and time
- disk numbers (free storage, percentage, total storage and used storage)
- available entropy
- hostname
- ip addresses
- ram numbers (free ram, percentage, total ram and used ram)
- temperature
- volume percentage (alsa)
- wifi percentage

Multiple entries per function are supported and everything can be reordered and customized via the C header file config.h (similar to DWM).

![screenshot](screenshot.png)

## Usage

### Installation

Before you continue, please be sure that a C compiler, GNU make and `alsa-lib` (for volume percentage) are installed. Then copy config.def.h to config.h and edit it to your needs. Recompile and install it after every change via `sudo make install`! 

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
