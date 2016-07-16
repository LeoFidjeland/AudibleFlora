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

// Objects
StateVariable <LOWPASS> svf; //möjliga filtertyper: LOWPASS/HIGHPASS/BANDPASS/NOTCH
Sample <kluck_NUM_CELLS, AUDIO_RATE> aSample(kluck_DATA);
EventDelay randomDelay;
EventDelay measurementDelay;
EventDelay triggerDelay;
EventDelay twitchDelay;
EventDelay twitchTimer;

// Sound constants, change these to modify sound behaviour
const float basePlayspeed = 30.0;
#define baseRandomPeriod 200 // ms per random period
#define randomPeriodRange 100
#define lowPassResonance 200 // Amount of resonance, between 1 and 255, where 1 is max and 255 min.
#define lowPassCutoff 3000 // Cut off frequency for low pass filter
#define speedChangeFactor 5.0
#define measureFactor 10
#define twitchLength 300
#define twitchFactor 10.0
#define twitchEvaluationRate 100 //ms evaluate if new twich
#define twitchProbability 3 // 1 / 10
int twitching = 0;

// Happy and talkative
//baseRandomPeriod 1000
//randomPeriodRange 0
//playspeed 2.0
//speedChangeFactor 1.0
//measureFactor 10
//randomPeriod 1000
//lowPassCutoff 400

// Variables used by program *don't modify*
volatile double cm = 0.0;
long startTime = 0;
long endTime = 0;
double lastCm = 0.0;
float playspeed = 0;
float speedchange = 0;
int randomPeriod = baseRandomPeriod;

void setup(){
  Serial.begin(9600);

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
  twitchDelay.set(twitchEvaluationRate);
  twitchTimer.set(twitchLength);
  
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
  playspeed = basePlayspeed + cmChange;
  if(twitching){
    playspeed = playspeed + twitchFactor;
  }
  speedchange = (float)rand((char)-100,(char)100)/800 * speedChangeFactor;
}



void updateControl(){
  if(randomDelay.ready()){
    // Initiate a new random period for audio
    chooseSpeedMod();
    randomPeriod = baseRandomPeriod + rand((char)-randomPeriodRange,(char)randomPeriodRange);
//    Serial.println(randomPeriod);
    randomDelay.set(randomPeriod);
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
  }else if(twitchDelay.ready()){
    if(rand((char)twitchProbability) == 0){
      twitching = 1;
      Serial.println("Start Twitch");
      playspeed = playspeed + twitchFactor;
      twitchTimer.start();
    }
    twitchDelay.start();
  }

  if(twitching && twitchTimer.ready()){
    Serial.println("End Twitch");
    playspeed = playspeed - twitchFactor;
    twitching = 0;
  }
  
  
  playspeed += speedchange;
  
//  if(twitching){
//    playspeedmod = playspeedmod + twitchFactor;
//  }
  
  Serial.println(playspeed);
  aSample.setFreq(playspeed);
  
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

