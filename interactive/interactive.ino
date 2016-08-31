/*  The interactive sound for Audible Flora project.
 *   
 *  The sound is based on a single sample (of a dolphin click) that is played in different speed and interval.
 *  
 *  There are 3 base "moods" that the sound can have: passive, active and angry.
 *  Based on sensor readings from an ultrasound sensor measuring distance to a human, the sound changes between happy and passive.
 *  Based on the presence of poisonous gases, the angry mode is turned on.
 *  
 *  The playback is controlled with the Mozzi sonification Library! http://sensorium.github.io/
 *  
 *  ---- Program Description -----
 *  
 *  The program has 2 main loops:
 *  
 *  Audio loop    Runs at 16384 Hz on interupt via Mozzi, every interupt sets a new value on pin 9
 *  Control loop  Runs at 128 Hz (configurable). Every iteration evaluate if any timers are triggered, 
 *                and then triggers timed behaviouy. Or just sets the frequency of the sample.
 *  
 *  
 *  The measurement from the ultrasound is triggered on interupt, and doesn't affect the control loop.  
 *  
 *  
 *  ---- Ultrasound Sensor ----
 *  
 *  The ultrasound is a HR-SR04 sensor. Connected to the Arduino like this:
 *  
 *  Arduino   --  HR-SR04
 *  4             trig
 *  2             echo
 *  GND           GND
 *  VCC           Vcc
 *  
 *  The echo pin connect is connected through a voltage divider to lower the output voltage.
 *  
 *  Echo --|
 *         >
 *         < 470 ohm resistor
 *         >
 *         ------ 2 on Arduino
 *         >
 *         < 470 ohm resistor
 *         >
 *  GND ---|
 *   
 *  ---- Audio Output ----
 *  
 *  Audio output is on pin 9. This can be connected directly to an amplifier.
 *  
 *  
 *  
 *  ----------------------
 *  
 *  NOTES
 *  
 *  Set the board to Arduino Leonardo to get the RX/TX Led pins correctly on the pro micro clone from Kjell & Company.
 *  
 *  Summer 2016
 *  Linnea Våglund - concept
 *  Leo Fidjeland - programming
 *  Jonas Thunberg - composer
 *  
 *  MIT Licence
*/


// Mozzi
#include <ADC.h>
#include <MozziGuts.h>
#include <Sample.h>
#include <EventDelay.h>
#include <mozzi_rand.h>
#include <StateVariable.h>
#include <Smooth.h>

// The sample
#include "kluck.h"

// Wiring
#define trigPin 4
#define echoPin 2
#define gasPin 7
#define ledPin 13
//int soundPin = 9; //Constant in Mozzi, cannot be changed

// Constants
#define CONTROL_RATE 128
#define MIN_CM 2.0
#define MAX_CM 400.0
#define triggerDelayTime 64 // Ultrasound trigger pulse length
#define measurementPeriod 200 //ms between each ultrasound measurement
#define twitchEvaluationRate 100 //ms between each twitch evaluation
#define measurementSmoothing 0.975f
#define backgroundSmoothing 0.999f
#define gasSmoothing 0.999f
#define gasAttack 0.99f

// Objects
StateVariable <LOWPASS> svf; //möjliga filtertyper: LOWPASS/HIGHPASS/BANDPASS/NOTCH
Sample <kluck_NUM_CELLS, AUDIO_RATE> aSample(kluck_DATA);
Smooth <float> aSmoothActivity(measurementSmoothing);
Smooth <float> backgroundLevelSmooth(backgroundSmoothing);
Smooth <float> gasEffectSmooth(gasSmoothing);
float backgroundLevel = MAX_CM;

// Timers, on every iteration:
EventDelay randomDelay;      // the speedchange variable is randomized, producing a "changing" sound
EventDelay measurementDelay; // a new measurement of the ultrasound is triggered
EventDelay triggerDelay;     // the ultrasound trig pin is put low again in preparation for next measurement
EventDelay twitchDelay;      // the frequency is temporarily increased, to create a "twitch" effect
EventDelay twitchTimer;      // the length of the twitch
EventDelay silentDelay;      // in low activity, every sample is spaced out with silence

// Sound constants, change these to modify sound behaviour
// Global sound behaviour
#define lowPassResonance 200  // Amount of resonance, between 1 and 255, where 1 is max and 255 min.
#define twitchLength 300      // ms the length of a twitch

// ANGRY
#define angryBasePlayspeed 30.0
#define angryBaseRandomPeriod 200 // ms per random period
#define angryRandomPeriodRange 100
#define angryLowPassCutoff 3000 // Cut off frequency for low pass filter
#define angrySpeedChangeFactor 5.0
#define angryTwitchFactor 10.0

// HAPPY (both active and passive)
#define happyBaseRandomPeriod 1000
#define happyRandomPeriodRange 50  // ms that random period can deviate from randomly
#define happyLowPassCutoff 1200    // Cut off frequency for low pass filter

