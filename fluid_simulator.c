
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

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
//                                               MOUSE DRIVER
// =======================================================================================================

#define PS2_BASE        0xFF200100
#define MOUSE_RADIUS    2

typedef struct mouseData {
  int x;
  int y;
  bool left, right, middle;
} mouseData;

void clearFIFO() {
  volatile int *PS2_ptr = (int *)PS2_BASE;
  int PS2_data = *PS2_ptr;
  int RAVAIL = PS2_data>>16;
  while(RAVAIL > 0){
    PS2_data = *PS2_ptr;
    RAVAIL = PS2_data>>16;
  }
}

void updateMouse(mouseData *data) {
  volatile int *PS2_ptr = (int *)PS2_BASE;
  int PS2_data, RVALID;
  char inputData;
  PS2_data = *(PS2_ptr);  // read the Data register in the PS/2 port
  inputData = PS2_data & 0xFF;
  RVALID = PS2_data & 0x8000;  // extract the RVALID field
  if (RVALID) {
    data->left = inputData & 1;
    data->middle = inputData & 4;
    data->right = inputData & 2;

    PS2_data = *(PS2_ptr);
    inputData = PS2_data & 0xFF;
    data->x += (2*inputData);

    PS2_data = *(PS2_ptr);
    inputData = PS2_data & 0xFF;
    data->y += (2*inputData);

    if (data->x >= MAX_X) data->x = MAX_X - 1;
    if (data->y >= MAX_Y) data->y = MAX_Y - 1;

    if (data->x < 0) data->x = 0;
    if (data->y < 0) data->y = 0;
  }
  clearFIFO();
}

void drawMouse(mouseData *data, short int colour) {
    
    int x = data -> x;
    int y = data -> y;

    if(x<MOUSE_RADIUS) x=MOUSE_RADIUS;
    else if (x>MAX_X-1-MOUSE_RADIUS) x = MAX_X-1-MOUSE_RADIUS;
    if(y<MOUSE_RADIUS) y=MOUSE_RADIUS;
    else if (y>MAX_Y-1-MOUSE_RADIUS) y = MAX_Y-1-MOUSE_RADIUS;

    for(int i = -1; i<2; i++) {
        drawIndividualPixel(x + i, y + MOUSE_RADIUS, colour);
        drawIndividualPixel(x + i, y - MOUSE_RADIUS, colour);
        drawIndividualPixel(x + MOUSE_RADIUS, y + i, colour);
        drawIndividualPixel(x - MOUSE_RADIUS, y + i, colour);
    }

}

void intializeMouse(mouseData *data) {
  volatile int *PS2_ptr = (int *)PS2_BASE;
  *(PS2_ptr) = 0xFF;  // reset

  data->x = MAX_X / 2;
  data->y = MAX_Y / 2;

  data->left = false;
  data->middle = false;
  data->right = false;
}

// =======================================================================================================
//                                          FLUID SIMULATION UTILS
// =======================================================================================================

// -g -Wall -O1 -ffunction-sections -fverbose-asm -fno-inline -mno-cache-volatile -mhw-div -mcustom-fpu-cfg=60-2 -mhw-mul -mhw-mulx

#define NUM_PARTICLES       48 // 192, 48, 12

#define WATER_COLOUR        27743
#define WATER_HUE           0.62
#define BLACK               0
#define WHITE               0xFFFF

#define PRESSURE_COLUMNS    24
#define PRESSURE_ROWS       32

#define G                   9.81
#define K                   20.0
#define PARTICLE_MASS       1
#define SPF                 0.01 // Seconds Per Frame
#define ELASTICITY          0.3 // 0 to 1
#define VELOCITY_COLOUR_SENSITIVITY 50.0
#define VISCOSITY           0.2
#define ROOT_TWO_SCALE      1.42

#define TUG_ACCELERATION    15.0
#define TUG_VELOCITY        0.001

#define M_PER_PX            0.013333333
#define PX_PER_M            75.0

#define DENSITY_RESTING     200.0
	
