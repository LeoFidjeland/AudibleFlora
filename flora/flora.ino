/*  Example of playing a sampled sound,
    using Mozzi sonification library.
  
    Demonstrates one-shot samples scheduled
    with EventDelay.
  
    Circuit: Audio output on digital pin 9 on a Uno or similar, or
    DAC/A14 on Teensy 3.1, or 
    check the README or http://sensorium.github.com/Mozzi/
  
    Mozzi help/discussion/announcements:
    https://groups.google.com/forum/#!forum/mozzi-users
  
    Tim Barrass 2012, CC by-nc-sa.
*/

//#include <ADC.h>  // Teensy 3.1 uncomment this line and install http://github.com/pedvide/ADC
#include <MozziGuts.h>
#include <Sample.h> // Sample template
#include "kluck.h"
#include <EventDelay.h>
#include <mozzi_rand.h>
#include <StateVariable.h>

// Wiring
#define trigPin 4
#define echoPin 2
//int soundPin = 9; //Constant in Mozzi

// Constants
#define CONTROL_RATE 128
#define MIN_CM 10.0
#define MAX_CM 250.0
#define triggerDelayTime 64 // Ultrasound trigger pulse length
#define measurementPeriod 200 //ms between each ultrasound measurement

// Variables used by program *don't modify*
volatile double cm = 0.0;
long startTime = 0;
long endTime = 0;
double lastCm = 0.0;
float playspeedmod = 0;
float speedchange = 0;

// Objects
StateVariable <LOWPASS> svf; //möjliga filtertyper: LOWPASS/HIGHPASS/BANDPASS/NOTCH
Sample <kluck_NUM_CELLS, AUDIO_RATE> aSample(kluck_DATA);
EventDelay randomDelay;
EventDelay measurementDelay;
EventDelay triggerDelay;

// Sound constants, change these to modify sound behaviour
const float playspeed = 2.0;
#define randomPeriod 1000 // ms per random period
#define lowPassResonance 200 // Amount of resonance, between 1 and 255, where 1 is max and 255 min.
#define lowPassCutoff 400 // Cut off frequency for low pass filter
#define speedChangeFactor 1.0
#define measureFactor 10

void setup(){
//  Serial.begin(9600);

  // Setup ultrasound ISR
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);
  triggerDelay.set(triggerDelayTime);
  measurementDelay.set(measurementPeriod);

  // Setup Mozzi
  randSeed(); // reseed the random generator for different results each time the sketch runs
  aSample.setFreq(playspeed*((float) kluck_SAMPLERATE / (float) kluck_NUM_CELLS));
  aSample.setLoopingOn();
  randomDelay.set(randomPeriod); // 1500 msec countdown, within resolution of CONTROL_RATE
  
  svf.setResonance(lowPassResonance);
  svf.setCentreFreq(lowPassCutoff);
  startMozzi(CONTROL_RATE);
}

void chooseSpeedMod(){
  double cmChange = 0;
  if(cm != -1){
    cmChange = lastCm - cm;
    lastCm = cm; 
  }
  cmChange = cmChange / measureFactor;
//  Serial.println(cmChange);
  playspeedmod = playspeed + cmChange;
  speedchange = (float)rand((char)-100,(char)100)/800 * speedChangeFactor;
}



void updateControl(){
  if(randomDelay.ready()){
    // Initiate a new random period for audio
    chooseSpeedMod();
    randomDelay.start();
  }else if (measurementDelay.ready()){
    // Initiate a new ultrasound measurement by sending a pulse on the trig pin
    digitalWrite(trigPin, HIGH);
    triggerDelay.start();
//    Serial.println(startTime);
//    Serial.println(endTime);
//    Serial.println(cm);
    measurementDelay.start();
  }else if (triggerDelay.ready()){
    digitalWrite(trigPin, LOW);
  }
  playspeedmod += speedchange;
//  Serial.println(playspeedmod);
  aSample.setFreq(playspeedmod);
  
}


int updateAudio(){
  return (int) svf.next(aSample.next());
}

void loop(){
  audioHook();
}

void echoISR(){
  if(digitalRead(echoPin) == HIGH){
    startTime = mozziMicros();
  }else{
    endTime = mozziMicros();
    long duration = endTime - startTime;
    double distance = duration / 29.0 / 2.0;
    if (distance < MIN_CM || distance > MAX_CM){
      cm = -1.0;
    }else{
      cm = distance;
    }
  }
}

