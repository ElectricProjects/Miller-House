
// http://jeelabs.net/
//http://opensource.org/licenses/mit-license.php
//12/27/13 Todd Miller
//Outside weather node using JeeNodes TX

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
byte wind :7;
int rain :10;
byte lobat :1;
} PayloadOut;

PayloadOut measureOut;

void setup() {

tmp.mode2(INPUT);
measureOut.temp = finalValue;
measureOut.humi = 0;
measureOut.wind = 0;
measureOut.rain = 0;
measureOut.lobat = 0;
  rf12_initialize(1, RF12_433MHZ, 75);
  rf12_easyInit(15); // every 15 seconds send out pkg
  Serial.begin(57600);
  delay(50);//wait for power to settle
}

void loop() {
value = tmp.anaRead();
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_mode();
  if (radioIsOn && rf12_easyPoll() == 0) {
    rf12_sleep(0); // turn the radio off
    radioIsOn = 0;
  }
  if (readoutTimer.poll(1000)) {
    calcTemp();
    measureOut.lobat=rf12_lowbat();
  }
  //byte sending = rf12_easySend( 0,measurement, sizeof measurement);
byte sending =  rf12_easySend(&measureOut, sizeof measureOut);
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
      Serial.print(measureOut.temp);
      Serial.print(F(" Humi = "));
      Serial.print(measureOut.humi);
      Serial.print(F(" Wind = "));
      Serial.print(measureOut.wind);
       Serial.print(F(" Rain = "));
      Serial.println(measureOut.rain);
    radioIsOn = 1;
  }
}
void calcTemp(){
  for (int i=0; i <= 3; i++){
    value = value + tmp.anaRead();
    Serial.print("value = ");
    Serial.println(value);
    delay(2);
  }

  value = (value/5);

  float voltage = value * 3.3;
  voltage /= 1024.0;
  float temperatureC = (voltage - 0.5) * 100 ;
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;
  finalValue=temperatureF;
  measureOut.temp=finalValue;
}