float h; // Spacing parameter between fluids in the simulation
int hpx; // h but in px
float inv_rho_naught;
float nu; // used for viscosity related acceleration
float alpha; // Cubic Bezier Constant for W_ij calc

typedef struct drawParticle {

    int x, y;

} drawParticle;

typedef struct Particle {

    int x, y;
    float pX, pY;
    float vx, vy;
    float ax, ay;
    float pressure, density;
    bool neighbours[NUM_PARTICLES];
    float gradQ[NUM_PARTICLES];
    float neighbourDXs[NUM_PARTICLES];
    float neighbourDYs[NUM_PARTICLES];
    float neighbourDistances[NUM_PARTICLES];
    short int colour;

} Particle;

Particle allParticles[NUM_PARTICLES];
drawParticle allEraseParticles[NUM_PARTICLES];

void initParticles() {

    double x = (double)NUM_PARTICLES*(double)MAX_Y/(double)MAX_X;
    int amtRows = ceil(sqrt(ceil(x)));
    int amtColumns = ceil((double)amtRows*(double)MAX_X/(double)MAX_Y);

    int stepX = MAX_X/amtColumns;
    int stepY = MAX_Y/amtRows;
    int initX = stepX/2;
    int initY = stepY/2;

    hpx = (stepX + stepY) / 2.0;
    h = M_PER_PX * (stepX + stepY) / 2.0;

    alpha = 5.0/(14.0*M_PI*h*h);
    inv_rho_naught = 1.0/(float)DENSITY_RESTING;
    nu = h*h/100.0;
    
    // DEBUG
    // printf("\nh: %f", h);
    // printf("\nalpha: %f", alpha);
    // printf("\ninv density: %f", inv_rho_naught);

    int xStepCount = 0;
    int yStepCount = 0;
	
    for (int i = 0; i < NUM_PARTICLES; i++) {
		
		srand(i);
        if(xStepCount >= amtColumns) {
            xStepCount = 0;
            yStepCount++;
        }
        if(yStepCount >= amtRows) {
            yStepCount = 0;
        }
        allParticles[i].x = initX + xStepCount*stepX + (rand() % 3) - 1;
        allParticles[i].y = initY + yStepCount*stepY + (rand() % 3) - 1;
        allParticles[i].vx = 0;
        allParticles[i].vy = 0;
        allParticles[i].colour = WATER_COLOUR;

        xStepCount++;

        allParticles[i].pX = M_PER_PX * allParticles[i].x;
        allParticles[i].pY = M_PER_PX * allParticles[i].y;
        allEraseParticles[i].x = allParticles[i].x;
        allEraseParticles[i].y = allParticles[i].y;
    }
}

void eraseParticles() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        drawIndividualPixel(allEraseParticles[i].x, allEraseParticles[i].y, BLACK);
    }
}
void drawParticles() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        drawIndividualPixel(allParticles[i].x, allParticles[i].y, allParticles[i].colour);
    }
}

// Reference:
// https://cg.informatik.uni-freiburg.de/course_notes/sim_10_sph.pdf

void stepSPHPositions(int i) {

    allEraseParticles[i].x = allParticles[i].x;
    allEraseParticles[i].y = allParticles[i].y;

    allParticles[i].pX += allParticles[i].vx * SPF;
    allParticles[i].pY += allParticles[i].vy * SPF;
    allParticles[i].x = PX_PER_M * allParticles[i].pX;
    allParticles[i].y = PX_PER_M * allParticles[i].pY;

    // If, for whatever reason, we went out of bounds after velocity application, fix them manually.
	if (allParticles[i].x <= 0){
		allParticles[i].x = 0;
	} else if (allParticles[i].x > (MAX_X - 1)) {
		allParticles[i].x = MAX_X-1;
	}
	if (allParticles[i].y < 0){
		allParticles[i].y = 0;
	} else if (allParticles[i].y > (MAX_Y -1)){
		allParticles[i].y = MAX_Y-1;
	}
    
}

