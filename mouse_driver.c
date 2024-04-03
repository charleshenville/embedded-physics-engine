struct mouseData {
  int x;
  int y;
  bool left, right, middle;
};

typedef struct mouseData mouseData;

void clearFIFO() {
  volatile int *PS2_ptr = (int *)PS2_BASE;
  int PS2_data = *PS2_ptr;
  int RAVAIL = PS2_data>>16;
  while(RAVAIL > 0){
    PS2_data = *PS2_ptr;
    RAVAIL = PS2_data>>16;
  }
}

int updateMouse(mouseData *data) {
  volatile int *PS2_ptr = (int *)PS2_BASE;
  int PS2_data, RVALID;
  signed char inputData[3];
  char buttons;

  int i = 0;
  while(!i){
    PS2_data = *(PS2_ptr);
    RVALID = PS2_data & 0x8000;
    if(RVALID){
      inputData[0] = PS2_data & 0xFF;
      buttons = inputData[0] & 0xF;
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

  data->left = inputData[0] & 1;
  data->middle = inputData[0] & 4;
  data->right = inputData[0] & 2;

  data->x += inputData[1];
  data->y -= inputData[2];

  if (data->x > MAX_X) data->x = MAX_X - 1;
  if (data->y > MAX_Y) data->y = MAX_Y - 1;

  if (data->x < 0) data->x = 0;
  if (data->y < 0) data->y = 0;
  
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

void intializeMouse(mouseData *data) {
  volatile int * PS2_ptr = (int *)0xFF200100;
  int PS2_data, RVALID;
  char byte1 = 0, byte2 = 0;

  data->x = MAX_X / 2;
  data->y = MAX_Y / 2;

  data->left = false;
  data->middle = false;
  data->right = false;

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
}