#include <SPI.h>

// =====================================================
// Raspberry Pi Pico + CC1101
// 315 MHz RAW OOK SNIFFER + 24-BIT AUTO DECODER
// =====================================================
//
// Designed for the protocol documented in this repo:
//   0 = HIGH ~335 us  + LOW ~1122 us
//   1 = HIGH ~1070 us + LOW ~397 us
//   sync = HIGH ~335 us + LOW ~11280 us
//
// The sniffer captures asynchronous data from CC1101 GDO0,
// searches for a sync pulse, then decodes the next 24 bits.
//
// Serial Monitor: 115200 baud
// =====================================================

#define CC_MISO  16
#define CC_CS    17
#define CC_SCK   18
#define CC_MOSI  19
#define CC_GDO0  20

#define CC_SRES   0x30
#define CC_SRX    0x34
#define CC_SIDLE  0x36
#define CC_SFRX   0x3A
#define CC_SCAL   0x33

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
#define CC_FSCAL3     0x23
#define CC_FSCAL2     0x24
#define CC_FSCAL1     0x25
#define CC_FSCAL0     0x26
#define CC_TEST2      0x2C
#define CC_TEST1      0x2D
#define CC_TEST0      0x2E
#define CC_PARTNUM    0x30
#define CC_VERSION    0x31

const float RX_FREQUENCY_MHZ = 315.000;
const uint8_t CODE_BITS = 24;

// Decoder windows. Widen these if your remote is similar but not exact.
const uint16_t SHORT_HIGH_MIN = 250;
const uint16_t SHORT_HIGH_MAX = 500;
const uint16_t LONG_HIGH_MIN  = 850;
const uint16_t LONG_HIGH_MAX  = 1250;
const uint16_t SHORT_LOW_MIN  = 250;
const uint16_t SHORT_LOW_MAX  = 550;
const uint16_t LONG_LOW_MIN   = 850;
const uint16_t LONG_LOW_MAX   = 1300;
const uint16_t SYNC_LOW_MIN   = 8000;
const uint16_t SYNC_LOW_MAX   = 14000;

const uint32_t CAPTURE_END_GAP_US = 25000;
const uint16_t MAX_PULSES = 1200;

struct Pulse {
  uint32_t duration;
  uint8_t level;
};

volatile Pulse pulses[MAX_PULSES];
volatile uint16_t pulseCount = 0;
volatile uint32_t lastEdgeUs = 0;
volatile bool capturing = false;

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

void configureRX() {
  ccStrobe(CC_SIDLE);

  // GDO0 = asynchronous serial data output.
  ccWriteReg(CC_IOCFG0, 0x0D);
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

  ccWriteReg(CC_FSCAL3, 0xE9);
  ccWriteReg(CC_FSCAL2, 0x2A);
  ccWriteReg(CC_FSCAL1, 0x00);
  ccWriteReg(CC_FSCAL0, 0x1F);
  ccWriteReg(CC_TEST2, 0x81);
  ccWriteReg(CC_TEST1, 0x35);
  ccWriteReg(CC_TEST0, 0x09);

  ccSetFrequency(RX_FREQUENCY_MHZ);
  ccStrobe(CC_SCAL);
  delay(3);
  ccStrobe(CC_SIDLE);
  ccStrobe(CC_SFRX);
  ccStrobe(CC_SRX);
}

void IRAM_ATTR gdo0Changed() {
  uint32_t now = micros();
  uint8_t currentLevel = digitalRead(CC_GDO0);

  if (!capturing) {
    capturing = true;
    lastEdgeUs = now;
    return;
  }

  uint32_t duration = now - lastEdgeUs;
  lastEdgeUs = now;

  // The edge has already happened, so the duration belongs to
  // the level that was present immediately before this edge.
  uint8_t previousLevel = currentLevel ? LOW : HIGH;

  if (pulseCount < MAX_PULSES) {
    pulses[pulseCount].duration = duration;
    pulses[pulseCount].level = previousLevel;
    pulseCount++;
  }
}

bool inRange(uint32_t value, uint32_t low, uint32_t high) {
  return value >= low && value <= high;
}

int decodeBit(const Pulse &highPulse, const Pulse &lowPulse) {
  if (highPulse.level != HIGH || lowPulse.level != LOW) return -1;

  // Logical 0: short high, long low.
  if (inRange(highPulse.duration, SHORT_HIGH_MIN, SHORT_HIGH_MAX) &&
      inRange(lowPulse.duration, LONG_LOW_MIN, LONG_LOW_MAX)) {
    return 0;
  }

  // Logical 1: long high, short low.
  if (inRange(highPulse.duration, LONG_HIGH_MIN, LONG_HIGH_MAX) &&
      inRange(lowPulse.duration, SHORT_LOW_MIN, SHORT_LOW_MAX)) {
    return 1;
  }

  return -1;
}

