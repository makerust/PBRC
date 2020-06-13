#include <SPI.h>
#include "Adafruit_MAX31855.h"
#include <LiquidCrystal.h>
#include <SoftwareSerial.h>

#define DEBUG 1

int thermocoupleSOPin  = 12;
int thermocoupleCLKPin = 10;
int thermocoupleCS0Pin = 11; //changed from 11 to 1
//int thermocoupleCS1Pin = 12;
//int thermocoupleCS2Pin = 13;
int heat1Pin           = A1;// heating element
int fanPin             = 3;
int lcdRsPin           = 8;
int lcdEPin            = 9;
int lcdD4Pin           = 4;
int lcdD5Pin           = 5;
int lcdD6Pin           = 6;
int lcdD7Pin           = 7;
//int ledRedPin        = 10;
int switch1Pin         = A0;//the "Right" Switch

int switchAVal         = 0;//analog read value for the switch
int fanPot             = A5 ;// Potentionmeter for the fan
bool resetLCD          = 0;        
          


int debounceTime       = 50;

//switches
static int nullVal     = 600;
static int leftVal     = 260;
static int rightVal    = 80;

bool switchRelease     =0;


const int rollIndx     = 5; //index for rolling average
int readIndx           = 0; //reading index
double circ_c[rollIndx];
double tempAvg         = 0;
double loopTemp        = 0;// debug

float deltaTempCurr    = 22.0;
float deltaTempPrev    = 22.0;
float deltaTempDelta   = 0;
bool  deltaLoop        = 0;

struct MinSec {
  unsigned int Min;
  unsigned int Sec;
};

MinSec roastTime = {0,0};

unsigned long elapsedTime     = 0;

typedef enum ROAST_STATUS
{
  S_IDLE,
  S_ROAST,
  S_ROAST_LOW,
} roastStatus_t;

roastStatus_t roastStatus = S_IDLE;

typedef enum SWITCH_STATE
{
  NULL_VAL,
  LEFT_VAL,
  RIGHT_VAL,
} switchState_t;  

switchState_t switchStateCurr = NULL_VAL;
switchState_t switchStatePrev = NULL_VAL;


LiquidCrystal lcd(lcdRsPin, lcdEPin, lcdD4Pin, lcdD5Pin, lcdD6Pin, lcdD7Pin);


Adafruit_MAX31855 thermocouple0(thermocoupleCLKPin, thermocoupleCS0Pin, thermocoupleSOPin);

MinSec MinSecfromMillis(unsigned long x){
  unsigned long runSec = x / 1000;
  MinSec Result;
  Result.Min = runSec / 60;
  Result.Sec = runSec % 60;
  return Result;
}

switchState_t switchResult(int x ){
  switchState_t swState;
  
  if (x <= rightVal){
    swState = RIGHT_VAL; 
  }
  else if( x >= leftVal && x <= nullVal){
    swState = LEFT_VAL;
  }
  else {
    swState = NULL_VAL;
  }
  
  return swState;
}

int confinePot(int x){
 return map(x, 0, 1023, 0, 255);
}

int confinePotHeat(int x){
  int y;
  y = map(x, 0, 1023, 0, 255);
  if(y < 77){//disallow setting the fan too low when there is heat
    y = 77;
  }
  
  return y;
}

double avgTemp(){

  double tempSum = 0;  
  
  double temp_c1 = thermocouple0.readCelsius();
  #ifdef DEBUG
  Serial.print("READ: ");
  Serial.print(temp_c1);
  Serial.print(" ");
  #endif
  
  if (isnan(temp_c1)){
    circ_c[readIndx]=tempAvg;
  }
  else{
    circ_c[readIndx]=temp_c1;
  }

  for (int i = rollIndx; i>0; i--){
     tempSum += circ_c[i-1];
     #ifdef DEBUG
     Serial.print(i-1);
     Serial.print(": ");
     Serial.print(circ_c[i-1], 2);
     Serial.print(" ");  
     #endif   
  }
  
  tempAvg = (tempSum / rollIndx);

  if (readIndx >= rollIndx){
    readIndx = 0;
  }
  else{
    readIndx ++;  
  }
  #ifdef DEBUG
  Serial.print("AVG: ");
  Serial.print(tempAvg, 2);
  Serial.print("\n");
  #endif
  return tempAvg;
}

