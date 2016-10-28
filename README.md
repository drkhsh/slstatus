![slstatus](slstatus.png)

**slstatus** is a suckless and lightweight status monitor for window managers that use WM_NAME as statusbar (e.g. DWM) or any other status bars if they support reading/piping from slstatus. It is written in pure C without any extern programs being executed and only reads from files most of the time. slstatus is meant to be a better alternative to Bash scripts (inefficient) and Conky (bloated and written in C++).

If you write a bash script that shows system information in WM_NAME (or any other status bar), it executes a huge amount of external commands (top, free etc.) every few seconds. This results in high system resource usage. slstatus solves this problem by only using C libraries and/or reading from files in sysfs/procfs most of the time.

Looking at the LOC (lines of code) of the [Conky project](https://github.com/brndnmtthws/conky), it is very interesting: *28346 lines C++, 219 lines Python and 110 lines Lua*. slstatus currently has about **800 lines of clean documented C code** and even includes additional possibilities as it can be customized and extended very easily. Configuring it by customizing the config.h (C header) file is very secure and fast as no config files are parsed at runtime.

The following information is included:

- Battery percentage/state
- CPU usage (in percent)
- Custom shell commands
- Date and time
- Disk[s] status (free storage, percentage, total storage and used storage)
- Available entropy
- Username/GID/UID
- Hostname
- IP addresses
- Load averages
- Memory status (free memory, percentage, total memory and used memory)
- Swap status (free swap, percentage, total swap and used swap)
- Temperature
- Uptime
- Volume percentage (ALSA)
- WiFi signal percentage and ESSID

Multiple entries per function (e.g. multiple batteries) are supported and everything can be reordered and customized via a C header file (similar to other suckless programs).

## Usage

### Installation

Be sure you satisfy the dependencies: X11 and optionally ALSA (for volume percentage, I will *not* support PulseAudio).
Also you should have basic development tools like a C compiler and GNU make installed.
Then copy config.def.h to config.h and customize it to your needs.
(Re)Compile (and install) it (after modifications):

	$ make clean all
	# make install

### Starting

If you use DWM or any other window manager that uses WM_NAME, write the following code to ~/.xinitrc (or any other initialization script) to start slstatus automatically:

	slstatus -d

If you use any other status bar or window manager you will have to figure it out yourself. Something like this could fit your requirements:

	slstatus -o | other_status_bar &

## Contributing

Hunt FIXME's in the code or do WTF you want! If it is useful, I will merge.

People who contributed are listed in [CONTRIBUTORS.md](CONTRIBUTORS.md). Please add yourself to this list afterwards.

For detailed information about coding style and restrictions see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

See [LICENSE](LICENSE).
