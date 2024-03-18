/*
*	This contains utilities to correctly draw to the frame buffer.
*/

#include <stdbool.h>
#include <stdlib.h>

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

// Struct to hold the physics-related data
typedef struct physicalState {
	int positionX;
	int positionY;
	int velocityX;
	int velocityY;
} physicalState;

// Allocate an array of lines with max size of NUM_LINES
nonblackLine nonblackLines [NUM_LINES];
nonblackPixel nonblackPixels [NUM_LINES];
physicalState physicalStates [NUM_LINES];

// Global telling us the starting address of the Pixel Buffer
int CURRENT_BACK_BUFFER_ADDRESS;

// Setup the vga Display for drawing to the back buffer.
int vgaSetup(void) {
	
	waitForVsync();

	volatile int *vgaCtlPtr = (volatile int *)VGA_CONTROLLER_BASE;
	CURRENT_BACK_BUFFER_ADDRESS = *vgaCtlPtr;
	
	clearWholeScreen();
	
	return 0;
	
}

// Function to convert a hue to an R(5)G(6)B(5) bit scheme
short int hueToRGB565(float hue) {

    // Declare variables to store red, green, and blue components (initially floats for calculations)
    float r, g, b;

    // Calculate the sector of the color wheel (shown in the case statement below)
    int sector = floor(hue * 6);

    // Calculate the fractional part to transition
    float f = hue * 6 - sector;
    // Invert the fractional part in some cases
    float nf = 1 - f;

    // Determine the RGB values based on the sector
    switch (sector) {
        case 0:  // Red
            r = 1.0; g = f; b = 0.0; break;
        case 1:  // Red -> Green
            r = nf; g = 1.0; b = 0.0; break;
        case 2:  // Green
            r = 0.0; g = 1.0; b = f; break;
        case 3:  // Green -> Blue
            r = 0.0; g = nf; b = 1.0; break;
        case 4:  // Blue
            r = f; g = 0.0; b = 1.0; break;
        default: // Blue -> Red
            r = 1.0; g = 0.0; b = nf; break;
    }

    // Scale the RGB values to fit 5, 6, 5 bit col channels
    r *= 31;
    g *= 63;
    b *= 31;

    // Combine the RGB values into a single RGB565 value
    return ((int)r << 11) | ((int)g << 5) | (int)b;
}

// Finds the absolue value of an int
int abs(int in){
	if (in>0) return in;
	return (0-in);
}

// Swaps two ints
void swap(int *a, int*b){
	int temp = *a;
	*a = *b;
	*b = temp;
}

void waitForVsync(){

	volatile int *vgaCtlPtr = (volatile int*)VGA_CONTROLLER_BASE;
	*vgaCtlPtr = 1; // 1->Front Buffer Address. Kickstarts our swap/rendering process
	
	// Poll status bit for a 0
	while ((*(vgaCtlPtr + 3) & 0x01)!=0);
		
}

// Erase all the lines and boxes we drew
void tracebackErase(){
	
	for (int lineIndex = 0; lineIndex < NUM_LINES; lineIndex++){
		nonblackLine cLine = nonblackLines[lineIndex];
		nonblackPixel cPx = nonblackPixels[lineIndex];
		drawBresenhamLine(cLine.x0, cLine.y0, cLine.x1, cLine.y1, 0);
		drawBox(cPx.x, cPx.y, 0);
	}
	
}

// Draws just one pixel to the appropriate frame buffer.
void drawIndividualPixel(int x, int y, short int colour){
	volatile short int *pixelAddress;
	pixelAddress = (int *) (CURRENT_BACK_BUFFER_ADDRESS + (y << 10) + (x << 1)); 
	*pixelAddress = colour;
}

// Writes black to every pixel in the pixel buffer
void clearWholeScreen(){
	
	for(int x = 0; x < MAX_X; x++){ // 320px
		for(int y = 0; y < MAX_Y; y++) { // by 240px
			drawIndividualPixel(x, y, 0); // draw a black pixel
		}
	}

}

// Draws a nxn box centered at the pixel x,y
void drawBox(int x, int y, short int colour){
	int n = 3;
	int shift = floor(n/2);
	for(int i = 0; i < n; i++){
		for(int j = 0; j < n; j++){
			drawIndividualPixel(x+(i-shift), y+(j-shift), colour);
		}
	}
}
// Draws a line between the two points specified on screen. 
void drawBresenhamLine(int x0, int y0, int x1, int y1, short int colour){
	
	bool isSteep = abs(x0-x1) < abs(y0-y1);
	
	if(isSteep){
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	if(x0>x1){
		swap(&x0, &x1);
		swap(&y0, &y1);
	}
	
	int dx = x1 - x0;
	int dy = abs(y1 - y0);
	int error = -dx/2;
	
	int moveY = y1>y0 ? 1 : -1;
	
	int y = y0;
	int x = x0;
	
	while(x<=x1) {
		
		if (isSteep) drawIndividualPixel(y,x, colour);
		else drawIndividualPixel(x,y,colour);
		
		error = error + dy;
		if (error > 0){
			y = y + moveY;
			error = error - dx;
		}
		
		x++;
		
	}

}
