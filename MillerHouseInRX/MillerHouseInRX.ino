// MillerHouseInRX  
// Configure some values in EEPROM for easy config of the RF12 later on.
// 2009-05-06 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php
//12/26/13 Todd Miller
// Inside weather receiver using JeeNodes & 20x4 LCD
// MillerHouseInRX 
// Used with MillerHouseOutTX, roomNodeSHT11, roomNodeTMP36 

#include <JeeLib.h>
#include <util/crc16.h>
#include <util/parity.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <PortsLCD.h>
#include <Wire.h>
#include <RTClib.h>


// RTC based on the DS1307 chip connected via the Ports library
class RTC_Plug :
public DeviceI2C {
  // shorthand
  static uint8_t bcd2bin (uint8_t val) {
    return RTC_DS1307::bcd2bin(val);
  }
  static uint8_t bin2bcd (uint8_t val) {
    return RTC_DS1307::bin2bcd(val);
  }
public:
  RTC_Plug (const PortI2C& port) :
  DeviceI2C (port, 0x68) {
  }

  void begin() {
  }

  void adjust(const DateTime& dt) {
    send();
    write(0);
    write(bin2bcd(dt.second()));
    write(bin2bcd(dt.minute()));
    write(bin2bcd(dt.hour()));
    write(bin2bcd(0));
    write(bin2bcd(dt.day()));
    write(bin2bcd(dt.month()));
    write(bin2bcd(dt.year() - 2000));
    write(0);
    stop();
  }

  DateTime now() {
    send();
    write(0);
    stop();

    receive();
    uint8_t ss = bcd2bin(read(0));
    uint8_t mm = bcd2bin(read(0));
    uint8_t hh = bcd2bin(read(0));
    read(0);
    uint8_t d = bcd2bin(read(0));
    uint8_t m = bcd2bin(read(0));
    uint16_t y = bcd2bin(read(1)) + 2000;

    return DateTime (y, m, d, hh, mm, ss);
  }
};

PortI2C myI2C (4);
LiquidCrystalI2C lcd (myI2C);

// ATtiny's only support outbound serial @ 38400 baud, and no DataFlash logging
int pkg=0;
#if defined(__AVR_ATtiny84__) ||defined(__AVR_ATtiny44__)
#define SERIAL_BAUD 38400
#else
#define SERIAL_BAUD 57600

#define DATAFLASH   1   // check for presence of DataFlash memory on JeeLink
#define FLASH_MBIT  16  // support for various dataflash sizes: 4/8/16 Mbit

#define LED_PIN     4   // activity LED, comment out to disable

#endif

#define COLLECT 0x20 // collect mode, i.e. pass incoming without sending acks

static unsigned long now () {
  // FIXME 49-day overflow
  return millis() / 1000;
}

static void activityLed (byte on) {
#ifdef LED_PIN
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !on);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// RF12 configuration setup code

typedef struct {
  byte nodeId;
  byte group;
  char msg[RF12_EEPROM_SIZE-4];
  word crc;
}
RF12Config;
static RF12Config config;

PortI2C i2cBus (3);
RTC_Plug RTC (i2cBus);

typedef struct {
  byte temp;
  byte humi :1;
  byte wind :7;
  int  rain :10;
  byte lobat :1;
}
PayloadOut;
PayloadOut measureOut;

typedef struct {
  byte light;
  byte moved :1;
  byte humi :7;
  int  temp :10;
  byte lobat :1;
}
PayloadIn;
PayloadIn measureIn;

typedef struct {
  byte light;
  byte moved :1;
  byte humi :7;
  int  temp :10;
  byte lobat :1;
}
PayloadIn2;
PayloadIn2 measureIn2;

byte degree[8] = {
  B00111,
  B00101,
  B00101,
  B00111,
  B00000,
  B00000,
  B00000,
};

byte battery[8] = {
  B00110,
  B01111,
  B01111,
  B01111,
  B01111,
  B01111,
  B01111,
};

byte in = 0;
int times;
int hstamp;
int mstamp;
byte out = 1;
long interval = 15000;
unsigned long previousMillis;

unsigned long interval1 = 180000;
unsigned long previousMillis1;
unsigned long currentMillis1;

unsigned long interval2 = 180000;
unsigned long previousMillis2;
unsigned long currentMillis2;

unsigned long interval3 = 180000;
unsigned long previousMillis3;
unsigned long currentMillis3;

