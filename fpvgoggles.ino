#include <EEPROM.h>
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>

// Channels to sent to the SPI registers
static const uint16_t channelTable[8*5] = {
  // Channel 1 - 8
  0x2A05,    0x299B,    0x2991,    0x2987,    0x291D,    0x2913,    0x2909,    0x289F,    // Band A
  0x2903,    0x290C,    0x2916,    0x291F,    0x2989,    0x2992,    0x299C,    0x2A05,    // Band B
  0x2895,    0x288B,    0x2881,    0x2817,    0x2A0F,    0x2A19,    0x2A83,    0x2A8D,    // Band E
  0x2906,    0x2910,    0x291A,    0x2984,    0x298E,    0x2998,    0x2A02,    0x2A0C,    // Band F / Airwave
  0x281D,    0x288F,    0x2902,    0x2914,    0x2978,    0x2999,    0x2A0C,    0x2A1E     // Band C / Immersion Raceband
};

// Channels with their Mhz Values
static const uint16_t channelFreqTable[8*5] = {
  // Channel 1 - 8
  5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // Band A
  5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // Band B
  5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // Band E
  5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // Band F / Airwave
  5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917  // Band C / Immersion Raceband
};

#define rx_channels     8
#define rx_bands        5
#define eeprom_band 0
#define eeprom_channel 1

const String  rx_band_text[5] = { "A", "B", "E", "F", "R" };

enum { btn_event_none=0, btn_event_click, btn_event_hold};
uint8_t         btn_pin        = 16;
uint8_t         btn_state      = 1;
uint8_t         btn_last_state = 1;
uint8_t         btn_event      = 0;
unsigned long   btn_time       = 0;

uint8_t         rx_pin_dat        = 9;
uint8_t         rx_pin_sel        = 10;
uint8_t         rx_pin_clk        = 11;
uint8_t         rx_channel        = 0;
uint8_t         rx_band           = 0;
uint8_t         rx_eeprom_channel = 0;
uint8_t         rx_eeprom_band    = 1;

uint8_t         rssi_pin          = A3;
int             min_recorded_rssi = 300;
int             max_recorded_rssi = 100;

//U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0);
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);   // pin remapping with ESP8266 HW I2C


void setup() {      
  Serial.begin(9600);

  u8g2.begin();
  u8g2.setFont(u8g2_font_crox4hb_tr);
  u8g2.setFontPosTop();
  u8g2.setContrast(1);

  pinMode(btn_pin, INPUT_PULLUP);  

  pinMode (rx_pin_dat, OUTPUT);
  pinMode (rx_pin_sel, OUTPUT);
  pinMode (rx_pin_clk, OUTPUT);
  
  //initialize all rx
  eeprom_read(rx_eeprom_channel, rx_channel);
  eeprom_read(rx_eeprom_band, rx_band);
  set_channel(rx_channel, rx_band);
}




void loop() {

  handle_buttons();

  if (btn_event == btn_event_click)
  {
    rx_channel++;
    if(rx_channel>7) rx_channel = 0;
    set_channel(rx_channel, rx_band);
    eeprom_write(rx_eeprom_channel, rx_channel);
  }

  if (btn_event == btn_event_hold)
  {
    rx_band++;
    if(rx_band>4) rx_band = 0;
    set_channel(rx_channel, rx_band);
    eeprom_write(rx_eeprom_band, rx_band);
  }

  u8g2.clearBuffer();
  
  u8g2.setFont(u8g2_font_crox4hb_tr);
  u8g2.setDrawColor(1);

  u8g2.setCursor((0 * 32) + 3, 2);
  u8g2.print(rx_band_text[rx_band]);
  u8g2.setCursor((0 * 32) + 17, 2);
  u8g2.print(rx_channel + 1);

  u8g2.setFont(u8g2_font_crox1hb_tr);
  u8g2.setCursor((0 * 32) + 1, 20);
  u8g2.print(channelFreqTable[rx_band*8+rx_channel]);

  u8g2.setCursor((1 * 32) + 3, 2);
  int rssi = analogRead(rssi_pin); // read the input pin
  Serial.println(rssi);
  max_recorded_rssi = max(rssi,max_recorded_rssi);
  min_recorded_rssi = min(rssi,min_recorded_rssi);
  float rssi_factor = ((float)(rssi-min_recorded_rssi))/((float)(max_recorded_rssi-min_recorded_rssi));
  int width = rssi_factor*((float)(3*32-6));
  u8g2.drawBox(32+3, 2, width, 30);
  
  u8g2.sendBuffer();

  delay(50);
}

