#include <SPI.h>
#include <Keyboard.h>

// =====================================================
// Raspberry Pi Pico + CC1101
// 360 BOOTH RF REMOTE
// PHYSICAL BUTTONS + WINDOWS SERIAL CONTROL + DSLRBOOTH KILL
// =====================================================

#define BTN_KILL       2
#define BTN_ONOFF      3
#define BTN_REVERSE    4
#define BTN_SPEED_UP   5
#define BTN_SPEED_DOWN 6

#define CC_MISO  16
#define CC_CS    17
#define CC_SCK   18
#define CC_MOSI  19
#define CC_GDO0  20

#define CC_SRES   0x30
#define CC_SCAL   0x33
#define CC_STX    0x35
#define CC_SIDLE  0x36

#define CC_IOCFG2     0x00
#define CC_IOCFG0     0x02
#define CC_PKTCTRL1   0x07
#define CC_PKTCTRL0   0x08
#define CC_FSCTRL1    0x0B
#define CC_FSCTRL0    0x0C
#define CC_FREQ2      0x0D
#define CC_FREQ1      0x0E
#define CC_FREQ0      0x0F
#define CC_MDMCFG4    0x10
#define CC_MDMCFG3    0x11
#define CC_MDMCFG2    0x12
#define CC_MDMCFG1    0x13
#define CC_MDMCFG0    0x14
#define CC_DEVIATN    0x15
#define CC_MCSM0      0x18
#define CC_FOCCFG     0x19
#define CC_AGCCTRL2   0x1B
#define CC_AGCCTRL1   0x1C
#define CC_AGCCTRL0   0x1D
#define CC_FREND0     0x22
#define CC_FSCAL3     0x23
#define CC_FSCAL2     0x24
#define CC_FSCAL1     0x25
#define CC_FSCAL0     0x26
#define CC_TEST2      0x2C
#define CC_TEST1      0x2D
#define CC_TEST0      0x2E
#define CC_PARTNUM    0x30
#define CC_VERSION    0x31
#define CC_PATABLE    0x3E

const float TX_FREQUENCY_MHZ = 315.000;

const uint32_t CODE_ONOFF      = 2095688UL; // 0x1FFA48
const uint32_t CODE_REVERSE    = 2095684UL; // 0x1FFA44
const uint32_t CODE_SPEED_UP   = 2095682UL; // 0x1FFA42
const uint32_t CODE_SPEED_DOWN = 2095681UL; // 0x1FFA41

const uint8_t CODE_BITS = 24;

const uint16_t ZERO_HIGH_US = 335;
const uint16_t ZERO_LOW_US  = 1122;
const uint16_t ONE_HIGH_US  = 1070;
const uint16_t ONE_LOW_US   = 397;
const uint16_t SYNC_HIGH_US = 335;
const uint16_t SYNC_LOW_US  = 11280;

const uint8_t REPEATS_ONOFF   = 12;
const uint8_t REPEATS_CONTROL = 20;
const uint8_t REPEATS_KILL    = 20;

const uint16_t DEBOUNCE_MS = 25;

String serialCommandBuffer;

void ccSelect() {
  digitalWrite(CC_CS, LOW);
  uint32_t start = micros();
  while (digitalRead(CC_MISO)) {
    if (micros() - start > 5000) break;
  }
}

void ccDeselect() {
  digitalWrite(CC_CS, HIGH);
}

uint8_t ccStrobe(uint8_t command) {
  ccSelect();
  uint8_t status = SPI.transfer(command);
  ccDeselect();
  return status;
}

void ccWriteReg(uint8_t address, uint8_t value) {
  ccSelect();
  SPI.transfer(address);
  SPI.transfer(value);
  ccDeselect();
}

uint8_t ccReadStatus(uint8_t address) {
  ccSelect();
  SPI.transfer(address | 0xC0);
  uint8_t value = SPI.transfer(0x00);
  ccDeselect();
  return value;
}

void ccReset() {
  digitalWrite(CC_CS, HIGH);
  delayMicroseconds(5);
  digitalWrite(CC_CS, LOW);
  delayMicroseconds(10);
  digitalWrite(CC_CS, HIGH);
  delayMicroseconds(50);
  ccSelect();
  SPI.transfer(CC_SRES);
  ccDeselect();
  delay(5);
}

void ccSetFrequency(float mhz) {
  uint32_t freqWord = (uint32_t)((mhz * 65536.0) / 26.0);
  ccWriteReg(CC_FREQ2, (freqWord >> 16) & 0xFF);
  ccWriteReg(CC_FREQ1, (freqWord >> 8) & 0xFF);
  ccWriteReg(CC_FREQ0, freqWord & 0xFF);
}

