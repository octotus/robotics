/*
 * UWB DW1000 Tag - ESP32
 *
 * This sketch configures an ESP32 + DW1000 (UWB) module as a TAG.
 * The tag performs Two-Way Ranging (TWR) with anchors and reports
 * distances over Serial.
 *
 * Library required: arduino-dw1000 by thotro
 *   https://github.com/thotro/arduino-dw1000
 *
 * Wiring (SPI):
 *   DW1000 SCK  -> GPIO 18
 *   DW1000 MISO -> GPIO 19
 *   DW1000 MOSI -> GPIO 23
 *   DW1000 CS   -> GPIO 5
 *   DW1000 RST  -> GPIO 27
 *   DW1000 IRQ  -> GPIO 34
 */

#include <SPI.h>
#include <DW1000.h>

// ── Pin definitions ──────────────────────────────────────────────────────────
const uint8_t PIN_RST = 27;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = 5;

// ── Tag identity ─────────────────────────────────────────────────────────────
const uint8_t TAG_ID = 1;          // Change per tag device

// ── Ranging state machine ────────────────────────────────────────────────────
enum State {
  IDLE,
  POLL_SENT,
  RANGE_SENT
};

volatile State state = IDLE;
volatile bool sentAck     = false;
volatile bool receivedAck = false;

// ── Message types ─────────────────────────────────────────────────────────────
#define MSG_POLL          0x61
#define MSG_POLL_ACK      0x50
#define MSG_RANGE         0x51
#define MSG_RANGE_REPORT  0x52
#define MSG_RANGE_FAILED  0x99

// ── Buffers ───────────────────────────────────────────────────────────────────
#define LEN_DATA 16
byte txData[LEN_DATA];
byte rxData[LEN_DATA];

// ── Timestamps ────────────────────────────────────────────────────────────────
DW1000Time timePollSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;

// ── Anchor to range against (cycle through if multiple) ──────────────────────
const uint8_t NUM_ANCHORS    = 3;
const uint8_t anchorIds[NUM_ANCHORS] = {1, 2, 3};
uint8_t currentAnchor = 0;

float lastDistances[NUM_ANCHORS];

// ── Timing ────────────────────────────────────────────────────────────────────
unsigned long lastRangeStart = 0;
const unsigned long RANGE_INTERVAL_MS = 200;  // range every 200 ms
const unsigned long TIMEOUT_MS        = 100;   // reply timeout

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("=== ESP32 UWB DW1000 Tag ==="));

  DW1000.begin(PIN_IRQ, PIN_RST);
  DW1000.select(PIN_SS);
  DW1000.newConfiguration();
  DW1000.setDefaults();
  DW1000.setDeviceAddress(TAG_ID);
  DW1000.setNetworkId(0xDECA);
  DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
  DW1000.commitConfiguration();

  DW1000.attachSentHandler(handleSent);
  DW1000.attachReceivedHandler(handleReceived);

  char info[128];
  DW1000.getPrintableDeviceIdentifier(info);
  Serial.print(F("DW1000 Device ID: ")); Serial.println(info);
  DW1000.getPrintableExtendedUniqueIdentifier(info);
  Serial.print(F("Unique ID: ")); Serial.println(info);

  Serial.printf("Tag ID: %d  |  Ranging against %d anchors\n", TAG_ID, NUM_ANCHORS);
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Start a new ranging cycle
  if (state == IDLE && (now - lastRangeStart) >= RANGE_INTERVAL_MS) {
    lastRangeStart = now;
    sendPoll(anchorIds[currentAnchor]);
  }

  // Handle interrupt flags
  if (sentAck) {
    sentAck = false;
    if (state == POLL_SENT) {
      DW1000.getTransmitTimestamp(timePollSent);
      listenForResponse();
    } else if (state == RANGE_SENT) {
      DW1000.getTransmitTimestamp(timeRangeSent);
      listenForResponse();
    }
  }

  if (receivedAck) {
    receivedAck = false;
    DW1000.getData(rxData, LEN_DATA);

    uint8_t msgType = rxData[0];

    if (msgType == MSG_POLL_ACK && state == POLL_SENT) {
      DW1000.getReceiveTimestamp(timePollAckReceived);
      sendRange(anchorIds[currentAnchor]);

    } else if (msgType == MSG_RANGE_REPORT && state == RANGE_SENT) {
      float dist = 0;
      memcpy(&dist, rxData + 1, sizeof(float));
      lastDistances[currentAnchor] = dist;
      Serial.printf("Anchor %d  ->  %.2f m\n", anchorIds[currentAnchor], dist);

      // Advance to next anchor
      currentAnchor = (currentAnchor + 1) % NUM_ANCHORS;
      state = IDLE;

    } else if (msgType == MSG_RANGE_FAILED) {
      Serial.printf("Anchor %d  ->  range FAILED\n", anchorIds[currentAnchor]);
      currentAnchor = (currentAnchor + 1) % NUM_ANCHORS;
      state = IDLE;
    }
  }

  // Timeout guard
  if (state != IDLE && (now - lastRangeStart) > TIMEOUT_MS) {
    Serial.printf("Anchor %d  ->  TIMEOUT\n", anchorIds[currentAnchor]);
    currentAnchor = (currentAnchor + 1) % NUM_ANCHORS;
    state = IDLE;
  }
}

// ── Interrupt handlers ────────────────────────────────────────────────────────
void handleSent()     { sentAck     = true; }
void handleReceived() { receivedAck = true; }

// ── Send POLL to a specific anchor ───────────────────────────────────────────
void sendPoll(uint8_t anchorId) {
  txData[0] = MSG_POLL;
  txData[1] = TAG_ID;
  txData[2] = anchorId;

  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData(txData, LEN_DATA);
  DW1000.startTransmit();
  state = POLL_SENT;
}

// ── Send RANGE message carrying timestamps ───────────────────────────────────
void sendRange(uint8_t anchorId) {
  txData[0] = MSG_RANGE;
  txData[1] = TAG_ID;
  txData[2] = anchorId;

  // Embed Poll-Sent and Poll-Ack-Received timestamps (5 bytes each)
  timePollSent.getTimestamp(txData + 3);
  timePollAckReceived.getTimestamp(txData + 8);

  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData(txData, LEN_DATA);
  DW1000.startTransmit();
  state = RANGE_SENT;
}

// ── Switch DW1000 to receive mode ────────────────────────────────────────────
void listenForResponse() {
  DW1000.newReceive();
  DW1000.setDefaults();
  DW1000.receivePermanently(false);
  DW1000.startReceive();
}
