
#define FPGA_PIXEL_BUF_BASE		0x08000000
#define FPGA_PIXEL_BUF_END		0x0803FFFF
#define VGA_CONTROLLER_BASE 	0xff203020
#define NUM_LINES 				8
#define NUM_PIXELS_IN_SCREEN 	76800 // 320px by 240px

#define MAX_X 320
#define MAX_Y 240

// TODO: Move these Prototypes to header file
// Utility Function Prototypes
int vgaSetup(void);
void swap(int*, int*);
int abs(int);
void waitForVsync();
short int hueToRGB565(float);

// Drawing Function Prototpes
void drawIndividualPixel(int, int, short int);
void drawBresenhamLine(int, int, int, int, short int);
void drawBox(int, int, short int);
void clearWholeScreen();
void tracebackErase();

// Struct and and 2D array of them that will define the lines we erase in
// our traceback erase.
typedef struct nonblackLine {
	int x0;
	int y0;
	int x1;
	int y1;
} nonblackLine;
typedef struct nonblackPixel {
	int x;
	int y;
} nonblackPixel;
