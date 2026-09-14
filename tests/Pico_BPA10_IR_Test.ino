#include <IRremote.hpp>

// ============================================================
// Raspberry Pi Pico + V1221 IR Transmitter -> Rockville BPA10
// IR test sketch using the scanned BPA10 NEC command set.
//
// Wiring:
//   V1221 DAT -> Pico GP7
//   V1221 GND -> Pico GND
//   V1221 VCC -> Pico VBUS (5V when Pico is USB-powered)
//
// Open Serial Monitor at 115200 baud and send one-letter commands.
// ============================================================

#define IR_SEND_PIN 7

const uint16_t BPA10_ADDRESS = 0x00;

const uint8_t IR_VOL_UP     = 0x15;
const uint8_t IR_VOL_DOWN   = 0x07;
const uint8_t IR_MUTE       = 0x47;
const uint8_t IR_MODE       = 0x46;
const uint8_t IR_PLAY_PAUSE = 0x43;
const uint8_t IR_NEXT       = 0x40;
const uint8_t IR_PREVIOUS   = 0x44;
const uint8_t IR_STOP       = 0x45;
const uint8_t IR_EQ         = 0x09;

void sendBPA10(uint8_t command, const char* name) {
  Serial.print("Sending ");
  Serial.print(name);
  Serial.print("  NEC addr=0x00 cmd=0x");
  if (command < 0x10) Serial.print('0');
  Serial.println(command, HEX);

  // Send twice for the same reliability approach used during scanning.
  IrSender.sendNEC(BPA10_ADDRESS, command, 0);
  delay(60);
  IrSender.sendNEC(BPA10_ADDRESS, command, 0);
}

void printHelp() {
  Serial.println();
  Serial.println("=== ROCKVILLE BPA10 IR TEST ===");
  Serial.println("p = Play/Pause");
  Serial.println("s = Stop");
  Serial.println("+ = Volume Up");
  Serial.println("- = Volume Down");
  Serial.println("m = Mute");
  Serial.println("o = Mode");
  Serial.println("n = Next track");
  Serial.println("b = Previous track");
  Serial.println("e = EQ");
  Serial.println("h = Help");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  IrSender.begin(IR_SEND_PIN);

  Serial.println("Pico BPA10 IR transmitter ready.");
  Serial.print("IR output pin: GP");
  Serial.println(IR_SEND_PIN);
  printHelp();
}

void loop() {
  if (!Serial.available()) return;

  char c = (char)Serial.read();

  switch (c) {
    case 'p': case 'P': sendBPA10(IR_PLAY_PAUSE, "PLAY/PAUSE"); break;
    case 's': case 'S': sendBPA10(IR_STOP, "STOP"); break;
    case '+':           sendBPA10(IR_VOL_UP, "VOL+"); break;
    case '-':           sendBPA10(IR_VOL_DOWN, "VOL-"); break;
    case 'm': case 'M': sendBPA10(IR_MUTE, "MUTE"); break;
    case 'o': case 'O': sendBPA10(IR_MODE, "MODE"); break;
    case 'n': case 'N': sendBPA10(IR_NEXT, "NEXT"); break;
    case 'b': case 'B': sendBPA10(IR_PREVIOUS, "PREVIOUS"); break;
    case 'e': case 'E': sendBPA10(IR_EQ, "EQ"); break;
    case 'h': case 'H': printHelp(); break;
    default: break;
  }
}
