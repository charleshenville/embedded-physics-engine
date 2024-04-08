
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

#define SW_BASE				    0xFF200040

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

bool isFluidSim = false;
bool lastFluidSim = false;
bool play = false;
int speedMult = 0;
float speedArray[5] = {1.0, 2.0, 3.0, 0.5, 0.25};
float SPF = 0.02;
float SPH_RB = 0.2;

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

#define PS2_BASE                0xFF200100
#define MOUSE_RADIUS            2
#define RESET_BUTTON_X          301
#define RESET_BUTTON_Y          20
#define SWITCH_BUTTON_X         301
#define SWITCH_BUTTON_Y         4 
#define PLAY_BUTTON_X           301
#define PLAY_BUTTON_Y           36
#define FF_BUTTON_X             301
#define FF_BUTTON_Y             52

void switchSimHandler();
void resetSimHandler();
void fastFowardHandler();

void buttonClickHandler();
void resetSimHandler();
typedef struct mouseData {
  int x;
  int y;
  int vx;
  int vy;
  bool left, right, middle;
} mouseData;

mouseData mData;
mouseData prevmData;

void updateMouse() {
  volatile int *PS2_ptr = (int *)PS2_BASE;
  int PS2_data, RVALID;
  signed char inputData[3];

  int i = 0;
  while(!i){
    PS2_data = *(PS2_ptr);
    RVALID = PS2_data & 0x8000;
    if(RVALID){
      inputData[0] = PS2_data & 0xFF;
      i++;
    }
  }

  while(i < 3){
    PS2_data = *(PS2_ptr);
    RVALID = PS2_data & 0x8000;
    if(RVALID){
      inputData[i] = PS2_data & 0xFF;
      i++;
    }
  }

  mData.left = inputData[0] & 1;
  mData.middle = inputData[0] & 4;
  mData.right = inputData[0] & 2;

  mData.x += inputData[1];
  mData.y -= inputData[2];

  mData.vx = inputData[1];
  mData.vy = -inputData[2];

  if (mData.x >= MAX_X) mData.x = MAX_X - 1;
  if (mData.y >= MAX_Y) mData.y = MAX_Y - 1;

  if (mData.x < 0) mData.x = 0;
  if (mData.y < 0) mData.y = 0;
  
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
    if(data -> left){
      for(int i = -1; i < 2; i++) {
        for(int j = -1; j < 2; j++){
          drawIndividualPixel(x + i, y+j, colour);
        }
      }
    }
    
}

void setA9stack(){
  int stack,mode;
  stack = 0xFFFFFFFF - 7;
  mode = 0b11010010;
  __asm__ volatile ("msr cpsr, %0":: "r"(mode));
  __asm__ volatile ("mov sp, %0":: "r"(stack));

  mode = 0b11010011;
  __asm__ volatile("msr cpsr, %0":: "r"(mode));
}

void enableInterrupt(){
  int status = 0b01010011;
  __asm__ volatile("msr cpsr, %0":: "r"(status));
}

void configGIC(){
  *((volatile int*) 0xFFFED84C) = 0x01000000;
  *((volatile int*) 0xFFFED108) = 0x00008000;

// all priority interupts enbaled
  *((volatile int*) 0xFFFEC104) = 0xFFFF;

  *((volatile int*) 0xFFFEC100) = 1;

  *((volatile int*) 0xFFFED000) = 1;
}

void __attribute__ ((interrupt)) __cs3_isr_irq(void){
  int interruptID = *((volatile int*) 0xFFFEC10C);

  if(interruptID != 79) while(1);
    //   prevmData = mData;
  updateMouse();
  if((mData.x >= SWITCH_BUTTON_X) && (mData.x < (SWITCH_BUTTON_X + 15)) && (mData.y >= SWITCH_BUTTON_Y) && (mData.y < (SWITCH_BUTTON_Y + 12))){
    if(!prevmData.left && mData.left){
      switchSimHandler();
    }
  }
  else if((mData.x >= RESET_BUTTON_X) && (mData.x < (RESET_BUTTON_X + 15)) && (mData.y >= RESET_BUTTON_Y) && (mData.y < (RESET_BUTTON_Y + 12))){
    if(!prevmData.left && mData.left){
      resetSimHandler();
    }
  }
  else if((mData.x >= PLAY_BUTTON_X) && (mData.x < (PLAY_BUTTON_X + 15)) && (mData.y >= PLAY_BUTTON_Y) && (mData.y < (PLAY_BUTTON_Y + 12))){
    if(!prevmData.left && mData.left){
      play = !play;
    }
  }
  else if((mData.x >= FF_BUTTON_X) && (mData.x < (FF_BUTTON_X + 15)) && (mData.y >= FF_BUTTON_Y) && (mData.y < (FF_BUTTON_Y + 12))){
    if(!prevmData.left && mData.left){
      fastFowardHandler();
    }
  }

  *((volatile int*) 0xFFFEC110) = interruptID;
  return;
}

void __attribute__ ((interrupt)) __cs3_isr_undef(void){while(1);}

void __attribute__ ((interrupt)) __cs3_isr_swi(void){while(1);}

void __attribute__ ((interrupt)) __cs3_isr_pabort(void){while(1);}

void __attribute__ ((interrupt)) __cs3_isr_dabort(void){while(1);}

void __attribute__ ((interrupt)) __cs3_isr_fiq(void){while(1);}

void intializeMouse() {
  volatile int * PS2_ptr = (volatile int *)0xFF200100;
  int PS2_data, RVALID;
  char byte1 = 0, byte2 = 0;

  mData.x = MAX_X / 2;
  mData.y = MAX_Y / 2;

  mData.vx = 0.0;
  mData.vy = 0.0;

  mData.left = false;
  mData.middle = false;
  mData.right = false;

  setA9stack();

  configGIC();

  // PS/2 mouse needs to be reset (must be already plugged in)
  *(PS2_ptr) = 0xFF; // reset
  while((byte2 != (char) 0xAA) || (byte1 != (char)0x00)){
    PS2_data = *(PS2_ptr);
    RVALID = PS2_data & 0x8000;
    if(RVALID){
      byte2 = byte1;
      byte1 = PS2_data & 0xFF;
    }
  }

  *(PS2_ptr) = 0xF3;
  byte1 = 0;
  while(byte1 != (char)0xFA){
    PS2_data = *(PS2_ptr);
    RVALID = PS2_data & 0x8000;
    if(RVALID){
      byte1 = PS2_data & 0xFF;
    }
  }

    //SAMPLE RATE
  *(PS2_ptr) = 40;
  byte1 = 0;
  while(byte1 != (char)0xFA){
    PS2_data = *(PS2_ptr);
    RVALID = PS2_data & 0x8000;
    if(RVALID)
      byte1 = PS2_data & 0xFF;
  }

  *(PS2_ptr) = 0xF4;
  byte1 = 0;
  while(byte1 != (char)0xFA){
    PS2_data = *(PS2_ptr);
    RVALID = PS2_data & 0x8000;
    if(RVALID)
      byte1 = PS2_data & 0xFF;
  }

  *(PS2_ptr + 1) = 1;

  enableInterrupt();
}

short int resetButton[180] =    {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA};

short int switchButton[2][180] = {{0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6Da,
                                   0xD6DA, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x001F, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0xD6DA,
                                   0xD6DA, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 
                                   0xD6DA, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x001F, 0xD6DA,
                                   0xD6DA, 0x001F, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0x0000, 0x001F, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA},
                                  {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07E0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x07E0, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x0000, 0x0000, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x0000, 0x0000, 0x0000, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x0000, 0x0000, 0x0000, 0x0000, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x07E0, 0x0000, 0x0000, 0x0000, 0x07E0, 0x0000, 0xD6DA,
                                   0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA,
                                   0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,}};

