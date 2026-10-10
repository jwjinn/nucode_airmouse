// BLE air mouse for NUCODE NU40-DK + LSM6DS3TR-C IMU module (I2C 0x6a).
// Wrist yaw -> cursor X, wrist pitch -> cursor Y. P0.11 = left click, P0.12 = right click,
// P0.24 = jump to screen center.
// Gyro bias is re-measured automatically whenever the module rests still for STILL_TICKS.
// Hold both buttons for RESET_HOLD_MS to forget all hosts (clear bonds + new BLE address).
#include <bluefruit.h>
#include <InternalFileSystem.h>
#include <Adafruit_LSM6DS3TRC.h>
using namespace Adafruit_LittleFS_Namespace;

// Tuning knobs: which gyro axis (0=x, 1=y, 2=z) drives each cursor axis, its sign, and gain.
// Watch the serial "gyro=" values while turning the module to pick axes and signs.
const int AXIS_H = 2, AXIS_V = 1;
const float SIGN_H = -1.0f, SIGN_V = -1.0f;
const float GAIN = 8.0f;        // cursor counts per (rad/s) per 10 ms tick
const float DEADZONE = 0.03f;   // rad/s (~1.7 dps); smaller rates are treated as hand tremor

// Rest detection for auto bias: gyro and accel must stay within these bands around the
// window's first sample. A slow rotation fails the accel band because gravity shifts.
const int STILL_TICKS = 50;      // 0.5 s at 10 ms
const float STILL_GYRO = 0.02f;  // rad/s (~1.1 dps)
const float STILL_ACCEL = 0.15f; // m/s^2

const uint32_t RESET_HOLD_MS = 3000;
static const char* ADDR_FILE = "/airmouse_addr";

BLEDis bledis;
// Mouse-only HID. BLEHidAdafruit also exposes a keyboard, which makes macOS open Keyboard Setup Assistant.
// Report 1: relative mouse (motion, clicks). Report 2: absolute pointer, only used to jump to the center
// because a relative mouse cannot know the screen size or cursor position.
static uint8_t const mouseReportMap[] = {
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(1)),
  TUD_HID_REPORT_DESC_ABSMOUSE(HID_REPORT_ID(2)),
};
BLEHidGeneric blehid(2);
Adafruit_LSM6DS3TRC imu;

static float bias[3];
static float accH = 0, accV = 0;  // fractional cursor counts carried between ticks
static uint8_t buttons = 0;

static float anchorG[3], anchorA[3], sumG[3];
static int still = 0;

static void readImu(float g[3], float a[3]) {
  sensors_event_t ae, ge, te;
  imu.getEvent(&ae, &ge, &te);
  g[0] = ge.gyro.x; g[1] = ge.gyro.y; g[2] = ge.gyro.z;
  a[0] = ae.acceleration.x; a[1] = ae.acceleration.y; a[2] = ae.acceleration.z;
}

static void trackBias(const float g[3], const float a[3]) {
  bool inBand = still > 0;
  for (int k = 0; k < 3 && inBand; k++)
    inBand = fabsf(g[k] - anchorG[k]) < STILL_GYRO && fabsf(a[k] - anchorA[k]) < STILL_ACCEL;
  if (!inBand) {  // open a new window at this sample
    still = 0;
    for (int k = 0; k < 3; k++) { anchorG[k] = g[k]; anchorA[k] = a[k]; sumG[k] = 0; }
  }
  for (int k = 0; k < 3; k++) sumG[k] += g[k];
  if (++still >= STILL_TICKS) {
    for (int k = 0; k < 3; k++) bias[k] = sumG[k] / still;
    still = 0;
  }
}

// Old hosts keep the previous address bonded and auto-reconnect to it. A new random static
// address (persisted so it survives reboots) makes them see an unknown device instead.
static void loadAddr() {
  File f(InternalFS);
  if (!f.open(ADDR_FILE, FILE_O_READ)) return;
  ble_gap_addr_t addr = {};
  addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
  if (f.read(addr.addr, 6) == 6) Bluefruit.setAddr(&addr);
  f.close();
}

static void forgetHosts() {
  uint8_t mac[6];
  while (sd_rand_application_vector_get(mac, 6) != NRF_SUCCESS) delay(1);
  mac[5] |= 0xC0;  // random static address: two most significant bits set
  Bluefruit.Periph.clearBonds();
  InternalFS.remove(ADDR_FILE);
  File f(InternalFS);
  if (f.open(ADDR_FILE, FILE_O_WRITE)) {
    f.write(mac, 6);
    f.close();
  }
}

