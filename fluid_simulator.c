
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

// =======================================================================================================
//                                              DISPLAY UTILS
// =======================================================================================================

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

    if (hue<0.0) hue = 0.0;
    if (hue>1.0) hue = 1.0;
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

// Draws just one pixel to the appropriate frame buffer.
void drawIndividualPixel(int x, int y, short int colour){
	volatile short int *pixelAddress;
	pixelAddress = (volatile short int *) (CURRENT_BACK_BUFFER_ADDRESS + (y << 10) + (x << 1)); 
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

// =======================================================================================================
//                                          FLUID SIMULATION UTILS
// =======================================================================================================

#define NUM_PARTICLES       48

#define WATER_COLOUR        27743
#define WATER_HUE           0.62
#define BLACK               0
#define PRESSURE_COLUMNS    24
#define PRESSURE_ROWS       32

#define G                   10.0
#define PARTICLE_MASS       1
#define SPF                 0.2 // Seconds Per Frame
#define ELASTICITY          0.9 // 0 to 1
#define VELOCITY_COLOUR_SENSITIVITY 200.0
#define VISCOSITY           1
#define RHO_NAUGHT          1.0/(1000.0*(float)NUM_PARTICLES)

int h; // Spacing parameter between fluids in the simulation

typedef struct Particle {

    float x, y;
    float vx, vy;
    float ax, ay;
    float pressure, density;
    int neighbours[NUM_PARTICLES];
    short int colour;

} Particle;

typedef struct PressureGridCell {

    float pressure;
    int particleCount;
    int beginX, endX;
    int beginY, endY;

} PressureGridCell;

Particle allParticles[NUM_PARTICLES];

void initParticles() {

    double x = (double)NUM_PARTICLES*(double)MAX_Y/(double)MAX_X;
    int amtRows = ceil(sqrt(ceil(x)));
    int amtColumns = ceil((double)amtRows*(double)MAX_X/(double)MAX_Y);

    int stepX = MAX_X/amtColumns;
    int stepY = MAX_Y/amtRows;
    int initX = stepX/2;
    int initY = stepY/2;

    h = (stepX + stepY) / 2;

    int xStepCount = 0;
    int yStepCount = 0;

    for (int i = 0; i < NUM_PARTICLES; i++) {

        if(xStepCount >= amtColumns) {
            xStepCount = 0;
            yStepCount++;
        }
        if(yStepCount >= amtRows) {
            yStepCount = 0;
        }
        allParticles[i].x = initX + xStepCount*stepX;
        allParticles[i].y = initY + yStepCount*stepY;
        xStepCount++;

        allParticles[i].colour = WATER_HUE;
    }
}

void eraseParticles() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        drawIndividualPixel(allParticles[i].x, allParticles[i].y, BLACK);
    }
}
void drawParticles() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        drawIndividualPixel(allParticles[i].x, allParticles[i].y, allParticles[i].colour);
    }
}

// Reference:
// https://cg.informatik.uni-freiburg.de/course_notes/sim_10_sph.pdf

void updateParticlePositions() {

    bool currentIsNull = false;
    float tempX, tempY;
    float referenceX, referenceY;
    for (int i=0; i<NUM_PARTICLES; i++){
        currentIsNull = !(int)allParticles[i].vx && !(int)allParticles[i].vy;
        referenceX = allParticles[i].x - allParticles[i].vx*SPF;
        referenceY = allParticles[i].y - allParticles[i].vy*SPF;
        // Apply Collision Mechanics (upper triangular manner is all that is merited -> O(n^2)/2)
        for (int j = i + 1; j < NUM_PARTICLES; j++) {

            if ((allParticles[i].x != allParticles[j].x) || (allParticles[i].y != allParticles[j].y)) continue; // cont. if pos is diff
            if(currentIsNull && !(int)allParticles[j].vx && !(int)allParticles[j].vy) continue; // cont. if velocities are zero

            // For all parties involved in this collision:
            // Remove Previous Velocity Application 
            allParticles[i].x -= allParticles[i].vx*SPF;
            allParticles[i].y -= allParticles[i].vy*SPF;
            allParticles[j].x -= allParticles[j].vx*SPF;
            allParticles[j].y -= allParticles[j].vy*SPF;
            // Set New Velocity
            tempX = allParticles[i].vx;
            tempY = allParticles[i].vy;
            allParticles[i].vx = allParticles[j].vx*ELASTICITY;
            allParticles[i].vy = allParticles[j].vy*ELASTICITY;
            allParticles[j].vx = tempX*ELASTICITY;
            allParticles[j].vy = tempY*ELASTICITY;
            // Apply New Velocity
            allParticles[i].x += allParticles[i].vx*SPF;
            allParticles[i].y += allParticles[i].vy*SPF;
            allParticles[j].x += allParticles[j].vx*SPF;
            allParticles[j].y += allParticles[j].vy*SPF;

            // If our change didnt do anything:
            if ((allParticles[i].x == allParticles[j].x) && (allParticles[i].y == allParticles[j].y)) {
                // Remove Previous Velocity Application (Again)
                allParticles[i].x -= allParticles[i].vx*SPF;
                allParticles[i].y -= allParticles[i].vy*SPF;
                allParticles[j].x -= allParticles[j].vx*SPF;
                allParticles[j].y -= allParticles[j].vy*SPF;
            }

        }

        // If, for whatever reason, we went out of bounds after velocity application, fix them manually.
        if (allParticles[i].x < 0 || allParticles[i].x > (MAX_X-1)) {
            if (allParticles[i].x < 0){
                allParticles[i].x = 0;
            } else {
                allParticles[i].x = MAX_X-1;
            }
        }
        if (allParticles[i].y < 0 || allParticles[i].y > (MAX_Y-1)) {
            if (allParticles[i].y < 0){
                allParticles[i].y = 0;
            } else {
                allParticles[i].y = MAX_Y-1;
            }
        }
    }
    
}

void applyForcesToParticles() {

    for (int i = 0; i < NUM_PARTICLES; i++) {

        // *********************************************** UPDATE PARTICLE VELOCITIES *********************************************************

        // Apply Gravity
        if(allParticles[i].y > 0 && allParticles[i].y < (MAX_Y-1)) {
            //allParticles[i].vy += G*SPF;
        } else {
            allParticles[i].vy = -allParticles[i].vy*ELASTICITY;
        }
        allParticles[i].vy += G*SPF;

        // Apply Navier Forces 

        // Update Colours
        allParticles[i].colour = hueToRGB565(WATER_HUE-sqrt(allParticles[i].vx*allParticles[i].vx + allParticles[i].vy*allParticles[i].vy)/VELOCITY_COLOUR_SENSITIVITY);

        // *********************************************** UPDATE PARTICLE POSITIONS *********************************************************
        // updateParticlePosition(i);
        allParticles[i].x += allParticles[i].vx*SPF;
        allParticles[i].y += allParticles[i].vy*SPF;

    }

}

void timeStepSPHApproximation() {

    // Find neighbours j
    for (int i = 0; i < NUM_PARTICLES; i++) {

    }

}

// int simulateFluid(void){ // main for this simulation
int main(void){ // main for this simulation

    initParticles();
    vgaSetup();

    // Program loop
    while(1) {

        waitForVsync();
        drawParticles();
        waitForVsync();
        eraseParticles();

        applyForcesToParticles();
        updateParticlePositions();

    }

    return 0;

}
