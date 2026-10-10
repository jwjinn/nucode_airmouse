# nucode

NUCODE NU40-DK(nRF52840) 보드와 NUCODE IMU 모듈용 펌웨어 저장소입니다. macOS에서 `arduino-cli`와 NUCODE의 Arduino 코어(NUBoards nRF52)로 빌드하고, USB-C 하나로 업로드와 시리얼 확인을 모두 처리합니다.

## 하드웨어

2026-10-10에 실제 보드에서 확인한 값입니다.

| 항목 | 값 |
|---|---|
| 메인 보드 | NUCODE NU40-DK (nRF52840 모듈 NU-40, USB-C) |
| 빌드 대상 | `nucode:nrf52:nu40dk` (NUBoards nRF52 1.0.2, Adafruit nRF52 코어 1.7.0을 기반으로 NU40DK 정의를 더한 포크) |
| 부트로더 | Adafruit UF2 Bootloader 0.11.0, `pca10056` 빌드 (2026-10-10에 처음 확인했을 때 0.9.1이었고 갱신함) |
| SoftDevice | S140 6.1.1 |
| USB VID / PID | `0x239A` / `0x00DA`: 부트로더 모드, `0x80AC`: 응용 펌웨어 실행 중(장치 이름 `NU40DK nRF52840`) |
| 센서 모듈 | NUCODE IMU 모듈(ST LSM6DS3TR-C, 가속도 3축과 자이로 3축), 4핀 케이블로 I2C에 연결, 주소 `0x6a` |
| 외부 플래시 | 없음. Zephyr의 NU40 보드 문서에 외부 플래시가 없고, QSPI 핀에서 JEDEC ID를 읽으면 `000000`이 나옵니다 |

`nu40dk` 보드 정의의 주요 핀입니다(NUCODE 코어의 `variants/nu40dk_nrf52840/variant.h`).

| 기능 | 핀 | 비고 |
|---|---|---|
| LED | P0.13(`LED_BUILTIN`, 빨강), P0.14(파랑), P0.15, P0.16 | 높은 레벨에서 켜짐(`LED_STATE_ON 1`) |
| 버튼 | P0.11, P0.12, P0.24, P0.25 | 누르면 낮은 레벨 |
| I2C (Wire) | SDA P0.26, SCL P0.27 | IMU 모듈이 연결된 버스 |