static void sendMouse(uint8_t buttons, int8_t dx, int8_t dy) {
  hid_mouse_report_t r = { buttons, dx, dy, 0, 0 };
  if (blehid.isBootMode()) blehid.bootMouseReport(&r, sizeof(r));
  else blehid.inputReport(1, &r, sizeof(r));
}

static void centerCursor() {
  hid_abs_mouse_report_t r = { 0, 0x4000, 0x4000, 0, 0 };  // absolute range is 0..0x7FFF on each axis
  if (!blehid.isBootMode()) blehid.inputReport(2, &r, sizeof(r));  // boot protocol has no absolute report
  accH = accV = 0;
}

static int8_t step(float rate, float& acc) {
  if (fabsf(rate) < DEADZONE) rate = 0;
  acc += rate * GAIN;
  int n = constrain((int)acc, -127, 127);
  acc -= n;
  return n;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  pinMode(PIN_BUTTON3, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  if (!imu.begin_I2C(0x6a)) {
    while (true) { Serial.println("airmouse imu=0"); delay(1000); }
  }
  imu.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
  imu.setGyroDataRate(LSM6DS_RATE_208_HZ);

  Bluefruit.begin();
  loadAddr();
  Bluefruit.setName("nucode-airmouse");
  Bluefruit.Periph.setConnInterval(9, 16);  // 11.25-20 ms
  Bluefruit.setTxPower(4);
  bledis.setManufacturer("NUCODE");
  bledis.setModel("NU40DK air mouse");
  bledis.begin();
  uint16_t inputLen[] = { sizeof(hid_mouse_report_t), sizeof(hid_abs_mouse_report_t) };
  blehid.setReportLen(inputLen);
  blehid.enableMouse(true);
  blehid.setReportMap(mouseReportMap, sizeof(mouseReportMap));
  blehid.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void loop() {
  float g[3], a[3];
  readImu(g, a);
  trackBias(g, a);
  for (int k = 0; k < 3; k++) g[k] -= bias[k];

  int8_t dx = step(SIGN_H * g[AXIS_H], accH);
  int8_t dy = step(SIGN_V * g[AXIS_V], accV);
  uint8_t now = (digitalRead(PIN_BUTTON1) == LOW ? MOUSE_BUTTON_LEFT : 0) |
                (digitalRead(PIN_BUTTON2) == LOW ? MOUSE_BUTTON_RIGHT : 0);
  bool conn = Bluefruit.connected();

  const uint8_t BOTH = MOUSE_BUTTON_LEFT | MOUSE_BUTTON_RIGHT;
  static uint32_t bothSince = 0;
  if (now != BOTH) {
    bothSince = 0;
  } else if (!bothSince) {
    bothSince = millis();
  } else if (millis() - bothSince >= RESET_HOLD_MS) {
    Serial.println("airmouse forget-hosts: bonds cleared, new address saved, reboot after release");
    forgetHosts();
    // Reboot only after release: P0.11/P0.12 held at reset make the bootloader enter DFU mode.
    while (digitalRead(PIN_BUTTON1) == LOW || digitalRead(PIN_BUTTON2) == LOW) {
      digitalToggle(LED_BUILTIN);
      delay(100);
    }
    NVIC_SystemReset();
  }
  uint8_t clicks = (now == BOTH) ? 0 : now;  // the reset gesture must not click

  if (conn && (dx || dy || clicks != buttons)) sendMouse(clicks, dx, dy);
  buttons = clicks;

  bool center = digitalRead(PIN_BUTTON3) == LOW;
  static bool centerPrev = false;
  if (conn && center && !centerPrev) centerCursor();
  centerPrev = center;

  static uint32_t lastLog = 0;
  if (millis() - lastLog >= 500) {
    lastLog = millis();
    uint8_t mac[6];
    Bluefruit.getAddr(mac);
    Serial.printf("airmouse imu=1 conn=%d addr=%02X:%02X:%02X:%02X:%02X:%02X gyro=%.1f,%.1f,%.1f bias=%.1f,%.1f,%.1f dps dx=%d dy=%d buttons=%d center=%d\n",
                  conn, mac[5], mac[4], mac[3], mac[2], mac[1], mac[0],
                  g[0] * 57.2958f, g[1] * 57.2958f, g[2] * 57.2958f,
                  bias[0] * 57.2958f, bias[1] * 57.2958f, bias[2] * 57.2958f, dx, dy, now, center);
  }
  delay(10);
}
