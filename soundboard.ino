// Includes

    #include "SD.h"                         // SD Card library, usually part of the standard install
    #include "driver/i2s_std.h"                 // Library of I2S routines, comes with ESP32 standard install
    #include "driver/gpio.h"
    #include "SPI.h"

// Definitions

// SD Card
#define SD_CS       7          // SD Card chip select
   
// I2S
#define I2S_DOUT    GPIO_NUM_3    // i2S Data out from ESP32 -> DIN pin
#define I2S_BCLK    GPIO_NUM_1    // Bit clock
#define I2S_LRC     GPIO_NUM_0    // Left/Right clock, also known as Frame clock or word select
#define I2S_NUM     0             // i2s port number

// Constants



const int MASK1 = 1;                  // Mask for decoding what column to power
const int MASK2 = 2;                  // Mask for decoding what column to power

const int TICKLENGTH = 20;            // Length of system power tick (in mili-seconds)
const int COOLOFF = 200;              // Amount of ticks before a new press could be interpreted
const int NOTPRESSED = -1;             // Arbitrary value denoting that no key was detected (must be less than 0 or greater than 3)

const uint8_t COLS_NUM = 2;           // Amount of column pins = log2(Actual amount of used columns in keypad), rounded up
const uint8_t ROWS_NUM = 3;           // Amount of row pins 
int COLS[COLS_NUM] = {8, 9};          // Define the column pins, green -> left, blue -> right
int ROWS[ROWS_NUM] = {21, 20, 10};       // Define the row pins, most significant to the left 
int currentColumn = 0;                // Storing the current "column cycle"

bool PRESSED = false;

int pressedTimer = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.println("Starting!");
  //Initializing columns
  for(int i = 0; i<COLS_NUM; i++){
    pinMode(COLS[i], OUTPUT);
  }
  //Initializing rows
  for(int i = 0; i<ROWS_NUM; i++){
    pinMode(ROWS[i], INPUT);
  }
}

void loop() {

  //Power columns
  powerCols(currentColumn);

  //Read from rows
  if(!PRESSED){
    int currentRow = readRows();
    Serial.print("Current Row: ");
    Serial.println(currentRow);
    Serial.println("----------\n");
    
    //If match found!    
    if(currentRow != NOTPRESSED){
      Serial.print("Found match on row #");
      Serial.print(currentRow);
      Serial.print(" and column #");
      Serial.println(currentColumn);
      Serial.print("Value of key is: ");
      Serial.println(currentRow*4 + currentColumn + 1);
      PRESSED = true;
      delay(200);
    }
  }else{
    //If pressed
    // Serial.print(pressedTimer);
    // Serial.print("/");
    // Serial.print(COOLOFF);
    // Serial.println(" cool-off cycle");
    pressedTimer = pressedTimer + 1;
    
    //if timer is over cooloff limit
    if(pressedTimer > COOLOFF){
      pressedTimer=0;
      PRESSED = false;
    }
  }


  
  //Read from the board


  //Convert input from board to binary
  // String inBin = "";
  // for (int i = 0; i<4; i++){
  //   inBin += vin[i];
  // }

  // Serial.print("ON 1: ");
  // Serial.print(on1);
  // Serial.print(", ON 2: ");
  // Serial.print(on2);
  // Serial.print(", ON #: ");
  // Serial.print(on1 + on2*2);
  // Serial.print(", IN BIN: ");
  // Serial.println(inBin);
  delay(20);
  currentColumn = (currentColumn + 1) % 4;
 
}

void powerCols(int currentColumn){
  int STATE[COLS_NUM];
  if(0 <= currentColumn && currentColumn <= 3){
    STATE[0] = (currentColumn & MASK1);
    STATE[1] = (currentColumn & MASK2);
  }else{
    Serial.println("Current column out of bounds");
    return;
  }

  //Write to the board
  for(int i = 0; i<COLS_NUM; i++){
    digitalWrite(COLS[i], STATE[i]);
  }

}

int readRows(){
  int totalNum=0;
  for(int i = 0; i<ROWS_NUM; i++){
    totalNum *= 2;
    Serial.print("Checking pin #");
    Serial.print(ROWS[i]);
    Serial.print(" in row #");
    Serial.print(i+1);
    Serial.print(" read ");
    int digRead = digitalRead(ROWS[i]);
    Serial.println(digRead);
    totalNum += digRead;
    Serial.println(totalNum);
  }
  Serial.println("");
  return (totalNum - 1);
}
