#include <SPI.h>
#include "df_can.h"

const int SPI_CS_PIN = 10;
MCPCAN CAN(SPI_CS_PIN);

const String OneByStep = "OneByStep=";
const String StepByStep = "StepByStep=";
const String ObjectSize = "ObjectSize=";
const long MaxPulseTarget = 40000;
const int OneMMStep = 235;
const int MaxPPS = 3500;
unsigned char len = 0;
unsigned char buf[8];
char str[20];
const int MAX_CONTROLS = 2;

void setup() {
  Serial.begin(115200);
  while(!Serial);
  
  int count = 50;
  do {
    CAN.init();
    if (CAN_OK == CAN.begin(CAN_1000KBPS)) {
      break;
    } else {

      delay(100);
    }

  } while (count--);

  attachInterrupt(0, MCP2515_ISR, FALLING);
}

void loop() {
  if (Serial.available()) {
    String receivedData = Serial.readStringUntil('\n');  // 개행 문자('\n')까지 데이터 읽기
    receivedData.trim();  // 앞뒤 공백 제거

    if (receivedData.length() > 0) {
      if (receivedData.indexOf(OneByStep) != -1) {
        float distance = parseAndProcessCommand(receivedData, OneByStep);
        long pulse_target = calPulseTarget_long(distance);
        oneByStep(pulse_target);
      } 
      else if (receivedData.indexOf(StepByStep) != -1) {
        float stepDistance = parseAndProcessCommand(receivedData, StepByStep);
        float objectSize = parseAndProcessCommand(receivedData, ObjectSize);
        stepByStep(stepDistance,objectSize);
      }
    }
  }
}

void mode0(uint16_t can_id, uint8_t ena, uint8_t dir, uint32_t pulse_target, uint16_t pps) {
  uint8_t data[8];
  data[0] = 0x00; // Mode 0
  data[1] = ena; // Enable
  data[2] = dir; // Direction
  data[3] = (uint8_t) (pulse_target      ); // Pulse Target Low
  data[4] = (uint8_t) (pulse_target >> 8 ); // Pulse Target Mid
  data[5] = (uint8_t) (pulse_target >> 16); // Pulse Target High
  data[6] = (uint8_t) (pps     ); // Pulse Per Second Low
  data[7] = (uint8_t) (pps >> 8); // Pulse Per Second High
  
  CAN.sendMsgBuf(can_id, 0, 8, data);
}

long calPulseTarget_long(float distance) {
  float pulse_target = OneMMStep * distance;
  return (long)pulse_target;
}
float calPulseTarget_float(float distance) {
  float pulse_target = OneMMStep * distance;
  return pulse_target;
}
void oneByStep(long pulse_target) {
  if (pulse_target > MaxPulseTarget) {
    long tenCMStep = OneMMStep * 100;
    int remainPulseTarget = pulse_target % tenCMStep;
    int step = (pulse_target - remainPulseTarget) / tenCMStep;
    for (int i = 0; i < step; i++) {
      mode0(0x001, 1, 0, tenCMStep, MaxPPS);
      delay((tenCMStep * 1000) / MaxPPS);
    }
    if(remainPulseTarget>0){
      mode0(0x001, 1, 0, remainPulseTarget, MaxPPS);
      delay((remainPulseTarget * 1000) / MaxPPS);

    }
  } 
  else{
    mode0(0x001, 1, 0, pulse_target, MaxPPS);
    delay((pulse_target * 1000) / MaxPPS);
  }

  Serial.println("OneByStep Finish");
}

void stepByStep(float stepDistance, float objectSize){
  int stepSize = objectSize / stepDistance;
  float remainDistance = objectSize - (stepSize * stepDistance);
  float stepD = calPulseTarget_float(stepDistance);
  for (int i = 0; i < stepSize; i++) {
    mode0(0x001, 1, 0, stepD, 800);  // 모터 명령 실행
    delay((stepD * (long)1000)/800+100);
  }

  // 남은 거리가 있을 경우 처리
  if (remainDistance > 0) {
    float remainD = calPulseTarget_float(remainDistance);
    mode0(0x001, 1, 0, remainD, 800);  // 모터 명령 실행
    delay((remainD*1000) / 800);
  }

  Serial.println("StepByStep Finish");
}
float parseAndProcessCommand(String data, String prefix) {
  int index = data.indexOf(prefix);
  if (index != -1) {
    String valueString = data.substring(index + prefix.length());

    // 값이 유효한 숫자인지 확인
    valueString.trim();  // 앞뒤 공백 제거
    if (valueString.length() > 0 && isDigit(valueString.charAt(0))) {
      return valueString.toFloat();
    }
  }

  // 실패할 경우 기본값 반환 (예: 0.0f 또는 다른 의미 있는 값)
  return 0.0f;
}

void MCP2515_ISR() {
  while (CAN_MSGAVAIL == CAN.checkReceive()) {
    buf[8] = {0, };
    CAN.readMsgBuf(&len, buf);
    // print the data
  }
}
