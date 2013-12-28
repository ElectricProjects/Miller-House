
// http://jeelabs.net/
//http://opensource.org/licenses/mit-license.php
//12/27/13 Todd Miller
//Inside weather node using JeeNodes TX
// http://jeelabs.net/
//http://opensource.org/licenses/mit-license.php
//12/27/13 Todd Miller
//Inside weather node using JeeNodes

#include <JeeLib.h>
#include <avr/sleep.h>
Port tmp (2);
int value;
int radioIsOn=1;
int finalValue=0;
MilliTimer readoutTimer, aliveTimer;

typedef struct {
byte temp;
byte humi :1;
byte light :7;
int mov :10;
byte lobat :1;
} PayloadIn;

PayloadIn measureIn;

void setup() {

tmp.mode2(INPUT);
measureIn.temp = finalValue;
measureIn.humi = 4;
measureIn.light = 1;
measureIn.mov = 1;
measureIn.lobat = 0;
  rf12_initialize(2, RF12_433MHZ, 75);
  rf12_easyInit(15); // every 15 seconds send out pkg
  Serial.begin(57600);
  delay(50);//wait for power to settle
}

void loop() {
measureIn.humi = 4;
value = tmp.anaRead();
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_mode();
  if (radioIsOn && rf12_easyPoll() == 0) {
    rf12_sleep(0); // turn the radio off
    radioIsOn = 0;
  }
  if (readoutTimer.poll(1000)) {
    calcTemp();
    measureIn.humi = 4;
    measureIn.light++;
    if (measureIn.light >=25)
    measureIn.light=1;
    measureIn.lobat=rf12_lowbat();
  }
  //byte sending = rf12_easySend( 0,measurement, sizeof measurement);
byte sending =  rf12_easySend(&measureIn, sizeof measureIn);
  if (aliveTimer.poll(60000)){
    sending = rf12_easySend(0, 0); // always returns 1
    Serial.println(F("Sending 'Alive' Message"));
  }
  if (sending) {
    // make sure the radio is on again
    if (!radioIsOn)
      rf12_sleep(-1); // turn the radio back on
      Serial.println("Sending data");
      Serial.print(F("Actual temp = "));
      Serial.print(measureIn.temp);
      Serial.print(F(" Humi = "));
      Serial.print(measureIn.humi);
      Serial.print(F(" Wind = "));
      Serial.print(measureIn.light);
       Serial.print(F(" Move = "));
      Serial.println(measureIn.mov);
       Serial.print(F(" Bat = "));
      Serial.println(measureIn.lobat);
    radioIsOn = 1;
  }
}
void calcTemp(){
  for (int i=0; i <= 3; i++){
    value = value + tmp.anaRead();
    delay(2);
  }

  value = (value/5);

  float voltage = value * 3.3;
  voltage /= 1024.0;
  float temperatureC = (voltage - 0.5) * 100 ;
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  finalValue=temperatureF;
  measureIn.temp=finalValue;
}




// http://jeelabs.net/
//http://opensource.org/licenses/mit-license.php
//12/27/13 Todd Miller
//Inside weather node using JeeNodes

#include <JeeLib.h>
#include <avr/sleep.h>
Port tmp (2);
int value;
int radioIsOn=1;
int finalValue=0;
MilliTimer readoutTimer, aliveTimer;

typedef struct {
byte temp;
byte humi :1;
byte light :7;
int mov :10;
byte lobat :1;
} PayloadIn;

PayloadIn measureIn;

void setup() {

tmp.mode2(INPUT);
measureIn.temp = finalValue;
measureIn.humi = 4;
measureIn.light = 1;
measureIn.mov = 1;
measureIn.lobat = 0;
  rf12_initialize(2, RF12_433MHZ, 75);
  rf12_easyInit(15); // every 15 seconds send out pkg
  Serial.begin(57600);
  delay(50);//wait for power to settle
}

void loop() {
measureIn.humi = 4;
value = tmp.anaRead();
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_mode();
  if (radioIsOn && rf12_easyPoll() == 0) {
    rf12_sleep(0); // turn the radio off
    radioIsOn = 0;
  }
  if (readoutTimer.poll(1000)) {
    calcTemp();
    measureIn.humi = 4;
    measureIn.light++;
    if (measureIn.light >=25)
    measureIn.light=1;
    measureIn.lobat=rf12_lowbat();
  }
  //byte sending = rf12_easySend( 0,measurement, sizeof measurement);
byte sending =  rf12_easySend(&measureIn, sizeof measureIn);
  if (aliveTimer.poll(60000)){
    sending = rf12_easySend(0, 0); // always returns 1
    Serial.println(F("Sending 'Alive' Message"));
  }
  if (sending) {
    // make sure the radio is on again
    if (!radioIsOn)
      rf12_sleep(-1); // turn the radio back on
      Serial.println("Sending data");
      Serial.print(F("Actual temp = "));
      Serial.print(measureIn.temp);
      Serial.print(F(" Humi = "));
      Serial.print(measureIn.humi);
      Serial.print(F(" Wind = "));
      Serial.print(measureIn.light);
       Serial.print(F(" Move = "));
      Serial.println(measureIn.mov);
       Serial.print(F(" Bat = "));
      Serial.println(measureIn.lobat);
    radioIsOn = 1;
  }
}
void calcTemp(){
  for (int i=0; i <= 3; i++){
    value = value + tmp.anaRead();
    delay(2);
  }

  value = (value/5);

  float voltage = value * 3.3;
  voltage /= 1024.0;
  float temperatureC = (voltage - 0.5) * 100 ;
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  finalValue=temperatureF;
  measureIn.temp=finalValue;
}