short int playButton[180] = {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                             0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                             0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                             0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                             0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA};

short int speedButton[5][180] = {{0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA},
                                 {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA},
                                 {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA},
                                 {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA},
                                 {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0x0000, 0x0000, 0x0000, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x0000, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA,
                                  0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA}};


void drawResetButton(){
  int x = RESET_BUTTON_X;
  int y = RESET_BUTTON_Y;
  
  for(int i = 0; i < 12; i++){
    for(int j = 0; j < 15; j++){
      drawIndividualPixel(x+j,y+i,resetButton[15*i + j]);
    }
  }
}

void drawSwitchButton(){
  int x = SWITCH_BUTTON_X;
  int y = SWITCH_BUTTON_Y;
  
  for(int i = 0; i < 12; i++){
    for(int j = 0; j < 15; j++){
      drawIndividualPixel(x+j,y+i,switchButton[isFluidSim][15*i + j]);
    }
  }
}

void drawPlayPause(){
  int x = PLAY_BUTTON_X;
  int y = PLAY_BUTTON_Y;
  
  for(int i = 0; i < 12; i++){
    for(int j = 0; j < 15; j++){
      drawIndividualPixel(x+j,y+i,playButton[15*i + j]);
    }
  }
}

void drawFFButton(){
  int x = FF_BUTTON_X;
  int y = FF_BUTTON_Y;
  
  for(int i = 0; i < 12; i++){
    for(int j = 0; j < 15; j++){
      drawIndividualPixel(x+j,y+i,speedButton[speedMult][15*i + j]);
    }
  }
}

void drawButtons(){
    drawSwitchButton();
    drawResetButton();
    drawPlayPause();
    drawFFButton();
}

// =======================================================================================================
//                                          FLUID SIMULATION UTILS
// =======================================================================================================

// -g -Wall -O1 -ffunction-sections -fverbose-asm -fno-inline -mno-cache-volatile -mhw-div -mcustom-fpu-cfg=60-2 -mhw-mul -mhw-mulx

#define NUM_PARTICLES       200 // 192, 48, 12

#define WATER_COLOUR        27743
#define WATER_HUE           0.62
#define BLACK               0
#define WHITE               0xFFFF

#define PRESSURE_COLUMNS    24
#define PRESSURE_ROWS       32

#define INIT_VAR            9

#define G                   2.0
#define K                   8.0
#define H_H                 5.0
#define DEFAULT_SPF         0.02 // Seconds Per Frame
#define VELOCITY_DECAY      0.92
#define ELASTICITY          0.2 // 0 to 1
#define VELOCITY_COLOUR_SENSITIVITY 20.0
#define VISCOSITY           1.0
#define ROOT_TWO_SCALE      1.414

#define TUG_ACCELERATION    10.0
#define EPSILON             0.001

#define M_PER_PX            0.02
#define PX_PER_M            50.0
#define MOUSE_A_MAG         150.0
#define MOUSE_ROE           30.0

#define DENSITY_RESTING     6500.0

// use malloc to create all of these instread of static later
#define NUM_BUCKETS 32
#define BUCKET_WIDTH (MAX_X/NUM_BUCKETS)
#define HALF_BUCKET_WIDTH (BUCKET_WIDTH>>1)

// for iteration almost exclusively.
int numElementsInBucket_even[NUM_BUCKETS] = {0};
int neighbourBucketIndexes[3] = {0};

int buckets_even[NUM_BUCKETS][NUM_PARTICLES];
int lastSeen[NUM_PARTICLES][NUM_PARTICLES];
int lastSeen2[NUM_PARTICLES][NUM_PARTICLES];
int timeStep = 0;
	
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
    int bucketIndexes[3];

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
    // h = M_PER_PX * (stepX + stepY) / 2.0;
    h = M_PER_PX * H_H;

    alpha = 5.0/(14.0*3.14159265*h*h);
    inv_rho_naught = 1.0/(float)DENSITY_RESTING;
    nu = h*h/100.0;
    
    // DEBUG
    // printf("\nh: %f", h);
    // printf("\nalpha: %f", alpha);
    // printf("\ninv density: %f", inv_rho_naught);

    int xStepCount = 0;
    int yStepCount = 0;
	
    for (int i = 0; i < NUM_PARTICLES; i++) {
		
        for(int j = 0; j < NUM_PARTICLES; j++){
            lastSeen[i][j] = -1;
            lastSeen2[i][j] = -1;
        }
		srand(i);
        if(xStepCount >= amtColumns) {
            xStepCount = 0;
            yStepCount++;
        }
        if(yStepCount >= amtRows) {
            yStepCount = 0;
        }
        allParticles[i].x = initX + xStepCount*stepX + (rand() % INIT_VAR) - (INIT_VAR>>1);
        allParticles[i].y = initY + yStepCount*stepY + (rand() % INIT_VAR) - (INIT_VAR>>1);
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

void draw2b2(int x, int y, short int colour) {
    drawIndividualPixel(x, y, colour);
    if (x < (MAX_X-2) && y > 1) {
        drawIndividualPixel(x + 1, y, colour);
        drawIndividualPixel(x + 1, y - 1, colour);
        drawIndividualPixel(x, y - 1, colour);
    } else if (x < (MAX_X-2)) {
        drawIndividualPixel(x + 1, y, colour);
    } else if (y > 1) {
        drawIndividualPixel(x, y - 1, colour);
    }
}
void eraseParticles() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        //drawIndividualPixel(allParticles[i].x, allParticles[i].y, BLACK);
        draw2b2(allEraseParticles[i].x, allEraseParticles[i].y, BLACK);
    }
}
void drawParticles() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        //drawIndividualPixel(allParticles[i].x, allParticles[i].y, allParticles[i].colour);
        draw2b2(allParticles[i].x, allParticles[i].y, allParticles[i].colour);
    }
}

// Reference:
// https://cg.informatik.uni-freiburg.de/course_notes/sim_10_sph.pdf

void stepSPHPositions(int i) {

    allParticles[i].pX += allParticles[i].vx * SPF;
    allParticles[i].pY += allParticles[i].vy * SPF;
    allParticles[i].x = PX_PER_M * allParticles[i].pX;
    allParticles[i].y = PX_PER_M * allParticles[i].pY;

    // If, for whatever reason, we went out of bounds after velocity application, fix them manually.
	if (allParticles[i].x <= 0){
		allParticles[i].x = 0;
        // allParticles[i].pX = EPSILON;
        
	} else if (allParticles[i].x > (MAX_X - 1)) {
		allParticles[i].x = MAX_X-1;
        // allParticles[i].pX = M_PER_PX * allParticles[i].x;
	}
	if (allParticles[i].y <= 0){
		allParticles[i].y = 0;
        // allParticles[i].pY = EPSILON;

	} else if (allParticles[i].y > (MAX_Y -1)){
		allParticles[i].y = MAX_Y-1;
        allParticles[i].pY = M_PER_PX * allParticles[i].y;
	}
    
}

void doVelocityStepCheck(int i) {
    // Container collision handling and application of TUG Accelerations.
    if((allParticles[i].x >= (MAX_X-1) && allParticles[i].vx > 0) || (allParticles[i].x <= 0 && allParticles[i].vx < 0)) {
        allParticles[i].vx = -allParticles[i].vx*ELASTICITY;
    }
    else if(allParticles[i].x <= hpx && allParticles[i].vx < EPSILON) {
        allParticles[i].ax += TUG_ACCELERATION;
    }
    else if(allParticles[i].x >= (MAX_X-1-hpx) && allParticles[i].vx > -EPSILON) {
        allParticles[i].ax -= TUG_ACCELERATION;
    }

    if((allParticles[i].y >= (MAX_Y-1) && allParticles[i].vy > 0) || (allParticles[i].y <= 0 && allParticles[i].vy < 0)) {
        allParticles[i].vy = -allParticles[i].vy*ELASTICITY;
    }
    else if(allParticles[i].y <= hpx && allParticles[i].vy < EPSILON) {
        allParticles[i].ay += TUG_ACCELERATION;
    }
    else if(allParticles[i].y >= (MAX_Y-1-hpx) && allParticles[i].vy > -EPSILON) {
        allParticles[i].ay -= TUG_ACCELERATION;
    }
}

