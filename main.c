/* main.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

int main(void)
{
	Display *d;
	Window w;
	XEvent e;
	char *msg = "Hello, World!";
	int s;

	d = XOpenDisplay(NULL);
	if (d == NULL) {
		fprintf(stderr, "Cannot open display\n");
		exit(1);
	}

	s = DefaultScreen(d);
	w = XCreateSimpleWindow(d, RootWindow(d, s), 10, 10, 100, 100, 1, BlackPixel(d, s), WhitePixel(d, s));
	XSelectInput(d, w, ExposureMask | KeyPressMask | ButtonPressMask);
	XMapWindow(d, w);

	while (1) {
		XNextEvent(d, &e);
		if (e.type == Expose) {
			XFillRectangle(d, w, DefaultGC(d, s), 20, 20, 10, 10);
			XDrawString(d, w, DefaultGC(d, s), 10, 50, msg, strlen(msg));
		}
		if (e.type == KeyPress)
			break;
		if (e.type == ButtonPress && e.xbutton.button == Button1 
			  && e.xbutton.x >=20 && e.xbutton.x <= 30 && e.xbutton.y >= 20 
			  && e.xbutton.y <= 30)
			break;
	}

	XCloseDisplay(d);

	return EXIT_SUCCESS;
}
