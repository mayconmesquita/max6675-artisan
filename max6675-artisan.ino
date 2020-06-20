/****************************************************************************
 * Max 6775 + Artisan + RoasterLogger [Arduino UNO]
 * 
 * Version = 1.0;
 * 
 * Author = Maycon Mesquita
 *
 * Sends data and receives instructions as key=value pairs.                 
 * Sends temperatures T1 and T2 at sendTimePeriod intervals as "T1=123.6" 
 * 
 * How to configure Artisan:
 * 1 - Config > Device > Select TC4
 * 2 - Port Configuration, insert:
 * - Your COM Port (eg. COM3)
 * - Baud Rate: 115200
 * - Byte Size: 8
 * - Parity: N
 * - Stopbits: 1
*****************************************************************************/

#include <Wire.h>
#include <SPI.h>

#define FAHRENHEIT // To output data in Celsius comment out this one line 
#define DP 1  // No. decimal places for serial output
#define maxLength 30   // maximum length for strings used

bool isArtisan = true;

// thermocouple reading Max 6675 pins
const int SO  = 8;   // SO pin on MAX6675
const int SCKa = 10; // SCKa pin on MAX6675
const int CS1 = 9;   // CS (chip 1 select) pin on MAX6675
const int CS2 = 7;   // CS (chip 2 select) pin on MAX6675 [optional: a second probe]

// thermocouple settings
float calibrate1 = 0.0; // Temperature compensation for T1
float calibrate2 = 0.0; // Temperature compensation for T2

// temporary values for temperature to be read
float temp1 = 0.0;                   // temporary temperature variable
float temp2 = 0.0;                   // temporary temperature variable 
float t1 = 0.0;                      // Last average temperature on thermocouple 1 - average of four readings
float t2 = 0.0;                      // Last average temperature on thermocouple 2 - average of four readings
float tCumulative1 = 0.0;            // cumulative total of temperatures read before averaged
float tCumulative2 = 0.0;            // cumulative total of temperatures read before averaged
int noGoodReadings1 = 0;             // counter of no. of good readings for average calculation 
int noGoodReadings2 = 0;             // counter of no. of good readings for average calculation

int inByte = 0;                      // incoming serial byte
String inString = String(maxLength); // input String

// loop control variables
unsigned long lastTCTimerLoop;       // for timing the thermocouple loop
int tcLoopCount = 0;                 // counter to run serial output once every 4 loops of 250 ms t/c loop

void setup() {
  Serial.begin(115200, SERIAL_8N1); // IMPORTANT: Set this in Artisan!

  // use establish contact if you want to wait until 'A' sent to Arduino before start - not used in this version
  // establishContact();  // send a byte to establish contact until receiver responds 

  // set up pin modes for Max6675's
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(SO, INPUT);
  pinMode(SCKa, OUTPUT);

  // deselect both Max6675's
  digitalWrite(CS1, HIGH);
  digitalWrite(CS2, HIGH);
}

// this not currently used
void establishContact() {
  //  while (Serial.available() <= 0) {
  //    Serial.print('A', BYTE); // send a capital A
  //    delay(300);
  //  }

  Serial.println("Opened but no contact yet - send A to start");
  int contact = 0;
  while (contact == 0) {
    if (Serial.available() > 0) {
      inByte = Serial.read();
      Serial.println(inByte);
      if (inByte == 'A') { 
        contact = 1;
        Serial.println("Contact established starting to send data");
      }    
    }
  }  
}

/****************************************************************************
 * Called when an input string is received from computer
 * designed for key=value pairs or simple text commands. 
 * Performs commands and splits key and value 
 * and if key is defined sets value otherwise ignores
*****************************************************************************/
void doInputCommand() {
  float v = -1;
  inString.toLowerCase();
  int indx = inString.indexOf('=');

  if (indx < 0) {  // this is a message not a key value pair (Artisan)
    if (inString.equals("read")) {  // send data to artisan
      //         null   1-ET               2-BT         null
      String x = "0," + String(t2) + "," + String(t1) + ",0";
      Serial.println(x);
    }

    else if(inString.equals("chan;1200")) {
      isArtisan = true;
      Serial.println("# CHAN;1200");      
    }

    Serial.flush();
  } else {  // this is a key value pair for decoding (RoastLogger)
    String key = inString.substring(0, indx);
    String value = inString.substring(indx+1, inString.length());

    // parse string value and return float v
    char buf[value.length()+1];
    value.toCharArray(buf,value.length()+1);
    v = atof (buf);
  }
}

/****************************************************************************
 * check if serial input is waiting if so add it to inString.  
 * Instructions are terminated with \n \r or 'z' 
 * If this is the end of input line then call doInputCommand to act on it.
*****************************************************************************/
void getSerialInput() {
  // check if data is coming in if so deal with it
  if (Serial.available() > 0) {

    // read the incoming data as a char:
    char inChar = Serial.read();

    // if it's a newline or return or z, print the string:
    if ((inChar == '\n') || (inChar == '\r') || (inChar == 'z')) {

      // do whatever is commanded by the input string
      if (inString.length() > 0) doInputCommand();

      inString = ""; // reset for next line of input
    } else {
      // if we are not at the end of the string, append the incoming character
      if (inString.length() < maxLength) {
        inString += inChar; 
      } else {
        inString = "";
        inString += inChar;
      }
    }

  }
}

