// Includes

    #include "SD.h"                               // SD Card library, usually part of the standard install
    #include "driver/i2s_std.h"                   // Library of I2S routines, comes with ESP32 standard install
    #include "driver/gpio.h"
    #include "SPI.h"                              // I don't know if it's necessary

//------------------------------------------------------------------------------------------------------------------------

// Definitions

    // SD Card
    #define SD_CS       7                         // SD Card chip select
   
    // I2S
    #define I2S_DOUT    GPIO_NUM_3                // i2S Data out from ESP32 -> DIN pin
    #define I2S_BCLK    GPIO_NUM_1                // Bit clock
    #define I2S_LRC     GPIO_NUM_0                // Left/Right clock, also known as Frame clock or word select
    #define I2S_NUM     0                         // i2s port number

    // Wav File reading
    #define NUM_BYTES_TO_READ_FROM_FILE 1024      // How many bytes to read from wav file at a time

//------------------------------------------------------------------------------------------------------------------------

// Constants
    // Keyboard
    const int MASK1 = 1;                          // Mask for decoding what column to power
    const int MASK2 = 2;                          // Mask for decoding what column to power
    
    const int TICKLENGTH = 20;                    // Length of system power tick (in mili-seconds)
    const int COOLOFF = 200;                      // Amount of ticks before a new press could be interpreted
    const int NOTPRESSED = -1;                    // Arbitrary value denoting that no key was detected (must be less than 0 or greater than 3)
    
    const uint8_t COLS_NUM = 2;                   // Amount of column pins = log2(Actual amount of used columns in keypad), rounded up
    const uint8_t ROWS_NUM = 3;                   // Amount of row pins 
    const int COLS[COLS_NUM] = {8, 9};            // Define the column pins, green -> left, blue -> right
    const int ROWS[ROWS_NUM] = {21, 20, 10};      // Define the row pins, most significant to the left 
    static const i2s_port_t i2s_num = I2S_NUM_0;  // i2s port number

//------------------------------------------------------------------------------------------------------------------------

// Global variables
    // Keypad
    int currentColumn = 0;                        // Storing the current "column cycle"
    bool PRESSED = false;                         // Boolean if a button is pressed
    int pressedTimer = 0;                         // Cooldown of button presses

    // Wav files
    int fileSize=0;                               // Current played file size 
    File WavFile;                                 // Object for root of SD card directory

    // I2S configuration

    i2s_chan_handle_t tx_handle;
    /* Get the default channel configuration by the helper macro.
     * This helper macro is defined in `i2s_common.h` and shared by all the I2S communication modes.
     * It can help to specify the I2S role and port ID */
    i2s_chan_config_t chan_cfg;
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100), //48000
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK,
            .ws = I2S_LRC,
            .dout = I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false, //WS 0 -> Left Channel
            },
        },
    };

//------------------------------------------------------------------------------------------------------------------------

// Structs
 struct WavHeader_Struct
      {
          //   RIFF Section    
          char RIFFSectionID[4];                  // Letters "RIFF"
          uint32_t Size;                          // Size of entire file less 8
          char RiffFormat[4];                     // Letters "WAVE"
          
          //   Format Section    
          char FormatSectionID[4];                // letters "fmt"
          uint32_t FormatSize;                    // Size of format section less 8
          uint16_t FormatID;                      // 1=uncompressed PCM
          uint16_t NumChannels;                   // 1=mono,2=stereo
          uint32_t SampleRate;                    // 44100, 16000, 8000 etc.
          uint32_t ByteRate;                      // =SampleRate * Channels * (BitsPerSample/8)
          uint16_t BlockAlign;                    // =Channels * (BitsPerSample/8)
          uint16_t BitsPerSample;                 // 8,16,24 or 32
        

          // Data Section
          char DataSectionID[4];                  // The letters "data"
          uint32_t DataSize;                      // Size of the data that follows
      }WavHeader;
//------------------------------------------------------------------------------------------------------------------------


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
  delay(TICKLENGTH);
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
