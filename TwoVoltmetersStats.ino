#include <TimerOne.h>
#include <stdio.h> // for function sprintf


#define LIGHT_PIN 11
#define RAM_PIN 9
#define CPU_PIN 10
#define STATE_DIODE_PIN 13

#define SERIAL_TIMEOUT_MS 50
#define SERIAL_SPEED 9600
#define FRAME_SIZE 4 // without start bit
#define START_BIT 0xFF

#define PWM_FREQ 100

int ramActualValue;
int ramRealValue;
int cpuActualValue;
int cpuRealValue;
int lightLVL;
byte voltRatio;

int timerCounter;
byte rxBuffer[FRAME_SIZE];
byte position = FRAME_SIZE;
byte tempByte;
char printBuffer[100];
bool diodeLastState;

int noDataCounter;
const int NO_DATA_LIMIT = 100;

enum deltaType {
  constDelta,
  linedDelta , 
  powerSquareDelta,
  powerOnePointFiveDelta
};

const deltaType actualDeltaType = constDelta;


void setup() {
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(RAM_PIN, OUTPUT);
  pinMode(CPU_PIN, OUTPUT);
  //pinMode(STATE_DIODE_PIN, OUTPUT);
  
  Timer1.initialize(1000000 / PWM_FREQ / 1000);
  Timer1.attachInterrupt(timerTick);
  //Timer1.pwm(CPU_PIN, 0);
  //Timer1.pwm(RAM_PIN, 0);
  //Timer1.pwm(LIGHT_PIN, 500);

  Serial.begin(SERIAL_SPEED);
  Serial.setTimeout(SERIAL_TIMEOUT_MS);
}

void loop() {
  if(Serial.available()) {
      noDataCounter = 0;
      tempByte = Serial.read();
      if (tempByte == START_BIT) {
          position = 0;
      } else if (position < FRAME_SIZE) {
          rxBuffer[position++] = tempByte;
      } 
      if (position == FRAME_SIZE) {
        //diodeLastState = !diodeLastState;
        //digitalWrite(STATE_DIODE_PIN, diodeLastState);
        cpuRealValue = rxBuffer[0] * 10;
        ramRealValue = rxBuffer[1] * 10;
        lightLVL = rxBuffer[2] * 100;
        voltRatio = rxBuffer[3];
        //digitalWrite(LIGHT_PIN, lightLVL > 0);
        
        snprintf(printBuffer, 100, "CPU: %d, RAM: %d, light: %d, voltRatio: %d", rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3]);
        Serial.println(printBuffer);
      }
  }
}

int slowlyVoltmeter(int actualValue, int realValue, int voltRatio, deltaType type) {
  switch(type) {
    case constDelta: {
      actualValue += getConstDeltaValue(actualValue, realValue, voltRatio);
    } break;  
    case linedDelta: {
      actualValue += getLinedDeltaValue(actualValue, realValue, voltRatio);
    } break;
    case powerOnePointFiveDelta: {
      actualValue += getPowerOnePointFiveDeltaValue(actualValue, realValue, voltRatio);
    } break;
    case powerSquareDelta: {
      actualValue += getPowerSquareDeltaValue(actualValue, realValue, voltRatio);
    } break;
  }
  return actualValue;
}

int getConstDeltaValue(int actualValue, int realValue, long voltRatio) {
  int sign = actualValue < realValue ? 1 : -1;
  //int normalDelta = abs(actualValue - realValue);
  return sign * voltRatio;
}

int getLinedDeltaValue(int actualValue, int realValue, int voltRatio) {
  int sign = actualValue < realValue ? 1 : -1;
  int normalDelta = abs(actualValue - realValue);
  return sign * ((((normalDelta * voltRatio) / 250)));
}

int getPowerSquareDeltaValue(int actualValue, int realValue, int voltRatio) {
  int sign = actualValue < realValue ? 1 : -1;
  int normalDelta = abs(actualValue - realValue);
  return sign * (((normalDelta * normalDelta / (int)200000) * voltRatio) + voltRatio);
}

int getPowerOnePointFiveDeltaValue(int actualValue, int realValue, int voltRatio) {
  int sign = actualValue < realValue ? 1 : -1;
  int normalDelta = abs(actualValue - realValue);
  return sign * ((pow(normalDelta, 1.5) / 7000 * voltRatio) + voltRatio);
}

void timerTick() {
    if (timerCounter++ >= 1000) {
      timerCounter = 0;
      timerInterrupt();
      digitalWrite(CPU_PIN, cpuActualValue > 0);
      digitalWrite(RAM_PIN, ramActualValue > 0);
      digitalWrite(LIGHT_PIN, lightLVL > 0);
    } 
    
    if (timerCounter == cpuActualValue)
      digitalWrite(CPU_PIN, 0);
    if (timerCounter == ramActualValue)
      digitalWrite(RAM_PIN, 0);
      if (timerCounter == lightLVL)
      digitalWrite(LIGHT_PIN, 0);  
}

void timerInterrupt() {
  if (noDataCounter < NO_DATA_LIMIT) 
    noDataCounter++;
  if (noDataCounter >= NO_DATA_LIMIT) {
     lightLVL = 0;
     cpuRealValue = 0;
     ramRealValue = 0;
     noDataCounter = 0;
  } else {
    if (cpuActualValue != cpuRealValue) {
      cpuActualValue = slowlyVoltmeter(cpuActualValue, cpuRealValue, voltRatio, actualDeltaType);
      Timer1.setPwmDuty(CPU_PIN, cpuActualValue);
    }
    if (ramActualValue != ramRealValue) {
      ramActualValue = slowlyVoltmeter(ramActualValue, ramRealValue, voltRatio, actualDeltaType);
      Timer1.setPwmDuty(RAM_PIN, ramActualValue);
    }
  }
}
