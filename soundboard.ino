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


void PlayWav()
{
  static bool ReadingFile=true;                       // True if reading file from SD. false if filling I2S buffer
  static byte Samples[NUM_BYTES_TO_READ_FROM_FILE];   // Memory allocated to store the data read in from the wav file
  static uint16_t BytesRead;                          // Num bytes actually read from the wav file which will either be
                                                      // NUM_BYTES_TO_READ_FROM_FILE or less than this if we are very
                                                      // near the end of the file. i.e. we can't read beyond the file.
  //Serial.println(Samples[0]);

  if(ReadingFile)                                     // Read next chunk of data in from file if needed
  {
  // Serial.println("Reading");
    BytesRead=ReadFile(Samples);                      // Read data into our memory buffer, return num bytes read in
    ReadingFile=false;                                // Switch to sending the buffer to the I2S
  }
  else
    ReadingFile=FillI2SBuffer(Samples,BytesRead);     // We keep calling this routine until it returns true, at which point
  //  Serial.println("Filling");                        // this will swap us back to Reading the next block of data from the file.
                                                      // Reading true means it has managed to push all the data to the I2S 
                                                      // Handler, false means there still more to do and you should call this
                                                      // routine again and again until it returns true.
}

uint16_t ReadFile(byte* Samples)
{
    static uint32_t BytesReadSoFar=0;                   // Number of bytes read from file so far
    uint16_t BytesToRead;                               // Number of bytes to read from the file
    
    if(BytesReadSoFar+NUM_BYTES_TO_READ_FROM_FILE>WavHeader.DataSize)   // If next read will go past the end then adjust the 
      BytesToRead=WavHeader.DataSize-BytesReadSoFar;                    // amount to read to whatever is remaining to read
    else
      BytesToRead=NUM_BYTES_TO_READ_FROM_FILE;                          // Default to max to read
      
    WavFile.read(Samples,BytesToRead);                  // Read in the bytes from the file
    BytesReadSoFar+=BytesToRead;                        // Update the total bytes red in so far
    
    if(BytesReadSoFar>=WavHeader.DataSize)              // Have we read in all the data?
    {
      WavFile.seek(44);                                 // Reset to start of wav data  
      BytesReadSoFar=0;                                 // Clear to no bytes read in so far                            
    }
    // Serial.print("Bytes read so far: ");
    // Serial.println(BytesReadSoFar);
    // Serial.print("Bytes %: ");
    // Serial.print(BytesReadSoFar*100/fileSize);
    // Serial.println("%");
    return BytesToRead;                                 // return the number of bytes read into buffer
}

bool FillI2SBuffer(byte* Samples,uint16_t BytesInBuffer)
{
    // Writes bytes to buffer, returns true if all bytes sent else false, keeps track itself of how many left
    // to write, so just keep calling this routine until returns true to know they've all been written, then
    // you can re-fill the buffer
    
    size_t BytesWritten;                        // Returned by the I2S write routine, 
    static uint16_t BufferIdx=0;                // Current pos of buffer to output next
    uint8_t* DataPtr;                           // Point to next data to send to I2S
    uint16_t BytesToSend;                       // Number of bytes to send to I2S
    
    // To make the code eaier to understand I'm using to variables to some calculations, normally I'd write this calcs
    // directly into the line of code where they belong, but this make it easier to understand what's happening
    
    DataPtr=Samples+BufferIdx;                               // Set address to next byte in buffer to send out
    BytesToSend=BytesInBuffer-BufferIdx;                     // This is amount to send (total less what we've already sent)
    //Serial.print("Bytes to send: ");
    //Serial.println(BytesToSend);
    i2s_channel_write(tx_handle,DataPtr,BytesToSend,&BytesWritten,1);  // Send the bytes, wait 1 RTOS tick to complete
    BufferIdx+=BytesWritten;                                 // increasue by number of bytes actually written
    //Serial.print("Bytes in buffer: ");
    //Serial.println(BytesInBuffer);
    if(BufferIdx>=BytesInBuffer)                 
    {
      // sent out all bytes in buffer, reset and return true to indicate this
      BufferIdx=0; 
      return true;                             
    }
    else
      return false;       // Still more data to send to I2S so return false to indicate this
}

