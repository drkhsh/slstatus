# slstatus

A suckless status monitor for DWM written in pure C without any system() calls.

# Information

- wifi percentage
- battery percentage
- cpu usage in percent
- cpu temperature
- ram usage in percent
- alsa volume level in percent
- date
- time

# Screenshot

![screenshot](screenshot.png)

# Installation

Just run ```sudo make install```!

# Configuration

Just edit config.h and recompile!

# Starting

Put this in your ~/.xinitrc:

```
while true; do
    slstatus
done &
```

# Contributing

See TODO.md for things that you could do.

People who contributed are listed in the CONTRIBUTORS.md file.

If you want to contribute, please use [the suckless coding style](http://suckless.org/coding_style)! For indentation please use 4 spaces.
