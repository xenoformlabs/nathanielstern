
/*
  Cyanthroplant - by Scott Kildall and Nathaniel Stern

  2 NeoPixel Strips: one for plant, one for human
  touch-activated when you put your fingers on two copper plates
  hardware is the MPR121 chip
  Use pins 0 and 1 for input on the chip (capMask = 3)
  
  MSTimer is a timing class used by Scott
  
*/
 
//-- Libraries
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPR121.h>
#include "MSTimer.h"

//-- NeoPixelStrip: illumination of plants
const int plantNeoPixelPin = 10;
const int numPlantPixels = 24;

Adafruit_NeoPixel plantStrip = Adafruit_NeoPixel(numPlantPixels, plantNeoPixelPin, NEO_GRB + NEO_KHZ800);

int rPlant = 220;
int gPlant = 110; 
int bPlant = 42; 

//-- NeoPixelStrip: illumination of human
const int humanNeoPixelPin = 11;
const int numHumanPixels = 16;

const int brightnessDelay = 25;

Adafruit_NeoPixel humanStrip = Adafruit_NeoPixel(numHumanPixels, humanNeoPixelPin, NEO_GRB + NEO_KHZ800);

int rHuman = 0;
int gHuman = 142; 
int bHuman = 98; 

int rHumanSteps;
int gHumanSteps;
int bHumanSteps;
const int numSteps = 10;
int stepCount = 0;
 
int plantBrightness = 0;  // for brightness 
int plantBrightDirection = 2;

int humanBrightness = 0;  // for brightness 
int humanBrightDirection = 4;


MSTimer touchedTimer;
MSTimer plantRespondingTimer;
MSTimer humanRespondingTimer;
MSTimer colorSyncTimer;

const int kWaiting = 0;
const int kJustTouched = 1;
const int kPlantResponding = 2;
const int kHumanResponding = 3;
const int kPlantHumanSync = 4;


int status = kWaiting;
boolean colorMatched = false;   // when in sync mode

//-- capacitance touch sensor
Adafruit_MPR121 cap = Adafruit_MPR121();

// To account for dry environments where cap sensors accidentally get triggers
#define NUM_SENSORS (2)
MSTimer capTouchTimer[NUM_SENSORS] = MSTimer();
boolean capTouchTimerTriggered[NUM_SENSORS];
#define CAP_TOUCH_DELAY (25)


void setup() {
  Serial.begin(115200);
  Serial.println("Starting up");
  Serial.println("Initializing NeoPixels");
  
  humanStrip.begin();
  plantStrip.begin();
  
  touchedTimer.setTimer(random(2500,3500));                   // 2500-3500
  plantRespondingTimer.setTimer(random(30000,60000));   // 15000-2000
  humanRespondingTimer.setTimer(random(15000,20000));   // 15000-2000
  colorSyncTimer.setTimer(random(2000,5000));
  //lowRangeTimerTest();
  flashStrips();
  
  Serial.println("done initializing NeoPixels");

  Serial.println("Initializing TouchCap Sensors");
  
  // Use 0x5A for 5V from Arduino, 0x5B for 3.3V
  // try to init the capacitance sensor, keep at it until it works, but likely you will have to reset the Arduino
  if (!cap.begin(0x5A)) {
    delay(100);
    Serial.println("no capacitance sensor connected");
  }

  Serial.println("successful touch cap init");
  
  // init globals for cap touch delays
  for( int i = 0; i < NUM_SENSORS; i++ ) {
    capTouchTimer[i].setTimer(CAP_TOUCH_DELAY);
    capTouchTimerTriggered[i] = false;
  }
  
  randomSeed(A0);

  calculateHumanSteps();
}

void lowRangeTimerTest() {
  touchedTimer.setTimer(random(500,1000));                   // 2500-3500
  plantRespondingTimer.setTimer(random(3000,5000));   // 15000-2000
  humanRespondingTimer.setTimer(random(3000,5000));   // 15000-2000
  colorSyncTimer.setTimer(random(2000,5000));
}

