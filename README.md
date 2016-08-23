slstatus
========

**slstatus** is a suckless and lightweight status monitor for window managers that use WM_NAME as statusbar (e.g. DWM). It is written in pure C without any system calls and only reads from files most of the time. It is meant to be a better alternative to Bash scripts (inefficient) and Conky (bloated for this use).

If you write a bash script that shows system information in WM_NAME, it executes a huge amount of external command (top, free etc.) every few seconds. This results in high system resource usage. slstatus solves this problem by only using C libraries and/or reading from files in sysfs/procfs.

Looking at the LOC (lines of code) of the [Conky project](https://github.com/brndnmtthws/conky), very interesting: *28.346 lines C++, 219 lines Python and 110 lines Lua*. slstatus currently has about **800 lines of clean documented C code** and even includes additional possibilities as it can be customized and extended very easily. Configure it by customizing the config.h (C header file) which is secure and fast as no config files are parsed at runtime.

The following information is included:

- Battery percentage
- CPU usage (in percent)
- Custom shell commands
- Date and time
- Disk[s] status (free storage, percentage, total storage and used storage)
- Available entropy
- username/gid/uid
- Hostname
- IP addresses
- Load average
- Memory status (free memory, percentage, total memory and used memory)
- Temperature
- Uptime
- Volume percentage + mute status (alsa)
- WiFi signal percentage and essid

Multiple entries per function are supported and everything can be reordered and customized via the C header file config.h (similar to DWM).

## Usage

### Installation

Before you continue, please be sure that a C compiler (preferrably gcc), GNU make and `alsa-lib` (for volume percentage) are installed. Then copy config.def.h to config.h and customize it to fit your needs. Recompile and install it after modifications:

	$ make clean all
	# make install

### Starting

Write the following code in your ~/.xinitrc (or any other initialization script):

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