/****************************************************************************
 * Send data to computer once every second.  Data such as temperatures, etc.
 * This allows current settings to be checked by the controlling program
 * and changed if, and only if, necessary.
 * This is quicker that resending data from the controller each second
 * and the Arduino having to read and interpret the results.
*****************************************************************************/
void doSerialOutput() {
  if (isArtisan) {
    return;
  }

  Serial.print("t1=");
  Serial.println(t1, DP);

  Serial.print("t2=");
  Serial.println(t2, DP);
}

/****************************************************************************
 * Read temperatures from Max6675 chips Sets t1 and t2, -1 if an error
 * occurred.  Max6675 needs 240 ms between readings or will return last
 * value again. I am reading it once per second.
*****************************************************************************/
void getTemperatures() {
 temp1 = readThermocouple(CS1, calibrate1);
 temp2 = readThermocouple(CS2, calibrate2);  
  
 if (temp1 > 0.0) {
    tCumulative1 = tCumulative1 + temp1;
    noGoodReadings1 ++;
 }

 if (temp2 > 0.0) {
    tCumulative2 = tCumulative2 + temp2;
    noGoodReadings2 ++;
 }
}

/****************************************************************************
 * Called by main loop once every 250 ms
 * Used to read each thermocouple once every 250 ms
 *
 * Once per second averages temperature results, updates potentiometer and outputs data
 * to serial port.
*****************************************************************************/
void do250msLoop() {
  getTemperatures();

  if (tcLoopCount > 3) { // once every four loops (1 second) calculate average temp, update Pot and do serial output
    tcLoopCount = 0;

    if (noGoodReadings1 > 0)  t1 = tCumulative1 / noGoodReadings1; else t1 = -1.0;
    if (noGoodReadings2 > 0)  t2 = tCumulative2 / noGoodReadings2; else t2 = -1.0;
    noGoodReadings1 = 0;
    noGoodReadings2 = 0;
    tCumulative1 = 0.0;
    tCumulative2 = 0.0;

    #ifdef FAHRENHEIT
      if (t1 > 0) t1 = (t1 * 9 / 5) + 32;
      if (t2 > 0) t2 = (t2 * 9 / 5) + 32;
    #endif

    doSerialOutput(); // once per second
  }

  tcLoopCount++;
}

/****************************************************************************
 * Main loop must not use delay!  PWM heater control relies on loop running
 * at least every 40 ms.  If it takes longer then heater will be on slightly
 * longer than planned. Not a big problem if 1% becomes 1.2%! But keep loop fast.
 * Currently loop takes about 4-5 ms to run so no problem.
*****************************************************************************/
void loop() {
  getSerialInput();// check if any serial data waiting

  // loop to run once every 250 ms to read TC's
  if (millis() - lastTCTimerLoop >= 250) {
    lastTCTimerLoop = millis();
    do250msLoop();
  }
}


/*****************************************************************
 * Read the Max6675 device 1 or 2.  Returns temp as float or  -1.0
 * if an error reading device.
 * Note at least 240 ms should elapse between readings of a device
 * to allow it to settle to new reading.  If not the last reading 
 * will be returned again.
******************************************************************/
float readThermocouple(int CS, float calibrate) { //device selected by passing in the relavant CS (chip select)
  int value = 0;
  int error_tc = 0;
  float temp = 0.0;

  digitalWrite(CS, LOW); // Enable device

  // wait for it to settle
  delayMicroseconds(1);

  // Cycle the clock for dummy bit 15
  digitalWrite(SCKa, HIGH);
  digitalWrite(SCKa, LOW);

  // wait for it to settle
  delayMicroseconds(1);

  /*
   Read bits 14-3 from MAX6675 for the Temp 
   Loop for each bit reading the value and 
   storing the final value in 'temp' 
  */
  for (int i=11; i>=0; i--) {
    digitalWrite(SCKa, HIGH);      // Set Clock to HIGH
    value += digitalRead(SO) << i; // Read data and add it to our variable
    digitalWrite(SCKa, LOW);       // Set Clock to LOW
  }

  // Read the TC input to check for error
  digitalWrite(SCKa, HIGH);        // Set Clock to HIGH
  error_tc = digitalRead(SO);      // Read data
  
  digitalWrite(SCKa, LOW);         // Set Clock to LOW
  digitalWrite(CS, HIGH);          // Disable device 1

  value = value + calibrate;       // Add the calibration value
  temp = (value*0.25);             // Multiply the value by 0.25 to get temp in ˚C

  // return -1 if an error occurred, otherwise return temp
  if (error_tc == 0) return temp; 
  else return -1.0; 
}