void doVelocityStepCheck(int i) {
    // Container collision handling and application of TUG Accelerations.
    if((allParticles[i].x >= (MAX_X-1) && allParticles[i].vx > 0) || (allParticles[i].x <= 0 && allParticles[i].vx < 0)) {
        allParticles[i].vx = -allParticles[i].vx*ELASTICITY;
    }
    else if(allParticles[i].x <= hpx && allParticles[i].vx < TUG_VELOCITY) {
        allParticles[i].ax += TUG_ACCELERATION;
    }
    else if(allParticles[i].x >= (MAX_X-1-hpx) && allParticles[i].vx > -TUG_VELOCITY) {
        allParticles[i].ax -= TUG_ACCELERATION;
    }

    if((allParticles[i].y >= (MAX_Y-1) && allParticles[i].vy > 0) || (allParticles[i].y <= 0 && allParticles[i].vy < 0)) {
        allParticles[i].vy = -allParticles[i].vy*ELASTICITY;
    }
    else if(allParticles[i].y <= hpx && allParticles[i].vy < TUG_VELOCITY) {
        allParticles[i].ay += TUG_ACCELERATION;
    }
    else if(allParticles[i].y >= (MAX_Y-1-hpx) && allParticles[i].vy > -TUG_VELOCITY) {
        allParticles[i].ay -= TUG_ACCELERATION;
    }
}

void stepSPHVelocities(int i) {
    
    allParticles[i].vx += allParticles[i].ax*SPF;
    allParticles[i].vy += allParticles[i].ay*SPF;
    allParticles[i].colour = hueToRGB565(WATER_HUE-sqrt(allParticles[i].vx*allParticles[i].vx + allParticles[i].vy*allParticles[i].vy)/VELOCITY_COLOUR_SENSITIVITY);

}

void calculateSPHAccelerations(int i) {

	allParticles[i].ax = 0;
    allParticles[i].ay = G; // Gravitational Acceleration
	
    float GRADW_ijx, GRADW_ijy; // Derivatives of the same Kernel we saw in the function that invokes this one
    float dx, dy;
    float dvx, dvy;
    float x_ij2, viscosScale;
    float x_ij, q;

    float pressureRatio_i = allParticles[i].pressure / (allParticles[i].density * allParticles[i].density);
    float inv_rho_j, pressureRatio_j;

    // doVelocityStepCheck(i);
    // if (allParticles[i].ax != 0 || allParticles[i].ay != G) return;

    for (int j = 0; j < NUM_PARTICLES; j++) {

        if (!allParticles[i].neighbours[j]) continue; // Dont check non-neighbours
        if (allParticles[i].neighbourDistances[j] == 0) continue; // Everything goes to zero if no distance

        q = allParticles[i].gradQ[j];
        if(!q) continue;

        dx = allParticles[i].neighbourDXs[j];
        dy = allParticles[i].neighbourDYs[j];

        x_ij = allParticles[i].neighbourDistances[j];
        x_ij2 = x_ij*x_ij;

        // if(q < 1){
        //     q = - 3 * pow((2-q), 2) + 12 * pow((1-q), 2);
        // } else if (q < 2) {
        //     q = - 3 * pow((2-q), 2);
        // } else {
        //     continue; // q is zero so save calcs by continuing
        // }
        
        GRADW_ijx = alpha * dx * q / (x_ij * h);
        GRADW_ijy = alpha * dy * q / (x_ij * h);

        // Pressure Acceleration

        inv_rho_j = 1/allParticles[j].density;
        pressureRatio_j = allParticles[j].pressure * inv_rho_j * inv_rho_j;
        allParticles[i].ax -= (pressureRatio_i + pressureRatio_j) * GRADW_ijx;
        allParticles[i].ay -= (pressureRatio_i + pressureRatio_j) * GRADW_ijy;

        // Viscosity Acceleration

        dvx = allParticles[i].vx - allParticles[j].vx;
        dvy = allParticles[i].vy - allParticles[j].vy;

        viscosScale = VISCOSITY * inv_rho_j * (dx*GRADW_ijx + dy*GRADW_ijy) / (x_ij2+nu);
        allParticles[i].ax += viscosScale * dvx;
        allParticles[i].ay += viscosScale * dvy;

    }

}

