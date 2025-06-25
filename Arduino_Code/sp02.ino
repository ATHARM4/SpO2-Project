/*
  This code will display the waveform as well as
  display the computed BPM and SPO2 onto the l2c display
  Author: Jason Huang
  Last Update: November 12, 2023
*/


/*
  Display libraries and dimension definitions
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels


#define RED_OFFSET 0.75   // Offset used for red signal frequency
#define IRED_OFFSET 1    // Offset used for ired signal frequency


// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// I want the display to update the frequency every 2s
float updateInterval = 2000;


// I want the display to plot a point of the waveform every 20 ms
// CHANGE THIS LATER IF NEEDED
float waveformInterval = 20;


int BPM;
int SPO2;


// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();


  // Line to format the display
  display.drawLine(0, 22, SCREEN_WIDTH - 1, 22, WHITE);
}


// the loop routine runs over and over again forever
// ASSUMPTION: the peak to peak is at least 2 volts.
void loop() {
  static float updateNextTime;
  static float updateNextWaveTime;


  float redVolt;
  float iredVolt;


  mainFSM();


  //The display will update the frequency every 2 s.
  if (millis() - updateNextTime >= updateInterval)
  {
    updateNextTime += updateInterval;
    displayStats();
  }
 
  // The display will update the waveform every 20 ms
  if (millis() - updateNextWaveTime >= waveformInterval) {
    updateNextWaveTime += waveformInterval;
    plotWaveform();
  }
}


/*
  The FSM that will compute the frequency.


  The state variable keeps track of what voltage values need to be measured
  State 0: Measure the firstMaxVolt
  State 1: Measure the firstMinVolt
  State 2: Measure the secondMaxVolt
  State 3: Measure the secondMinVolt


  After state 3 it will loop back to state 1.
*/
void mainFSM() {
  // Maximum and Minimums;
  static float firstMaxVolt, firstMaxVoltI;
  static float secondMaxVolt;
  static float firstMinVolt, firstMinVoltI;
  static float secondMinVolt;


  static unsigned long firstPeakTime;
  static unsigned long secondPeakTime;


  static float frequency[5];
  static float R[5];
  static float avgFrequency;


  static int state;


  // Voltage variables
  int sensorValue = analogRead(A0);
  int sensorValue2 = analogRead(A1);
  float currentVolt = sensorValue * (5.0 / 1023.0);
  //Serial.println(currentVolt);
  float currentVoltI = sensorValue2 *  (5.0 / 1023.0);
  unsigned long currentTime = millis();


  if (state == 0) {
    if (currentVolt > firstMaxVolt) {
      firstMaxVolt = currentVolt;
      firstMaxVoltI = currentVoltI;
      firstPeakTime = currentTime;
    }
   
    else if (currentVolt < (firstMaxVolt - RED_OFFSET)) {
      state = 1;
      firstMinVolt = currentVolt;
    }
  }


  if (state == 1) {
    if (currentVolt < firstMinVolt) {
      firstMinVolt = currentVolt;
      firstMinVoltI = currentVoltI;
    }


    else if (currentVolt > (firstMinVolt +  RED_OFFSET)) {
      state = 2;
      secondMaxVolt = currentVolt;
    }
  }


  if (state == 2) {
    if (currentVolt > secondMaxVolt) {
      secondMaxVolt = currentVolt;
      secondPeakTime = currentTime;
    }


    else if (currentVolt < (secondMaxVolt - RED_OFFSET)) {
      state = 3;
      static int i = 0;
      frequency[i] = 1/((float)(secondPeakTime - firstPeakTime) / 1000);
      float RedAC = (firstMaxVolt - firstMinVolt) / 2.0;
      float IRedAC = (firstMaxVoltI - firstMinVoltI) / 2.0;
      float RedDC = (analogRead(A2)  * (5.0 / 1023.0));
      float IRedDC = (analogRead(A3) * (5.0 / 1023.0));
      R[i] = (RedAC / IRedAC) * (IRedDC / RedDC);
      Serial.println(currentVolt);
      Serial.println(currentVoltI);


      i++;


      if (i == 5) {
        float freqAverage = 0;
        float avgR = 0;
        for (int j = 0; j < 5; j++) {
          freqAverage += frequency[j];
          avgR += R[j];
        }
        avgFrequency = (freqAverage / 5.0);
        avgR /= 5.0;
        Serial.println();
        BPM = round(60.0 * avgFrequency);
        SPO2 = round(110 - (25 * avgR));
        i = 0;
      }
      secondMinVolt = currentVolt;
    }
  }


  if (state == 3) {
    if (currentVolt < secondMinVolt) {
      secondMinVolt = currentVolt;
    }


    else if (currentVolt > (secondMinVolt +  RED_OFFSET)) {
      state = 0;
      firstMaxVolt = currentVolt;
    }
  }
  return round(60.0 * avgFrequency);
}


/*
  To update the frequency without modifying anything else on the display,
  I cleared the area underneath the waveform with a black rectangle and
  redisplayed the frequency.
*/
void displayStats() {
    display.fillRect(0, 23, SCREEN_WIDTH - 1, 9, BLACK);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 24);
    display.print("BPM: ");
    display.print(BPM);


    display.setCursor(70, 24);
    display.print("SPO2: ");
    display.print(SPO2);
    display.display();
}


/*
  To display the waveform, I took a sample every 20 ms and plotted it
  on the display. Since the display is limited to a height of 32 pixels,
  I had to convert a range of 0-5 volts to 0-21 pixel height since the rest
  of the heigh is used for the frequency display. The waveform plots from
  left to right. When the waveform reaches the right edge, it will clear the waveform
  and start from the left again.
*/
void plotWaveform() {
  // If you want to plot the Ired signal, use A1 instead of A0
  int sensorValue = analogRead(A0);
  float currentVolt = sensorValue * (5.0 / 1023.0);


  static int x = 11;
  int y = 21 - round(currentVolt * 4.2);


  if (x <= 127) {
    display.drawPixel(x++, y, WHITE);
    display.fillRect(0, 0, 10, 21, BLACK);
    display.fillRect(0, y, 10, (21 - y), WHITE);
    display.display();
  }


  else {
    x = 11;
    display.clearDisplay();
    display.drawLine(0, 22, SCREEN_WIDTH - 1, 22, WHITE);
    displayStats();
    display.drawPixel(x, y, WHITE);
    display.fillRect(0, 0, 10, 21, BLACK);
    display.fillRect(0, y, 10, (21 - y), WHITE);
    display.display();
  }
}