void setup() {
  // put your setup code here, to run once:
  


  #ifdef DEBUG
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  #endif

  lcd.begin(16, 2);
  //lcd.createChar(0, degree);
  lcd.clear();
  lcd.print("Roast");
  lcd.setCursor(0, 1);
  lcd.print("Control 0.2");


  // SSR pin initialization to ensure reflow oven is off
  pinMode(heat1Pin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  digitalWrite(heat1Pin, LOW);
  analogWrite(fanPin, 0);

  pinMode(switch1Pin, INPUT);

  // Start-up splash

  for(int i = rollIndx; i == 0; i--){ 
  circ_c[i]=thermocouple0.readCelsius();
  }

  delay(1000);
  lcd.clear();
  digitalWrite(lcdRsPin, LOW);//Stop errant writes
                              //Why there is no native function for this idk
                              
}





void loop() {

    // Serial communication at 9600 bps

//************ROAST***********************************************************************************

  switch(roastStatus) {
    case S_ROAST:
      digitalWrite(heat1Pin, HIGH);//set fan and heat to on
      analogWrite(fanPin, confinePotHeat(analogRead(fanPot)));//This gross. It reads the POT, maps to the right valuespace and writes to an anal out
      
      
      loopTemp = avgTemp();
      roastTime = MinSecfromMillis(millis()-elapsedTime);//-elapsedTime

      #ifdef DEBUG
      Serial.println(roastTime.Min);
      Serial.println(roastTime.Sec, 2);
//      Serial.println(millis);
      Serial.println(elapsedTime);
      Serial.println(loopTemp);
      Serial.println();
      #endif
      
      if(roastTime.Sec % 10 == 0 && resetLCD ==0){
        lcd.begin(8,2);
        resetLCD = 1;
      }
      else if(roastTime.Sec % 10 != 0){
        resetLCD=0;
      }
      if(roastTime.Sec % 2 == 0 && deltaLoop == 0){
        deltaLoop = 1;
        deltaTempCurr = loopTemp;
        deltaTempDelta = (deltaTempCurr - deltaTempPrev);
        deltaTempPrev=deltaTempCurr;  
      }
      else if(roastTime.Sec % 2 != 0)
        deltaLoop=0;

      lcd.setCursor(0,0);
      lcd.print("Heat ");
      lcd.setCursor(5, 0);
      lcd.print(roastTime.Min);
      lcd.setCursor(7,0);
      lcd.print(":");
      lcd.setCursor(8,0);
      if (roastTime.Sec >9){//This is the easiest way I could find of making the seconds place always print two digits
        lcd.print(roastTime.Sec);
      }
      else{
        lcd.print(0);
        lcd.print(roastTime.Sec);  
      }
      lcd.setCursor(10,0);
      lcd.print(" D");
      lcd.setCursor(12,0);
      lcd.print(deltaTempDelta , 1);
      lcd.setCursor(0, 1);
      lcd.print(loopTemp,1);
      lcd.setCursor(7, 1);
      lcd.print(thermocouple0.readInternal(),1);
      digitalWrite(lcdRsPin, LOW);//Stop errant lcd writes


      switchAVal = analogRead(switch1Pin); //Read buttons
      switchStateCurr = switchResult(switchAVal);

      #ifdef DEBUG
      Serial.flush();
      Serial.print(" Switch Curr: ");
      Serial.print(switchStateCurr);
      Serial.print(" Switch Prev: ");
      Serial.println(switchStatePrev);
      #endif      

      if(switchStateCurr == 0 && switchStatePrev != 0){
        if (switchStatePrev == 2){      
            lcd.clear();
            digitalWrite(lcdRsPin, LOW);//Stop errant lcd writes
            roastStatus = S_IDLE;
          }
      
        else if (switchStatePrev == 1){
          lcd.clear();
          digitalWrite(lcdRsPin, LOW);//Stop errant lcd writes      
          roastStatus = S_ROAST_LOW;
        }
      }
      else {
        roastStatus = S_ROAST;
      }

      switchStatePrev=switchStateCurr;
      
      break;

//***********************IDLE******************************************************************

    case S_IDLE:
      digitalWrite(heat1Pin, LOW);//set fan and heat to off
      digitalWrite(fanPin, LOW);      //set fan and heat to off

      loopTemp=avgTemp();
      
      lcd.setCursor(0,0);
      lcd.print("Idle");
      lcd.setCursor(0, 1);
      lcd.print(loopTemp,1);
      lcd.setCursor(7, 1);
      lcd.print(thermocouple0.readInternal(),1);
      digitalWrite(lcdRsPin, LOW);//Stop errant writes



//    Going analog
      switchAVal = analogRead(switch1Pin);
      switchStateCurr = switchResult(switchAVal);

      #ifdef DEBUG
      Serial.print("Switch Curr: ");
      Serial.print(switchStateCurr);
      Serial.print(" Switch Prev: ");
      Serial.println(switchStatePrev);
      #endif      

     if (switchStateCurr == 0 && switchStatePrev == 2){
            lcd.clear();
            digitalWrite(lcdRsPin, LOW);//Stop errant lcd writes
            elapsedTime=millis();
            roastStatus = S_ROAST;
          }

     
      else {
        roastStatus = S_IDLE;
      }
      switchStatePrev=switchStateCurr;     
      break;

      
//*****************************ROAST**LOW*************************************************************

    case S_ROAST_LOW:
      digitalWrite(heat1Pin, LOW);//set fan and heat to off
      analogWrite(fanPin, confinePot(analogRead(fanPot)));//This gross. It reads the POT, maps to the right valuespace and writes to an anal out
      
      roastTime = MinSecfromMillis(millis()-elapsedTime);
      loopTemp = avgTemp();

      if(roastTime.Sec % 10 == 0 && resetLCD ==0){
        lcd.begin(8,2);
        resetLCD = 1;
      }
      else if(roastTime.Sec % 10 !=0) {
        resetLCD=0;
      }

      if(roastTime.Sec % 2 == 0 && deltaLoop == 0){
        deltaLoop = 1;
        deltaTempCurr = loopTemp;
        deltaTempDelta = (deltaTempCurr - deltaTempPrev);
        deltaTempPrev=deltaTempCurr;  
      }
      else if(roastTime.Sec % 2 != 0)
        deltaLoop=0;      
          
      lcd.setCursor (0, 0);
      lcd.print("Low ");
      lcd.setCursor(5, 0);
      lcd.print(roastTime.Min);
      lcd.setCursor(7,0);
      lcd.print(":");
      lcd.setCursor(8,0);
      if (roastTime.Sec >9){
        lcd.print(roastTime.Sec);
      }
      else{
        lcd.print(0);
        lcd.print(roastTime.Sec);  
      }
      lcd.setCursor(10,0);
      lcd.print(" D");
      lcd.setCursor(12,0);
      lcd.print(deltaTempDelta , 1);
      
      lcd.setCursor(0, 1);
      lcd.print(loopTemp,1);
      lcd.setCursor(7, 1);
      lcd.print(thermocouple0.readInternal(),1);      
      digitalWrite(lcdRsPin, LOW);//Stop errant lcd writes

      switchAVal = analogRead(switch1Pin);
      switchStateCurr = switchResult(switchAVal);

      #ifdef DEBUG
      //Serial.flush();
      Serial.print("Switch Curr: ");
      Serial.print(switchStateCurr);
      Serial.print(" Switch Prev: ");
      Serial.println(switchStatePrev);
      #endif 

      if(switchStateCurr == 0 && switchStatePrev != 0){
        if (switchStatePrev == 2){
          lcd.clear();
          digitalWrite(lcdRsPin, LOW);//Stop errant lcd writes
          roastStatus = S_IDLE;
          }
              
        else if (switchStatePrev == 1){
            lcd.clear();
            digitalWrite(lcdRsPin, LOW);//Stop errant lcd writes
            roastStatus = S_ROAST;
            }
        
      else {
        roastStatus = S_ROAST_LOW;
        }
      }

      switchStatePrev=switchStateCurr;
      break;

//------------DEFAULT
    default:
      digitalWrite(heat1Pin, LOW);//set fan and heat to off
      digitalWrite(fanPin, LOW);


      roastStatus = S_IDLE;
      break;
  }
  
}