void handle_buttons()
{
  btn_event = 0;
  btn_state = digitalRead(btn_pin);

  if (btn_state == LOW && btn_last_state == HIGH)
  {
    //just pushed
    btn_time = millis();
  }

  if (btn_state == HIGH && btn_last_state == LOW)
  {
    //just released
    
    unsigned long t_duration = millis() - btn_time;

    //short click
    if (t_duration < 500)
    {
        btn_event = btn_event_click;
    }

    //long click
    if (t_duration >= 500)
    {
       btn_event = btn_event_hold;
    }
  }
  btn_last_state = btn_state;
}

template <class T> int eeprom_write(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          EEPROM.write(ee++, *p++);
    return i;
}

template <class T> int eeprom_read(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          *p++ = EEPROM.read(ee++);
    return i;
}

void set_channel(uint8_t c, uint8_t b)
{
  setRXChannel(b * 8 + c, rx_pin_dat, rx_pin_sel, rx_pin_clk);
}

void setRXChannel(uint8_t channelIndex, uint8_t rx_sdi, uint8_t rx_sel, uint8_t rx_sck)
{
  uint8_t i;
  uint16_t channelData = channelTable[channelIndex];
  
  // bit bash out 25 bits of data
  // Order: A0-3, !R/W, D0-D19
  // A0=0, A1=0, A2=0, A3=1, RW=0, D0-19=0
  SERIAL_ENABLE_HIGH(rx_sel);
  delay(2);
  SERIAL_ENABLE_LOW(rx_sel);

  SERIAL_SENDBIT0(rx_sdi, rx_sck);
  SERIAL_SENDBIT0(rx_sdi, rx_sck);
  SERIAL_SENDBIT0(rx_sdi, rx_sck);
  SERIAL_SENDBIT1(rx_sdi, rx_sck);
  SERIAL_SENDBIT0(rx_sdi, rx_sck);
  
  // remaining zeros
  for (i=20;i>0;i--)
    SERIAL_SENDBIT0(rx_sdi, rx_sck);
  
  // Clock the data in
  SERIAL_ENABLE_HIGH(rx_sel);
  delayMicroseconds(1);
  SERIAL_ENABLE_LOW(rx_sel);

  // Second is the channel data from the lookup table
  // 20 bytes of register data are sent, but the MSB 4 bits are zeros
  // register address = 0x1, write, data0-15=channelData data15-19=0x0
  SERIAL_ENABLE_HIGH(rx_sel);
  SERIAL_ENABLE_LOW(rx_sel);
  
  // Register 0x1
  SERIAL_SENDBIT1(rx_sdi, rx_sck);
  SERIAL_SENDBIT0(rx_sdi, rx_sck);
  SERIAL_SENDBIT0(rx_sdi, rx_sck);
  SERIAL_SENDBIT0(rx_sdi, rx_sck);
  
  // Write to register
  SERIAL_SENDBIT1(rx_sdi, rx_sck);
  
  // D0-D15
  //   note: loop runs backwards as more efficent on AVR
  for (i=16;i>0;i--)
  {
    // Is bit high or low?
    if (channelData & 0x1)
    {
      SERIAL_SENDBIT1(rx_sdi, rx_sck);
    }
    else
    {
      SERIAL_SENDBIT0(rx_sdi, rx_sck);
    }
    
    // Shift bits along to check the next one
    channelData >>= 1;
  }
  
  // Remaining D16-D19
  for (i=4;i>0;i--)
    SERIAL_SENDBIT0(rx_sdi, rx_sck);
  
  // Finished clocking data in
  SERIAL_ENABLE_HIGH(rx_sel);
  delay(2);
  
  digitalWrite(rx_sel, LOW); 
  digitalWrite(rx_sck, LOW);
  digitalWrite(rx_sdi, LOW);
}

void SERIAL_SENDBIT1(uint8_t rx_sdi, uint8_t rx_sck)
{
  digitalWrite(rx_sck, LOW);
  delayMicroseconds(300);
  digitalWrite(rx_sdi, HIGH);
  delayMicroseconds(300);
  digitalWrite(rx_sck, HIGH);
  delayMicroseconds(300);
  digitalWrite(rx_sck, LOW);
  delayMicroseconds(300);
}

void SERIAL_SENDBIT0(uint8_t rx_sdi, uint8_t rx_sck)
{
  digitalWrite(rx_sck, LOW);
  delayMicroseconds(300);
  digitalWrite(rx_sdi, LOW);
  delayMicroseconds(300);
  digitalWrite(rx_sck, HIGH);
  delayMicroseconds(300);
  digitalWrite(rx_sck, LOW);
  delayMicroseconds(300);
}

void SERIAL_ENABLE_LOW(uint8_t rx_sel)
{
  delayMicroseconds(100);
  digitalWrite(rx_sel,LOW);
  delayMicroseconds(100);
}

void SERIAL_ENABLE_HIGH(uint8_t rx_sel)
{
  delayMicroseconds(100);
  digitalWrite(rx_sel,HIGH);
  delayMicroseconds(100);
}
