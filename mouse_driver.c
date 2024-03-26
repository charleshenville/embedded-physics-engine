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

void drawMouse(mouseData *data) {
  drawIndividualPixel(data->x, data->y, 0xFFFF);
  if (data->x > 0) drawIndividualPixel(data->x - 1, data->y, 0xFFFF);
  if (data->y > 0) drawIndividualPixel(data->x, data->y - 1, 0xFFFF);
  if (data->x < MAX_X - 1) drawIndividualPixel(data->x + 1, data->y, 0xFFFF);
  if (data->y < MAX_Y - 1) drawIndividualPixel(data->x, data->y + 1, 0xFFFF);
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