void loop() {
  //testColors();
  
  boolean touching = readTouchCap();

  //-- not touching, check to see if we were in a different status, if so turn off NeoPixels
  if( touching == false ) {
    if( status != kWaiting ) {
     status = kWaiting;
     allOff();
    }
    
    delay(10);
     return;
  }

//-- THE TWO PADS ARE BEING TOUCHED 
  Serial.print("status = ");
  Serial.println(status);
  
//-- CASE 1: We *just* touched the plates, set status to indicate that we just touched the plated. start the touched timer
  if( status == kWaiting ) {
    status = kJustTouched;
    
    //-- reset color matching
    colorMatched = false;
    stepCount = 0;
    touchedTimer.start();
  }  

// CASE 2: we are waiting for a few seconds to let the plant respond, when the touched timer expires, switch to plant responding status
  else if( status == kJustTouched && touchedTimer.isExpired() ) {
      if( status == kJustTouched ) {
        plantRespondingTimer.start();
        status = kPlantResponding;
        quickRampPlantStrip();
      }
  }
  
// CASE 3: check to see if its time for human response
   else if( status == kPlantResponding && plantRespondingTimer.isExpired() ) {
      humanRespondingTimer.start();
      status = kHumanResponding;
      quickRampHumanStrip();  
  }

  else if( status == kHumanResponding && humanRespondingTimer.isExpired() ) {
    status = kPlantHumanSync;
    colorSyncTimer.start();
  }
  
//-- DO SOME THINGS
  // (1) IF plant responding: activate *only* the plant stirp
   if( status == kPlantResponding ) {
      activatePlantStrip();
      adjustPlantBrightness();
      
      delay(brightnessDelay);
  }

   // (2) IF human responding: activate *both* the plant strip and the human strip
   else if( status == kHumanResponding ) {
       activatePlantStrip();
       adjustPlantBrightness();
       
       activateHumanStrip();
        adjustHumanBrightness();
       delay(brightnessDelay);
  }

  else if( status == kPlantHumanSync ) {
       adjustPlantBrightness();
       activatePlantStrip();
       
      // human follows plant
       humanBrightness = plantBrightness;
       humanBrightDirection = plantBrightDirection;
       
       activateHumanStrip();

       // do steps for color sync
       if( colorSyncTimer.isExpired() && stepCount < numSteps  ) {
          oneStep();

          if(  stepCount == numSteps ) {
            equalizeColors();
          }

          // restart timer
          colorSyncTimer.start();
       }
       
       delay(brightnessDelay);
   }
}

void testColors() {

  for( int j = 0; j < numPlantPixels; j++ ) {
      plantStrip.setPixelColor(j, rPlant,gPlant, bPlant);
      humanStrip.setPixelColor(j, rHuman, gHuman, bHuman );
    }

   plantStrip.show();
   humanStrip.show();
   
  delay(1000);

  oneStep();

  if(  stepCount == numSteps ) {
    equalizeColors();

    while(true)
      ;
  }
}
//-- returns true if cap is touched
boolean readTouchCap() {
  //-- store in a local var for legibility
  uint16_t capBitmask = cap.touched();

Serial.print("cap mask: " );
Serial.println(capBitmask);
    
  //-- do cap-filtering here
   for(int i = 0; i < NUM_SENSORS; i++ ) {
      //-- bitshift operators are not working with Arduino, so we are fudging it
      uint16_t bitMask = 0;
    
      //-- bitmask hack because >> isn't working
      bitMask = 1;
      for( int j = 0; j < i; j++ )
        bitMask = bitMask * 2;

        //-- both touching
        if( capBitmask == 3)
          return true;
   }

   return false;
}

void allOff() {
  plantStripOff();
  humanStripOff();
}

void plantStripOff() {
  for( int i = 0; i < numPlantPixels; i++ ) 
       plantStrip.setPixelColor(i, 0, 0, 0 );
   
   plantStrip.show();
}


