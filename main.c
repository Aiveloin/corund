/*!
	\file
	\brief Main C code file for corund.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <X11/Xlib.h>

#define PNG_DEBUG 3
#include <png.h>

static Display *d;
static Window w;
static XEvent e;
static int s;

const int corundDefaultWidth = 640;
const int corundDefaultHeight = 480;

typedef char * corundString;

/*
	Я хочу, чтобы пикселы в памяти хранились в виде RGBA, 32-битная структура.
	На х86 тип unsigned имеет обратный порядок байтов ABGR, поэтому записывать
	и читать данные через unsigned нельзя. Но и не нужно - структуру можно
	присваивать ничуть не хуже, а доступ через struct.byte[i] или struct.r 
	вполне удобен. Понадобится ли арифметика с цветом (сложение, вычитание,
	умножение) над всеми полями одновременно? Вряд ли, и тогда порядок 
	байтов не будет иметь значения.
*/

typedef union {
	uint32_t color; // ABGR on x86
	uint8_t byte[4];
	struct {
		uint8_t r, g, b, a;
	};
} rgba_t;

/*
	Запись "int *var" показывает, какой тип будет иметь выражение "*var"
*/

typedef struct {
	unsigned Width, Height;
	rgba_t *Pixel; // for bliting
} image_t;

void corundAbort(const char * s, ...)
{
	va_list args;
	va_start(args, s);
	vfprintf(stderr, s, args);
	fprintf(stderr, "\n");
	va_end(args);
	abort();
}

image_t corundLoadPNG(corundString file_path)
{
	image_t Image;
	FILE *fp = fopen(file_path, "rb");
	if (!fp)
	{
		corundAbort("File %s could not be opened for reading", file_path);
	}
	const unsigned number = 8; // 8 is the maximum size that can be checked
	unsigned char header[number];
	if (fread(header, 1, number, fp) != number)
	{
		corundAbort("Cannot read file %s signature", file_path);
	}
	const int is_png = !png_sig_cmp(header, 0, number);
	if (!is_png)
	{
		corundAbort("File %s is not recognized as a PNG file", file_path);
	}
	// NULL, NULL, NULL => no user err var, no user err callback, no warning callback
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		corundAbort("png_create_read_struct failed");
	}
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		corundAbort("png create info_ptr failed");
	}
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		corundAbort("png create end_info failed");
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		fclose(fp);
		corundAbort("Error during init_io");
	}
	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, number);
	/* TODO: handle progressive PNG (see manual). */
	png_read_info(png_ptr, info_ptr);
	uint32_t width, height, color_type, bit_depth;
	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	if (color_type != PNG_COLOR_TYPE_RGBA)
	{
		corundAbort("Color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
		             PNG_COLOR_TYPE_RGBA, color_type);
		// обработать RGB
		// обработать palette/grayscale
	}
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	if (bit_depth != 8)
	{
		corundAbort("Bit depth of input file must be 8 bit per channel (is %d)", bit_depth);
		// обработать packed (1, 2, 4 bit)
		// обработать 16 bit
	}
	/* Handle interlacing */
	uint32_t number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);
	png_bytep row_pointers[height];
	rgba_t * pixels = calloc(width*height, sizeof(rgba_t));
	if (!pixels)
	{
		corundAbort("Cannot alloc enough memory for image");
	}
	for (uint32_t y = 0; y < height; ++y)
	{
		row_pointers[y] = (png_bytep) &pixels[width*y];
	}
	png_set_bgr(png_ptr);
	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	Image.Width = width;
	Image.Height = height;
	Image.Pixel = pixels;
	return Image;
}

void corundFreeImage(image_t Image)
{
	free(Image.Pixel);
}

void corundDrawImage(image_t Image, int x, int y)
{
	int width, height;
	width = Image.Width;
	height = Image.Height;
	for (int pix_y=0; pix_y<height; pix_y++) {
		for (int pix_x=0; pix_x<width; pix_x++) {
			unsigned long color = Image.Pixel[width*pix_y+pix_x].color;
			XSetForeground(d, DefaultGC(d, s), color);
			XDrawPoint(d, w, DefaultGC(d, s), pix_x + x, pix_y + y);
		}
	}
}

typedef struct
{
	float x, y; //, z, w, u, v;
} Vertex;

