/* main.c */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <X11/Xlib.h>

#define PNG_DEBUG 3
#include <png.h>

unsigned long createRGB ( const int r, const int g, const int b )
{   
    return ( ( r & 0xff ) << 16 ) + ( ( g & 0xff ) << 8 ) + ( b & 0xff );
}

void abort_(const char * s, ...)
{
		va_list args;
		va_start(args, s);
		vfprintf(stderr, s, args);
		fprintf(stderr, "\n");
		va_end(args);
		abort();
}

int x, y;

int width, height;
png_byte color_type;
png_byte bit_depth;

png_structp png_ptr;
png_infop info_ptr;
int number_of_passes;
png_bytep * row_pointers;

void read_png_file(char* file_name)
{
		char header[8];    // 8 is the maximum size that can be checked

		/* open file and test for it being a png */
		FILE *fp = fopen(file_name, "rb");
		if (!fp)
				abort_("[read_png_file] File %s could not be opened for reading", file_name);
		fread(header, 1, 8, fp);
		if (png_sig_cmp(header, 0, 8))
				abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);


		/* initialize stuff */
		png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

		if (!png_ptr)
				abort_("[read_png_file] png_create_read_struct failed");

		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
				abort_("[read_png_file] png_create_info_struct failed");

		if (setjmp(png_jmpbuf(png_ptr)))
				abort_("[read_png_file] Error during init_io");

		png_init_io(png_ptr, fp);
		png_set_sig_bytes(png_ptr, 8);

		png_read_info(png_ptr, info_ptr);

		width = png_get_image_width(png_ptr, info_ptr);
		height = png_get_image_height(png_ptr, info_ptr);
		color_type = png_get_color_type(png_ptr, info_ptr);
		bit_depth = png_get_bit_depth(png_ptr, info_ptr);

		number_of_passes = png_set_interlace_handling(png_ptr);
		png_read_update_info(png_ptr, info_ptr);


		/* read file */
		if (setjmp(png_jmpbuf(png_ptr)))
				abort_("[read_png_file] Error during read_image");

		row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
		for (y=0; y<height; y++)
				row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));

		png_read_image(png_ptr, row_pointers);

		fclose(fp);
}

int main(void)
{
	Display *d;
	Window w;
	XEvent e;
	char *msg = "Hello, World!";
	int s;

	read_png_file("test.png");

	if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB)
		abort_("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA (lacks the alpha channel)");

	if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGBA)
		abort_("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
		       PNG_COLOR_TYPE_RGBA, png_get_color_type(png_ptr, info_ptr));

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
			for (y=0; y<height; y++) {
				png_byte* row = row_pointers[y];
				for (x=0; x<width; x++) {
					png_byte* ptr = &(row[x*4]);
					printf("Pixel at position [ %d - %d ] has RGBA values: %d - %d - %d - %d\n", x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
					unsigned long color = createRGB ( ptr[0], ptr[1], ptr[2]);
					XSetForeground(d, DefaultGC(d, s), color);
					XDrawPoint(d, w, DefaultGC(d, s), x, y);
				}
			}
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