void timeStepSPHApproximation() {

    for (int i = 0; i < NUM_PARTICLES; i++) {

        // 1. Find nearest neighbours j for particle i
        // 2. Calculate Density and Pressure at every particle i

        float dx, dy;
        float x_ij, q, rho;
        float fp, sp, gradQ;
		
        for (int j = i + 1; j < NUM_PARTICLES; j++) {
			
            dx = allParticles[i].pX - allParticles[j].pX;
            dy = allParticles[i].pY - allParticles[j].pY;
            x_ij = sqrt(dx*dx+dy*dy);
			
            if (x_ij<ROOT_TWO_SCALE*h) {

                allParticles[i].neighbourDXs[j] = dx;
                allParticles[i].neighbourDYs[j] = dy;
                allParticles[i].neighbourDistances[j] = x_ij;

                allParticles[j].neighbourDXs[i] = -dx;
                allParticles[j].neighbourDYs[i] = -dy;
                allParticles[j].neighbourDistances[i] = x_ij;

                allParticles[i].neighbours[j] = true;
                allParticles[j].neighbours[i] = true;

                q = x_ij/h;

                if(q < 1){
                    fp = pow((2-q), 2);
                    sp = pow((1-q), 2);

                    gradQ = -3 * fp + 12 * sp;
                    allParticles[i].gradQ[j] = gradQ;
                    allParticles[j].gradQ[i] = gradQ;

                    q = fp*(2-q) - 4 * sp*(1-q);
                } else if (q < 2) {
                    fp = pow((2-q), 2);

                    gradQ = -3 * fp;
                    allParticles[i].gradQ[j] = gradQ;
                    allParticles[j].gradQ[i] = gradQ;

                    q = fp*(2-q);
                } else {
                    allParticles[i].gradQ[j] = 0;
                    allParticles[j].gradQ[i] = 0;
                    continue; // q is zero so save calcs by continuing
                }

                
                rho = alpha*q;
                //printf("\nrho: %f", rho);
                allParticles[i].density += rho;
                allParticles[j].density += rho;

            } else {
                allParticles[i].neighbours[j] = false;
                allParticles[j].neighbours[i] = false;
            }
			
        }

        allParticles[i].pressure = K * pow((allParticles[i].density*inv_rho_naught), 7) - K;

        // 3. Calculate Accelearations (Approx)
        calculateSPHAccelerations(i);

        // 4. Step Velocities and then positions.
        doVelocityStepCheck(i);
        stepSPHVelocities(i);
        stepSPHPositions(i);

    }

}

// =======================================================================================================
//                                             RIGID BODY UTILS
// =======================================================================================================

#define RB_HUE              0.35
#define RB_COLOUR           0x07e0

#define ELASTICITY_RB       0.2
#define SPH_RB              0.02
#define NUM_BODIES          3

#define PX_PER_M_RB         10.0
#define M_PER_PX_RB         0.1

#define VERTICIES_PER_BODY  4
#define VERT_VARIANCE       20
#define VELOCITY_COLOUR_SENSITIVITY_RB 100.0

#define BODY_DENSITY        2

typedef struct Vector2D {

    float x;
    float y;

} Vector2D;

typedef struct ExternalForce {
    
    Vector2D r;
    Vector2D force;
    bool isActive;

} ExternalForce;

typedef struct DrawBody {

    int xs [VERTICIES_PER_BODY];
    int ys [VERTICIES_PER_BODY]; 

} DrawBody;

typedef struct RigidBody {

    int xs [VERTICIES_PER_BODY];
    int ys [VERTICIES_PER_BODY]; 
    float pxs [VERTICIES_PER_BODY];
    float pys [VERTICIES_PER_BODY];
    float vDistances [VERTICIES_PER_BODY];
    Vector2D v;
    Vector2D a;
    float cx, cy;
    float I;
    float mass;
    float theta;
    float constThetas [VERTICIES_PER_BODY];
    float omega;
    float alpha;

    ExternalForce extForces [VERTICIES_PER_BODY];

    short int colour;

} RigidBody;