float floatAbs(float in){
    return in > 0 ? in : -in;
}
void stepSPHVelocities(int i) {
    
    if(floatAbs(allParticles[i].vx) < VELOCITY_COLOUR_SENSITIVITY/2){
        allParticles[i].vx += allParticles[i].ax*SPF;
    } else if ((allParticles[i].vx > 0) != (allParticles[i].ax > 0)) {
        allParticles[i].vx += allParticles[i].ax*SPF;
    } if (isnan(allParticles[i].vx)) {
        allParticles[i].vx = 0.0;
    }
    if(floatAbs(allParticles[i].vy) < VELOCITY_COLOUR_SENSITIVITY/2){
        allParticles[i].vy += allParticles[i].ay*SPF;
    } else if ((allParticles[i].vy > 0) != (allParticles[i].ay > 0)) {
        allParticles[i].vy += allParticles[i].ay*SPF;
    } if (isnan(allParticles[i].vy)) {
        allParticles[i].vy = 0.0;
    }
    allParticles[i].vx *= VELOCITY_DECAY;
    allParticles[i].vx *= VELOCITY_DECAY;
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

    // if (-EPSILON < allParticles[i].density < EPSILON) {
    //     allParticles[i].density = EPSILON;
    // }
    float pressureRatio_i = allParticles[i].pressure / (allParticles[i].density * allParticles[i].density);
    float inv_rho_j, pressureRatio_j;

    for(int nbIdx = 0; nbIdx < 3; nbIdx++){

        int buck = allParticles[i].bucketIndexes[nbIdx];
        if (buck<0 || buck>=NUM_BUCKETS) break;

        for (int pos_j = 0; pos_j < numElementsInBucket_even[buck]; pos_j++) {
        
            int j = buckets_even[buck][pos_j];
            if (i==j) continue;

            if (lastSeen[i][j] == timeStep || lastSeen[j][i] == timeStep) continue;
            lastSeen[i][j] = timeStep;
            lastSeen[j][i] = timeStep;

            if (!allParticles[i].neighbours[j]) continue; // Dont check non-neighbours
            if (allParticles[i].neighbourDistances[j] == 0) continue; // Everything goes to zero if no distance

            q = allParticles[i].gradQ[j];
            if(!q) continue;

            dx = allParticles[i].neighbourDXs[j];
            dy = allParticles[i].neighbourDYs[j];

            x_ij = allParticles[i].neighbourDistances[j];
            // if(-EPSILON < x_ij < EPSILON) {
            //     x_ij = EPSILON;
            // }
            x_ij2 = x_ij*x_ij;

            GRADW_ijx = alpha * dx * q / (x_ij * h);
            GRADW_ijy = alpha * dy * q / (x_ij * h);

            // Pressure Acceleration

            // if (-EPSILON < allParticles[j].density < EPSILON) continue;
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

    // Check for nan
    if(isnan(allParticles[i].ax) || isnan(allParticles[i].ay)) {
        allParticles[i].ax = 0;
        allParticles[i].ay = G;
    }
    // Mouse Acceleration
    if(!mData.left) return;
    // printf("HERE");
    dx = (float)allParticles[i].x - (float)mData.x;
    dy = (float)allParticles[i].y - (float)mData.y;
    float mag = sqrt(dx*dx+dy*dy);
    if (mag < MOUSE_ROE) {
        allParticles[i].ax += MOUSE_A_MAG * dx/(mag);
        allParticles[i].ay += MOUSE_A_MAG * dy/(mag);
    }

} 

void timeStepSPHApproximation(int i, int j) {
    
    if (i==j) return;
    // 1. Find nearest neighbours j for particle i
    // 2. Calculate Density and Pressure at every particle i

    float dx, dy;
    float x_ij, q, rho;
    float fp, sp, gradQ;

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
            return; // q is zero so save calcs by continuing
        }

        
        rho = alpha*q;
        allParticles[i].density += rho;
        allParticles[j].density += rho;

    } else {
        allParticles[i].neighbours[j] = false;
        allParticles[j].neighbours[i] = false;
    }

}

void generalParticleUpdate(int i) {
    
    allParticles[i].pressure = K * pow((allParticles[i].density*inv_rho_naught), 7) - K;

    // 3. Calculate Accelearations (Approx)
    calculateSPHAccelerations(i);

    // 4. Step Velocities and then positions.
    doVelocityStepCheck(i);
    stepSPHVelocities(i);
    stepSPHPositions(i);

}

void timeStepBucketwiseParticleUpdate() {
    
    // clean buckets
    for (int bucket = 0; bucket < NUM_BUCKETS; bucket++) {
        numElementsInBucket_even[bucket] = 0;
    }

    // Populate all buckets with particle indicies appropriately
    for (int i = 0; i < NUM_PARTICLES; i++) {

        allEraseParticles[i].x = allParticles[i].x;
        allEraseParticles[i].y = allParticles[i].y;

        int evenBucket = (allParticles[i].x/BUCKET_WIDTH);
        // assert(evenBucket >= 0 && evenBucket < NUM_BUCKETS);
        buckets_even[evenBucket][numElementsInBucket_even[evenBucket]] = i;
        numElementsInBucket_even[evenBucket]++;

    }

    for (int bucket = 0; bucket < NUM_BUCKETS; bucket++) {

        if(bucket == 0){
            neighbourBucketIndexes[0] = bucket;
            neighbourBucketIndexes[1] = bucket+1;
            neighbourBucketIndexes[2] = -1;
        } else if(bucket == (NUM_BUCKETS - 1)) {
            neighbourBucketIndexes[0] = bucket-1;
            neighbourBucketIndexes[1] = bucket;
            neighbourBucketIndexes[2] = -1;
        } else {
            neighbourBucketIndexes[0] = bucket-1;
            neighbourBucketIndexes[1] = bucket;
            neighbourBucketIndexes[2] = bucket+1;
        }

        for (int pos_i = 0; pos_i < numElementsInBucket_even[bucket]; pos_i++) {
            int i = buckets_even[bucket][pos_i];

            for(int nbIdx = 0; nbIdx < 3; nbIdx++){
                
                int buck = neighbourBucketIndexes[nbIdx];
                allParticles[i].bucketIndexes[nbIdx] = buck;

                if (buck<0 || buck>=NUM_BUCKETS) break;

                for (int pos_j = 0; pos_j < numElementsInBucket_even[buck]; pos_j++) {

                    int j = buckets_even[buck][pos_j];
                    // if(j<(i+1)) continue;

                    if (lastSeen[i][j] == timeStep || lastSeen[j][i] == timeStep) continue;
                    lastSeen[i][j] = timeStep; 
                    lastSeen[j][i] = timeStep; 

                    // Call helper function to actually process i/j collision
                    timeStepSPHApproximation(i,j);

                }

            }
            
            timeStep++;
            generalParticleUpdate(i);
            timeStep--;
        }

        
    }
    timeStep++;
}



// =======================================================================================================
//                                             RIGID BODY UTILS
// =======================================================================================================

#define RB_HUE              0.35
#define RB_COLOUR           0x07e0

