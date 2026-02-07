# 🚀 IR + 초음파 센서 거리 측정 시스템

Raspberry Pi를 이용한 IR 센서 기반 물체 감지 및 초음파 센서 거리 측정 시스템입니다.  
IR 센서가 물체를 감지하면 초음파 센서로 정확한 거리를 측정하고, 설정된 임계값 이내에 접근 시 LED로 경고합니다.

## 📋 목차

- [주요 기능](#주요-기능)
- [하드웨어 구성](#하드웨어-구성)
- [회로 연결](#회로-연결)
- [소프트웨어 요구사항](#소프트웨어-요구사항)
- [설치 방법](#설치-방법)
- [사용 방법](#사용-방법)
- [데이터베이스 구조](#데이터베이스-구조)
- [문제 해결](#문제-해결)
- [개발 정보](#개발-정보)

## ✨ 주요 기능

- **인터럽트 기반 물체 감지**: IR 센서로 물체를 감지하면 자동으로 거리 측정 시작
- **정확한 거리 측정**: HC-SR04 초음파 센서로 2~400cm 범위의 거리 측정
- **시각적 경고 시스템**: 설정된 임계값(기본 20cm) 이내 접근 시 LED 점등
- **데이터 로깅**: 모든 측정값을 SQLite 데이터베이스에 자동 저장
- **실시간 모니터링**: 측정 데이터와 통계를 실시간으로 확인 가능

## 🔧 하드웮어 구성

### 필수 부품

| 부품명 | 수량 | 설명 |
|--------|------|------|
| Raspberry Pi 4 | 1 | 메인 컨트롤러 (다른 모델도 가능) |
| HC-SR04 초음파 센서 | 1 | 거리 측정용 |
| IR 센서 모듈 | 1 | 물체 감지용 (장애물 감지 센서) |
| LED | 1 | 경고 표시용 (5mm, 색상 무관) |
| 220Ω 저항 | 1 | LED 전류 제한용 |
| 점퍼 와이어 | 적당량 | 회로 연결용 |
| 브레드보드 | 1 | 회로 구성용 (선택사항) |

### 권장 사양

- Raspberry Pi OS (Bullseye 이상)
- 최소 1GB RAM
- microSD 카드 8GB 이상

## 🔌 회로 연결

### GPIO 핀 배치

```
Raspberry Pi 4 (40-pin Header)
┌─────────────────────────────┐
│  3.3V  (1)  (2)  5V         │
│  GPIO2 (3)  (4)  5V         │
│  GPIO3 (5)  (6)  GND        │
│        ...                  │
│  GPIO17(11) (12) GPIO18     │ ← Echo Pin
│  GPIO27(13) (14) GND        │ ← Trig Pin
│        ...                  │
│  GPIO22(15) (16) GPIO23     │ ← IR Pin / LED Pin
│        ...                  │
└─────────────────────────────┘
```

### 상세 연결도

#### 1. 초음파 센서 (HC-SR04)

```
HC-SR04          Raspberry Pi
────────         ────────────
VCC      ─────── 5V (Pin 2 or 4)
TRIG     ─────── GPIO 27 (Pin 13)
ECHO     ─────── GPIO 17 (Pin 11)
GND      ─────── GND (Pin 6, 9, 14, 20, 25, 30, 34, 39)
```

#### 2. IR 센서

```
IR 센서          Raspberry Pi
───────          ────────────
VCC      ─────── 3.3V or 5V (Pin 1 or 2)
OUT      ─────── GPIO 22 (Pin 15)
GND      ─────── GND
```

#### 3. LED

```
                 220Ω
GPIO 23 ─────── [저항] ─────── LED(+) ─────── LED(-) ─────── GND
(Pin 16)
```

### 회로도 (간략)

```
                  ┌─────────────────┐
                  │  Raspberry Pi   │
                  ├─────────────────┤
  HC-SR04         │                 │        LED
  ┌─────┐         │                 │      ┌─────┐
  │ VCC ├─────────┤ 5V              │      │     │
  │ TRIG├─────────┤ GPIO 27         │      │  +  │
  │ ECHO├─────────┤ GPIO 17         │   ┌──┤     ├──┐
  │ GND ├─────┐   │                 │   │  │  -  │  │
  └─────┘     │   │                 │   │  └─────┘  │
              │   │                 │   │           │
  IR 센서      │   │                 │  220Ω        │
  ┌─────┐     │   │                 │   │           │
  │ VCC ├─────┼───┤ 3.3V            │   │           │
  │ OUT ├─────┼───┤ GPIO 22         ├───┘           │
  │ GND ├─────┼───┤                 │               │
  └─────┘     │   │ GPIO 23 ────────┼───────────────┘
              │   │                 │
              └───┤ GND             │
                  └─────────────────┘
```

## 💻 소프트웨어 요구사항

### 필수 패키지

```bash
# 컴파일러
gcc

# GPIO 제어 라이브러리
libgpiod-dev

# SQLite 데이터베이스
sqlite3
libsqlite3-dev

# 빌드 도구
make
```

## 📦 설치 방법

### 1. 시스템 업데이트 및 패키지 설치

```bash
# 시스템 업데이트
sudo apt update
sudo apt upgrade -y

# 필수 패키지 설치
sudo apt install -y gcc make libgpiod-dev sqlite3 libsqlite3-dev

# 설치 확인
gcc --version
sqlite3 --version
```

### 2. 프로젝트 클론 또는 다운로드

```bash
# Git이 설치되어 있다면
git clone <repository-url>
cd rpi_sensor_logger

# 또는 파일을 직접 복사
```

### 3. 컴파일

```bash
# src 디렉터리로 이동
cd src

# 전체 컴파일
make

# 또는 특정 파일만 컴파일
make ir_ultrasonic_sensor
```

### 4. 테스트 코드 실행 (선택사항)

개별 센서를 먼저 테스트하고 싶다면:

```bash
# lab 디렉터리로 이동
cd lab

# 테스트 파일 컴파일
make

# 예: LED 테스트
sudo ./led_test

# 예: 초음파 센서 테스트
sudo ./ultrasonic_test
```

## 🎮 사용 방법

### 기본 실행

```bash
# src 디렉터리로 이동
cd src

# 프로그램 실행 (sudo 권한 필요)
make run

# 또는 직접 실행
sudo ./ir_ultrasonic_sensor
```

### 프로그램 종료

- **정상 종료**: `Ctrl + C`
- **강제 종료**: 다른 터미널에서 `make stop`

### 데이터 확인

```bash
# src 디렉터리에서

# 최근 측정 데이터 조회 (최근 20개)
make view_db

# 측정 통계 확인
make stats

# SQLite 직접 접근
sqlite3 ultrasonic.db

# 또는 절대 경로로
sqlite3 /home/pi/rpi_sensor_logger/src/ultrasonic.db
```

### 전체 명령어

```bash
# src 디렉터리에서 실행

make         # 모든 소스 파일 컴파일
make run     # 프로그램 실행
make view_db # 데이터 조회
make stats   # 통계 보기
make stop    # 프로그램 종료
make clean   # 빌드 파일 및 DB 삭제
make help    # 도움말 표시
```

```bash
# lab 디렉터리에서 실행 (개별 센서 테스트)

make                # 모든 테스트 파일 컴파일
sudo ./led_test     # LED 테스트
sudo ./ultrasonic_test  # 초음파 센서 테스트
sudo ./dht11_test   # 온습도 센서 테스트
make clean          # 테스트 파일 삭제
```

## 🗄️ 데이터베이스 구조

### 테이블: ultrasonic

| 컬럼명 | 타입 | 설명 |
|--------|------|------|
| id | INTEGER | 고유 ID (자동 증가) |
| measurement_num | INT | 측정 번호 |
| distance | REAL | 측정된 거리 (cm) |
| ir_triggered | BOOL | IR 센서 트리거 여부 (1: 감지됨, 0: 미감지) |
| timestamp | DATETIME | 측정 시간 (자동 생성) |

### SQL 쿼리 예시

```sql
-- 모든 데이터 조회
SELECT * FROM ultrasonic;

-- 최근 10개 데이터
SELECT * FROM ultrasonic ORDER BY id DESC LIMIT 10;

-- 20cm 이내 측정값만 조회
SELECT * FROM ultrasonic WHERE distance < 20.0;

-- 시간대별 평균 거리
SELECT 
  DATE(timestamp) as 날짜,
  AVG(distance) as 평균거리,
  COUNT(*) as 측정횟수
FROM ultrasonic
GROUP BY DATE(timestamp);
```

## 🛠️ 문제 해결

### 컴파일 에러

**문제**: `libgpiod.h: No such file or directory`
```bash
# 해결
sudo apt install libgpiod-dev
```

**문제**: `sqlite3.h: No such file or directory`
```bash
# 해결
sudo apt install libsqlite3-dev
```

### 실행 에러

**문제**: `Error: Chip Open Failed`
```bash
# GPIO 칩 확인
ls -l /dev/gpiochip*

# 권한 확인 (sudo로 실행해야 함)
sudo ./ir_ultrasonic_sensor
```

**문제**: `Permission denied`
```bash
# 실행 권한 부여
chmod +x ir_ultrasonic_sensor

# sudo로 실행
sudo ./ir_ultrasonic_sensor
```

**문제**: `Echo timeout`
- 초음파 센서 연결 확인 (VCC, GND, TRIG, ECHO)
- 센서와 라즈베리파이 사이 거리 확인
- 점퍼 와이어 불량 확인

**문제**: IR 센서 반응 없음
- IR 센서 전원 연결 확인
- 감지 거리 조절 (센서에 있는 가변저항 조정)
- 센서 출력 신호 확인 (LOW active인지 확인)

### 센서 문제

**초음파 센서 측정값이 불안정할 때**
- 센서를 평평하고 단단한 곳에 고정
- 측정 대상이 울퉁불퉁하거나 부드러운 재질인 경우 반사가 약할 수 있음
- 전원 공급 안정성 확인 (5V 충분히 공급되는지)

**IR 센서가 계속 트리거될 때**
- 센서 감도 조절 (가변저항 조정)
- 주변 광원 확인 (일부 IR 센서는 주변광에 민감)
- 센서 위치 조정

## 📝 커스터마이징

### 거리 임계값 변경

`ir_ultrasonic_sensor.c` 파일에서:

```c
const double THRESHOLD = 20.0;  // 20cm → 원하는 값으로 변경
```

### GPIO 핀 변경

```c
const int trig_pin = 27;   // 초음파 트리거
const int echo_pin = 17;   // 초음파 에코
const int ir_pin = 22;     // IR 센서
const int led_pin = 23;    // LED
```

### 측정 주기 변경

```c
usleep(500000);  // 0.5초 → 원하는 값으로 변경 (마이크로초 단위)
// 1초 = 1,000,000 마이크로초
```

## 🔍 동작 원리

### 1. IR 센서 물체 감지
- IR 센서가 물체를 감지하면 출력 핀이 HIGH → LOW로 변경 (Falling Edge)
- GPIO 인터럽트가 발생하여 프로그램에 알림

### 2. 초음파 거리 측정
1. TRIG 핀에 10μs 펄스 전송
2. 센서가 40kHz 초음파 8번 발사
3. 물체에 반사되어 돌아온 초음파를 센서가 수신
4. ECHO 핀이 HIGH 상태로 유지된 시간 측정
5. 거리 계산: `거리 = (시간 × 음속) / 2`
   - 음속 = 343 m/s = 34,300 cm/s
   - 2로 나누는 이유: 왕복 거리

### 3. LED 제어
- 측정 거리 < 임계값(20cm) → LED ON
- 측정 거리 ≥ 임계값 → LED OFF

### 4. 데이터 저장
- 모든 측정값을 SQLite DB에 타임스탬프와 함께 저장

## 📊 성능 특성

- **측정 범위**: 2cm ~ 400cm
- **측정 정확도**: ±3mm (이상적 조건)
- **측정 간격**: 약 0.5초 (조정 가능)
- **응답 시간**: IR 감지 후 약 10ms 이내 측정 시작
- **전력 소비**: 약 15mA (센서 동작 시)

## 👨‍💻 개발 정보

### 개발 환경

- **OS**: Raspberry Pi OS (Bullseye)
- **언어**: C
- **컴파일러**: GCC
- **라이브러리**: libgpiod, SQLite3

### 파일 구조

```
rpi_sensor_logger/
│
├── lab/                         # 센서 테스트 코드
│   ├── button_test.c           # 버튼 센서 테스트
│   ├── db_test.c               # 데이터베이스 테스트
│   ├── dht11_test.c            # 온습도 센서 테스트
│   ├── led_test.c              # LED 제어 테스트
│   ├── line_trace_test.c       # 라인 트레이서 테스트
│   ├── mpu6050_test.c          # 자이로/가속도 센서 테스트
│   ├── ultrasonic_test.c       # 초음파 센서 테스트
│   ├── test.c                  # 기타 테스트
│   ├── Makefile                # 테스트 빌드 파일
│   └── NOTICE.txt              # 테스트 관련 안내
│
├── src/                         # 메인 소스 코드
│   ├── ir_ultrasonic_sensor.c  # IR + 초음파 통합 시스템 (메인)
│   ├── ultrasonic_db.c         # 초음파 센서 DB 로거
│   ├── Makefile                # 메인 빌드 파일
│   └── ultrasonic.db           # 데이터베이스 (실행 후 생성)
│
├── .gitignore                   # Git 제외 파일 목록
└── README.md                    # 프로젝트 문서 (이 파일)
```

### 버전 히스토리

- **v1.0** (2026-02-07)
  - 초기 릴리즈
  - IR 센서 + 초음파 센서 통합
  - SQLite 데이터 로깅 구현
  - LED 경고 시스템 추가

## 📄 라이센스

이 프로젝트는 교육 목적으로 자유롭게 사용 가능합니다.


---

**⚡ 즐거운 개발 되세요! ⚡**