byte bat1 = 0;
byte bat2 = 0;
byte bat3 = 0;

int tmpLow = 0;
int tmpHigh = 0;
int tmpOutLow = 0;
int tmpOutHigh = 0;
//int prevTemp =0;
byte tmp=0;
byte tmp2=0;
static char cmd;

static byte value, stack[RF12_MAXDATA], top, sendLen, dest, quiet;
static byte testbuf[RF12_MAXDATA], testCounter;

static void addCh (char* msg, char c) {
  byte n = strlen(msg);
  msg[n] = c;
}

static void addInt (char* msg, word v) {
  if (v >= 10)
    addInt(msg, v / 10);
  addCh(msg, '0' + v % 10);
}

static void saveConfig () {
  // set up a nice config string to be shown on startup
  memset(config.msg, 0, sizeof config.msg);
  strcpy(config.msg, " ");

  byte id = config.nodeId & 0x1F;
  addCh(config.msg, '@' + id);
  strcat(config.msg, " i");
  addInt(config.msg, id);
  if (config.nodeId & COLLECT)
    addCh(config.msg, '*');

  strcat(config.msg, " g");
  addInt(config.msg, config.group);

  strcat(config.msg, " @ ");
  static word bands[4] = {
    315, 433, 868, 915     };
  word band = config.nodeId >> 6;
  addInt(config.msg, bands[band]);
  strcat(config.msg, " MHz ");

  config.crc = ~0;
  for (byte i = 0; i < sizeof config - 2; ++i)
    config.crc = _crc16_update(config.crc, ((byte*) &config)[i]);

  // save to EEPROM
  for (byte i = 0; i < sizeof config; ++i) {
    byte b = ((byte*) &config)[i];
    eeprom_write_byte(RF12_EEPROM_ADDR + i, b);
  }

  if (!rf12_config())
    Serial.println(F("config save failed"));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// OOK transmit code

// Turn transmitter on or off, but also apply asymmetric correction and account
// for 25 us SPI overhead to end up with the proper on-the-air pulse widths.
// With thanks to JGJ Veken for his help in getting these values right.
static void ookPulse(int on, int off) {
  rf12_onOff(1);
  delayMicroseconds(on + 150);
  rf12_onOff(0);
  delayMicroseconds(off - 200);
}

static void fs20sendBits(word data, byte bits) {
  if (bits == 8) {
    ++bits;
    data = (data << 1) | parity_even_bit(data);
  }
  for (word mask = bit(bits-1); mask != 0; mask >>= 1) {
    int width = data & mask ? 600 : 400;
    ookPulse(width, width);
  }
}

static void fs20cmd(word house, byte addr, byte cmd) {
  byte sum = 6 + (house >> 8) + house + addr + cmd;
  for (byte i = 0; i < 3; ++i) {
    fs20sendBits(1, 13);
    fs20sendBits(house >> 8, 8);
    fs20sendBits(house, 8);
    fs20sendBits(addr, 8);
    fs20sendBits(cmd, 8);
    fs20sendBits(sum, 8);
    fs20sendBits(0, 1);
    delay(10);
  }
}

static void kakuSend(char addr, byte device, byte on) {
  int cmd = 0x600 | ((device - 1) << 4) | ((addr - 1) & 0xF);
  if (on)
    cmd |= 0x800;
  for (byte i = 0; i < 4; ++i) {
    for (byte bit = 0; bit < 12; ++bit) {
      ookPulse(375, 1125);
      int on = bitRead(cmd, bit) ? 1125 : 375;
      ookPulse(on, 1500 - on);
    }
    ookPulse(375, 375);
    delay(11); // approximate
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// DataFlash code

#if DATAFLASH

#define DF_ENABLE_PIN   8           // PB0

#if FLASH_MBIT == 4
// settings for 0.5 Mbyte flash in JLv2
#define DF_BLOCK_SIZE   16          // number of pages erased at same time
#define DF_LOG_BEGIN    32          // first 2 blocks reserved for future use
#define DF_LOG_LIMIT    0x0700      // last 64k is not used for logging
#define DF_MEM_TOTAL    0x0800      // 2048 pages, i.e. 0.5 Mbyte
#define DF_DEVICE_ID    0x1F44      // see AT25DF041A datasheet
#define DF_PAGE_ERASE   0x20        // erase one block of flash memory
#endif

#if FLASH_MBIT == 8
// settings for 1 Mbyte flash in JLv2
#define DF_BLOCK_SIZE   16          // number of pages erased at same time
#define DF_LOG_BEGIN    32          // first 2 blocks reserved for future use
#define DF_LOG_LIMIT    0x0F00      // last 64k is not used for logging
#define DF_MEM_TOTAL    0x1000      // 4096 pages, i.e. 1 Mbyte
#define DF_DEVICE_ID    0x1F45      // see AT26DF081A datasheet
#define DF_PAGE_ERASE   0x20        // erase one block of flash memory
#endif

#if FLASH_MBIT == 16
// settings for 2 Mbyte flash in JLv3
#define DF_BLOCK_SIZE   256         // number of pages erased at same time
#define DF_LOG_BEGIN    512         // first 2 blocks reserved for future use
#define DF_LOG_LIMIT    0x1F00      // last 64k is not used for logging
#define DF_MEM_TOTAL    0x2000      // 8192 pages, i.e. 2 Mbyte
#define DF_DEVICE_ID    0x2020      // see M25P16 datasheet
#define DF_PAGE_ERASE   0xD8        // erase one block of flash memory
#endif

// structure of each page in the log buffer, size must be exactly 256 bytes
typedef struct {
  byte data [248];
  word seqnum;
  long timestamp;
  word crc;
}
FlashPage;

// structure of consecutive entries in the data area of each FlashPage
typedef struct {
  byte length;
  byte offset;
  byte header;
  byte data[RF12_MAXDATA];
}
FlashEntry;

static FlashPage dfBuf;     // for data not yet written to flash
static word dfLastPage;     // page number last written
static byte dfFill;         // next byte available in buffer to store entries

static byte df_present () {
  return dfLastPage != 0;
}

static void df_enable () {
  // digitalWrite(ENABLE_PIN, 0);
  bitClear(PORTB, 0);
}

static void df_disable () {
  // digitalWrite(ENABLE_PIN, 1);
  bitSet(PORTB, 0);
}

static byte df_xfer (byte cmd) {
  SPDR = cmd;
  while (!bitRead(SPSR, SPIF))
    ;
  return SPDR;
}

void df_command (byte cmd) {
  for (;;) {
    cli();
    df_enable();
    df_xfer(0x05); // Read Status Register
    byte status = df_xfer(0);
    df_disable();
    sei();
    // don't wait for ready bit if there is clearly no dataflash connected
    if (status == 0xFF || (status & 1) == 0)
      break;
  }

  cli();
  df_enable();
  df_xfer(cmd);
}

static void df_deselect () {
  df_disable();
  sei();
}

static void df_writeCmd (byte cmd) {
  df_command(0x06); // Write Enable
  df_deselect();
  df_command(cmd);
}

void df_read (word block, word off, void* buf, word len) {
  df_command(0x03); // Read Array (Low Frequency)
  df_xfer(block >> 8);
  df_xfer(block);
  df_xfer(off);
  for (word i = 0; i < len; ++i)
    ((byte*) buf)[(byte) i] = df_xfer(0);
  df_deselect();
}

void df_write (word block, const void* buf) {
  df_writeCmd(0x02); // Byte/Page Program
  df_xfer(block >> 8);
  df_xfer(block);
  df_xfer(0);
  for (word i = 0; i < 256; ++i)
    df_xfer(((const byte*) buf)[(byte) i]);
  df_deselect();
}

// wait for current command to complete
void df_flush () {
  df_read(0, 0, 0, 0);
}

static void df_wipe () {
  Serial.println("DF W");

  df_writeCmd(0xC7); // Chip Erase
  df_deselect();
  df_flush();
}

static void df_erase (word block) {
  Serial.print("DF E ");
  Serial.println(block);

  df_writeCmd(DF_PAGE_ERASE); // Block Erase
  df_xfer(block >> 8);
  df_xfer(block);
  df_xfer(0);
  df_deselect();
  df_flush();
}

static word df_wrap (word page) {
  return page < DF_LOG_LIMIT ? page : DF_LOG_BEGIN;
}

static void df_saveBuf () {
  if (dfFill == 0)
    return;

  dfLastPage = df_wrap(dfLastPage + 1);
  if (dfLastPage == DF_LOG_BEGIN)
    ++dfBuf.seqnum; // bump to next seqnum when wrapping

  // set remainder of buffer data to 0xFF and calculate crc over entire buffer
  dfBuf.crc = ~0;
  for (byte i = 0; i < sizeof dfBuf - 2; ++i) {
    if (dfFill <= i && i < sizeof dfBuf.data)
      dfBuf.data[i] = 0xFF;
    dfBuf.crc = _crc16_update(dfBuf.crc, dfBuf.data[i]);
  }

  df_write(dfLastPage, &dfBuf);
  dfFill = 0;

  // wait for write to finish before reporting page, seqnum, and time stamp
  df_flush();
  Serial.print(F("DF S "));
  Serial.print(dfLastPage);
  Serial.print(' ');
  Serial.print(dfBuf.seqnum);
  Serial.print(' ');
  Serial.println(dfBuf.timestamp);

  // erase next block if we just saved data into a fresh block
  // at this point in time dfBuf is empty, so a lengthy erase cycle is ok
  if (dfLastPage % DF_BLOCK_SIZE == 0)
    df_erase(df_wrap(dfLastPage + DF_BLOCK_SIZE));
}

static void df_append (const void* buf, byte len) {
  //FIXME the current logic can't append incoming packets during a save!

  // fill in page time stamp when appending to a fresh page
  if (dfFill == 0)
    dfBuf.timestamp = now();

  long offset = now() - dfBuf.timestamp;
  if (offset >= 255 || dfFill + 1 + len > sizeof dfBuf.data) {
    df_saveBuf();

    dfBuf.timestamp = now();
    offset = 0;
  }

  // append new entry to flash buffer
  dfBuf.data[dfFill++] = offset;
  memcpy(dfBuf.data + dfFill, buf, len);
  dfFill += len;
}

// go through entire log buffer to figure out which page was last saved
static void scanForLastSave () {
  dfBuf.seqnum = 0;
  dfLastPage = DF_LOG_LIMIT - 1;
  // look for last page before an empty page
  for (word page = DF_LOG_BEGIN; page < DF_LOG_LIMIT; ++page) {
    word currseq;
    df_read(page, sizeof dfBuf.data, &currseq, sizeof currseq);
    if (currseq != 0xFFFF) {
      dfLastPage = page;
      dfBuf.seqnum = currseq + 1;
    }
    else if (dfLastPage == page - 1)
      break; // careful with empty-filled-empty case, i.e. after wrap
  }
}

static void df_initialize () {
  // assumes SPI has already been initialized for the RFM12B
  df_disable();
  pinMode(DF_ENABLE_PIN, OUTPUT);
  df_command(0x9F); // Read Manufacturer and Device ID
  word info = df_xfer(0) << 8;
  info |= df_xfer(0);
  df_deselect();

  if (info == DF_DEVICE_ID) {
    df_writeCmd(0x01);  // Write Status Register ...
    df_xfer(0);         // ... Global Unprotect
    df_deselect();

    scanForLastSave();
    // df_wipe();
    df_saveBuf(); //XXX
  }
}

static void discardInput () {
  while (Serial.read() >= 0)
    ;
}

static void df_dump () {
  struct {
    word seqnum;
    long timestamp;
    word crc;
  }
  curr;
  discardInput();
  for (word page = DF_LOG_BEGIN; page < DF_LOG_LIMIT; ++page) {
    if (Serial.read() >= 0)
      break;
    // read marker from page in flash
    df_read(page, sizeof dfBuf.data, &curr, sizeof curr);
    if (curr.seqnum == 0xFFFF)
      continue; // page never written to
  }
}

static word scanForMarker (word seqnum, long asof) {
  word lastPage = 0;
  struct {
    word seqnum;
    long timestamp;
  }
  last, curr;
  last.seqnum = 0xFFFF;
  // go through all the pages in log area of flash
  for (word page = DF_LOG_BEGIN; page < DF_LOG_LIMIT; ++page) {
    // read seqnum and timestamp from page in flash
    df_read(page, sizeof dfBuf.data, &curr, sizeof curr);
    if (curr.seqnum == 0xFFFF)
      continue; // page never written to
    if (curr.seqnum >= seqnum && curr.seqnum < last.seqnum) {
      last = curr;
      lastPage = page;
    }
    if (curr.seqnum == last.seqnum && curr.timestamp <= asof)
      lastPage = page;
  }
  return lastPage;
}

static void df_replay (word seqnum, long asof) {
  word page = scanForMarker(seqnum, asof);

  discardInput();
  word savedSeqnum = dfBuf.seqnum;
  while (page != dfLastPage) {
    if (Serial.read() >= 0)
      break;
    page = df_wrap(page + 1);
    df_read(page, 0, &dfBuf, sizeof dfBuf); // overwrites ram buffer!
    if (dfBuf.seqnum == 0xFFFF)
      continue; // page never written to
    // skip and report bad pages
    word crc = ~0;
    for (word i = 0; i < sizeof dfBuf; ++i)
      crc = _crc16_update(crc, dfBuf.data[i]);
    if (crc != 0) {
      continue;
    }
    // report each entry as "R seqnum time <data...>"
    byte i = 0;
    while (i < sizeof dfBuf.data && dfBuf.data[i] < 255) {
      if (Serial.available())
        break;
      byte n = dfBuf.data[i++];
      while (n-- > 0) {
        Serial.print(' ');
        Serial.print((int) dfBuf.data[i++]);
      }
      Serial.println();
    }
  
  }
  dfFill = 0; // ram buffer is no longer valid
  dfBuf.seqnum = savedSeqnum + 1; // so next replay will start at a new value

}

#else // DATAFLASH

#define df_present() 0
#define df_initialize()
#define df_dump()
#define df_replay(x,y)
#define df_erase(x)

#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void showString (PGM_P s) {
  for (;;) {
    char c = pgm_read_byte(s++);
    if (c == 0)
      break;
    if (c == '\n')
      Serial.print('\r');
    Serial.print(c);
  }
}


static void handleInput (char c) {
  if ('0' <= c && c <= '9')
    value = 10 * value + c - '0';
  else if (c == ',') {
    if (top < sizeof stack)
      stack[top++] = value;
    value = 0;
  }
  else if ('a' <= c && c <='z') {
    Serial.print(F("> "));
    Serial.print((int) value);
    Serial.println(c);
    switch (c) {
    default:

      break;
    case 'i': // set node id
      config.nodeId = (config.nodeId & 0xE0) + (value & 0x1F);
      saveConfig();
      break;
    case 'b': // set band: 4 = 433, 8 = 868, 9 = 915
      value = value == 8 ? RF12_868MHZ :
      value == 9 ? RF12_915MHZ : RF12_433MHZ;
      config.nodeId = (value << 6) + (config.nodeId & 0x3F);
      saveConfig();
      break;
    case 'g': // set network group
      config.group = value;
      saveConfig();
      break;
    case 'c': // set collect mode (off = 0, on = 1)
      if (value)
        config.nodeId |= COLLECT;
      else
        config.nodeId &= ~COLLECT;
      saveConfig();
      break;
    case 't': // broadcast a maximum size test packet, request an ack
      cmd = 'a';
      sendLen = RF12_MAXDATA;
      dest = 0;
      for (byte i = 0; i < RF12_MAXDATA; ++i)
        testbuf[i] = i + testCounter;
      Serial.print("test ");
      Serial.println((int) testCounter); // first byte in test buffer
      ++testCounter;
      break;
    case 'a': // send packet to node ID N, request an ack
    case 's': // send packet to node ID N, no ack
      cmd = c;
      sendLen = top;
      dest = value;
      memcpy(testbuf, stack, top);
      break;
    case 'l': // turn activity LED on or off
      activityLed(value);
      break;
    case 'f': // send FS20 command: <hchi>,<hclo>,<addr>,<cmd>f
      rf12_initialize(0, RF12_868MHZ);
      activityLed(1);
      fs20cmd(256 * stack[0] + stack[1], stack[2], value);
      activityLed(0);
      rf12_config(); // restore normal packet listening mode
      break;
    case 'k': // send KAKU command: <addr>,<dev>,<on>k
      rf12_initialize(0, RF12_433MHZ);
      activityLed(1);
      kakuSend(stack[0], stack[1], value);
      activityLed(0);
      rf12_config(); // restore normal packet listening mode
      break;
    case 'd': // dump all log markers
      if (df_present())
        df_dump();
      break;
    case 'r': // replay from specified seqnum/time marker
      if (df_present()) {
        word seqnum = (stack[0] << 8) || stack[1];
        long asof = (stack[2] << 8) || stack[3];
        asof = (asof << 16) | ((stack[4] << 8) || value);
        df_replay(seqnum, asof);
      }
      break;
    case 'e': // erase specified 4Kb block
      if (df_present() && stack[0] == 123) {
        word block = (stack[1] << 8) | value;
        df_erase(block);
      }
      break;
    case 'w': // wipe entire flash memory
      if (df_present() && stack[0] == 12 && value == 34) {
        df_wipe();
        Serial.println("erased");
      }
      break;
    case 'q': // turn quiet mode on or off (don't report bad packets)
      quiet = value;
      break;
    }
    value = top = 0;
    memset(stack, 0, sizeof stack);
  }
  else if ('A' <= c && c <= 'Z') {
    config.nodeId = (config.nodeId & 0xE0) + (c & 0x1F);
    saveConfig();
  }
  else if (c > ' ')
    int cv=0;

}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.print(F("\n[RF12demo.8]"));
  activityLed(0);
  lcd.begin(20, 4);
  lcd.createChar(0, degree);
  lcd.createChar(1, battery);
  RTC.begin();
  DateTime now = RTC.now();
  //RTC.adjust(DateTime(__DATE__, __TIME__));

  if (rf12_config()) {
    config.nodeId = eeprom_read_byte(RF12_EEPROM_ADDR);
    config.group = eeprom_read_byte(RF12_EEPROM_ADDR + 1);
  }
  else {
    config.nodeId = 0x41; // 433 MHz, node 1
    config.group = 0x4b;  // default group 75
    saveConfig();
  }

  df_initialize();
  homeScreenOut();
}

void loop() {
  lcd.setCursor(2,1);
  DateTime now = RTC.now();
  if (now.hour() > 12){
    lcd.print (now.hour()-12);
  }
  else
  lcd.print(now.hour());
  
  lcd.print(':');
  if (now.minute()<10)
    lcd.print("0");
  lcd.print(now.minute());
 
  unsigned long currentMillis = millis();
  currentMillis3 =millis();
  currentMillis2 =millis();
  currentMillis1 =millis();
  
  if (Serial.available())
    handleInput(Serial.read());
    
      if (currentMillis-previousMillis >interval){
      previousMillis=currentMillis;
      if (out == 1){
        homeScreenOut();
        out=0;
        delay(100);
        }
      
      else {
       
        if (out == 0){
          homeScreenIn();
          out=2;
        }
         else {
       
        if (out == 2){
          homeScreenIn2();
          out=1;
        }
      }
     }
    }
   
  if (rf12_recvDone()) {
    activityLed(0);

    byte n = rf12_len;
    if (rf12_crc == 0) {
      Serial.print(F("OK "));
      pkg++;
    }
    if (config.group == 0) {
      Serial.print(F("G "));
      Serial.print((int) rf12_grp);
    }
    if(rf12_hdr == 33 || rf12_hdr == 1){
      measureOut= *(PayloadOut*) rf12_data;
      previousMillis3 = currentMillis3;
      bat3 = 0;
      if (tmp2==0)
    {
      tmpOutLow=measureOut.temp;
      tmpOutHigh=measureOut.temp;
      tmp2=1;

    }
      if(measureOut.temp <tmpOutLow && tmpOutLow-measureOut.temp < 5)
      tmpOutLow=measureOut.temp;

    if(measureOut.temp >tmpOutHigh && measureOut.temp - tmpOutHigh < 5)
      tmpOutHigh=measureOut.temp;
    }
    if (rf12_hdr == 34 || rf12_hdr == 2){
     
      measureIn= *(PayloadIn*) rf12_data;
      previousMillis2 = currentMillis1;
      bat2 = 0;   
      }
    
    if(rf12_hdr == 35 || rf12_hdr == 3){
      measureIn2= *(PayloadIn2*) rf12_data;
      previousMillis1 = currentMillis3;
      bat1 = 0;
      if (tmp==0)
    {
      tmpLow=measureIn2.temp;
      tmpHigh=measureIn2.temp;
      tmp=1;

    }
      if(measureIn2.temp <tmpLow && tmpLow-measureIn2.temp < 5)
      tmpLow=measureIn2.temp;

    if(measureIn2.temp >tmpHigh && measureIn2.temp - tmpHigh < 5)
      tmpHigh=measureIn2.temp;
    }

    if (rf12_crc == 0) {

      if (df_present())
        df_append((const char*) rf12_data - 2, rf12_len + 2);

      if (RF12_WANTS_ACK && (config.nodeId & COLLECT) == 0) {
        Serial.println(" -> ack");
        rf12_sendStart(RF12_ACK_REPLY, 0, 0);
      }
    }
  activityLed(1);

  }

  if (cmd && rf12_canSend()) {
    Serial.print(F(" -> "));
    Serial.print((int) sendLen);
    Serial.println(F(" b"));
    byte header = cmd == 'a' ? RF12_HDR_ACK : 0;
    if (dest)
      header |= RF12_HDR_DST | dest;
    rf12_sendStart(header, testbuf, sendLen);
    cmd = 0;
  }
}


void homeScreenOut()
{
  if (currentMillis3-previousMillis3 >interval3){
          previousMillis3=currentMillis3;
          bat3 = 1;
  }
  lcd.clear();
  lcd.print(F("Miller House Out "));
  screenBasics();
  lcd.print(measureOut.temp);
  lcd.write(byte(0));
  lcd.print(F(" F"));
  lcd.setCursor(0,2);
  lcd.print(F("W"));
  lcd.setCursor(2,2);
  lcd.print(measureOut.wind);
  lcd.print(F(" R "));
  lcd.print(measureOut.rain);
  lcd.print(F(" H "));
  lcd.print(tmpOutHigh);
  lcd.print(F(" L "));
  lcd.print(tmpOutLow);
  lcd.setCursor(0,3);
  lcd.print(F("Bat "));
  if (measureOut.lobat == 0 && bat3 == 0)
  lcd.print(F("Good"));
  else{
 badBat();
  }
}



void homeScreenIn()
{
   if (currentMillis2-previousMillis2 >interval2){
          previousMillis2=currentMillis2;
          bat2 = 1;
        }
    lcd.clear();
  lcd.print(F("Miller House In "));
  screenBasics();
  lcd.print(measureIn.temp);
  lcd.write(byte(0));
  lcd.print(F(" F"));
  lcd.setCursor(0,2);
  lcd.print(F("L "));
  lcd.print(measureIn.light);
  lcd.print(F(" M "));
  lcd.print(measureIn.moved);
  lcd.print(F(" H "));
  lcd.print(measureIn.humi);
  lcd.print(F(" %"));
  lcd.setCursor(0,3);
  lcd.print(F("Bat "));
  if (measureIn.lobat == 0 && bat2==0)
  lcd.print(F("Good"));
  else{
 badBat();
  }
}

void homeScreenIn2()
{
   if (currentMillis1-previousMillis1 >interval1){
          previousMillis1=currentMillis1;
          bat1 = 1;
        }
    lcd.clear();
  lcd.print(F("Miller House In 2"));
  screenBasics();
  lcd.print(measureIn2.temp);
  lcd.write(byte(0));
  lcd.print(F(" F"));
  lcd.setCursor(0,2);
  lcd.print(F("L "));
  lcd.print(measureIn2.light);
  lcd.print(F(" M "));
  lcd.print(measureIn2.moved);
  lcd.print(F(" H "));
  lcd.print(tmpHigh);
  lcd.print(F(" L "));
  lcd.print(tmpLow);
  lcd.setCursor(0,3);
  lcd.print(F("Bat "));
  if (measureIn2.lobat == 0 && bat2==0)
  lcd.print(F("Good"));
  else{
        badBat();
      }
}

void screenBasics(){
  lcd.setCursor(0,1);
  lcd.print(F("T "));
  DateTime now = RTC.now();
  if (now.hour() > 12){
    lcd.print (now.hour()-12);
  }
  else
  lcd.print(now.hour());
  lcd.print(':');
  if (now.minute()<10)
    lcd.print(F("0"));
  lcd.print(now.minute());
  lcd.setCursor(8,1);
  lcd.print(F("T "));
}

void badBat(){
    DateTime now = RTC.now();
    lcd.print(F("Bad"));
    lcd.write(byte(1));
    hstamp = now.hour();
    if (now.hour() > 12)
    hstamp = now.hour()-12;  
  else
    hstamp = now.hour();
    
    mstamp=now.minute();
    lcd.setCursor(10,3);
    lcd.print(F("          "));
    lcd.print(hstamp);
    lcd.print(':');
    if (mstamp < 10)
      lcd.print(F("0"));
    lcd.print(mstamp);
}