bool collisionMap [MAX_X][MAX_Y];
DrawBody eraseRBs [NUM_BODIES];
RigidBody allBodies [NUM_BODIES];

void initRigidBodies() {

    float x = (float)NUM_BODIES*(float)MAX_Y/(float)MAX_X;
    int amtRows = ceil(sqrt(ceil(x)));
    int amtColumns = ceil((double)amtRows*(double)MAX_X/(double)MAX_Y);

    int stepX = MAX_X/amtColumns;
    int stepY = MAX_Y/amtRows;

    int avgStepParam =  (stepX + stepY) >> 3;

    int initX = stepX/2;
    int initY = stepY/2;

    int xStepCount = 0;
    int yStepCount = 0;

    for (int i = 0; i < NUM_BODIES; i++) {

        allBodies[i].colour = RB_COLOUR;

        if(xStepCount >= amtColumns) {
            xStepCount = 0;
            yStepCount++;
        }
        if(yStepCount >= amtRows) {
            yStepCount = 0;
        }

        int centX =  initX + xStepCount * stepX;
        int centY =  initY + yStepCount * stepY;

        int signX = -1;
        int signY = -1;
        bool changeFlag = false;

        float sumX = 0;
        float sumY = 0;

        float runningAreaCount = 0;

        allBodies[i].v.x = 0;
        allBodies[i].v.y = 0;
        allBodies[i].theta = 0;
        allBodies[i].omega = 0;

        for (int j = 0; j < VERTICIES_PER_BODY; j++) {

            srand(i+j);
            allBodies[i].xs[j] = centX + signX*avgStepParam + rand() % VERT_VARIANCE - (VERT_VARIANCE >> 1);
            allBodies[i].ys[j] = centY + signY*avgStepParam + rand() % VERT_VARIANCE - (VERT_VARIANCE >> 1);
            
            eraseRBs[i].xs[j] = allBodies[i].xs[j];
            eraseRBs[i].ys[j] = allBodies[i].ys[j];

            allBodies[i].pxs[j] = M_PER_PX_RB * allBodies[i].xs[j];
            allBodies[i].pys[j] = M_PER_PX_RB * allBodies[i].ys[j];

            sumX += allBodies[i].pxs[j];
            sumY += allBodies[i].pys[j];

            if(signX == -1) {
                signX = 1;
            } else {
                if(changeFlag) {
                    signX = -1;
                    changeFlag = false;
                }
                changeFlag = true;
                signY = 1;
            }

            int nextIndex = j==VERTICIES_PER_BODY-1 ? 0 : j+1;
            runningAreaCount += (allBodies[i].pxs[j] + allBodies[i].pxs[nextIndex]) * (allBodies[i].pys[j] - allBodies[i].pys[nextIndex]) / 2.0;

        }

        allBodies[i].cx = M_PER_PX_RB * sumX/(float)VERTICIES_PER_BODY;
        allBodies[i].cy = M_PER_PX_RB * sumY/(float)VERTICIES_PER_BODY;

        float maxX = -1;
        float maxY = -1;
        float minX = M_PER_PX_RB * MAX_X;
        float minY = M_PER_PX_RB * MAX_Y;

        for (int j = 0; j < VERTICIES_PER_BODY; j++) {

            maxX = allBodies[i].pxs[j] > maxX ? allBodies[i].pxs[j] : maxX;
            maxY = allBodies[i].pys[j] > maxY ? allBodies[i].pys[j] : maxY;
            minX = allBodies[i].pxs[j] < minX ? allBodies[i].pxs[j] : minX;
            minY = allBodies[i].pys[j] < minY ? allBodies[i].pys[j] : minY;

            float dx = allBodies[i].cx - allBodies[i].pxs[j];
            float dy = allBodies[i].cy - allBodies[i].pys[j];

            allBodies[i].vDistances[j] = sqrt(dx*dx+dy*dy);
            allBodies[i].constThetas[j] = atan(dy/dx);

        }

        allBodies[i].mass = BODY_DENSITY * abs(runningAreaCount);
        allBodies[i].I = allBodies[i].mass * (pow(maxX-minX, 2) + pow(maxY-minY, 2)) / 12.0;

        xStepCount++;

    }

}

