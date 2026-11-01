#include <Arduino.h>
#include "soc/gpio_reg.h"
#include "soc/soc.h"
// EXPERIMENTAL: GPIO4=D+, GPIO5=D-. No flash access.
// Requires external pullups, FM-1 on battery, no USB host on this bus.
constexpr uint32_t DP=4, DM=5, MASK=(1U<<DP)|(1U<<DM);
void float_bus(){REG_WRITE(GPIO_ENABLE_W1TC_REG,MASK);}
bool both_low(){return digitalRead(DP)==LOW && digitalRead(DM)==LOW;}
void configure_bus(){
  digitalWrite(DP,LOW); digitalWrite(DM,LOW);
  pinMode(DP,OUTPUT_OPEN_DRAIN);pinMode(DM,OUTPUT_OPEN_DRAIN);
  digitalWrite(DP,LOW); digitalWrite(DM,LOW); float_bus();
}
void send_key(){
  for(int b=15;b>=0;--b){
    REG_WRITE(GPIO_ENABLE_W1TS_REG,1U<<DP);
    if((0x16ef>>b)&1) REG_WRITE(GPIO_ENABLE_W1TC_REG,1U<<DM);
    else REG_WRITE(GPIO_ENABLE_W1TS_REG,1U<<DM);
    delayMicroseconds(8);
    REG_WRITE(GPIO_ENABLE_W1TC_REG,1U<<DP);
    delayMicroseconds(12);
  }
  float_bus();
}
void setup(){
  pinMode(DP,INPUT);pinMode(DM,INPUT);pinMode(6,INPUT);pinMode(7,INPUT);
  pinMode(8,OUTPUT);digitalWrite(8,HIGH); // common active-low LED; board variants differ
  delay(1500); configure_bus();
}
bool armed=false;
void loop(){
  float_bus();
  if(!both_low()) send_key();
  // If both lines are held low, stop transmitting before measuring ACK.
  uint32_t gap=micros();
  while((uint32_t)(micros()-gap)<200){
    if(digitalRead(DP)==HIGH && digitalRead(DM)==HIGH) armed=true;
    if(armed && both_low()){
      uint32_t start=micros();
      while(both_low() && (uint32_t)(micros()-start)<3000){}
      uint32_t width=micros()-start;
      if(width>=850 && width<=2500){
        float_bus();digitalWrite(8,LOW);
        uint32_t next=micros(), end=millis()+6000;
        while((int32_t)(millis()-end)<0){
          while((int32_t)(micros()-next)<0){}
          next+=1000;
          REG_WRITE(GPIO_ENABLE_W1TS_REG,1U<<DP);
          delayMicroseconds(4);
          REG_WRITE(GPIO_ENABLE_W1TC_REG,1U<<DP);
        }
        float_bus(); digitalWrite(8,HIGH);
        while(true) delay(1000);
      }
      armed=false;
      break;
    }
  }
}