void humanStripOff() {
  for( int i = 0; i < numHumanPixels; i++ ) 
       humanStrip.setPixelColor(i, 0, 0, 0 );

   humanStrip.show();
}

void flashStrips() {
  int flashDelayTime = 100;
  for( int i = 0; i < 4; i++ ) {
    
    for( int j = 0; j < numPlantPixels; j++ ) {
      plantStrip.setPixelColor(j, 0,255,0);
      humanStrip.setPixelColor(j, 0,255,0);
    }

    plantStrip.show();
    humanStrip.show();

    delay(flashDelayTime);

    allOff();
    delay(flashDelayTime);
  }
}
  
void activateHumanStrip() {   
  for( int i = 0; i < numHumanPixels; i++ ) 
     humanStrip.setPixelColor(i, rHuman, gHuman, bHuman);
     
  humanStrip.setBrightness(humanBrightness); 
  humanStrip.show();
  
  //adjustHumanBrightness();
}

void activatePlantStrip() {   
  for( int i = 0; i < numPlantPixels; i++ ) 
     plantStrip.setPixelColor(i, rPlant, gPlant, bPlant);
     
  plantStrip.setBrightness(plantBrightness);  
  plantStrip.show();
}

void adjustPlantBrightness() {
  boolean flip = false;
  const int pbFloor = 50;
  
  plantBrightness = plantBrightness + plantBrightDirection;
  if( plantBrightness < pbFloor ) {
     plantBrightness = pbFloor;
     plantBrightDirection = -plantBrightDirection;
     flip = true;
  }
  
  else if( plantBrightness > 255 ) {
     plantBrightness = 255;
     plantBrightDirection = -plantBrightDirection;
     flip = true;
  } 

  if( flip ) {
    plantBrightDirection = random(-4,4);

    if( plantBrightDirection == 0 )
        plantBrightDirection = 1;
  } 
}

void adjustHumanBrightness() {
  boolean flip = false;
  
  humanBrightness = humanBrightness + humanBrightDirection;
  if( humanBrightness < 50 ) {
     humanBrightness = 50;
     humanBrightDirection = -humanBrightDirection;
     flip = true;
  }
  
  else if( humanBrightness > 255 ) {
     humanBrightness = 255;
     humanBrightDirection = -humanBrightDirection;
     flip = true;
  } 

  if( flip ) {
    humanBrightDirection = random(-8,8);

    if( humanBrightDirection == 0 )
        humanBrightDirection = 1;
  } 
}

void calculateHumanSteps() {
  rHumanSteps = (rPlant - rHuman)/numSteps;
  gHumanSteps = (gPlant - gHuman)/numSteps;
  bHumanSteps = (bPlant - bHuman)/numSteps;

  Serial.println(rHumanSteps);
  Serial.println(gHumanSteps);
  Serial.println(bHumanSteps);
  
}

void oneStep() {
   Serial.println("one step");
   
  rHuman += rHumanSteps;
  gHuman += gHumanSteps;
  bHuman += bHumanSteps;

  Serial.println(rHuman);
  Serial.println(gHuman);
  Serial.println(bHuman);
  
  stepCount++;
}

void equalizeColors() {
  rHuman = rPlant;
  gHuman = gPlant;
  bHuman = bPlant;
}

void quickRampPlantStrip() {
 for( int i = 0; i < numPlantPixels; i++ ) 
     plantStrip.setPixelColor(i, rPlant, gPlant, bPlant);

  for( int j = 0; j < 50; j++ ) {
   plantStrip.setBrightness(j);  
   plantStrip.show();
   delay(20);
  }
}

void quickRampHumanStrip() {
  for( int i = 0; i < numPlantPixels; i++ ) 
     humanStrip.setPixelColor(i, rHuman, gHuman, bHuman);

  for( int j = 0; j < 50; j++ ) {
   humanStrip.setBrightness(j);  
   humanStrip.show();
   delay(20);
  }
}
