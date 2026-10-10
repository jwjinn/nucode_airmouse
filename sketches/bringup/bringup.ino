// Bring-up check for nRF52840 DK (PCA10056): LED, BLE (SoftDevice S140), I2C scan.
// Prints every 2s: nucode bringup led=<0|1> ble=<0|1> i2c=<count>:<addrs|none>
#include <bluefruit.h>
#include <Wire.h>

static bool bleOk = false;
static bool ledLit = false;
static uint8_t i2cAddrs[0x78 - 0x08];
static uint8_t i2cCount = 0;

void setup() {
  Serial.begin(115200);  // no while (!Serial): must run without a monitor attached
  pinMode(LED_BUILTIN, OUTPUT);

  bleOk = Bluefruit.begin();
  if (bleOk) {
    Bluefruit.setName("nucode");
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.start(0);
  }

  Wire.begin();  // SDA P0.26, SCL P0.27
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) i2cAddrs[i2cCount++] = addr;
  }
}

void loop() {
  ledLit = !ledLit;
  digitalWrite(LED_BUILTIN, ledLit ? LED_STATE_ON : !LED_STATE_ON);

  Serial.printf("nucode bringup led=%d ble=%d i2c=%d:", ledLit, bleOk, i2cCount);
  if (i2cCount == 0) Serial.print("none");
  for (uint8_t i = 0; i < i2cCount; i++) Serial.printf("%s0x%02x", i ? "," : "", i2cAddrs[i]);
  Serial.println();

  delay(2000);
}