void eraseBodies() {

    for (int i = 0; i<NUM_BODIES; i++) {
        for (int j = 1; j<VERTICIES_PER_BODY; j++) {
            drawBresenhamLine(eraseRBs[i].xs[j-1], eraseRBs[i].ys[j-1], eraseRBs[i].xs[j], eraseRBs[i].ys[j], BLACK);
        }
        drawBresenhamLine(eraseRBs[i].xs[VERTICIES_PER_BODY-1], eraseRBs[i].ys[VERTICIES_PER_BODY-1], eraseRBs[i].xs[0], eraseRBs[i].ys[0], BLACK);
    }   

}

void drawBodies() {

    for (int i = 0; i<NUM_BODIES; i++) {
        for (int j = 1; j<VERTICIES_PER_BODY; j++) {
            drawBresenhamLine(allBodies[i].xs[j-1], allBodies[i].ys[j-1], allBodies[i].xs[j], allBodies[i].ys[j], allBodies[i].colour);
        }
        drawBresenhamLine(allBodies[i].xs[VERTICIES_PER_BODY-1], allBodies[i].ys[VERTICIES_PER_BODY-1], allBodies[i].xs[0], allBodies[i].ys[0], allBodies[i].colour);
    }   

}

void stepBodyPositions(int i) {

    // Must also update positions of all verticies

    float pt = allBodies[i].theta;

    allBodies[i].theta += allBodies[i].omega * SPH_RB;
    allBodies[i].cx += allBodies[i].v.x * SPH_RB;
    allBodies[i].cy += allBodies[i].v.y * SPH_RB;

    float ndx, ndy, nt;
    for (int j = 0; j < VERTICIES_PER_BODY; j++) {
        
        eraseRBs[i].xs[j] = allBodies[i].xs[j];
        eraseRBs[i].ys[j] = allBodies[i].ys[j];

        nt = allBodies[i].constThetas[j] + allBodies[i].theta;
        ndx = allBodies[i].vDistances[j] * cos(nt);
        ndy = allBodies[i].vDistances[j] * sin(nt);

        allBodies[i].pxs[j] = allBodies[i].cx + ndx;
        allBodies[i].pys[j] = allBodies[i].cy + ndy;

        allBodies[i].xs[j] = PX_PER_M_RB * allBodies[i].pxs[j];
        allBodies[i].ys[j] = PX_PER_M_RB * allBodies[i].pys[j];

        // Revert last position application if any vert out of bounds.
        bool mustAdjust = false;

        if (allBodies[i].xs[j] < 0 ){
            allBodies[i].cx += 0 - allBodies[i].xs[j];
        } else if (allBodies[i].xs[j] > (MAX_X-1)){
            allBodies[i].cx -= allBodies[i].xs[j] - (MAX_X-1);
        }
        if (allBodies[i].ys[j] < 0 ){
            allBodies[i].cy += 0 - allBodies[i].ys[j];
        } else if (allBodies[i].ys[j] > (MAX_Y-1)){
            allBodies[i].cy -= allBodies[i].ys[j] - (MAX_Y-1);
        }

        if(mustAdjust) {

            allBodies[i].theta = pt;
            for(int k = 0; k < VERTICIES_PER_BODY; k++) {
                nt = allBodies[i].constThetas[k] + allBodies[i].theta;
                ndx = allBodies[i].vDistances[k] * cos(nt);
                ndy = allBodies[i].vDistances[k] * sin(nt);

                allBodies[i].pxs[k] = allBodies[i].cx + ndx;
                allBodies[i].pys[k] = allBodies[i].cy + ndy;
            }

            allBodies[i].xs[j] = PX_PER_M_RB * allBodies[i].pxs[j];
            allBodies[i].ys[j] = PX_PER_M_RB * allBodies[i].pys[j];

        }

    }

}

