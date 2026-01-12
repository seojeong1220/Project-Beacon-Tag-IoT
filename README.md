# Project-Beacon-Tag-IoT
### BLE Beacon 기반 IoT 분실 탐지 시스템

---

## 프로젝트 개요

본 프로젝트는 **BLE iBeacon 신호(RSSI + UUID)를 기반으로 거리 추정 및 상태 판별을 수행하는 IoT 분실 탐지 시스템**입니다.  
ESP32에서 Beacon 신호를 수집하고, 서버(Raspberry Pi)에서 데이터를 처리한 뒤,  
웹 기반 UI를 통해 **실시간 상태와 탐지 결과를 시각화**합니다.

단순 RSSI 값 출력이 아닌,  
**연속 측정 기반 상태 머신(State Machine)** 과  
**이벤트 지속 조건(Time-based Condition)** 을 적용하여  
BLE 신호의 불안정한 환경에서도 **안정적인 분실 탐지**를 목표로 설계되었습니다.

---

##  프로젝트 목표

- BLE Beacon RSSI 기반 거리 추정
- UUID 기반 TAG 식별 구조 설계
- 센서 노이즈를 고려한 상태 기반 판단 로직 구현
- 시간 조건을 포함한 이벤트(FIND) 판별
- 단말–서버–DB–UI 계층 분리 구조 설계
- IoT 시스템 관점의 데이터 흐름 및 상태 관리 구현

---

## 시스템 아키텍처

<img width="800" height="370" alt="architecture" src="https://github.com/user-attachments/assets/9bff43f8-1753-47e1-823f-aa74e6da5102" />

### 역할 분리

- **ESP32 Scanner**
  - BLE Beacon 스캔
  - UUID + RSSI 수집
  - TCP Socket을 통해 서버로 전송

- **Raspberry Pi Server**
  - RSSI → 거리(distance) 변환
  - 상태(State) 및 이벤트(FIND) 판별
  - DB 저장 및 상태 관리

- **Database (MariaDB)**
  - Beacon 로그 저장
  - 최신 상태 캐싱

- **Web UI**
  - 실시간 상태 모니터링
  - 거리 및 이벤트 시각화

---

## 하드웨어 구성

| 구분 | 부품 | 역할 | 인터페이스 |
|----|----|----|----|
| Scanner | ESP32 | BLE Beacon 스캔 | BLE |
| Beacon | iBeacon | UUID + RSSI 송출 | BLE |
| Server | Raspberry Pi | 데이터 처리 | Linux |
| Storage | MariaDB | 로그 및 상태 저장 | SQL |
| UI | Web Browser | 상태 모니터링 | HTTP |

---

## 시스템 동작 흐름

1. ESP32가 주변 iBeacon 신호(UUID + RSSI)를 주기적으로 스캔
2. RSSI 데이터를 TCP 소켓을 통해 서버로 전송
3. 서버에서 RSSI → 거리(distance) 변환 수행
4. 거리 값을 기반으로 상태(State) 판별
5. 상태 지속 시간 조건 충족 시 이벤트(FIND) 발생
6. 결과를 DB에 저장하고 Web UI에 반영

---

## 거리 추정 및 상태 분류

### 거리 추정
- Beacon RSSI 값을 기반으로 거리 모델 적용
- 단일 샘플이 아닌 **연속 측정값**을 사용하여 안정성 확보

### 상태 분류 기준

| Distance (m) | State |
|--------------|-------|
| ≥ 10.0 | VERY FAR |
| ≥ 5.0 | FAR |
| ≥ 1.0 | NEAR |
| < 1.0 | VERY NEAR |

## 상태 머신(State Machine) 설계

- 상태는 거리 범위에 따라 결정
- 상태 전이는 **즉시 발생하지 않음**
- 연속 조건 충족 시에만 상태 유지 또는 전이

### 설계 목적
- BLE RSSI의 순간적 변동으로 인한 오동작 방지
- 이벤트 오탐(false positive) 최소화
- 시스템 안정성 확보

---

## 이벤트 기반 FIND 로직

- VERY NEAR 상태가 **일정 시간 이상(4초) 지속될 경우 FOUND 이벤트 발생**
- 단발성 RSSI 급변에 의한 오판정 방지
- FOUND 상태 진입 시:
  - 이벤트 상태 고정
  - 추가 상태 전이 제한
- 거리 이탈 시 FOUND 상태 해제

➡ 단순 임계값 비교가 아닌  
➡ **시간 + 상태 지속성 기반 이벤트 판단 구조**

---

## Web UI (모니터링)

<img width="700" height="756" alt="ui-table" src="https://github.com/user-attachments/assets/bc589af9-21f5-4715-98a0-6f0a5fbb1321" />

![Image](https://github.com/user-attachments/assets/47b899cf-cfc7-4093-aec4-fdd914b0c07e)

### 주요 기능

- 최신 거리 및 상태 실시간 표시
- 상태 변화에 따른 색상 및 위치 시각화
- FOUND 이벤트 발생 시 명확한 상태 전달

### 사람 기준 원형 거리 UI

- 중앙의 사람(🧍)은 사용자 기준 위치
- 동심원은 실제 거리 기준 (1m / 3m / 5m / 10m)
- WALLET TAG는 거리 값에 따라 원형 맵 상에서 이동
- 숫자를 보지 않아도 **상대적인 거리감 즉시 인지 가능**


---

##  파일 구조

