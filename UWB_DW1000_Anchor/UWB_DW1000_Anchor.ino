/*
 * UWB DW1000 Anchor - ESP32
 *
 * This sketch configures an ESP32 + DW1000 (UWB) module as an ANCHOR.
 * The anchor responds to ranging requests from tags using Two-Way Ranging (TWR),
 * calculates the distance, and sends it back to the tag.
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
 *
 * Set ANCHOR_ID uniquely for each anchor device (1, 2, 3 …).
 */

#include <SPI.h>
#include <DW1000.h>

// ── Pin definitions ──────────────────────────────────────────────────────────
const uint8_t PIN_RST = 27;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = 4;   // Makerfabs/generic Chinese board uses CS=4

// ── Anchor identity ───────────────────────────────────────────────────────────
const uint8_t ANCHOR_ID = 1;   // ← Change to 2 or 3 for other anchors

// ── Message types (must match tag sketch) ────────────────────────────────────
#define MSG_POLL          0x61
#define MSG_POLL_ACK      0x50
#define MSG_RANGE         0x51
#define MSG_RANGE_REPORT  0x52
#define MSG_RANGE_FAILED  0x99

// ── Ranging state machine ────────────────────────────────────────────────────
enum State { IDLE, POLL_RECEIVED, RANGE_RECEIVED };
volatile State state = IDLE;

volatile bool sentAck     = false;
volatile bool receivedAck = false;

// ── Buffers ───────────────────────────────────────────────────────────────────
#define LEN_DATA 16
byte txData[LEN_DATA];
byte rxData[LEN_DATA];

// ── Timestamps ────────────────────────────────────────────────────────────────
DW1000Time timePollReceived;
DW1000Time timePollAckSent;
DW1000Time timeRangeReceived;

// Speed of light (m/s)
const double SPEED_OF_LIGHT = 299702547.0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("=== ESP32 UWB DW1000 Anchor (ID: %d) ===\n", ANCHOR_ID);

  DW1000.begin(PIN_IRQ, PIN_RST);
  DW1000.select(PIN_SS);
  DW1000.newConfiguration();
  DW1000.setDefaults();
  DW1000.setDeviceAddress(ANCHOR_ID + 100);   // offset anchors from tag IDs
  DW1000.setNetworkId(0xDECA);
  DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
  DW1000.commitConfiguration();

  DW1000.attachSentHandler(handleSent);
  DW1000.attachReceivedHandler(handleReceived);

  char info[128];
  DW1000.getPrintableDeviceIdentifier(info);
  Serial.print(F("DW1000 Device ID: ")); Serial.println(info);

  listenForMessages();
  Serial.println(F("Listening for tag POLL messages…"));
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  DW1000.handleEvents();  // process deferred IRQ (ESP32 safe)
  if (sentAck) {
    sentAck = false;
    if (state == POLL_RECEIVED) {
      // Record when POLL_ACK left the antenna
      DW1000.getTransmitTimestamp(timePollAckSent);
      listenForMessages();
    }
  }

  if (receivedAck) {
    receivedAck = false;
    DW1000.getData(rxData, LEN_DATA);
    uint8_t msgType  = rxData[0];
    uint8_t fromTag  = rxData[1];
    uint8_t toAnchor = rxData[2];

    // Ignore messages not addressed to this anchor
    if (toAnchor != ANCHOR_ID) {
      listenForMessages();
      return;
    }

    if (msgType == MSG_POLL && state == IDLE) {
      DW1000.getReceiveTimestamp(timePollReceived);
      state = POLL_RECEIVED;
      Serial.printf("POLL from Tag %d\n", fromTag);
      sendPollAck(fromTag);

    } else if (msgType == MSG_RANGE && state == POLL_RECEIVED) {
      DW1000.getReceiveTimestamp(timeRangeReceived);
      state = RANGE_RECEIVED;

      // Extract timestamps sent by the tag
      DW1000Time tPollSent, tPollAckReceived;
      tPollSent.setTimestamp(rxData + 3);
      tPollAckReceived.setTimestamp(rxData + 8);

      // TWR distance calculation
      double distance = computeDistance(tPollSent, tPollAckReceived);
      Serial.printf("Tag %d  ->  %.2f m\n", fromTag, distance);

      sendRangeReport(fromTag, (float)distance);
      state = IDLE;
    } else {
      listenForMessages();
    }
  }
}

// ── Interrupt handlers ────────────────────────────────────────────────────────
void handleSent()     { sentAck     = true; }
void handleReceived() { receivedAck = true; }

// ── Send POLL_ACK ─────────────────────────────────────────────────────────────
void sendPollAck(uint8_t tagId) {
  txData[0] = MSG_POLL_ACK;
  txData[1] = ANCHOR_ID;
  txData[2] = tagId;

  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData(txData, LEN_DATA);
  DW1000.startTransmit();
}

// ── Send RANGE_REPORT with computed distance ──────────────────────────────────
void sendRangeReport(uint8_t tagId, float distance) {
  txData[0] = MSG_RANGE_REPORT;
  txData[1] = ANCHOR_ID;
  txData[2] = tagId;
  memcpy(txData + 3, &distance, sizeof(float));

  DW1000.newTransmit();
  DW1000.setDefaults();
  DW1000.setData(txData, LEN_DATA);
  DW1000.startTransmit();
  listenForMessages();
}

// ── Put DW1000 into receive mode ─────────────────────────────────────────────
void listenForMessages() {
  state = IDLE;
  DW1000.newReceive();
  DW1000.setDefaults();
  DW1000.receivePermanently(true);
  DW1000.startReceive();
}

// ── Symmetric Double-Sided TWR distance formula ───────────────────────────────
double computeDistance(DW1000Time& tPollSent,
                       DW1000Time& tPollAckReceived) {
  // Time-of-flight using anchor-side timestamps
  DW1000Time round1 = timePollReceived   - tPollSent;       // tag round-trip out
  DW1000Time reply1 = timePollAckSent    - timePollReceived; // anchor reply delay
  DW1000Time round2 = timeRangeReceived  - timePollAckSent;  // anchor round-trip
  DW1000Time reply2 = tPollAckReceived   - tPollSent;        // tag reply delay

  // ToF = (round1 * round2 - reply1 * reply2) / (round1 + round2 + reply1 + reply2)
  DW1000Time tof;
  double r1 = round1.getAsMeters();
  double r2 = round2.getAsMeters();
  double rp1 = reply1.getAsMeters();
  double rp2 = reply2.getAsMeters();

  double numerator   = r1 * r2 - rp1 * rp2;
  double denominator = r1 + r2 + rp1 + rp2;
  double tofMeters   = (numerator / denominator);

  // Convert DW1000 time units to distance
  return tofMeters * SPEED_OF_LIGHT / 1000.0;
}