// http://jeelabs.net/
//http://opensource.org/licenses/mit-license.php
//12/27/13 Todd Miller
//Inside weather node using JeeNodes

#include <JeeLib.h>
#include <avr/sleep.h>
Port tmp (2);
int value;
int radioIsOn=1;
int finalValue=0;
MilliTimer readoutTimer, aliveTimer;

typedef struct {
byte temp;
byte humi :1;
byte light :7;
int mov :10;
byte lobat :1;
} PayloadIn;

PayloadIn measureIn;

void setup() {

tmp.mode2(INPUT);
measureIn.temp = finalValue;
measureIn.humi = 4;
measureIn.light = 1;
measureIn.mov = 1;
measureIn.lobat = 0;
  rf12_initialize(2, RF12_433MHZ, 75);
  rf12_easyInit(15); // every 15 seconds send out pkg
  Serial.begin(57600);
  delay(50);//wait for power to settle
}

void loop() {
measureIn.humi = 4;
value = tmp.anaRead();
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_mode();
  if (radioIsOn && rf12_easyPoll() == 0) {
    rf12_sleep(0); // turn the radio off
    radioIsOn = 0;
  }
  if (readoutTimer.poll(1000)) {
    calcTemp();
    measureIn.humi = 4;
    measureIn.light++;
    if (measureIn.light >=25)
    measureIn.light=1;
    measureIn.lobat=rf12_lowbat();
  }
  //byte sending = rf12_easySend( 0,measurement, sizeof measurement);
byte sending =  rf12_easySend(&measureIn, sizeof measureIn);
  if (aliveTimer.poll(60000)){
    sending = rf12_easySend(0, 0); // always returns 1
    Serial.println(F("Sending 'Alive' Message"));
  }
  if (sending) {
    // make sure the radio is on again
    if (!radioIsOn)
      rf12_sleep(-1); // turn the radio back on
      Serial.println("Sending data");
      Serial.print(F("Actual temp = "));
      Serial.print(measureIn.temp);
      Serial.print(F(" Humi = "));
      Serial.print(measureIn.humi);
      Serial.print(F(" Wind = "));
      Serial.print(measureIn.light);
       Serial.print(F(" Move = "));
      Serial.println(measureIn.mov);
       Serial.print(F(" Bat = "));
      Serial.println(measureIn.lobat);
    radioIsOn = 1;
  }
}
void calcTemp(){
  for (int i=0; i <= 3; i++){
    value = value + tmp.anaRead();
    delay(2);
  }

  value = (value/5);

  float voltage = value * 3.3;
  voltage /= 1024.0;
  float temperatureC = (voltage - 0.5) * 100 ;
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  finalValue=temperatureF;
  measureIn.temp=finalValue;
}





#include <JeeLib.h>
#include <avr/sleep.h>
Port tmp (2);
int value;
int radioIsOn=1;
int finalValue=0;
MilliTimer readoutTimer, aliveTimer;

typedef struct {
byte temp;
byte humi :1;
byte light :7;
int mov :10;
byte lobat :1;
} PayloadIn;

PayloadIn measureIn;

void setup() {

tmp.mode2(INPUT);
measureIn.temp = finalValue;
measureIn.humi = 4;
measureIn.light = 1;
measureIn.mov = 1;
measureIn.lobat = 0;
  rf12_initialize(2, RF12_433MHZ, 75);
  rf12_easyInit(15); // every 15 seconds send out pkg
  Serial.begin(57600);
  delay(50);//wait for power to settle
}

void loop() {
measureIn.humi = 4;
value = tmp.anaRead();
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_mode();
  if (radioIsOn && rf12_easyPoll() == 0) {
    rf12_sleep(0); // turn the radio off
    radioIsOn = 0;
  }
  if (readoutTimer.poll(1000)) {
    calcTemp();
    measureIn.humi = 4;
    measureIn.light++;
    if (measureIn.light >=25)
    measureIn.light=1;
    measureIn.lobat=rf12_lowbat();
  }
  //byte sending = rf12_easySend( 0,measurement, sizeof measurement);
byte sending =  rf12_easySend(&measureIn, sizeof measureIn);
  if (aliveTimer.poll(60000)){
    sending = rf12_easySend(0, 0); // always returns 1
    Serial.println(F("Sending 'Alive' Message"));
  }
  if (sending) {
    // make sure the radio is on again
    if (!radioIsOn)
      rf12_sleep(-1); // turn the radio back on
      Serial.println("Sending data");
      Serial.print(F("Actual temp = "));
      Serial.print(measureIn.temp);
      Serial.print(F(" Humi = "));
      Serial.print(measureIn.humi);
      Serial.print(F(" Wind = "));
      Serial.print(measureIn.light);
       Serial.print(F(" Move = "));
      Serial.println(measureIn.mov);
       Serial.print(F(" Bat = "));
      Serial.println(measureIn.lobat);
    radioIsOn = 1;
  }
}
void calcTemp(){
  for (int i=0; i <= 3; i++){
    value = value + tmp.anaRead();
    delay(2);
  }

  value = (value/5);

  float voltage = value * 3.3;
  voltage /= 1024.0;
  float temperatureC = (voltage - 0.5) * 100 ;
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  finalValue=temperatureF;
  measureIn.temp=finalValue;
}




