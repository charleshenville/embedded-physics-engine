#include <stdbool.h>
#include <stdio.h>

#define PS2_BASE 0xFF200100
#define MAX_X 319
#define MAX_Y 239
#define FPGA_PIXEL_BUF_BASE		0xff203020
#define MOUSE_RADIUS 2
#define BUTTON_X 301
#define BUTTON_Y 4

struct mouseData {
  int x;
  int y;
  bool left, right, middle;
};

typedef struct mouseData mouseData;

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
  prevmData = mData;
  updateMouse();
  if((mData.x >= BUTTON_X) && (mData.x < (BUTTON_X + 15)) && (mData.y >= BUTTON_Y) && (mData.y < (BUTTON_Y + 12))){
    if(!prevmData.left && mData.left){
      drawIndividualPixel(100,100,0xFFFF);
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

void drawButton(short int *colour){
  int x = BUTTON_X;
  int y = BUTTON_Y;
  
  for(int i = 0; i < 12; i++){
    for(int j = 0; j < 15; j++){
      drawIndividualPixel(x+j,y+i,colour[15*i + j]);
    }
  }
}

int main(){
  short int switchButton[180] = {0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xF800, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xF800, 0xF800, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xF800, 0xF800, 0xD6DA, 0xF800, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xF800, 0xF800, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xF800, 0xF800, 0xF800, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xF800, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xF800, 0xF800, 0xF800, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xF800, 0xF800, 0xF800, 0xF800, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x001F, 0x001F, 0x001F, 0x001F, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x001F, 0x001F, 0x001F, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x001F, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x001F, 0x001F, 0x001F, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x001F, 0x001F, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0x001F, 0xD6DA, 0x001F, 0x001F, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x001F, 0x001F, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 
                                 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA, 0xD6DA};

    intializeMouse();
    prevmData = mData;

    vgaSetup();

    

    // Program loop
    while(1) {
        drawMouse(&prevmData, 0);

        drawButton(switchButton);

        drawMouse(&mData, 0xFFFF);

        // Wait for Stuff
        waitForVsync();
    }
    return 0;
}