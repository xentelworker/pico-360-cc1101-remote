#include <SPI.h>
#include <Wire.h>
#include <Keyboard.h>
#include <IRremote.hpp>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// Raspberry Pi Pico + CC1101 + BPA10 IR + SSD1306 OLED
// Combined 360 booth controller.
// Original Pico_360_CC1101_Controller.ino is intentionally untouched.
// =====================================================

#define BTN_KILL       2
#define BTN_ONOFF      3
#define BTN_REVERSE    4
#define BTN_SPEED_UP   5
#define BTN_SPEED_DOWN 6
#define IR_SEND_PIN    7

#define OLED_SDA 14
#define OLED_SCL 15
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define CC_MISO  16
#define CC_CS    17
#define CC_SCK   18
#define CC_MOSI  19
#define CC_GDO0  20

#define CC_SRES   0x30
#define CC_SCAL   0x33
#define CC_STX    0x35
#define CC_SIDLE  0x36
#define CC_IOCFG2 0x00
#define CC_IOCFG0 0x02
#define CC_PKTCTRL1 0x07
#define CC_PKTCTRL0 0x08
#define CC_FSCTRL1 0x0B
#define CC_FSCTRL0 0x0C
#define CC_FREQ2 0x0D
#define CC_FREQ1 0x0E
#define CC_FREQ0 0x0F
#define CC_MDMCFG4 0x10
#define CC_MDMCFG3 0x11
#define CC_MDMCFG2 0x12
#define CC_MDMCFG1 0x13
#define CC_MDMCFG0 0x14
#define CC_DEVIATN 0x15
#define CC_MCSM0 0x18
#define CC_FOCCFG 0x19
#define CC_AGCCTRL2 0x1B
#define CC_AGCCTRL1 0x1C
#define CC_AGCCTRL0 0x1D
#define CC_FREND0 0x22
#define CC_FSCAL3 0x23
#define CC_FSCAL2 0x24
#define CC_FSCAL1 0x25
#define CC_FSCAL0 0x26
#define CC_TEST2 0x2C
#define CC_TEST1 0x2D
#define CC_TEST0 0x2E
#define CC_PARTNUM 0x30
#define CC_VERSION 0x31
#define CC_PATABLE 0x3E

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

const float TX_FREQUENCY_MHZ = 315.000;
const uint32_t CODE_ONOFF = 2095688UL;
const uint32_t CODE_REVERSE = 2095684UL;
const uint32_t CODE_SPEED_UP = 2095682UL;
const uint32_t CODE_SPEED_DOWN = 2095681UL;
const uint8_t CODE_BITS = 24;
const uint16_t ZERO_HIGH_US = 335, ZERO_LOW_US = 1122;
const uint16_t ONE_HIGH_US = 1070, ONE_LOW_US = 397;
const uint16_t SYNC_HIGH_US = 335, SYNC_LOW_US = 11280;
const uint8_t REPEATS_ONOFF = 12;
const uint8_t REPEATS_CONTROL = 20;
const uint8_t REPEATS_KILL = 20;
const uint16_t DEBOUNCE_MS = 25;

// Rockville BPA10 scanned NEC codes.
const uint16_t BPA10_ADDRESS = 0x00;
const uint8_t IR_VOL_UP = 0x15;
const uint8_t IR_VOL_DOWN = 0x07;
const uint8_t IR_PLAY_PAUSE = 0x43;
const uint8_t IR_NEXT = 0x40;
const uint8_t IR_STOP = 0x45;

// Music cycle: 15 seconds total, fade from 9s through 14s.
const uint32_t MUSIC_TOTAL_MS = 15000UL;
const uint32_t MUSIC_FADE_START_MS = 9000UL;
const uint32_t MUSIC_FADE_STEP_MS = 1000UL;
const uint8_t MUSIC_FADE_STEPS = 6;

