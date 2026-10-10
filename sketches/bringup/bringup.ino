// Bring-up check for NUCODE NU40-DK (nRF52840): LED, BLE (SoftDevice S140), I2C scan,
// CPU temperature, IMU at 0x6a (NUCODE LSM6DS3TR-C module).
// Prints every 2s:
//   nucode bringup led=<0|1> ble=<0|1> i2c=<count>:<addrs|none> temp=<C> imu=<0|1> accel=<x,y,z|none>
#include <bluefruit.h>
#include <Wire.h>
#include <Adafruit_LSM6DS3TRC.h>

static bool bleOk = false;
static bool ledLit = false;
static uint8_t i2cAddrs[0x78 - 0x08];
static uint8_t i2cCount = 0;

static Adafruit_LSM6DS3TRC imu;
static bool imuOk = false;

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
  imuOk = imu.begin_I2C(0x6a);
}

void loop() {
  ledLit = !ledLit;
  digitalWrite(LED_BUILTIN, ledLit ? LED_STATE_ON : !LED_STATE_ON);

  Serial.printf("nucode bringup led=%d ble=%d i2c=%d:", ledLit, bleOk, i2cCount);
  if (i2cCount == 0) Serial.print("none");
  for (uint8_t i = 0; i < i2cCount; i++) Serial.printf("%s0x%02x", i ? "," : "", i2cAddrs[i]);

  Serial.printf(" temp=%.1f imu=%d accel=", readCPUTemperature(), imuOk);
  if (imuOk) {
    sensors_event_t a, g, t;
    imu.getEvent(&a, &g, &t);
    Serial.printf("%.2f,%.2f,%.2f", a.acceleration.x, a.acceleration.y, a.acceleration.z);
  } else {
    Serial.print("none");
  }
  Serial.println();

  delay(2000);
}