#define ELASTICITY_RB       0.4
#define DEFAULT_SPH_RB      0.2
#define NUM_BODIES          12

#define G_RB                9.0
#define EPSILON_RB          0.00001
#define PX_PER_M_RB         1.0
#define M_PER_PX_RB         1.0

#define STEP_THRESH             2
#define BAD_STEP_DAMP           0.1
#define MAX_COLLISIONPT_DELTA   10.0

#define INT_MAX_C           2147483647
#define INT_MIN_C           -2147483648

#define VERTICIES_PER_BODY  4
#define MAX_EXTERNAL_FORCES (VERTICIES_PER_BODY + NUM_BODIES)
#define VERT_VARIANCE       21
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
    int minPX;
    int minPY;
    int maxPX;
    int maxPY;
    float cx, cy;
    float I;
    float mass;
    float theta;
    float constThetas [VERTICIES_PER_BODY];
    float omega;
    float alpha;
    
    Vector2D lastPointofCollision;
    float lastPositionDelta;
    bool cLast;

    ExternalForce extForces [MAX_EXTERNAL_FORCES];

    short int colour;

} RigidBody;

short int collisionMap [MAX_X][MAX_Y];
bool bookMarkedCollisions[NUM_BODIES][NUM_BODIES];
DrawBody eraseRBs [NUM_BODIES];
RigidBody allBodies [NUM_BODIES];

int currentMouseInteractionObj;

float dotProd2D(Vector2D * a, Vector2D * b){
    return a->x * b->x + a->y * b->y;
}
float magnitudeCrossProd2D(Vector2D * a, Vector2D * b){
    return a->x * b->y - a->y * b->x;
}
float floatMin(float a, float b){
    return a < b? a : b;
}
float floatMax(float a, float b){
    return a > b? a : b;
}
float getMag(Vector2D * a){
    return sqrt(a->x*a->x + a->y*a->y);
}
Vector2D addVec2 (Vector2D * a, Vector2D * b) {
    Vector2D res;
    res.x = a->x + b->x;
    res.y = a->y + b->y;
    return res;
}
Vector2D subVec2 (Vector2D * a, Vector2D * b) {
    Vector2D res;
    res.x = a->x - b->x;
    res.y = a->y - b->y;
    return res;
}
Vector2D multVec2 (Vector2D * a, float m) {
    Vector2D res;
    res.x = a->x * m;
    res.y = a->y * m;
    return res;
}
Vector2D constrVec (float x, float y) {
    Vector2D res;
    res.x = x;
    res.y = y;
    return res;
}

void resetBodyFromCenter(int i) {
    for(int k = 0; (k < VERTICIES_PER_BODY); k++) {
        float nt = allBodies[i].constThetas[k] + allBodies[i].theta;
        float ndx = allBodies[i].vDistances[k] * cos(nt);
        float ndy = allBodies[i].vDistances[k] * sin(nt);

        allBodies[i].pxs[k] = allBodies[i].cx + ndx;
        allBodies[i].pys[k] = allBodies[i].cy + ndy;

        allBodies[i].xs[k] = PX_PER_M_RB * allBodies[i].pxs[k];
        allBodies[i].ys[k] = PX_PER_M_RB * allBodies[i].pys[k];
    }
}

bool pointIsInsideRB(float x, float y, int rbIdx){
    
    int counter = 0;
    for (int i =0; i < VERTICIES_PER_BODY; i++) {
        int nextIdx = (i+1) % VERTICIES_PER_BODY;
        if ((allBodies[rbIdx].pys[i] <= y && y < allBodies[rbIdx].pys[nextIdx]) ||
            (allBodies[rbIdx].pys[nextIdx] <= y && y < allBodies[rbIdx].pys[i])) {
            float m = (allBodies[rbIdx].pxs[nextIdx] - allBodies[rbIdx].pxs[i])/(allBodies[rbIdx].pys[nextIdx] - allBodies[rbIdx].pys[i]);
            if (isnan(m)) m = 1.0e38;
            float root = allBodies[rbIdx].pxs[i] + (y - allBodies[rbIdx].pys[i]) * m;
            if (x < root) counter++;
        }
    }
    return ((counter % 2 ) == 1); // If we crossed an odd amt. of times. we must be inside.

    // bool diffsInX = false;
    // bool diffsInY = false;

    // float lastX = allBodies[rbIdx].pxs[0];
    // float lastY = allBodies[rbIdx].pys[0];

    // for(int i = 1; i< VERTICIES_PER_BODY; i++) {
    //     if ((x - lastX >= 0) != (x - allBodies[rbIdx].pxs[i] >= 0)) diffsInX = true;
    //     if ((y - lastY >= 0) != (y - allBodies[rbIdx].pys[i] >= 0)) diffsInY = true;
    //     lastX = allBodies[rbIdx].pxs[i];
    //     lastY = allBodies[rbIdx].pys[i];
    // }

    // return (diffsInX && diffsInY);

} 