코어 소스는 [Nucode01/Adafruit_nRF52_Arduino](https://github.com/Nucode01/Adafruit_nRF52_Arduino)에 있습니다.

## 설치

```sh
brew install arduino-cli
```

툴체인(`arm-none-eabi-gcc 9-2019q4`)과 업로드 도구(`adafruit-nrfutil`)가 x86_64 전용이므로 Apple Silicon 맥에서는 Rosetta가 필요합니다. 설치되어 있지 않다면 다음 명령으로 설치합니다.

```sh
softwareupdate --install-rosetta --agree-to-license
```

코어(`nucode:nrf52` 1.0.2)와 툴체인, 라이브러리는 처음 컴파일할 때 `sketches/bringup/sketch.yaml`의 프로파일에 따라 자동으로 설치됩니다. 새 스케치를 만들 때는 이 `sketch.yaml`을 복사해서 같은 버전을 쓰게 합니다.

NUCODE 코어 1.0.2는 zip으로 배포되어서 macOS용 업로드 도구의 실행 권한이 빠져 있습니다. 처음 컴파일하면 마지막에 `adafruit-nrfutil: permission denied` 오류가 나므로, 한 번만 다음 명령을 실행한 뒤 다시 컴파일합니다.

```sh
chmod +x ~/Library/Arduino15/internal/nucode_nrf52_1.0.2_*/tools/adafruit-nrfutil/macos/adafruit-nrfutil
```

## 라이브러리

코어에 포함되어 있어서 따로 설치하지 않아도 되는 라이브러리입니다.

| 라이브러리 | 용도 |
|---|---|
| Bluefruit52Lib, BLEAdafruitService, BLEHomekit | BLE |
| Adafruit TinyUSB | USB(CDC 시리얼, HID, MSC) |
| InternalFileSystem, Adafruit LittleFS | 내부 플래시 파일 시스템 |
| Adafruit nRFCrypto | 하드웨어 암호화(CC310) |
| Wire, SPI, SoftwareSerial, Servo, PDM, RotaryEncoder | 주변장치 |

CPU 내부 온도는 코어 함수 `readCPUTemperature()`로 읽습니다.

`sketch.yaml` 프로파일에 버전을 고정해서 추가한 라이브러리입니다.

| 라이브러리 | 버전 | 용도 |
|---|---|---|
| Adafruit LSM6DS | 4.7.4 | IMU 모듈. `Adafruit_LSM6DS3TRC` 클래스로 읽습니다(WHO_AM_I `0x6a`) |
| Adafruit BusIO, Adafruit Unified Sensor | 1.17.4, 1.1.15 | LSM6DS 의존성 |

NFC 태그 기능은 Arduino 라이브러리가 없습니다. 코어에는 저수준 드라이버(`nrfx_nfct`)만 있습니다.

## 빌드, 업로드, 시리얼 확인

```sh
arduino-cli compile sketches/bringup
arduino-cli board list                                     # 포트 확인 (Serial Port (USB) 항목)
arduino-cli upload -p /dev/cu.usbmodemXXXX sketches/bringup
arduino-cli monitor -p /dev/cu.usbmodemXXXX sketches/bringup
```

- 포트 이름은 연결할 때마다 바뀔 수 있습니다(`usbmodem1101`, `usbmodem101` 등). 매번 `arduino-cli board list`로 확인합니다.
- `monitor`에도 스케치 경로를 함께 지정해야 합니다. 코어를 프로파일로만 설치했기 때문에, 경로를 빼면 `No monitor available for the port protocol serial` 오류가 납니다.
- 응용 펌웨어가 실행 중일 때 `upload`를 실행하면 1200bps 터치로 보드를 부트로더 모드로 전환한 뒤 업로드합니다. 버튼을 누를 필요가 없습니다.

`sketches/bringup`은 2초마다 다음 형식의 줄을 출력합니다.

```
nucode bringup led=<0|1> ble=<0|1> i2c=<개수>:<주소 목록|none> temp=<°C> imu=<0|1> accel=<x,y,z|none>
```

- `led`: P0.13(빨강) LED가 켜져 있으면 1입니다.
- `ble`: `Bluefruit.begin()`이 성공하면 1입니다. 성공하면 `nucode`라는 이름으로 BLE 광고를 시작합니다.
- `i2c`: 부팅할 때 Wire 버스에서 `0x08`~`0x77`을 스캔한 결과입니다.
- `temp`: CPU 내부 온도입니다.
- `imu`, `accel`: IMU 모듈의 초기화 결과와 가속도(m/s²)입니다. 모듈이 정지해 있으면 세 축 값의 크기가 약 9.8입니다.

2026-10-10에 실제 보드에서 확인한 출력입니다.

```
nucode bringup led=1 ble=1 i2c=1:0x6a temp=24.8 imu=1 accel=6.51,-6.46,3.34
```

## 수동으로 부트로더 모드에 들어가기

응용 펌웨어가 멈춰서 자동 업로드가 되지 않으면 RESET 버튼을 빠르게 두 번 누르거나, P0.11 버튼을 누른 채로 USB를 연결합니다. `NRF52BOOT` 드라이브가 나타나면 부트로더 모드입니다. 이 절차는 NU40 보드 문서와 부트로더 설정(`BUTTON_DFU` P0.11)을 근거로 적은 것이며, 실제 보드에서 시험하지는 않았습니다. 이 상태에서는 `.uf2` 파일을 드라이브에 복사해서 플래시할 수도 있습니다.

NU40-DK의 공장 출하 부트로더는 드라이브 이름이 `BARAM-NU40`입니다. 이 보드에는 2026-10-10에 처음 확인했을 때 이미 `pca10056` 빌드(`NRF52BOOT`)가 설치되어 있었습니다. `nu40dk` 정의로 빌드한 펌웨어도 이 부트로더로 업로드되는 것을 확인했습니다.

## 하지 말아야 할 것

- `arduino-cli burn-bootloader`를 실행하지 않습니다. UF2 부트로더를 덮어쓰므로, 실패하면 USB로는 더 이상 업로드할 수 없고 SWD 디버그 프로브로만 복구할 수 있습니다. 부트로더는 릴리스의 `update-pca10056_bootloader-<버전>_nosd.uf2` 파일을 `NRF52BOOT`에 복사하는 방식으로만 갱신합니다.
- SoftDevice를 7.x로 올리지 않습니다. NUCODE 코어 1.0.2의 `nu40dk` 정의는 S140 6.1.1만 지원합니다.