String serialCommandBuffer;
bool oledReady = false;
bool cc1101Ready = false;
bool pcConnected = false;
bool boothRunning = false;
bool reverseDirection = false;
uint8_t speedLevel = 3;
unsigned long lastPcMessageMs = 0;

bool musicActive = false;
bool musicTriggeredThisSession = false;
unsigned long musicStartMs = 0;
uint8_t musicFadeStepsSent = 0;

void oledHeader(const char* title) {
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0); display.println(title); display.drawLine(0,10,127,10,SSD1306_WHITE);
}
void showIdle(const char* status="READY") {
  if (!oledReady) return; oledHeader("360 BOOTH CONTROLLER");
  display.setTextSize(2); display.setCursor(28,18); display.println(status);
  display.setTextSize(1); display.setCursor(0,44); display.print("RF: "); display.println(cc1101Ready?"READY":"ERROR");
  display.setCursor(0,54); display.print("PC: "); display.println(pcConnected?"CONNECTED":"WAITING"); display.display();
}
void showCountdown(int seconds) {
  if (!oledReady) return; oledHeader("     GET READY!"); display.setTextSize(4);
  display.setCursor(seconds>=10?40:52,20); display.println(seconds); display.setTextSize(1);
  display.setCursor(24,54); display.println("BOOTH STARTING"); display.display();
}
void showGo() {
  if (!oledReady) return; display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(4);
  display.setCursor(34,15); display.println("GO!"); display.setTextSize(1); display.setCursor(24,53); display.println("360 BOOTH LIVE"); display.display();
}
void showLive(const char* last=nullptr) {
  if (!oledReady) return; oledHeader("   360 BOOTH LIVE"); display.setTextSize(1); display.setCursor(0,16); display.println("STATUS: RUNNING");
  display.setCursor(0,28); display.print("DIR:    "); display.println(reverseDirection?"REVERSE":"FORWARD");
  display.setCursor(0,40); display.print("SPEED:  "); for(uint8_t i=1;i<=5;i++) display.print(i<=speedLevel?'#':'-');
  display.setCursor(0,52); if(last){display.print("LAST: ");display.print(last);}else{display.print("PC: ");display.print(pcConnected?"CONNECTED":"WAITING");} display.display();
}
void showMessage(const char* a,const char* b=nullptr) {
  if(!oledReady)return; display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(2);display.setCursor(0,13);display.println(a);
  if(b){display.setTextSize(1);display.setCursor(0,44);display.println(b);}display.display();
}
void initOLED(){Wire1.setSDA(OLED_SDA);Wire1.setSCL(OLED_SCL);Wire1.begin();if(display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDRESS)){oledReady=true;showMessage("360 BOOTH","BOOTING...");}}