void SDCardInit(){   
    //Serial.println(SD_CS);
    //pinMode(SD_CS, OUTPUT); 
    //digitalWrite(SD_CS, HIGH); // SD card chips select, must use GPIO 5 (ESP32 SS)
    if(!SD.begin(SD_CS))
    {
        Serial.println("Error talking to SD card!");
        while(true);                  // end program
    }else{
      Serial.println("Done talking");
    }
}

bool ValidWavData(WavHeader_Struct* Wav){
  if(memcmp(Wav->RIFFSectionID,"RIFF",4)!=0) 
  {    
    Serial.print("Invalid data - Not RIFF format");
    return false;        
  }
  if(memcmp(Wav->RiffFormat,"WAVE",4)!=0)
  {
    Serial.print("Invalid data - Not Wave file");
    return false;           
  }
  if(memcmp(Wav->FormatSectionID,"fmt",3)!=0) 
  {
    Serial.print("Invalid data - No format section found");
    return false;       
  }
  if(memcmp(Wav->DataSectionID,"data",4)!=0) 
  {
    Serial.print("Invalid data - data section not found");
    return false;      
  }
  if(Wav->FormatID!=1) 
  {
    Serial.print("Invalid data - format Id must be 1");
    return false;                          
  }
  if(Wav->FormatSize!=16) 
  {
    Serial.print("Invalid data - format section size must be 16.");
    return false;                          
  }
  if((Wav->NumChannels!=1)&(Wav->NumChannels!=2))
  {
    Serial.print("Invalid data - only mono or stereo permitted.");
    return false;   
  }
  if(Wav->SampleRate>48000) 
  {
    Serial.print("Invalid data - Sample rate cannot be greater than 48000");
    return false;                       
  }
  if((Wav->BitsPerSample!=8)& (Wav->BitsPerSample!=16)) 
  {
    Serial.print("Invalid data - Only 8 or 16 bits per sample permitted.");
    return false;                        
  }
  return true;
}


void DumpWAVHeader(WavHeader_Struct* Wav)
{
  if(memcmp(Wav->RIFFSectionID,"RIFF",4)!=0)
  {
    Serial.print("Not a RIFF format file - ");    
    PrintData(Wav->RIFFSectionID,4);
    return;
  } 
  if(memcmp(Wav->RiffFormat,"WAVE",4)!=0)
  {
    Serial.print("Not a WAVE file - ");  
    PrintData(Wav->RiffFormat,4);  
    return;
  }  
  if(memcmp(Wav->FormatSectionID,"fmt",3)!=0)
  {
    Serial.print("fmt ID not present - ");
    PrintData(Wav->FormatSectionID,3);      
    return;
  } 
  if(memcmp(Wav->DataSectionID,"data",4)!=0)
  {
    Serial.print("data ID not present - "); 
    PrintData(Wav->DataSectionID,4);
    return;
  }  
  // All looks good, dump the data
  Serial.print("Total size :");Serial.println(Wav->Size);
  Serial.print("Format section size :");Serial.println(Wav->FormatSize);
  Serial.print("Wave format :");Serial.println(Wav->FormatID);
  Serial.print("Channels :");Serial.println(Wav->NumChannels);
  Serial.print("Sample Rate :");Serial.println(Wav->SampleRate);
  Serial.print("Byte Rate :");Serial.println(Wav->ByteRate);
  Serial.print("Block Align :");Serial.println(Wav->BlockAlign);
  Serial.print("Bits Per Sample :");Serial.println(Wav->BitsPerSample);
  Serial.print("Data Size :");Serial.println(Wav->DataSize);
}

void PrintData(const char* Data,uint8_t NumBytes){
    for(uint8_t i=0;i<NumBytes;i++)
      Serial.print(Data[i]); 
      Serial.println();  
}