// Returns true if bodies I 
bool isColliding(int i, int j){
    bool hasCollided = true;
    float dx, dy, miniDot, minjDot, maxiDot, maxjDot;
    // Loop over edge normals of both shapes.
    for(int vertIdx = 0; vertIdx < (VERTICIES_PER_BODY<<1); vertIdx++) {
        
        int startIdxi = (vertIdx % 4)? (vertIdx % 4)-1 : VERTICIES_PER_BODY - 1;
        int endIdxi = (vertIdx % 4);
        
        // dx and dy here define our current edge on either i or j (and the vector parallel to it).
        if(vertIdx < VERTICIES_PER_BODY){
            dx = allBodies[i].pxs[endIdxi] - allBodies[i].pxs[startIdxi];
            dy = allBodies[i].pys[endIdxi] - allBodies[i].pys[startIdxi];
        } else {
            dx = allBodies[j].pxs[endIdxi] - allBodies[j].pxs[startIdxi];
            dy = allBodies[j].pys[endIdxi] - allBodies[j].pys[startIdxi];
        }

        Vector2D normalVec, cVec;
        float testDot, testiIdxMin, testjIdxMin, testiIdxMax, testjIdxMax;
        normalVec.x = -dy;
        normalVec.y = dx;

        miniDot = 1.0e38;
        minjDot = 1.0e38;
        maxiDot = -1.0e38;
        maxjDot = -1.0e38;

        for(int vertIdxi = 0; vertIdxi < VERTICIES_PER_BODY; vertIdxi++) {
            cVec.x = allBodies[i].pxs[vertIdxi];
            cVec.y = allBodies[i].pys[vertIdxi];
            testDot = dotProd2D(&normalVec, &cVec);
            if(testDot < miniDot){miniDot = testDot; testiIdxMin = vertIdxi;}
            if(testDot > maxiDot){maxiDot = testDot; testiIdxMax = vertIdxi;}
        }
        for(int vertIdxj = 0; vertIdxj < VERTICIES_PER_BODY; vertIdxj++) {
            cVec.x = allBodies[j].pxs[vertIdxj];
            cVec.y = allBodies[j].pys[vertIdxj];
            testDot = dotProd2D(&normalVec, &cVec);
            if(testDot < minjDot){minjDot = testDot; testjIdxMin = vertIdxj;}
            if(testDot > maxjDot){maxjDot = testDot; testjIdxMax = vertIdxj;}
        }

        // Test to see if we have found a seperating axis
        if(miniDot>=maxjDot || maxiDot<=minjDot) {hasCollided = false; break;}
        
    }
    return hasCollided;
}
// Check if rigid body I has coillided with any rigid body j
// Credit to the SAT. (Seperating Axis Theorem).
void checkSATInterBodyCollision(int i){
    
    // if (i==currentMouseInteractionObj) return;
    float dy, dx;
    float maxiDot, miniDot;
    float maxjDot, minjDot;

    int forceIndex = VERTICIES_PER_BODY - 1;
    bool neverCollided = true;

    for(int j = 0; j < NUM_BODIES; j++){

        forceIndex += 1;
        if (j==i) continue;
        // if (j==currentMouseInteractionObj) continue;

        // if (bookMarkedCollisions[i][j]) continue;
        // Hold the minimum found sep value between bodies i and j.
        float minSep = 1.0e38;
        int testiIdxMin, testjIdxMin, testiIdxMax, testjIdxMax;
        int minSepEdgeBodyIdx = 0;
        int minSepVertBodyIdx = 0;
        int minSepBodyVertIdx = 0;
        Vector2D minEdgeResponsible, normMinEdgeResponsible;

        bool hasCollided = true;

        // Loop over edge normals of both shapes.
        for(int vertIdx = 0; vertIdx < (VERTICIES_PER_BODY<<1); vertIdx++) {
            
            int startIdxi = (vertIdx % 4)? (vertIdx % 4)-1 : VERTICIES_PER_BODY - 1;
            int endIdxi = (vertIdx % 4);
            
            // dx and dy here define our current edge on either i or j (and the vector parallel to it).
            if(vertIdx < VERTICIES_PER_BODY){
                dx = allBodies[i].pxs[endIdxi] - allBodies[i].pxs[startIdxi];
                dy = allBodies[i].pys[endIdxi] - allBodies[i].pys[startIdxi];
            } else {
                dx = allBodies[j].pxs[endIdxi] - allBodies[j].pxs[startIdxi];
                dy = allBodies[j].pys[endIdxi] - allBodies[j].pys[startIdxi];
            }

            Vector2D normalVec, cVec;
            float testDot;
            normalVec.x = -dy;
            normalVec.y = dx;

            miniDot = 1.0e38;
            minjDot = 1.0e38;
            maxiDot = -1.0e38;
            maxjDot = -1.0e38;

            for(int vertIdxi = 0; vertIdxi < VERTICIES_PER_BODY; vertIdxi++) {
                cVec.x = allBodies[i].pxs[vertIdxi];
                cVec.y = allBodies[i].pys[vertIdxi];
                testDot = dotProd2D(&normalVec, &cVec);
                if(testDot < miniDot){miniDot = testDot; testiIdxMin = vertIdxi;}
                if(testDot > maxiDot){maxiDot = testDot; testiIdxMax = vertIdxi;}
            }
            for(int vertIdxj = 0; vertIdxj < VERTICIES_PER_BODY; vertIdxj++) {
                cVec.x = allBodies[j].pxs[vertIdxj];
                cVec.y = allBodies[j].pys[vertIdxj];
                testDot = dotProd2D(&normalVec, &cVec);
                if(testDot < minjDot){minjDot = testDot; testjIdxMin = vertIdxj;}
                if(testDot > maxjDot){maxjDot = testDot; testjIdxMax = vertIdxj;}
            }

            // Test to see if we have found a seperating axis
            if(miniDot>=maxjDot || maxiDot<=minjDot) {hasCollided = false; break;}

            // If we are still here then we have not found the seperating axis and can still collide.
            float seperation = -floatMin(maxiDot-minjDot, maxjDot-miniDot);
            // printf("SEP: %f\n\n", seperation);
            if (seperation <= minSep) {
                // printf("we haere\n");
                minSep = seperation;
                minEdgeResponsible.x = dx;
                minEdgeResponsible.y = dy;
                float magPos = sqrt(dx*dx+dy*dy);
                normMinEdgeResponsible.x = -dx;
                normMinEdgeResponsible.y = -dy;

                minSepEdgeBodyIdx = vertIdx < VERTICIES_PER_BODY ? i : j;
                minSepVertBodyIdx = minSepEdgeBodyIdx == i ? j : i;
                
                // Which vert of the vert body is responsible:
                if(maxiDot-minjDot < maxjDot-miniDot) {
                    if (minSepVertBodyIdx==i){
                        minSepBodyVertIdx = testiIdxMax;
                    } else {
                        minSepBodyVertIdx = testjIdxMin;
                    }
                } else {
                    if (minSepVertBodyIdx==i){
                        minSepBodyVertIdx = testiIdxMin;
                    } else {
                        minSepBodyVertIdx = testjIdxMax;
                    }
                }

            }
        }

        if (hasCollided /*&& !bookMarkedCollisions[i][j]*/) {

            neverCollided = false;
            // Find the vert of the so-called vert body that was responsible for the collision.
            int vertInsideCount = 0;
            minSepBodyVertIdx = -1;
            for(int k = 0; k<VERTICIES_PER_BODY; k++){
                if(pointIsInsideRB(allBodies[minSepVertBodyIdx].pxs[k], allBodies[minSepVertBodyIdx].pys[k], minSepEdgeBodyIdx)) {
                    minSepBodyVertIdx = k;
                    vertInsideCount++;
                    //draw2b2(allBodies[minSepVertBodyIdx].xs[k], allBodies[minSepVertBodyIdx].ys[k], WATER_COLOUR);
                    //break;
                } 
            }
            if (minSepBodyVertIdx == -1) {
                vertInsideCount = 0;
                swap(&minSepEdgeBodyIdx, &minSepVertBodyIdx);
                for(int k = 0; k<VERTICIES_PER_BODY; k++){
                    if(pointIsInsideRB(allBodies[minSepVertBodyIdx].pxs[k], allBodies[minSepVertBodyIdx].pys[k], minSepEdgeBodyIdx)) {
                        minSepBodyVertIdx = k;
                        vertInsideCount++;
                        //draw2b2(allBodies[minSepVertBodyIdx].xs[k], allBodies[minSepVertBodyIdx].ys[k], WATER_COLOUR);
                        //break;
                    } 
                }
                if (minSepBodyVertIdx == -1) minSepBodyVertIdx = 0;
            }
            
            Vector2D normedA;
            if(!bookMarkedCollisions[minSepVertBodyIdx][minSepEdgeBodyIdx] && !bookMarkedCollisions[minSepEdgeBodyIdx][minSepVertBodyIdx]) {
                normedA = constrVec(allBodies[minSepVertBodyIdx].pxs[minSepBodyVertIdx], allBodies[minSepVertBodyIdx].pys[minSepBodyVertIdx]);
                Vector2D normedB;

                normedB = subVec2(&normedA, &allBodies[minSepVertBodyIdx].lastPointofCollision);
                allBodies[minSepVertBodyIdx].lastPositionDelta = getMag(&normedB);
                allBodies[minSepVertBodyIdx].lastPointofCollision = normedA;

                normedB = subVec2(&normedA, &allBodies[minSepEdgeBodyIdx].lastPointofCollision);
                allBodies[minSepEdgeBodyIdx].lastPositionDelta = getMag(&normedB);
                allBodies[minSepEdgeBodyIdx].lastPointofCollision = normedA;

            }

            // Manual Positional Adjustment
            float signX = allBodies[minSepVertBodyIdx].pxs[minSepBodyVertIdx] > allBodies[minSepEdgeBodyIdx].cx ? 1 : -1;
            float signY = allBodies[minSepVertBodyIdx].pys[minSepBodyVertIdx] > allBodies[minSepEdgeBodyIdx].cy ? 1 : -1;

            Vector2D unitNorm = multVec2(&normMinEdgeResponsible, (1/getMag(&normMinEdgeResponsible)));
            dx = unitNorm.x > 0 ? unitNorm.x : -unitNorm.x;
            dy = unitNorm.y > 0 ? unitNorm.y : -unitNorm.y;
            dx *= signX;
            dy *= signY;
            
            // while(pointIsInsideRB(
            //     allBodies[minSepVertBodyIdx].pxs[minSepBodyVertIdx],
            //     allBodies[minSepVertBodyIdx].pys[minSepBodyVertIdx],
            //     minSepEdgeBodyIdx
            // ) || pointIsInsideRB(
            //     allBodies[minSepVertBodyIdx].cx,
            //     allBodies[minSepVertBodyIdx].cy,
            //     minSepEdgeBodyIdx)) {
            while (isColliding(minSepVertBodyIdx, minSepEdgeBodyIdx)){
                // draw2b2(allBodies[minSepVertBodyIdx].xs[minSepBodyVertIdx], allBodies[minSepVertBodyIdx].ys[minSepBodyVertIdx], WATER_COLOUR);
                
                allBodies[minSepVertBodyIdx].cx += dx;
                allBodies[minSepVertBodyIdx].cy += dy;

                allBodies[minSepEdgeBodyIdx].cx -= dx;
                allBodies[minSepEdgeBodyIdx].cy -= dy;

                resetBodyFromCenter(minSepVertBodyIdx);
                resetBodyFromCenter(minSepEdgeBodyIdx);

            }

            // Torque handling
            unitNorm.x = dx;
            unitNorm.y = dy;

            allBodies[minSepVertBodyIdx].extForces[forceIndex].isActive = true;
            float magB = allBodies[minSepEdgeBodyIdx].mass * getMag(&allBodies[minSepEdgeBodyIdx].a);
            normedA = multVec2(&unitNorm, magB);
            allBodies[minSepVertBodyIdx].extForces[forceIndex].force = normedA;
            allBodies[minSepVertBodyIdx].extForces[forceIndex].r.x = allBodies[minSepVertBodyIdx].pxs[minSepBodyVertIdx] - allBodies[minSepVertBodyIdx].cx;
            allBodies[minSepVertBodyIdx].extForces[forceIndex].r.y = allBodies[minSepVertBodyIdx].pys[minSepBodyVertIdx] - allBodies[minSepVertBodyIdx].cy;

            allBodies[minSepEdgeBodyIdx].extForces[forceIndex].isActive = true;
            float magA = allBodies[minSepVertBodyIdx].mass * getMag(&allBodies[minSepVertBodyIdx].a);
            normedA = multVec2(&unitNorm, -magA);
            allBodies[minSepEdgeBodyIdx].extForces[forceIndex].force = normedA;
            allBodies[minSepEdgeBodyIdx].extForces[forceIndex].r.x = allBodies[minSepVertBodyIdx].pxs[minSepBodyVertIdx] - allBodies[minSepEdgeBodyIdx].cx;
            allBodies[minSepEdgeBodyIdx].extForces[forceIndex].r.y = allBodies[minSepVertBodyIdx].pys[minSepBodyVertIdx] - allBodies[minSepEdgeBodyIdx].cy;

            if(!allBodies[minSepVertBodyIdx].cLast && !allBodies[minSepEdgeBodyIdx].cLast) {
                allBodies[minSepVertBodyIdx].v.x *= -ELASTICITY_RB;
                allBodies[minSepVertBodyIdx].v.y *= -ELASTICITY_RB;
                allBodies[minSepEdgeBodyIdx].v.x *= -ELASTICITY_RB;
                allBodies[minSepEdgeBodyIdx].v.y *= -ELASTICITY_RB;

                allBodies[minSepVertBodyIdx].omega *= -ELASTICITY_RB;
                allBodies[minSepEdgeBodyIdx].omega *= -ELASTICITY_RB;
            } else {
                allBodies[minSepVertBodyIdx].v.x *= ELASTICITY_RB;
                allBodies[minSepVertBodyIdx].v.y *= ELASTICITY_RB;
                allBodies[minSepEdgeBodyIdx].v.x *= ELASTICITY_RB;
                allBodies[minSepEdgeBodyIdx].v.y *= ELASTICITY_RB;

                allBodies[minSepVertBodyIdx].omega *= ELASTICITY_RB;
                allBodies[minSepEdgeBodyIdx].omega *= ELASTICITY_RB;
            }
            bookMarkedCollisions[minSepVertBodyIdx][minSepEdgeBodyIdx] = true;
            bookMarkedCollisions[minSepEdgeBodyIdx][minSepVertBodyIdx] = true;
            allBodies[minSepVertBodyIdx].cLast = true;
            allBodies[minSepEdgeBodyIdx].cLast = true;
            // continue;

            // Collision Resolution (Impulse-Based):
            normMinEdgeResponsible = unitNorm;
            Vector2D c1 = subVec2(&allBodies[minSepVertBodyIdx].v, &allBodies[minSepEdgeBodyIdx].v);
            Vector2D rAP = constrVec(
                allBodies[minSepVertBodyIdx].pxs[minSepBodyVertIdx] - allBodies[minSepVertBodyIdx].cx,
                allBodies[minSepVertBodyIdx].pys[minSepBodyVertIdx] - allBodies[minSepVertBodyIdx].cy
            );
            Vector2D rBP = constrVec(
                allBodies[minSepVertBodyIdx].pxs[minSepBodyVertIdx] - allBodies[minSepEdgeBodyIdx].cx,
                allBodies[minSepVertBodyIdx].pys[minSepBodyVertIdx] - allBodies[minSepEdgeBodyIdx].cy
            );
            c1 = multVec2(&c1, (-1-ELASTICITY_RB));
            float jCoeffNum = dotProd2D(&c1, &normMinEdgeResponsible); 
            c1 = multVec2(&normMinEdgeResponsible, ((1/allBodies[minSepVertBodyIdx].mass) + (1/allBodies[minSepEdgeBodyIdx].mass)));
            float jCoeffDenom = dotProd2D(&c1, &normMinEdgeResponsible);
            jCoeffDenom += pow(dotProd2D(&rAP, &normMinEdgeResponsible),2)/allBodies[minSepVertBodyIdx].I;
            jCoeffDenom += pow(dotProd2D(&rBP, &normMinEdgeResponsible),2)/allBodies[minSepEdgeBodyIdx].I;

            float jCoeff = jCoeffNum/jCoeffDenom;

            // Linear Velocity response
            c1 = multVec2(&normMinEdgeResponsible, (jCoeff/allBodies[minSepVertBodyIdx].mass));
            allBodies[minSepVertBodyIdx].v = addVec2(&allBodies[minSepVertBodyIdx].v, &c1);
            // allBodies[minSepVertBodyIdx].v = c1;
            c1 = multVec2(&normMinEdgeResponsible, (jCoeff/allBodies[minSepEdgeBodyIdx].mass));
            allBodies[minSepEdgeBodyIdx].v = subVec2(&allBodies[minSepEdgeBodyIdx].v, &c1);
            // allBodies[minSepEdgeBodyIdx].v = multVec2(&c1, -1.0);
            
            // Angular Velocity response
            c1 = multVec2(&normMinEdgeResponsible, jCoeff);
            allBodies[minSepVertBodyIdx].omega += dotProd2D(&rAP, &c1)/allBodies[minSepVertBodyIdx].I;
            allBodies[minSepEdgeBodyIdx].omega -= dotProd2D(&rBP, &c1)/allBodies[minSepEdgeBodyIdx].I;

        }
    }
    if (neverCollided) {
        allBodies[i].cLast = false;
    }
    
}

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

        int centX = initX + xStepCount * stepX;
        int centY = initY + yStepCount * stepY;

        int signX = -1;
        int signY = -1;
        bool changeFlag = false;

        float sumX = 0;
        float sumY = 0;

        float runningAreaCount = 0;

        allBodies[i].v.x = 0;
        allBodies[i].v.y = rand() % VERT_VARIANCE - (VERT_VARIANCE >> 1);
        allBodies[i].theta = 0;
        allBodies[i].omega = 0;

        allBodies[i].lastPointofCollision = constrVec(0.0, 0.0);
        for (int j = 0; j < VERTICIES_PER_BODY; j++) {

            // srand(i+j);
            allBodies[i].xs[j] = centX + signX*avgStepParam + rand() % VERT_VARIANCE - (VERT_VARIANCE >> 1);
            allBodies[i].ys[j] = centY + signY*avgStepParam + rand() % VERT_VARIANCE - (VERT_VARIANCE >> 1);
            
            eraseRBs[i].xs[j] = allBodies[i].xs[j];
            eraseRBs[i].ys[j] = allBodies[i].ys[j];

            allBodies[i].pxs[j] = M_PER_PX_RB * allBodies[i].xs[j];
            allBodies[i].pys[j] = M_PER_PX_RB * allBodies[i].ys[j];

            sumX += allBodies[i].pxs[j];
            sumY += allBodies[i].pys[j];

            // printf("\nsX:%d", signX);
            // printf("\nsY:%d\n\n", signY);

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

            if(j!=0) runningAreaCount += (allBodies[i].pxs[j-1] + allBodies[i].pxs[j]) * (allBodies[i].pys[j-1] - allBodies[i].pys[j]) / 2.0;

        }

        runningAreaCount += (allBodies[i].pxs[VERTICIES_PER_BODY-1] + allBodies[i].pxs[0]) * (allBodies[i].pys[VERTICIES_PER_BODY-1] - allBodies[i].pys[0]) / 2.0;

        allBodies[i].cx = sumX / (float)VERTICIES_PER_BODY;
        allBodies[i].cy = sumY / (float)VERTICIES_PER_BODY;

        float maxX = -1;
        float maxY = -1;
        float minX = M_PER_PX_RB * MAX_X;
        float minY = M_PER_PX_RB * MAX_Y;

        for (int j = 0; j < VERTICIES_PER_BODY; j++) {

            maxX = allBodies[i].pxs[j] > maxX ? allBodies[i].pxs[j] : maxX;
            maxY = allBodies[i].pys[j] > maxY ? allBodies[i].pys[j] : maxY;
            minX = allBodies[i].pxs[j] < minX ? allBodies[i].pxs[j] : minX;
            minY = allBodies[i].pys[j] < minY ? allBodies[i].pys[j] : minY;

            float dx = allBodies[i].pxs[j] - allBodies[i].cx;
            float dy = allBodies[i].pys[j] - allBodies[i].cy;

            allBodies[i].vDistances[j] = sqrt(dx*dx+dy*dy);
            allBodies[i].constThetas[j] = atan2(dy, dx);

        }

        allBodies[i].maxPX = maxX * PX_PER_M_RB;
        allBodies[i].maxPY = maxY * PX_PER_M_RB;
        allBodies[i].minPX = minX * PX_PER_M_RB;
        allBodies[i].minPY = minY * PX_PER_M_RB;

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

