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

#define CONTROL_RATE 500

int trigPin = 4;
int echoPin = 2;

double minCM = 10.0;
double maxCM = 250.0;
long startTime = 0;
long endTime = 0;
volatile double cm = 0.0;

// use: Sample <table_size, update_rate> SampleName (wavetable)
Sample <kluck_NUM_CELLS, AUDIO_RATE> aSample(kluck_DATA);

// for scheduling sample start
EventDelay kTriggerDelay;

// for scheduling trigger pulse
EventDelay triggerDelay;
EventDelay outputDelay;

long measureTime = 0;

void setup(){
  Serial.begin(9600);
  while (!Serial.available()) {
    Serial.println("Press any key to start.");
    delay (1000);
  }
  Serial.println("Starting");
  startMozzi(CONTROL_RATE);
  aSample.setFreq((float) kluck_SAMPLERATE / (float) kluck_NUM_CELLS); // play at the speed it was recorded
//  aSample.setLoopingOn();
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);
  kTriggerDelay.set(600); // 1500 msec countdown, within resolution of CONTROL_RATE
  triggerDelay.set(64);
  outputDelay.set(200);
}


void updateControl(){
  if(kTriggerDelay.ready()){
    aSample.start();
    kTriggerDelay.start();
  }else if (outputDelay.ready()){
    digitalWrite(trigPin, HIGH);
    triggerDelay.start();
//    Serial.println(startTime);
//    Serial.println(endTime);
    Serial.println(cm);
    outputDelay.start();
  }else if (triggerDelay.ready()){
    digitalWrite(trigPin, LOW);
  }
}


int updateAudio(){
  return (int) aSample.next();
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
    }
    cm = distance;
//    detachInterrupt(digitalPinToInterrupt(echoPin));
  }
}

