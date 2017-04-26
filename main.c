/* main.c */

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

unsigned long createRGB ( const int r, const int g, const int b )
{   
    return ( ( r & 0xff ) << 16 ) + ( ( g & 0xff ) << 8 ) + ( b & 0xff );
}

uint32_t createRGBA (uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return ((r&0xff)<<24) + ((g&0xff)<<16) + ((b&0xff)<<8) + (a&0xff);
}

void corundAbort(const char * s, ...)
{
		va_list args;
		va_start(args, s);
		vfprintf(stderr, s, args);
		fprintf(stderr, "\n");
		va_end(args);
		abort();
}

typedef struct
{
	int Width;
	int Height;
	uint32_t ** RGBA;
} corundImage;

corundImage corundLoadPNG(corundString file_path)
{
	corundImage Image;

	int x, y;
	int width, height;
	png_byte color_type;
	png_byte bit_depth;

	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;
	png_bytep * row_pointers;

	char header[8];    // 8 is the maximum size that can be checked

	/* open file and test for it being a png */
	FILE *fp = fopen(file_path, "rb");
	if (!fp)
		corundAbort("[read_png_file] File %s could not be opened for reading", file_path);
	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8))
		corundAbort("[read_png_file] File %s is not recognized as a PNG file", file_path);


	/* initialize stuff */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
		corundAbort("[read_png_file] png_create_read_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		corundAbort("[read_png_file] png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		corundAbort("[read_png_file] Error during init_io");

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
			corundAbort("[read_png_file] Error during read_image");

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
	for (y=0; y<height; y++)
			row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));

	png_read_image(png_ptr, row_pointers);

	fclose(fp);

	if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB)
		corundAbort("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA (lacks the alpha channel)");

	if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGBA)
		corundAbort("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
		       PNG_COLOR_TYPE_RGBA, png_get_color_type(png_ptr, info_ptr));

	Image.Width = width;
	Image.Height = height;
	Image.RGBA = row_pointers;

	return Image;
}

void corundDrawImage(corundImage Image, int x, int y)
{
	int width, height;
	png_bytep * row_pointers;

	width = Image.Width;
	height = Image.Height;
	row_pointers = Image.RGBA;

	for (int pix_y=0; pix_y<height; pix_y++) {
		png_byte* row = row_pointers[pix_y];
		for (int pix_x=0; pix_x<width; pix_x++) {
			png_byte* ptr = &(row[pix_x*4]);
			unsigned long color = createRGB ( ptr[0], ptr[1], ptr[2]);
			XSetForeground(d, DefaultGC(d, s), color);
			XDrawPoint(d, w, DefaultGC(d, s), pix_x + x, pix_y + y);
		}
	}
}

typedef struct
{
	float x, y;//, z, w, u, v;
} Vertex;

typedef struct
{
	unsigned char r, g, b, a;
} Color;

typedef struct
{
	int Count;
	Vertex * Geometry;
} VertexBuffer;

typedef VertexBuffer corundMesh;

corundImage corundRenderMesh(corundMesh Mesh)
{
	corundImage Image;

	Image.Width = corundDefaultWidth;
	Image.Height = corundDefaultHeight;

	Image.RGBA = calloc(Image.Height, sizeof(uint32_t *));
	if (!Image.RGBA)
	{
		corundAbort("[Render] Not enough memory for framebuffer columns!");
	}

	for (int i = 0; i < Image.Height; i++)
	{
		Image.RGBA[i] = calloc(Image.Width, sizeof(uint32_t));
		if (!Image.RGBA[i])
		{
			corundAbort("[Render] Not enough memory for framebuffer rows!");
		}
	}

	for (int i = 0; i < Mesh.Count; i+=3)
	{
		/*create triangle*/
		Vertex Triangle[3] = {Mesh.Geometry[i], Mesh.Geometry[i+1], Mesh.Geometry[i+2]};

		/*rasterize triangle (x, y, z, w) => (x, y)*/
		/* sort vertices by vertical axis */
		if (Triangle[1].y < Triangle[0].y)
		{
			Vertex Swap = Triangle[0];
			Triangle[0] = Triangle[1];
			Triangle[1] = Swap;
		}

		if (Triangle[2].y < Triangle[0].y)
		{
			Vertex Swap = Triangle[0];
			Triangle[0] = Triangle[2];
			Triangle[2] = Swap;
		}

		if (Triangle[2].y < Triangle[1].y)
		{
			Vertex Swap = Triangle[1];
			Triangle[1] = Triangle[2];
			Triangle[2] = Swap;
		}

		/*upper part*/
		int ydiff = (int)Triangle[1].y - Triangle[0].y;

		int xdiff1 = (int)Triangle[0].x - Triangle[1].x;
		int xdiff2 = (int)Triangle[0].x - Triangle[2].x;

		int xstep1 = xdiff1/ydiff; 		
		int xstep2 = xdiff2/ydiff;

		for (int y = 0; y < ydiff; y++)
		{
			int x1 = y * xstep1;
			int x2 = y * xstep2;

			if (x1 > x2)
			{
				int swap = x1;
				x1 = x2;
				x2 = swap;
			}

			for (int x = x1; x <= x2; x++)
			{
				int yreal = Triangle[0].y + y;
				Image.RGBA[y][x] = createRGBA(255, 0, 0, 255);
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

corundImage Image;

int corundRun(void)
{
	XNextEvent(d, &e);

	if (e.type == Expose) {return 1;}

	if (e.type == KeyPress) {return 0;}

	return 1;
}

int main(void)
{
	corundInit(corundDefaultWidth, corundDefaultHeight);

	Image = corundLoadPNG("test.png");

	Vertex v1 = {100, 100};
	Vertex v2 = {100, 400};
	Vertex v3 = {400, 400};

	Vertex * Geometry = calloc(3, sizeof(Vertex));
	Geometry[0] = v1;
	Geometry[1] = v2;
	Geometry[2] = v3;

	corundMesh Mesh = {3, Geometry};

	corundImage Render = corundRenderMesh(Mesh);

	while (corundRun()) {
		corundDrawImage(Render, 1, 1);
	};

	XCloseDisplay(d);

	return EXIT_SUCCESS;
}