void ccSelect(){digitalWrite(CC_CS,LOW);uint32_t s=micros();while(digitalRead(CC_MISO)){if(micros()-s>5000)break;}}
void ccDeselect(){digitalWrite(CC_CS,HIGH);}
uint8_t ccStrobe(uint8_t c){ccSelect();uint8_t s=SPI.transfer(c);ccDeselect();return s;}
void ccWriteReg(uint8_t a,uint8_t v){ccSelect();SPI.transfer(a);SPI.transfer(v);ccDeselect();}
uint8_t ccReadStatus(uint8_t a){ccSelect();SPI.transfer(a|0xC0);uint8_t v=SPI.transfer(0);ccDeselect();return v;}
void ccReset(){digitalWrite(CC_CS,HIGH);delayMicroseconds(5);digitalWrite(CC_CS,LOW);delayMicroseconds(10);digitalWrite(CC_CS,HIGH);delayMicroseconds(50);ccSelect();SPI.transfer(CC_SRES);ccDeselect();delay(5);}
void ccSetFrequency(float mhz){uint32_t f=(uint32_t)((mhz*65536.0)/26.0);ccWriteReg(CC_FREQ2,(f>>16)&0xFF);ccWriteReg(CC_FREQ1,(f>>8)&0xFF);ccWriteReg(CC_FREQ0,f&0xFF);}
void ccWritePATABLE(){ccSelect();SPI.transfer(CC_PATABLE|0x40);SPI.transfer(0x00);SPI.transfer(0x51);ccDeselect();}
void configureTX(){
  ccStrobe(CC_SIDLE);ccWriteReg(CC_IOCFG0,0x2E);ccWriteReg(CC_IOCFG2,0x29);ccWriteReg(CC_PKTCTRL1,0x00);ccWriteReg(CC_PKTCTRL0,0x32);
  ccWriteReg(CC_FSCTRL1,0x06);ccWriteReg(CC_FSCTRL0,0x00);ccWriteReg(CC_MDMCFG4,0x7B);ccWriteReg(CC_MDMCFG3,0x83);ccWriteReg(CC_MDMCFG2,0x30);
  ccWriteReg(CC_MDMCFG1,0x22);ccWriteReg(CC_MDMCFG0,0xF8);ccWriteReg(CC_DEVIATN,0x00);ccWriteReg(CC_MCSM0,0x18);ccWriteReg(CC_FOCCFG,0x16);
  ccWriteReg(CC_AGCCTRL2,0x43);ccWriteReg(CC_AGCCTRL1,0x40);ccWriteReg(CC_AGCCTRL0,0x91);ccWriteReg(CC_FREND0,0x11);ccWriteReg(CC_FSCAL3,0xE9);
  ccWriteReg(CC_FSCAL2,0x2A);ccWriteReg(CC_FSCAL1,0x00);ccWriteReg(CC_FSCAL0,0x1F);ccWriteReg(CC_TEST2,0x81);ccWriteReg(CC_TEST1,0x35);ccWriteReg(CC_TEST0,0x09);
  ccSetFrequency(TX_FREQUENCY_MHZ);ccWritePATABLE();ccStrobe(CC_SCAL);delay(3);ccStrobe(CC_SIDLE);
}
void sendBit(bool b){if(b){digitalWrite(CC_GDO0,HIGH);delayMicroseconds(ONE_HIGH_US);digitalWrite(CC_GDO0,LOW);delayMicroseconds(ONE_LOW_US);}else{digitalWrite(CC_GDO0,HIGH);delayMicroseconds(ZERO_HIGH_US);digitalWrite(CC_GDO0,LOW);delayMicroseconds(ZERO_LOW_US);}}
void sendSync(){digitalWrite(CC_GDO0,HIGH);delayMicroseconds(SYNC_HIGH_US);digitalWrite(CC_GDO0,LOW);delayMicroseconds(SYNC_LOW_US);}
void sendFrame(uint32_t code){for(int b=CODE_BITS-1;b>=0;b--)sendBit((code>>b)&1U);sendSync();}
void transmitCommand(uint32_t code,const char* name,uint8_t repeats){Serial.print("TX ");Serial.println(name);digitalWrite(CC_GDO0,LOW);ccStrobe(CC_STX);delayMicroseconds(1000);for(uint8_t r=0;r<repeats;r++)sendFrame(code);digitalWrite(CC_GDO0,LOW);ccStrobe(CC_SIDLE);}

