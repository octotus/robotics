/*
 * UWB DW1000 Tag + BLE bridge - ESP32
 *
 * Extends the basic UWB tag to broadcast measured distances over BLE so the
 * Android app (UWB_Android_App) can display them in real time.
 *
 * BLE Service  : 4FAFC201-1FB5-459E-8FCC-C5C9C331914B
 * Characteristic: BEB5483E-36E1-4688-B7F5-EA07361B26A8  (NOTIFY)
 *
 * Notification payload (5 bytes):
 *   [0]   anchor ID  (uint8)
 *   [1-4] distance   (float, little-endian, metres)
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
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ── Pin definitions ──────────────────────────────────────────────────────────
const uint8_t PIN_RST = 27;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = 4;   // Makerfabs/generic Chinese board uses CS=4

// ── Tag identity ─────────────────────────────────────────────────────────────
const uint8_t TAG_ID = 1;

// ── BLE UUIDs (must match Android app) ───────────────────────────────────────
#define SERVICE_UUID      "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_DISTANCE_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ── Message types ─────────────────────────────────────────────────────────────
#define MSG_POLL          0x61
#define MSG_POLL_ACK      0x50
#define MSG_RANGE         0x51
#define MSG_RANGE_REPORT  0x52
#define MSG_RANGE_FAILED  0x99

// ── State machine ─────────────────────────────────────────────────────────────
enum State { IDLE, POLL_SENT, RANGE_SENT };
volatile State state = IDLE;
volatile bool sentAck     = false;
volatile bool receivedAck = false;

// ── Buffers ───────────────────────────────────────────────────────────────────
#define LEN_DATA 16
byte txData[LEN_DATA];
byte rxData[LEN_DATA];

// ── Timestamps ────────────────────────────────────────────────────────────────
DW1000Time timePollSent;
DW1000Time timePollAckReceived;
DW1000Time timeRangeSent;

// ── Anchors ───────────────────────────────────────────────────────────────────
const uint8_t NUM_ANCHORS    = 3;
const uint8_t anchorIds[NUM_ANCHORS] = {1, 2, 3};
uint8_t currentAnchor = 0;

// ── Timing ────────────────────────────────────────────────────────────────────
unsigned long lastRangeStart = 0;
const unsigned long RANGE_INTERVAL_MS = 200;
const unsigned long TIMEOUT_MS        = 100;

// ── BLE ───────────────────────────────────────────────────────────────────────
BLEServer*         bleServer     = nullptr;
BLECharacteristic* distanceChar  = nullptr;
bool bleClientConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*)    override { bleClientConnected = true;  Serial.println("BLE client connected");    }
  void onDisconnect(BLEServer* s) override {
    bleClientConnected = false;
    Serial.println("BLE client disconnected – advertising again");
    s->startAdvertising();
  }
};

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== ESP32 UWB Tag + BLE ===");

  // ── DW1000 init ────────────────────────────────────────────────────────────
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
  Serial.print("DW1000 Device ID: "); Serial.println(info);

  // ── BLE init ───────────────────────────────────────────────────────────────
  BLEDevice::init("UWB_Tag");
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = bleServer->createService(SERVICE_UUID);
  distanceChar = svc->createCharacteristic(
    CHAR_DISTANCE_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  distanceChar->addDescriptor(new BLE2902());
  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising as 'UWB_Tag'");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  DW1000.handleEvents();  // process deferred IRQ (ESP32 safe)
  unsigned long now = millis();

  if (state == IDLE && (now - lastRangeStart) >= RANGE_INTERVAL_MS) {
    lastRangeStart = now;
    Serial.printf("Sending POLL to Anchor %d\n", anchorIds[currentAnchor]);
    sendPoll(anchorIds[currentAnchor]);
  }

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
      Serial.printf("Anchor %d -> %.2f m\n", anchorIds[currentAnchor], dist);
      notifyBLE(anchorIds[currentAnchor], dist);
      currentAnchor = (currentAnchor + 1) % NUM_ANCHORS;
      state = IDLE;

    } else if (msgType == MSG_RANGE_FAILED) {
      currentAnchor = (currentAnchor + 1) % NUM_ANCHORS;
      state = IDLE;
    }
  }

  // Timeout
  if (state != IDLE && (now - lastRangeStart) > TIMEOUT_MS) {
    Serial.printf("TIMEOUT waiting for Anchor %d (state=%d)\n", anchorIds[currentAnchor], state);
    currentAnchor = (currentAnchor + 1) % NUM_ANCHORS;
    state = IDLE;
  }
}

// ── Interrupt handlers ────────────────────────────────────────────────────────
void handleSent()     { sentAck     = true; }
void handleReceived() { receivedAck = true; }

// ── UWB helpers ───────────────────────────────────────────────────────────────
void sendPoll(uint8_t anchorId) {
  txData[0] = MSG_POLL; txData[1] = TAG_ID; txData[2] = anchorId;
  DW1000.newTransmit(); DW1000.setDefaults();
  DW1000.setData(txData, LEN_DATA); DW1000.startTransmit();
  state = POLL_SENT;
}

void sendRange(uint8_t anchorId) {
  txData[0] = MSG_RANGE; txData[1] = TAG_ID; txData[2] = anchorId;
  timePollSent.getTimestamp(txData + 3);
  timePollAckReceived.getTimestamp(txData + 8);
  DW1000.newTransmit(); DW1000.setDefaults();
  DW1000.setData(txData, LEN_DATA); DW1000.startTransmit();
  state = RANGE_SENT;
}

void listenForResponse() {
  DW1000.newReceive(); DW1000.setDefaults();
  DW1000.receivePermanently(false); DW1000.startReceive();
}

// ── BLE notify ────────────────────────────────────────────────────────────────
void notifyBLE(uint8_t anchorId, float distance) {
  if (!bleClientConnected) return;
  uint8_t payload[5];
  payload[0] = anchorId;
  memcpy(payload + 1, &distance, sizeof(float));   // little-endian on ESP32
  distanceChar->setValue(payload, sizeof(payload));
  distanceChar->notify();
}