void updateRBMinsAndMaxes(int i){

    int cMaxX = INT_MIN_C;
    int cMaxY = INT_MIN_C;
    int cMinX = INT_MAX_C;
    int cMinY = INT_MAX_C;
    for (int vertIdx = 0; vertIdx < VERTICIES_PER_BODY; vertIdx++) {
        if (allBodies[i].xs[vertIdx] > cMaxX) cMaxX = allBodies[i].xs[vertIdx];
        if (allBodies[i].xs[vertIdx] < cMinX) cMinX = allBodies[i].xs[vertIdx];
        if (allBodies[i].ys[vertIdx] > cMaxY) cMaxY = allBodies[i].ys[vertIdx];
        if (allBodies[i].ys[vertIdx] < cMinY) cMinY = allBodies[i].ys[vertIdx];
    }
    allBodies[i].maxPX = cMaxX;
    allBodies[i].minPX = cMinX;
    allBodies[i].maxPY = cMaxY;
    allBodies[i].minPY = cMinY;
    // if (i == 0) {
    //     printf("maxX:%d\n", cMaxX);
    //     printf("minX:%d\n", cMinX);
    //     printf("maxY:%d\n", cMaxY);
    //     printf("minY:%d\n", cMinY);
    // }

}

void stepBodyPositions(int i) {

    // Must also update positions of all verticies

    float pt = allBodies[i].theta;
    if(allBodies[i].cLast){
        allBodies[i].omega *= ELASTICITY_RB * EPSILON_RB;
        allBodies[i].omega *= ELASTICITY_RB * EPSILON_RB;
    }
    bool goStep = !allBodies[i].cLast || (allBodies[i].cLast && (allBodies[i].lastPositionDelta < MAX_COLLISIONPT_DELTA));
    if (i!=currentMouseInteractionObj && goStep){

        if(-EPSILON_RB > allBodies[i].omega || EPSILON_RB < allBodies[i].omega){
            allBodies[i].theta += allBodies[i].omega * SPH_RB;
        }
        if(-EPSILON_RB > allBodies[i].v.x || EPSILON_RB < allBodies[i].v.x) {
            allBodies[i].cx += allBodies[i].v.x * SPH_RB;
        }
        if(-EPSILON_RB > allBodies[i].v.y || EPSILON_RB < allBodies[i].v.y) {
            allBodies[i].cy += allBodies[i].v.y * SPH_RB;
        }
        
    } else if (!goStep) {

        //Kill everything.
        for(int k = 0; k < NUM_BODIES; k++){
            allBodies[i].extForces[k].force.x = 0;
            allBodies[i].extForces[k].force.y = 0;
            allBodies[i].extForces[k].isActive = false;
        }
        allBodies[i].alpha = 0;
        // allBodies[i].v.x *= ELASTICITY_RB;
        // allBodies[i].v.y *= ELASTICITY_RB;
        if(-EPSILON_RB > allBodies[i].v.x || EPSILON_RB < allBodies[i].v.x) {
            allBodies[i].cx += allBodies[i].v.x * SPH_RB;// * 0.01;
        }
        if(-EPSILON_RB > allBodies[i].v.y || EPSILON_RB < allBodies[i].v.y) {
            allBodies[i].cy += allBodies[i].v.y * SPH_RB;// * 0.01;
        }
    }
    // else {
    //     allBodies[i].cx = mData.x;
    //     allBodies[i].cy = mData.y;
    //     allBodies[i].omega = 0;
    //     allBodies[i].v.x = 0.0;
    //     allBodies[i].v.y = 0.0;
    // }
    
    // Revert last position application if any vert out of bounds.
    bool mustAdjust = false;

    float ndx, ndy, nt;

    for (int j = 0; j < VERTICIES_PER_BODY; j++) {    

        // eraseRBs[i].xs[j] = allBodies[i].xs[j];
        // eraseRBs[i].ys[j] = allBodies[i].ys[j];

        nt = allBodies[i].constThetas[j] + allBodies[i].theta;
        ndx = allBodies[i].vDistances[j] * cos(nt);
        ndy = allBodies[i].vDistances[j] * sin(nt);

        allBodies[i].pxs[j] = allBodies[i].cx + ndx;
        allBodies[i].pys[j] = allBodies[i].cy + ndy;

        allBodies[i].xs[j] = PX_PER_M_RB * allBodies[i].pxs[j];
        allBodies[i].ys[j] = PX_PER_M_RB * allBodies[i].pys[j];
        
        // Logic for necessary aadjustment if any
        if (allBodies[i].xs[j] < 0 ){
            allBodies[i].cx += 0 - M_PER_PX_RB * allBodies[i].xs[j];
            mustAdjust = true;
        } else if (allBodies[i].xs[j] > (MAX_X-1)){
            allBodies[i].cx -=  M_PER_PX_RB * (allBodies[i].xs[j] - MAX_X + 1);
            mustAdjust = true;
        }
        if (allBodies[i].ys[j] < 0 ){
            allBodies[i].cy += 0 - M_PER_PX_RB * allBodies[i].ys[j];
            mustAdjust = true;
        } else if (allBodies[i].ys[j] > (MAX_Y-1)){
            allBodies[i].cy -= M_PER_PX_RB * (allBodies[i].ys[j] - MAX_Y + 1);
            mustAdjust = true;
        }

    }
    if(mustAdjust) resetBodyFromCenter(i);
    updateRBMinsAndMaxes(i);

}