void sendBPA10(uint8_t cmd,const char* name){
  Serial.print("IR ");Serial.println(name);IrSender.sendNEC(BPA10_ADDRESS,cmd,0);delay(60);IrSender.sendNEC(BPA10_ADDRESS,cmd,0);
}
void restoreMusicVolume(){for(uint8_t i=0;i<musicFadeStepsSent;i++){sendBPA10(IR_VOL_UP,"VOL+");delay(120);}musicFadeStepsSent=0;}
void finishMusicCycle(bool advanceTrack){
  if(musicActive)sendBPA10(IR_STOP,"STOP"); musicActive=false; restoreMusicVolume();
  if(advanceTrack){delay(150);sendBPA10(IR_NEXT,"NEXT");} Serial.println("MUSIC READY");
}
void startMusicCycle(){
  if(musicActive||musicTriggeredThisSession)return; musicTriggeredThisSession=true;musicFadeStepsSent=0;musicStartMs=millis();musicActive=true;
  sendBPA10(IR_PLAY_PAUSE,"PLAY");Serial.println("MUSIC CYCLE START 15s");
}
void serviceMusicCycle(){
  if(!musicActive)return;uint32_t elapsed=millis()-musicStartMs;
  if(elapsed>=MUSIC_TOTAL_MS){finishMusicCycle(true);return;}
  if(elapsed>=MUSIC_FADE_START_MS){uint8_t target=1+(elapsed-MUSIC_FADE_START_MS)/MUSIC_FADE_STEP_MS;if(target>MUSIC_FADE_STEPS)target=MUSIC_FADE_STEPS;while(musicFadeStepsSent<target){sendBPA10(IR_VOL_DOWN,"VOL-");musicFadeStepsSent++;}}
}

void cancelDslrBooth(){Serial.println("HID ESC");Keyboard.press(KEY_ESC);delay(100);Keyboard.release(KEY_ESC);delay(50);Keyboard.releaseAll();}
void killBooth(){showMessage("!!! STOP !!!","CANCELING SESSION");Serial.println("KILL START");if(musicActive)finishMusicCycle(false);cancelDslrBooth();transmitCommand(CODE_ONOFF,"KILL",REPEATS_KILL);boothRunning=false;showMessage("STOPPED","RF STOP SENT");delay(900);showIdle("STOPPED");Serial.println("KILL COMPLETE");}
void onOffAction(){transmitCommand(CODE_ONOFF,"ONOFF",REPEATS_ONOFF);boothRunning=!boothRunning;if(boothRunning)showLive("ON/OFF");else showIdle("STOPPED");}
void reverseAction(){transmitCommand(CODE_REVERSE,"REVERSE",REPEATS_CONTROL);reverseDirection=!reverseDirection;if(boothRunning)showLive("REVERSE");else showMessage("REVERSE","COMMAND SENT");}
void speedUpAction(){transmitCommand(CODE_SPEED_UP,"SPEED_UP",REPEATS_CONTROL);if(speedLevel<5)speedLevel++;if(boothRunning)showLive("SPEED +");else showMessage("SPEED +","COMMAND SENT");}
void speedDownAction(){transmitCommand(CODE_SPEED_DOWN,"SPEED_DOWN",REPEATS_CONTROL);if(speedLevel>1)speedLevel--;if(boothRunning)showLive("SPEED -");else showMessage("SPEED -","COMMAND SENT");}

