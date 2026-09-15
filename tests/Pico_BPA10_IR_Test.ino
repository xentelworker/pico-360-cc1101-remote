#include <IRremote.hpp>

// ============================================================
// Raspberry Pi Pico + V1221 IR Transmitter -> Rockville BPA10
// 15-second music-cycle test using confirmed BPA10 NEC codes.
//
// Wiring:
//   V1221 DAT -> Pico GP7
//   V1221 GND -> Pico GND
//   V1221 VCC -> Pico VBUS (5V when Pico is USB-powered)
//
// Open Serial Monitor at 115200 baud.
// Send A to test the complete automatic music cycle.
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

// Music timing.
const unsigned long MUSIC_TOTAL_MS = 15000UL;
const unsigned long FADE_START_MS  = 9000UL;
const unsigned long FADE_STEP_MS   = 1000UL;
const uint8_t FADE_STEPS = 6;

bool musicCycleActive = false;
unsigned long musicStartedMs = 0;
uint8_t fadeStepsSent = 0;

void sendBPA10(uint8_t command, const char* name) {
  Serial.print("IR ");
  Serial.print(name);
  Serial.print("  NEC addr=0x00 cmd=0x");
  if (command < 0x10) Serial.print('0');
  Serial.println(command, HEX);

  // Two transmissions were confirmed reliable with this BPA10/V1221 setup.
  IrSender.sendNEC(BPA10_ADDRESS, command, 0);
  delay(60);
  IrSender.sendNEC(BPA10_ADDRESS, command, 0);
}

void startMusicCycle() {
  if (musicCycleActive) {
    Serial.println("Music cycle already running.");
    return;
  }

  fadeStepsSent = 0;
  musicCycleActive = true;

  Serial.println();
  Serial.println("=== MUSIC CYCLE START ===");
  sendBPA10(IR_PLAY_PAUSE, "PLAY/PAUSE");

  // Start the 15-second timer after the PLAY command has been sent.
  musicStartedMs = millis();
  Serial.println("Music playing at normal volume.");
}

void finishMusicCycle() {
  Serial.println("15s: STOP");
  sendBPA10(IR_STOP, "STOP");

  Serial.print("Restoring volume: VOL+ x");
  Serial.println(fadeStepsSent);
  for (uint8_t i = 0; i < fadeStepsSent; i++) {
    sendBPA10(IR_VOL_UP, "VOL+");
    delay(80);
  }

  Serial.println("Advancing to NEXT song while stopped.");
  sendBPA10(IR_NEXT, "NEXT");

  musicCycleActive = false;
  fadeStepsSent = 0;
  Serial.println("=== MUSIC CYCLE COMPLETE ===");
  Serial.println("Next guest is ready for the next song.");
  Serial.println();
}

void cancelMusicCycle() {
  if (!musicCycleActive) {
    sendBPA10(IR_STOP, "STOP");
    return;
  }

  Serial.println("Canceling music cycle.");
  sendBPA10(IR_STOP, "STOP");

  // Restore only the volume steps that were actually faded.
  for (uint8_t i = 0; i < fadeStepsSent; i++) {
    sendBPA10(IR_VOL_UP, "VOL+");
    delay(80);
  }

  // Deliberately do not advance track on a canceled test.
  musicCycleActive = false;
  fadeStepsSent = 0;
  Serial.println("Music stopped and volume restored.");
}

void updateMusicCycle() {
  if (!musicCycleActive) return;

  unsigned long elapsed = millis() - musicStartedMs;

  // Fade at 9, 10, 11, 12, 13 and 14 seconds.
  while (fadeStepsSent < FADE_STEPS &&
         elapsed >= FADE_START_MS + ((unsigned long)fadeStepsSent * FADE_STEP_MS)) {
    fadeStepsSent++;
    Serial.print("Fade step ");
    Serial.print(fadeStepsSent);
    Serial.print("/");
    Serial.println(FADE_STEPS);
    sendBPA10(IR_VOL_DOWN, "VOL-");
    elapsed = millis() - musicStartedMs;
  }

  if (elapsed >= MUSIC_TOTAL_MS) {
    finishMusicCycle();
  }
}

void printHelp() {
  Serial.println();
  Serial.println("=== ROCKVILLE BPA10 IR TEST ===");
  Serial.println("a = AUTOMATIC 15-second music/fade/next test");
  Serial.println("x = Cancel automatic cycle / Stop");
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
  updateMusicCycle();

  if (!Serial.available()) return;

  char c = (char)Serial.read();

  switch (c) {
    case 'a': case 'A': startMusicCycle(); break;
    case 'x': case 'X': cancelMusicCycle(); break;
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
