#include <X11/Xlib.h>

int
main(void)
{
	Display *dpy = XOpenDisplay(NULL);
	XStoreName(dpy, DefaultRootWindow(dpy), NULL);
	XCloseDisplay(dpy);
	return (0);
}
