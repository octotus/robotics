/*
 * UWB DW1000 SPI Pin Scanner v2
 * Tries CS=4 (confirmed active) with all 4 SPI modes and multiple speeds.
 * Expected Device ID: 0xDECA0130
 */
#include <SPI.h>

#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23
#define PIN_CS   4
#define PIN_RST  27

uint32_t readDeviceID(uint8_t mode, uint32_t freq) {
    SPI.beginTransaction(SPISettings(freq, MSBFIRST, mode));
    digitalWrite(PIN_CS, LOW);
    delayMicroseconds(5);
    SPI.transfer(0x00);  // read register 0x00 (Device ID)
    uint8_t b0 = SPI.transfer(0xFF);
    uint8_t b1 = SPI.transfer(0xFF);
    uint8_t b2 = SPI.transfer(0xFF);
    uint8_t b3 = SPI.transfer(0xFF);
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();
    return ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;
}

void hwReset() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(10);
    pinMode(PIN_RST, INPUT);
    delay(10);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== DW1000 SPI Mode + Speed Scanner ===");
    Serial.printf("CS=%d  SCK=%d  MISO=%d  MOSI=%d  RST=%d\n\n", PIN_CS, PIN_SCK, PIN_MISO, PIN_MOSI, PIN_RST);

    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);

    const char* modeNames[] = {"MODE0","MODE1","MODE2","MODE3"};
    uint8_t modes[] = {SPI_MODE0, SPI_MODE1, SPI_MODE2, SPI_MODE3};
    uint32_t freqs[] = {200000, 500000, 1000000, 2000000, 3000000};

    for (uint8_t m = 0; m < 4; m++) {
        for (uint8_t f = 0; f < 5; f++) {
            hwReset();
            uint32_t id = readDeviceID(modes[m], freqs[f]);
            Serial.printf("%-6s  %4dkHz  -> 0x%08X", modeNames[m], freqs[f]/1000, id);
            if (id == 0xDECA0130) Serial.print("  *** DW1000 FOUND! ***");
            Serial.println();
        }
    }

    // Also try LSBFIRST
    Serial.println("\n--- LSBFIRST ---");
    for (uint8_t m = 0; m < 4; m++) {
        hwReset();
        SPI.beginTransaction(SPISettings(500000, LSBFIRST, modes[m]));
        digitalWrite(PIN_CS, LOW);
        delayMicroseconds(5);
        SPI.transfer(0x00);
        uint8_t b0=SPI.transfer(0xFF), b1=SPI.transfer(0xFF),
                b2=SPI.transfer(0xFF), b3=SPI.transfer(0xFF);
        digitalWrite(PIN_CS, HIGH);
        SPI.endTransaction();
        uint32_t id = ((uint32_t)b3<<24)|((uint32_t)b2<<16)|((uint32_t)b1<<8)|b0;
        Serial.printf("%-6s  LSBFIRST  -> 0x%08X\n", modeNames[m], id);
    }

    Serial.println("\nDone.");
}

void loop() {}
