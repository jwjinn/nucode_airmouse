# nucode

nucode 기기(Nordic nRF52840 DK, PCA10056)용 펌웨어 저장소입니다. macOS에서 `arduino-cli`와 Adafruit nRF52 Arduino 코어로 빌드하고, USB 하나로 업로드와 시리얼 확인을 모두 처리합니다.

## 보드 구성

2026-10-10에 실제 보드에서 확인한 값입니다.

| 항목 | 값 |
|---|---|
| 보드 | Nordic nRF52840 DK (PCA10056) |
| 부트로더 | Adafruit UF2 Bootloader 0.11.0 (0.9.1에서 갱신함) |
| SoftDevice | S140 6.1.1 |
| USB VID | `0x239A` |
| USB PID | `0x00DA`: 부트로더 모드, `0x80DA`: 응용 펌웨어 실행 중 |

USB 케이블은 DK에서 `nRF USB`라고 표시된 포트에 연결합니다. J-Link 디버거 포트는 사용하지 않습니다.

## 설치

```sh
brew install arduino-cli
```

툴체인(`arm-none-eabi-gcc 9-2019q4`)과 업로드 도구(`adafruit-nrfutil`)가 x86_64 전용이므로 Apple Silicon 맥에서는 Rosetta가 필요합니다. 설치되어 있지 않다면 다음 명령으로 설치합니다.

```sh
softwareupdate --install-rosetta --agree-to-license
```

코어(`adafruit:nrf52` 1.7.0)와 툴체인은 처음 컴파일할 때 `sketches/bringup/sketch.yaml`의 프로파일에 따라 자동으로 설치됩니다.

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
nucode bringup led=<0|1> ble=<0|1> i2c=<개수>:<주소 목록|none>
```

- `led`: LED1(P0.13)이 켜져 있으면 1입니다.
- `ble`: `Bluefruit.begin()`이 성공하면 1입니다. 성공하면 `nucode`라는 이름으로 BLE 광고를 시작합니다.
- `i2c`: 부팅할 때 기본 Wire 버스(SDA P0.26, SCL P0.27)에서 `0x08`~`0x77`을 스캔한 결과입니다.

## 수동으로 부트로더 모드에 들어가기

응용 펌웨어가 멈춰서 자동 업로드가 되지 않으면 BUTTON1(P0.11)을 누른 채로 보드를 리셋합니다. `NRF52BOOT` 드라이브가 나타나면 부트로더 모드입니다. 이 절차는 부트로더 소스의 `BUTTON_DFU` 설정(P0.11)을 근거로 적은 것이며, 실제 보드에서 시험하지는 않았습니다. 이 상태에서는 `.uf2` 파일을 드라이브에 복사해서 플래시할 수도 있습니다.

## 하지 말아야 할 것

- `arduino-cli burn-bootloader`를 실행하지 않습니다. UF2 부트로더를 덮어쓰므로, 실패하면 USB로는 더 이상 업로드할 수 없고 J-Link로만 복구할 수 있습니다. 부트로더는 릴리스의 `update-pca10056_bootloader-<버전>_nosd.uf2` 파일을 `NRF52BOOT`에 복사하는 방식으로만 갱신합니다.
- SoftDevice를 7.x로 올리지 않습니다. Adafruit 코어 1.7.0은 PCA10056에서 S140 6.1.1만 지원합니다.