void ccWritePATABLE() {
  ccSelect();
  SPI.transfer(CC_PATABLE | 0x40);
  SPI.transfer(0x00); // OOK off
  SPI.transfer(0x51); // OOK on, approx 0 dBm at 315 MHz
  ccDeselect();
}

void configureTX() {
  ccStrobe(CC_SIDLE);
  ccWriteReg(CC_IOCFG0, 0x2E);
  ccWriteReg(CC_IOCFG2, 0x29);
  ccWriteReg(CC_PKTCTRL1, 0x00);
  ccWriteReg(CC_PKTCTRL0, 0x32); // asynchronous serial mode
  ccWriteReg(CC_FSCTRL1, 0x06);
  ccWriteReg(CC_FSCTRL0, 0x00);
  ccWriteReg(CC_MDMCFG4, 0x7B);
  ccWriteReg(CC_MDMCFG3, 0x83);
  ccWriteReg(CC_MDMCFG2, 0x30); // ASK/OOK
  ccWriteReg(CC_MDMCFG1, 0x22);
  ccWriteReg(CC_MDMCFG0, 0xF8);
  ccWriteReg(CC_DEVIATN, 0x00);
  ccWriteReg(CC_MCSM0, 0x18);
  ccWriteReg(CC_FOCCFG, 0x16);
  ccWriteReg(CC_AGCCTRL2, 0x43);
  ccWriteReg(CC_AGCCTRL1, 0x40);
  ccWriteReg(CC_AGCCTRL0, 0x91);
  ccWriteReg(CC_FREND0, 0x11);
  ccWriteReg(CC_FSCAL3, 0xE9);
  ccWriteReg(CC_FSCAL2, 0x2A);
  ccWriteReg(CC_FSCAL1, 0x00);
  ccWriteReg(CC_FSCAL0, 0x1F);
  ccWriteReg(CC_TEST2, 0x81);
  ccWriteReg(CC_TEST1, 0x35);
  ccWriteReg(CC_TEST0, 0x09);
  ccSetFrequency(TX_FREQUENCY_MHZ);
  ccWritePATABLE();
  ccStrobe(CC_SCAL);
  delay(3);
  ccStrobe(CC_SIDLE);
}

void sendBit(bool bitValue) {
  if (bitValue) {
    digitalWrite(CC_GDO0, HIGH);
    delayMicroseconds(ONE_HIGH_US);
    digitalWrite(CC_GDO0, LOW);
    delayMicroseconds(ONE_LOW_US);
  } else {
    digitalWrite(CC_GDO0, HIGH);
    delayMicroseconds(ZERO_HIGH_US);
    digitalWrite(CC_GDO0, LOW);
    delayMicroseconds(ZERO_LOW_US);
  }
}

void sendSync() {
  digitalWrite(CC_GDO0, HIGH);
  delayMicroseconds(SYNC_HIGH_US);
  digitalWrite(CC_GDO0, LOW);
  delayMicroseconds(SYNC_LOW_US);
}

void sendFrame(uint32_t code) {
  for (int bit = CODE_BITS - 1; bit >= 0; bit--) {
    sendBit((code >> bit) & 1U);
  }
  sendSync();
}

void transmitCommand(uint32_t code, const char* commandName, uint8_t repeats) {
  Serial.print("TX ");
  Serial.print(commandName);
  Serial.print(" ");
  Serial.print(code);
  Serial.print(" 0x");
  Serial.println(code, HEX);

  digitalWrite(CC_GDO0, LOW);
  ccStrobe(CC_STX);
  delayMicroseconds(1000);

  for (uint8_t repeat = 0; repeat < repeats; repeat++) {
    sendFrame(code);
  }

  digitalWrite(CC_GDO0, LOW);
  ccStrobe(CC_SIDLE);
  Serial.print("OK ");
  Serial.println(commandName);
}

void cancelDslrBooth() {
  Serial.println("HID ESC");
  Keyboard.press(KEY_ESC);
  delay(100);
  Keyboard.release(KEY_ESC);
  delay(50);
  Keyboard.releaseAll();
}

void killBooth() {
  Serial.println("KILL START");
  cancelDslrBooth();

  // The receiver's ON/OFF command is a toggle. This is an operational
  // stop/cancel only, not a safety-rated emergency-stop mechanism.
  transmitCommand(CODE_ONOFF, "KILL", REPEATS_KILL);
  Serial.println("KILL COMPLETE");
}

void processSerialCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command.length() == 0) return;

  if (command == "PING") {
    Serial.println("PICO360 READY");
  }
  else if (command == "STATUS") {
    Serial.println("PICO360 STATUS READY 315.000MHz");
  }
  else if (command == "ONOFF") {
    transmitCommand(CODE_ONOFF, "ONOFF", REPEATS_ONOFF);
  }
  else if (command == "REVERSE") {
    transmitCommand(CODE_REVERSE, "REVERSE", REPEATS_CONTROL);
  }
  else if (command == "SPEED_UP") {
    transmitCommand(CODE_SPEED_UP, "SPEED_UP", REPEATS_CONTROL);
  }
  else if (command == "SPEED_DOWN") {
    transmitCommand(CODE_SPEED_DOWN, "SPEED_DOWN", REPEATS_CONTROL);
  }
  else if (command == "KILL") {
    killBooth();
  }
  else {
    Serial.print("ERR UNKNOWN_COMMAND ");
    Serial.println(command);
  }
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialCommandBuffer.length() > 0) {
        processSerialCommand(serialCommandBuffer);
        serialCommandBuffer = "";
      }
    }
    else if (serialCommandBuffer.length() < 64) {
      serialCommandBuffer += c;
    }
    else {
      serialCommandBuffer = "";
      Serial.println("ERR COMMAND_TOO_LONG");
    }
  }
}

bool buttonPressed(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    delay(DEBOUNCE_MS);
    if (digitalRead(pin) == LOW) return true;
  }
  return false;
}

void waitForRelease(uint8_t pin) {
  while (digitalRead(pin) == LOW) {
    handleSerialInput();
    delay(5);
  }
  delay(30);
}

void setup() {
  Serial.begin(115200);
  Keyboard.begin();
  delay(1500);

  pinMode(BTN_KILL, INPUT_PULLUP);
  pinMode(BTN_ONOFF, INPUT_PULLUP);
  pinMode(BTN_REVERSE, INPUT_PULLUP);
  pinMode(BTN_SPEED_UP, INPUT_PULLUP);
  pinMode(BTN_SPEED_DOWN, INPUT_PULLUP);

  pinMode(CC_CS, OUTPUT);
  digitalWrite(CC_CS, HIGH);
  pinMode(CC_MISO, INPUT);
  pinMode(CC_GDO0, OUTPUT);
  digitalWrite(CC_GDO0, LOW);

  SPI.setRX(CC_MISO);
  SPI.setCS(CC_CS);
  SPI.setSCK(CC_SCK);
  SPI.setTX(CC_MOSI);
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  ccReset();

  uint8_t part = ccReadStatus(CC_PARTNUM);
  uint8_t version = ccReadStatus(CC_VERSION);

  Serial.println("PICO360 BOOT");
  Serial.print("PARTNUM 0x");
  if (part < 16) Serial.print('0');
  Serial.println(part, HEX);
  Serial.print("VERSION 0x");
  if (version < 16) Serial.print('0');
  Serial.println(version, HEX);

  if (part == 0x00 && version == 0x14) {
    Serial.println("CC1101 OK");
  } else {
    Serial.println("CC1101 WARNING");
  }

  configureTX();

  Serial.println("PICO360 READY");
  Serial.println("COMMANDS PING STATUS ONOFF REVERSE SPEED_UP SPEED_DOWN KILL");
}

void loop() {
  handleSerialInput();

  if (buttonPressed(BTN_KILL)) {
    killBooth();
    waitForRelease(BTN_KILL);
  }
  else if (buttonPressed(BTN_ONOFF)) {
    transmitCommand(CODE_ONOFF, "ONOFF", REPEATS_ONOFF);
    waitForRelease(BTN_ONOFF);
  }
  else if (buttonPressed(BTN_REVERSE)) {
    transmitCommand(CODE_REVERSE, "REVERSE", REPEATS_CONTROL);
    waitForRelease(BTN_REVERSE);
  }
  else if (buttonPressed(BTN_SPEED_UP)) {
    transmitCommand(CODE_SPEED_UP, "SPEED_UP", REPEATS_CONTROL);
    waitForRelease(BTN_SPEED_UP);
  }
  else if (buttonPressed(BTN_SPEED_DOWN)) {
    transmitCommand(CODE_SPEED_DOWN, "SPEED_DOWN", REPEATS_CONTROL);
    waitForRelease(BTN_SPEED_DOWN);
  }

  delay(2);
}