void processSerialCommand(String command){
  command.trim();if(!command.length())return;pcConnected=true;lastPcMessageMs=millis();String upper=command;upper.toUpperCase();
  if(upper=="PING"){Serial.println("PICO360 READY");if(!boothRunning)showIdle();}
  else if(upper=="STATUS"){Serial.println("PICO360 STATUS READY 315.000MHz OLED BPA10");if(!boothRunning)showIdle();}
  else if(upper=="ONOFF")onOffAction();else if(upper=="REVERSE")reverseAction();else if(upper=="SPEED_UP")speedUpAction();else if(upper=="SPEED_DOWN")speedDownAction();else if(upper=="KILL")killBooth();
  else if(upper=="MUSIC_TEST"){musicTriggeredThisSession=false;startMusicCycle();}
  else if(upper=="MUSIC_STOP"){if(musicActive)finishMusicCycle(false);}
  else if(upper=="DSLR_SESSION_START"){musicTriggeredThisSession=false;showMessage("PREPARING","DSLRBOOTH SESSION");Serial.println("OK DSLR_SESSION_START");}
  else if(upper.startsWith("DSLR_COUNTDOWN ")){int seconds=upper.substring(15).toInt();if(seconds<0)seconds=0;if(seconds>99)seconds=99;if(!musicTriggeredThisSession)startMusicCycle();showCountdown(seconds);Serial.print("OK DSLR_COUNTDOWN ");Serial.println(seconds);}
  else if(upper=="DSLR_GO"){boothRunning=true;showGo();Serial.println("OK DSLR_GO");}
  else if(upper=="DSLR_PROCESSING"){boothRunning=false;showMessage("PROCESSING","PLEASE WAIT...");Serial.println("OK DSLR_PROCESSING");}
  else if(upper=="DSLR_SHARING"){boothRunning=false;showMessage("COMPLETE","THANK YOU!");Serial.println("OK DSLR_SHARING");}
  else if(upper=="DSLR_SESSION_END"){boothRunning=false;if(musicActive)finishMusicCycle(true);showIdle();Serial.println("OK DSLR_SESSION_END");}
  else{Serial.print("ERR UNKNOWN_COMMAND ");Serial.println(command);}
}
void handleSerialInput(){while(Serial.available()>0){char c=(char)Serial.read();if(c=='\n'||c=='\r'){if(serialCommandBuffer.length()){processSerialCommand(serialCommandBuffer);serialCommandBuffer="";}}else if(serialCommandBuffer.length()<64)serialCommandBuffer+=c;else{serialCommandBuffer="";Serial.println("ERR COMMAND_TOO_LONG");}}}
bool buttonPressed(uint8_t pin){if(digitalRead(pin)==LOW){delay(DEBOUNCE_MS);if(digitalRead(pin)==LOW)return true;}return false;}
void waitForRelease(uint8_t pin){while(digitalRead(pin)==LOW){handleSerialInput();serviceMusicCycle();delay(5);}delay(30);}

void setup(){
  Serial.begin(115200);Keyboard.begin();delay(1000);initOLED();IrSender.begin(IR_SEND_PIN);
  pinMode(BTN_KILL,INPUT_PULLUP);pinMode(BTN_ONOFF,INPUT_PULLUP);pinMode(BTN_REVERSE,INPUT_PULLUP);pinMode(BTN_SPEED_UP,INPUT_PULLUP);pinMode(BTN_SPEED_DOWN,INPUT_PULLUP);
  pinMode(CC_CS,OUTPUT);digitalWrite(CC_CS,HIGH);pinMode(CC_MISO,INPUT);pinMode(CC_GDO0,OUTPUT);digitalWrite(CC_GDO0,LOW);
  SPI.setRX(CC_MISO);SPI.setCS(CC_CS);SPI.setSCK(CC_SCK);SPI.setTX(CC_MOSI);SPI.begin();SPI.beginTransaction(SPISettings(1000000,MSBFIRST,SPI_MODE0));
  ccReset();uint8_t part=ccReadStatus(CC_PARTNUM);uint8_t version=ccReadStatus(CC_VERSION);cc1101Ready=(part==0x00&&version==0x14);configureTX();showIdle();
  Serial.println("PICO360+BPA10 READY");Serial.println("COMMANDS PING STATUS ONOFF REVERSE SPEED_UP SPEED_DOWN KILL MUSIC_TEST MUSIC_STOP");
}
void loop(){
  handleSerialInput();serviceMusicCycle();
  if(pcConnected&&millis()-lastPcMessageMs>15000UL){pcConnected=false;if(!boothRunning)showIdle();}
  if(buttonPressed(BTN_KILL)){killBooth();waitForRelease(BTN_KILL);}else if(buttonPressed(BTN_ONOFF)){onOffAction();waitForRelease(BTN_ONOFF);}else if(buttonPressed(BTN_REVERSE)){reverseAction();waitForRelease(BTN_REVERSE);}else if(buttonPressed(BTN_SPEED_UP)){speedUpAction();waitForRelease(BTN_SPEED_UP);}else if(buttonPressed(BTN_SPEED_DOWN)){speedDownAction();waitForRelease(BTN_SPEED_DOWN);}delay(2);
}