typedef struct
{
	int Count;
	Vertex * Geometry;
} VertexBuffer;

typedef VertexBuffer corundMesh;

image_t corundRenderMesh(corundMesh Mesh)
{
	image_t Image;

	Image.Width = corundDefaultWidth;
	Image.Height = corundDefaultHeight;
	Image.Pixel = calloc(Image.Height*Image.Width, sizeof(rgba_t));

	if(!Image.Pixel) {
		corundAbort("Cannot alloc framebuffer!");
	}

	for (int i = 0; i < Mesh.Count; i+=3) { // loop over tris
		/*build triangle*/
		Vertex Triangle[3] = {Mesh.Geometry[i], Mesh.Geometry[i+1], Mesh.Geometry[i+2]};

		/*
			vertex3d: scene space (float)
			vertex2d: screen space (float)
			fragment: screen space (float)
			pixel: bitmap space (int)
			color: memory space (int)
		*/
		/*rasterize triangle (x, y, z, w) => (x, y)*/

		/* sort vertices by vertical axis */
		if (Triangle[1].y < Triangle[0].y) {
			Vertex Swap = Triangle[0];
			Triangle[0] = Triangle[1];
			Triangle[1] = Swap;
		}
		if (Triangle[2].y < Triangle[0].y) {
			Vertex Swap = Triangle[0];
			Triangle[0] = Triangle[2];
			Triangle[2] = Swap;
		}
		if (Triangle[2].y < Triangle[1].y) {
			Vertex Swap = Triangle[1];
			Triangle[1] = Triangle[2];
			Triangle[2] = Swap;
		}

		/*upper part*/
		float ydiff = Triangle[1].y - Triangle[0].y;

		float xdiff1 = Triangle[1].x - Triangle[0].x;
		float xdiff2 = Triangle[2].x - Triangle[0].x;

		float xstep1 = xdiff1/ydiff;
		float xstep2 = xdiff2/ydiff;

		for (int y = 0; y < ydiff; y++) { // go along y-axis
			float x1 = y * xstep1; // left
			float x2 = y * xstep2; // right

			if (x1 > x2) {
				float swap = x1;
				x1 = x2;
				x2 = swap;
			}

			for (int x = x1; x <= x2; x++) { // go along x-axis
				float yreal = Triangle[0].y + y;
				rgba_t color;
				color.r = 255;
				color.g = 255;
				color.b = 255;
				int offset = (int)yreal* Image.Width + x;
				Image.Pixel[offset] = color;
			}
		}

		/*per-fragment*/
		/*interpolate u, v, color, etc.*/
		/*blend color to framebuffer*/
		/*lower part*/
		/*per-fragment*/
		/*interpolate u, v, color, etc.*/
		/*blend color to framebuffer*/
	}
	return Image;
}

void corundInit(int Width, int Height)
{
	d = XOpenDisplay(NULL);
	if (d == NULL) {
		corundAbort("[X11] Cannot open display: XOpenDisplay(NULL) returned NULL.");
	}
	s = DefaultScreen(d);
	w = XCreateSimpleWindow(d, RootWindow(d, s), 10, 10, Width, Height, 1, BlackPixel(d, s), WhitePixel(d, s));
	XSelectInput(d, w, ExposureMask | KeyPressMask | ButtonPressMask);
	XMapWindow(d, w);
}

int corundRun(void)
{
	XNextEvent(d, &e);
	if (e.type == Expose) {return 1;}
	if (e.type == KeyPress) {return 0;}
	return 1;
}

int main(void)
{
	/* TODO: test PNG features */
	/* TODO: test GCC features */

	/* coords in screenspace - transform not done yet */
	Vertex v1 = {0, 0};
	Vertex v2 = {100, 100};
	Vertex v3 = {400, 400};

	Vertex * Geometry = calloc(3, sizeof(Vertex));

	Geometry[0] = v1;
	Geometry[1] = v2;
	Geometry[2] = v3;

	corundMesh Mesh = {3, Geometry};

	image_t Render = corundRenderMesh(Mesh);
	free(Geometry);

	corundInit(corundDefaultWidth, corundDefaultHeight);

	while (corundRun()) {
		corundDrawImage(Render, 0, 0);
	};

	corundFreeImage(Render);
	XCloseDisplay(d);
	return EXIT_SUCCESS;
}