// ACTIVE
#define activeBasePlayspeed 4.0
#define activeSpeedChangeFactor 1.0
#define activeTwitchFactor 1.0 // How much playspeed to add per twitch period, set to 0 to disable twitching
#define activeTwitchProbability 3
#define activeBaseSilentPeriod 0
#define activeSilentPeriodRange 0

// PASSIVE
#define passiveBasePlayspeed 1.0
#define passiveSpeedChangeFactor 0.5
#define passiveTwitchFactor 10.0 // How much playspeed to add per twitch period, set to 0 to disable twitching
#define passiveTwitchProbability 300
#define passiveBaseSilentPeriod 2000
#define passiveSilentPeriodRange 500


// Variables used by program *don't modify*
// Ultrasound related
volatile float cm = 0.0;
long startTime = 0;
long endTime = 0;
float lastCm = 0.0;
//MOVEMENT
float distanceFactor = 0.0;
float activityFactor = 0.0;
#define activityCounts 5
float activityFloats[activityCounts] = {0.0};
int activityIndex = 0;
float slowActivityFactor = 0.0;
float totalActivity = 0.0;
float smoothed_activity = 0.0;
// GAS
volatile float gasActive = 0.0;
float gasEffect = 0.0;

bool singleMode = true;
int silentPeriodRange = passiveSilentPeriodRange;
int baseSilentPeriod = passiveBaseSilentPeriod;
float speedChangeFactor = passiveSpeedChangeFactor;
float twitchFactor = passiveTwitchFactor;
int twitchProbability = passiveTwitchProbability;


float baseSpeed = 0.0;
float addSpeed = 0.0;
float twitchSpeed = 0.0;
float totalSpeed = 0.0;

float speedchange = 0;
int randomPeriod = happyBaseRandomPeriod;
int silentPeriod = baseSilentPeriod;
int twitching = 0;
int silent = 0;
int lowPassCutoff = happyLowPassCutoff;


void setup(){
  Serial.begin(9600);

  // Setup ultrasound ISR
  pinMode(trigPin,OUTPUT);
  pinMode(ledPin,OUTPUT);
  pinMode(echoPin,INPUT);
  pinMode(gasPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(gasPin), gasISR, CHANGE);
  triggerDelay.set(triggerDelayTime);
  measurementDelay.set(measurementPeriod);

  // Turn off the lights
  digitalWrite(ledPin, LOW);

  // Setup Mozzi
  randSeed(); // reseed the random generator for different results each time the sketch runs
  aSample.setFreq(((float) kluck_SAMPLERATE / (float) kluck_NUM_CELLS));
  aSample.setLoopingOn();
  randomDelay.set(randomPeriod); // 1500 msec countdown, within resolution of CONTROL_RATE
  twitchDelay.set(twitchEvaluationRate);
  twitchTimer.set(twitchLength);
  silentDelay.set(silentPeriod);

  // Fill up the smooth filters
  backgroundLevelSmooth.setSmoothness(0);
  backgroundLevelSmooth.next(MAX_CM);
  backgroundLevelSmooth.setSmoothness(backgroundSmoothing);
  aSmoothActivity.setSmoothness(0);
  aSmoothActivity.next(0);
  aSmoothActivity.setSmoothness(measurementSmoothing);
  gasEffectSmooth.setSmoothness(0);
  gasEffectSmooth.next(0);
  gasEffectSmooth.setSmoothness(gasSmoothing);
  
  svf.setResonance(lowPassResonance);
  svf.setCentreFreq(happyLowPassCutoff);
  startMozzi(CONTROL_RATE);
}

void chooseSpeedMod(){
  // The added speed of every random period is kept in a separate variable
  // This is reset every random period
  // This means that the add speed varies:

  //       .                    ..
  //     .                    ..
  //   .                    ..
  // -|-----|-----|--------|------|------|-----|------
  //        ...    ....
  //           ...     ....
  //              .

  
  addSpeed = 0.0;
  speedChangeFactor = activeSpeedChangeFactor + (passiveSpeedChangeFactor - activeSpeedChangeFactor) * (1 - smoothed_activity );
  speedChangeFactor += angrySpeedChangeFactor * gasEffect;
  speedchange = (float)rand(-100, 100)/800 * speedChangeFactor;

  if(singleMode && speedchange < 0){
    // Only positive speedchange in single mode
    speedchange = -speedchange;
  }
}

void reactToMeasurement(){
  if(cm != -1){
    distanceFactor = 1 - (cm - MIN_CM) / (backgroundLevel - MIN_CM);
    activityFactor = abs((lastCm - cm) / (backgroundLevel - MIN_CM));

    //Summarize the last measurements
    activityFloats[activityIndex] = activityFactor;
    activityIndex++;
    if(activityIndex > activityCounts - 1){
      activityIndex = 0;
    }

    slowActivityFactor = 0.0;
    for (int k = 0; k < activityCounts; k++){
      slowActivityFactor += activityFloats[k];
    }

    if(slowActivityFactor < 0.1){
      backgroundLevel = backgroundLevelSmooth.next(cm);
    }

    totalActivity = (distanceFactor + activityFactor + slowActivityFactor ) / 1;
    lastCm = cm;
  }

//  gas = analogRead(gasPin);
  Serial.println(cm);
}