void stepBodyVelocities(int i) {

    allBodies[i].omega += allBodies[i].alpha * SPH_RB;
    allBodies[i].v.x += allBodies[i].a.x * SPH_RB;
    allBodies[i].v.y += allBodies[i].a.y * SPH_RB;

    allBodies[i].colour = hueToRGB565(RB_HUE-sqrt(allBodies[i].v.x*allBodies[i].v.x + allBodies[i].v.y*allBodies[i].v.y)/VELOCITY_COLOUR_SENSITIVITY_RB);

}

void checkCollisions(int i) {

    int collisionCount = 0;
    checkSATInterBodyCollision(i);
    
    for (int j = 0; j < VERTICIES_PER_BODY; j++) {

        bool setActive = false;

        // Container collision handling
        if(allBodies[i].xs[j] >= (MAX_X-1) || allBodies[i].xs[j] <= 0) {
            
            if((allBodies[i].v.x > 0) == (allBodies[i].xs[j] > 0)) {
                allBodies[i].v.x = -allBodies[i].v.x * ELASTICITY_RB;
            }

            allBodies[i].extForces[j].force.x = -allBodies[i].mass * allBodies[i].a.x;
            setActive = true;

        }
        if(allBodies[i].ys[j] >= (MAX_Y-1) || allBodies[i].ys[j] <= 0) {

            if((allBodies[i].v.y > 0) == (allBodies[i].ys[j] >= (MAX_Y-1))) {
                allBodies[i].v.y = -allBodies[i].v.y * ELASTICITY_RB;
            }
            allBodies[i].extForces[j].force.y = -allBodies[i].mass * allBodies[i].a.y;
            if(allBodies[i].ys[j] >= (MAX_Y-1)) collisionCount++;;
            setActive = true;
            

        }

        if (collisionCount > 1) {
            for(int k = 0; k <= j; k++){
                allBodies[i].extForces[k].force.x = 0;
                allBodies[i].extForces[k].force.y = 0;
                allBodies[i].extForces[k].isActive = false;
            }
            allBodies[i].alpha = 0;
            allBodies[i].v.x = 0;
            allBodies[i].v.y = 0;
            allBodies[i].a.x = 0;
            allBodies[i].a.y = 0;
            continue;
        }
        
        if (setActive) {
            //if((allBodies[i].omega > 0) == (allBodies[i].alpha > 0)) allBodies[i].omega = -allBodies[i].omega * ELASTICITY_RB;
            allBodies[i].omega *= ELASTICITY_RB * (0.001);
            allBodies[i].extForces[j].r.x = allBodies[i].pxs[j] - allBodies[i].cx;
            allBodies[i].extForces[j].r.y = allBodies[i].pys[j] - allBodies[i].cy;
        } else {
            allBodies[i].extForces[j].force.x = 0;
            allBodies[i].extForces[j].force.y = 0;
        }
        allBodies[i].extForces[j].isActive = setActive;

    }

}