bool isSync(const Pulse &highPulse, const Pulse &lowPulse) {
  return highPulse.level == HIGH &&
         lowPulse.level == LOW &&
         inRange(highPulse.duration, SHORT_HIGH_MIN, SHORT_HIGH_MAX) &&
         inRange(lowPulse.duration, SYNC_LOW_MIN, SYNC_LOW_MAX);
}

void printBinary24(uint32_t value) {
  for (int bit = 23; bit >= 0; bit--) {
    Serial.print((value >> bit) & 1U);
  }
}

void decodeCapture(Pulse *capture, uint16_t count) {
  bool found = false;
  uint32_t lastPrinted = 0xFFFFFFFFUL;

  Serial.println();
  Serial.println("========================================");
  Serial.print("Captured pulses: ");
  Serial.println(count);

  // A repeated transmission is normally:
  // [frame][sync][frame][sync]...
  // Search for a sync pair and decode the 24 pairs after it.
  for (uint16_t i = 0; i + 2 + (CODE_BITS * 2) <= count; i++) {
    if (!isSync(capture[i], capture[i + 1])) continue;

    uint16_t p = i + 2;
    uint32_t value = 0;
    bool valid = true;

    for (uint8_t bit = 0; bit < CODE_BITS; bit++) {
      int decoded = decodeBit(capture[p], capture[p + 1]);
      if (decoded < 0) {
        valid = false;
        break;
      }

      value = (value << 1) | (uint32_t)decoded;
      p += 2;
    }

    if (valid) {
      found = true;

      // Avoid flooding Serial Monitor with identical repeated frames.
      if (value != lastPrinted) {
        Serial.println();
        Serial.println("VALID 24-BIT FRAME");
        Serial.print("Binary : ");
        printBinary24(value);
        Serial.println();
        Serial.print("Decimal: ");
        Serial.println(value);
        Serial.print("Hex    : 0x");
        Serial.println(value, HEX);
        lastPrinted = value;
      }
    }
  }

  if (!found) {
    Serial.println("No valid 24-bit frame found.");
    Serial.println("If the signal is visible but does not decode, adjust timing thresholds or frequency.");
  }

  Serial.println("========================================");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(CC_CS, OUTPUT);
  digitalWrite(CC_CS, HIGH);
  pinMode(CC_MISO, INPUT);
  pinMode(CC_GDO0, INPUT);

  SPI.setRX(CC_MISO);
  SPI.setCS(CC_CS);
  SPI.setSCK(CC_SCK);
  SPI.setTX(CC_MOSI);
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  ccReset();

  uint8_t part = ccReadStatus(CC_PARTNUM);
  uint8_t version = ccReadStatus(CC_VERSION);

  Serial.println();
  Serial.println("========================================");
  Serial.println("CC1101 315 MHz AUTO DECODER");
  Serial.println("========================================");
  Serial.print("PARTNUM: 0x");
  if (part < 16) Serial.print('0');
  Serial.println(part, HEX);
  Serial.print("VERSION: 0x");
  if (version < 16) Serial.print('0');
  Serial.println(version, HEX);

  if (part == 0x00 && version == 0x14) {
    Serial.println("CC1101 detected successfully.");
  } else {
    Serial.println("WARNING: Unexpected CC1101 ID.");
  }

  configureRX();

  pulseCount = 0;
  capturing = false;
  lastEdgeUs = micros();

  attachInterrupt(digitalPinToInterrupt(CC_GDO0), gdo0Changed, CHANGE);

  Serial.println("Frequency: 315.000 MHz");
  Serial.println("Press and hold one button on the original remote, then release it.");
  Serial.println("Waiting for signal...");
}

void loop() {
  if (!capturing || pulseCount == 0) {
    delay(2);
    return;
  }

  // Process after the remote has gone quiet long enough to indicate release.
  uint32_t quietFor = micros() - lastEdgeUs;
  if (quietFor < CAPTURE_END_GAP_US) {
    delay(1);
    return;
  }

  noInterrupts();

  uint16_t count = pulseCount;
  if (count > MAX_PULSES) count = MAX_PULSES;

  static Pulse capture[MAX_PULSES];
  for (uint16_t i = 0; i < count; i++) {
    capture[i].duration = pulses[i].duration;
    capture[i].level = pulses[i].level;
  }

  pulseCount = 0;
  capturing = false;
  lastEdgeUs = micros();

  interrupts();

  decodeCapture(capture, count);

  // Re-enter receive mode in case noise or state changes disturbed RX.
  ccStrobe(CC_SIDLE);
  ccStrobe(CC_SFRX);
  ccStrobe(CC_SRX);

  Serial.println("Waiting for next button...");
}