void updateControl(){
  if(randomDelay.ready()){
    // Initiate a new random period for base clicks
    chooseSpeedMod();
    int baseRandomPeriod = happyBaseRandomPeriod + (int)((float)(angryBaseRandomPeriod - happyBaseRandomPeriod) * gasEffect);
    int randomPeriodRange = happyRandomPeriodRange + (int)((float)(angryRandomPeriodRange - happyRandomPeriodRange) * gasEffect);
    randomPeriod = baseRandomPeriod + rand(-randomPeriodRange, randomPeriodRange);
    randomDelay.set(randomPeriod);
    randomDelay.start();
  }else if (measurementDelay.ready()){
    // Initiate a new ultrasound measurement by sending a pulse on the trig pin
    reactToMeasurement();
    digitalWrite(trigPin, HIGH);
    triggerDelay.start();
    measurementDelay.start();
  }else if (triggerDelay.ready()){
    // End of measurement period
    digitalWrite(trigPin, LOW);
  }else if(twitchDelay.ready()){
    // Here we use an evaluation timer and a fixed probability to produce an event with low occurance and high variance
    if(rand((byte)twitchProbability) == 0){
      // If we have a new twitch
      twitching = 1;
      twitchFactor = passiveTwitchFactor + (activeTwitchFactor - passiveTwitchFactor) * (min(smoothed_activity / 1.5 + gasEffect, 1.0));
      twitchTimer.start();
    }
    twitchProbability = passiveTwitchProbability + (activeTwitchProbability - passiveTwitchProbability) * (smoothed_activity / 1.2);
    if (twitchProbability < activeTwitchProbability) twitchProbability = activeTwitchProbability;
    twitchDelay.start();
  } else if(twitching && twitchTimer.ready()){
    twitching = 0;
  } else if(singleMode && silentDelay.ready()){
    aSample.start();
    silent = 1 - silent;
    baseSilentPeriod = passiveBaseSilentPeriod * (1 - smoothed_activity);
    silentPeriodRange = passiveSilentPeriodRange * (1 - smoothed_activity);
    silentPeriod = baseSilentPeriod + rand(-silentPeriodRange,silentPeriodRange);
    silentDelay.set(silentPeriod);
    silentDelay.start();
  }

  // Activity
  smoothed_activity = aSmoothActivity.next(totalActivity);
  gasEffect = gasEffectSmooth.next(gasActive);
  baseSpeed = passiveBasePlayspeed + (activeBasePlayspeed - passiveBasePlayspeed) * smoothed_activity;  
  addSpeed += speedchange;
  
  twitchSpeed = twitching ? twitchFactor : 0.0;

  // Add the gas effect
  baseSpeed += angryBasePlayspeed * gasEffect;

  // Summarize
  totalSpeed = baseSpeed + addSpeed + twitchSpeed;


  // Set filter effect
  lowPassCutoff = happyLowPassCutoff + (angryLowPassCutoff - happyLowPassCutoff) * gasEffect;
  svf.setCentreFreq(lowPassCutoff);

  if(smoothed_activity < 0.6 && gasEffect < 0.1) {
    singleMode = true;
  }else{
    singleMode = false;
  }

  if(!singleMode || twitching){
    aSample.setLoopingOn();
  } else {
    aSample.setLoopingOff();
  }

  if(singleMode && silent){
    aSample.setFreq(0);
  }else{
    aSample.setFreq(totalSpeed);  
  }
}


int updateAudio(){
  return (int) svf.next(aSample.next());
}

void loop(){
  audioHook();
}

/*
 * 
 *
 * Ultrasonic ranging module HC - SR04 
 * 
 * provides 2cm - 400cm non-contact measurement function, the ranging accuracy can reach to 3mm.
 * The modules includes ultrasonic transmitters, receiver and control circuit. 
 * 
 * The basic principle of work:
 * 
 * (1) Using IO trigger for at least 10us high level signal,
 * (2) The Module automatically sends eight 40 kHz and detect whether there is a pulse signal back.
 * (3) IF the signal back, through high level , time of high output IO duration is the time from 
 *     sending ultrasonic to returning.
 *     
 *     Test distance = high level time × velocity of sound (340M/S) / 2
 * 
 */
void echoISR(){
  if(digitalRead(echoPin) == HIGH){
    startTime = mozziMicros();
  }else{
    endTime = mozziMicros();
    long duration = endTime - startTime;
    float distance = duration / 29.41176 / 2.0;
    if (distance < MIN_CM || distance > MAX_CM){
      cm = -1.0;
      digitalWrite(ledPin, LOW);
    }else{
      cm = distance;
      digitalWrite(ledPin, HIGH);
    }
  }
}

void gasISR(){
  if(digitalRead(gasPin) == LOW){//Active low
    gasActive = 1.0;
    gasEffectSmooth.setSmoothness(gasAttack);
//    digitalWrite(ledPin, HIGH);
//    RXLED1;
  }else{
    gasActive = 0.0;
    gasEffectSmooth.setSmoothness(gasSmoothing);
//    digitalWrite(ledPin, LOW);
//    RXLED0;
  }
}