void stepBodyVelocities(int i) {

    allBodies[i].omega += allBodies[i].alpha * SPH_RB;
    allBodies[i].v.x += allBodies[i].a.x * SPH_RB;
    allBodies[i].v.y += allBodies[i].a.y * SPH_RB;

    allBodies[i].colour = hueToRGB565(RB_HUE-sqrt(allBodies[i].v.x*allBodies[i].v.x + allBodies[i].v.y*allBodies[i].v.y)/VELOCITY_COLOUR_SENSITIVITY_RB);

}

void checkCollisions(int i) {

    for (int j = 0; j < VERTICIES_PER_BODY; j++) {
        
        bool setActive = false;
        // Container collision handling
        if(allBodies[i].xs[j] >= (MAX_X-1) || allBodies[i].xs[j] <= 0) {
            
            if((allBodies[i].v.x > 0) == (allBodies[i].xs[j] > 0)) {
                // allBodies[i].omega = -allBodies[i].omega * ELASTICITY_RB;
                allBodies[i].v.x = -allBodies[i].v.x * ELASTICITY_RB;
            }

            allBodies[i].extForces[j].force.x = -allBodies[i].mass * allBodies[i].a.x;
            
            setActive = true;

        }
        if(allBodies[i].ys[j] >= (MAX_Y-1) || allBodies[i].ys[j] <= 0) {

            if((allBodies[i].v.y > 0) == (allBodies[i].ys[j] >= (MAX_Y-1))) {
                // allBodies[i].omega = -allBodies[i].omega * ELASTICITY_RB;
                allBodies[i].v.y = -allBodies[i].v.y * ELASTICITY_RB;
            }

            allBodies[i].extForces[j].force.y = -allBodies[i].mass * allBodies[i].a.y;
            
            setActive = true;

        }

        if (setActive) {
            allBodies[i].extForces[j].r.x = allBodies[i].cx - allBodies[i].pxs[j];
            allBodies[i].extForces[j].r.y = allBodies[i].cy - allBodies[i].pys[j];
        } else {
            allBodies[i].extForces[j].force.x = 0;
            allBodies[i].extForces[j].force.y = 0;
        }
        allBodies[i].extForces[j].isActive = setActive;

    }

}

void timeStepRBForceApplication() {

    // model collisions with normal forces.
    for (int i = 0; i < NUM_BODIES; i++) {
        
        allBodies[i].a.x = 0;
        allBodies[i].a.y = G;
        
        float torque = 0;
        for (int j = 0; j < VERTICIES_PER_BODY; j++) {
            if (!allBodies[i].extForces[j].isActive) continue;
            torque += allBodies[i].extForces[j].r.x * allBodies[i].extForces[j].force.y - allBodies[i].extForces[j].r.y * allBodies[i].extForces[j].force.x;
        }
        allBodies[i].alpha = torque / allBodies[i].I;
        checkCollisions(i);
        stepBodyVelocities(i);
        stepBodyPositions(i);

    }

}

// =======================================================================================================
//                                                   MAIN
// =======================================================================================================

bool isFluidSim = false;
int main(void){ // main for this simulation

    mouseData mData;
    mouseData prevmData;

    initParticles();
    initRigidBodies();

    intializeMouse(&mData);
    prevmData = mData;

    vgaSetup();

    // Program loop
    while(1) {

        // Erase Stuff
        if (isFluidSim) eraseParticles();
        else eraseBodies();

        // drawMouse(&prevmData, BLACK);

        // Draw Stuff
        if (isFluidSim) drawParticles();
        else drawBodies();

        // drawMouse(&mData, WHITE);
        
        // Update Stuff 
        // prevmData = mData;
        // updateMouse(&mData);
        
        if (isFluidSim) timeStepSPHApproximation();
        else timeStepRBForceApplication();

        // Wait for Stuff
        waitForVsync();

    }

    return 0;

}
