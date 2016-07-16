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

#define CONTROL_RATE 128

int trigPin = 4;
int echoPin = 2;

double minCM = 10.0;
double maxCM = 250.0;
long startTime = 0;
long endTime = 0;
double lastCm = 0.0;
volatile double cm = 0.0;

//FILTER//
StateVariable <LOWPASS> svf; //möjliga filtertyper: LOWPASS/HIGHPASS/BANDPASS/NOTCH


const float playspeed = 4.0;
float playspeedmod = 0;
float speedchange = 0;

const unsigned int full = (int) (1000.f/playspeed) - 23; // adjustment approx for CONTROL_RATE difference

// use: Sample <table_size, update_rate> SampleName (wavetable)
Sample <kluck_NUM_CELLS, AUDIO_RATE> aSample(kluck_DATA);

// for scheduling sample start
EventDelay kTriggerDelay;

// for scheduling trigger pulse
EventDelay triggerDelay;
EventDelay outputDelay;

long measureTime = 0;

void setup(){
//  Serial.begin(9600);
//  while (!Serial.available()) {
//    Serial.println("Press any key to start.");
//    delay (1000);
//  }
//  Serial.println("Starting");

  // Setup ultrasound ISR
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);
  triggerDelay.set(64);
  outputDelay.set(200);

  // Setup Mozzi
  randSeed(); // reseed the random generator for different results each time the sketch runs
  aSample.setFreq(playspeed*((float) kluck_SAMPLERATE / (float) kluck_NUM_CELLS));
  aSample.setLoopingOn();
//  kTriggerDelay.set(full);
  kTriggerDelay.set(100); // 1500 msec countdown, within resolution of CONTROL_RATE
  startMozzi(CONTROL_RATE);
  svf.setResonance(200);
  svf.setCentreFreq(400);
}

void chooseSpeedMod(){
  double cmChange = 0;
  if(cm != -1){
    cmChange = lastCm - cm;
    lastCm = cm; 
  }
  cmChange = cmChange / 10;
//  Serial.println(cmChange);
  playspeedmod = playspeed + cmChange;
  
  
//  if (rand((byte)1) == 0){
    speedchange = (float)rand((char)-100,(char)100)/800;
//    Serial.println(speedchange);
//    float startspeed = (float)rand((char)-100,(char)100)/100;
//    Serial.println(startspeed);
//    Serial.println();
//    playspeedmod = playspeed + startspeed;
//  }
//  else{
//    speedchange = 0;
//    playspeedmod = playspeed;
//  }
//  Serial.println(speedchange);
}



void updateControl(){
  if(kTriggerDelay.ready()){
    chooseSpeedMod();
    kTriggerDelay.start();
  }else if (outputDelay.ready()){
    digitalWrite(trigPin, HIGH);
    triggerDelay.start();
//    Serial.println(startTime);
//    Serial.println(endTime);
//    Serial.println(cm);
    outputDelay.start();
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
    if (distance < minCM || distance > maxCM){
      cm = -1.0;
    }else{
      cm = distance;
    }
//    detachInterrupt(digitalPinToInterrupt(echoPin));
  }
}