void checkMouseLocation(int i) {

    if (!mData.left) {
        currentMouseInteractionObj = -1;
        return;
    }
    if((allBodies[i].minPX <= mData.x) && (mData.x <= allBodies[i].maxPX) && (allBodies[i].minPY <= mData.y) && (mData.y <= allBodies[i].maxPY)) {
        if(currentMouseInteractionObj == -1) currentMouseInteractionObj = i;   
    }

}

void timeStepRBForceApplication() {

    // model collisions with normal forces.
    for (int i = 0; i < NUM_BODIES; i++) {   
        for (int j = 0; j < VERTICIES_PER_BODY; j++) {
            eraseRBs[i].xs[j] = allBodies[i].xs[j];
            eraseRBs[i].ys[j] = allBodies[i].ys[j];
            bookMarkedCollisions[i][j] = false;
        }

        if (i==currentMouseInteractionObj) {
            allBodies[i].cx = mData.x;
            allBodies[i].cy = mData.y;
            allBodies[i].omega = 0;
            allBodies[i].v.x = M_PER_PX_RB * (float)mData.vx;
            allBodies[i].v.y = M_PER_PX_RB * (float)mData.vy;
        }
    }
    for (int i = 0; i < NUM_BODIES; i++) {   

        if (i != currentMouseInteractionObj) {

            allBodies[i].a.x = 0;
            allBodies[i].a.y = G_RB;
            
            float torque = 0;
            for (int j = 0; j < MAX_EXTERNAL_FORCES; j++) {
                if (!allBodies[i].extForces[j].isActive) continue;
                allBodies[i].extForces[j].isActive = false;
                // allBodies[i].a.x += allBodies[i].extForces[j].force.x / allBodies[i].mass;
                // allBodies[i].a.y += allBodies[i].extForces[j].force.y / allBodies[i].mass;
                torque += allBodies[i].extForces[j].r.x * allBodies[i].extForces[j].force.y - allBodies[i].extForces[j].r.y * allBodies[i].extForces[j].force.x;
            }
            
            allBodies[i].alpha = torque / allBodies[i].I;
            

        } 

        checkMouseLocation(i);
        checkCollisions(i);
        stepBodyVelocities(i);

        stepBodyPositions(i);

    }

}

// =======================================================================================================
//                                                   MAIN
// =======================================================================================================

void switchSimHandler() {
    isFluidSim = !isFluidSim;
    speedMult = 0;
    resetSimHandler();
}

void resetSimHandler() {
    clearWholeScreen();
    if (isFluidSim) {
        initParticles(); 
    }
    else {
        initRigidBodies();
    }
    // speedMult = 4;
    // fastFowardHandler();
}

void fastFowardHandler(){
    if(speedMult == 4)speedMult = 0;
    else speedMult++;
    SPF = speedArray[speedMult] * DEFAULT_SPF;
    SPH_RB = speedArray[speedMult] * DEFAULT_SPH_RB;
}

int main(void){ // main for this simulation

    // volatile int * sw_ptr = (volatile int *)SW_BASE;

    play = true;
    initParticles();
    initRigidBodies();

    intializeMouse(&mData);
    prevmData = mData;

    vgaSetup();

    // Program loop
    while(1) {
        
        // errSt = (*sw_ptr & 1) == 1;

        // if(errSt!=errStLast) resetSimHandler();
        // errStLast = errSt;
        // if (isFluidSim != lastFluidSim) {
        //     if (lastFluidSim) eraseParticles();
        //     else eraseBodies();
        // }

        
        lastFluidSim = isFluidSim;
        // Erase Stuff
        if (isFluidSim) eraseParticles();
        else eraseBodies();

        drawMouse(&prevmData, BLACK);

        // Draw Stuff
        if (isFluidSim) drawParticles();
        else drawBodies();

        drawButtons();
        drawMouse(&mData, WHITE);
        prevmData = mData;
        
        // Update Stuff 
        if(play){
            if (isFluidSim) timeStepBucketwiseParticleUpdate();
            else timeStepRBForceApplication();
        }

        // Wait for Stuff
        waitForVsync();

    }

    return 0;

}