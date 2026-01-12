#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

/* ===== OLED ===== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ===== HC-06 ===== */
SoftwareSerial BTSerial(10, 11); // RX, TX

/* ===== 설정 ===== */
const char* BEACON_UUID = "E2C56DB5-DFFB-48D2-B060-D0F5A71096E0";
const char* ITEM_NAME  = "Wallet";
unsigned long prevReq = 0;

/* ===== FOUND 로직 ===== */
unsigned long veryNearStart = 0;
bool found = false;
bool inVeryNear = false;
String lastSentState = "";

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Beacon Client");
  display.display();
  delay(1000);
}

void loop() {
  /* 1초마다 서버 요청 */
  if (millis() - prevReq > 1000) {
    prevReq = millis();
    BTSerial.print("[LT_SQL]GETBEACON@");
    BTSerial.print(BEACON_UUID);
    BTSerial.print("\n");
  }

  if (BTSerial.available()) {
    handleBT();
  }
}

/* ===== BT 수신 ===== */
void handleBT() {
  char buf[128];
  int len = BTSerial.readBytesUntil('\n', buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  char *p[10];
  int idx = 0;
  char *tok = strtok(buf, "[@]");
  while (tok && idx < 10) {
    p[idx++] = tok;
    tok = strtok(NULL, "[@]");
  }

  if (idx < 6) return;

  int rssi   = atoi(p[3]);
  float dist = atof(p[4]);

  showBeacon(rssi, dist);
}

/* ===== 상태 서버 전송 ===== */
void sendStateToServer(int rssi, float dist, String state) {
  BTSerial.print("[LT_SQL]BEACON_STATE@");
  BTSerial.print(BEACON_UUID);
  BTSerial.print("@");
  BTSerial.print(rssi);
  BTSerial.print("@");
  BTSerial.print(dist, 2);
  BTSerial.print("@");
  BTSerial.print(state);
  BTSerial.print("\n");
}

/* ===== 핵심 OLED + 상태 로직 ===== */
void showBeacon(int rssi, float dist) {

  /* ===== FOUND 상태 ===== */
  if (found) {
  display.clearDisplay();
  display.setTextSize(2);

  String line1 = "WALLET";
  String line2 = "FOUND!!!";

  /* 두 줄 중 더 긴 문자열 기준으로 X 계산 */
  int maxLen = max(line1.length(), line2.length());
  int x = (128 - maxLen * 12) / 2;
  if (x < 0) x = 0;

  /* 같은 X 위치에 두 줄 출력 */
  display.setCursor(x, 0);
  display.println(line1);

  display.setCursor(x, 16);
  display.println(line2);

  display.display();

  /* FOUND 해제 조건 */
  if (dist >= 1.1) {
    found = false;
    inVeryNear = false;
    veryNearStart = 0;
    lastSentState = "";
  }
  return;
}

  display.clearDisplay();

  /* ===== 거리 (윗줄) ===== */
  display.setTextSize(2);
  String distStr = String(dist, 1) + "m";
  int xDist = (128 - distStr.length() * 12) / 2;
  display.setCursor(xDist, 0);
  display.println(distStr);

  String status;

  /* ===== VERY NEAR 히스테리시스 ===== */
  if (dist < 0.9) {
    if (!inVeryNear) {
      inVeryNear = true;
      veryNearStart = millis();
    }
    status = "VERY NEAR";
  }
  else if (dist < 1.1 && inVeryNear) {
    status = "VERY NEAR";
  }
  else {
    inVeryNear = false;
    veryNearStart = 0;

    if (dist >= 10.0)      status = "VERY FAR";
    else if (dist >= 5.0)  status = "FAR";
    else                   status = "NEAR";
  }

  /* ===== FOUND 판정 ===== */
  if (inVeryNear) {
    if ((unsigned long)(millis() - veryNearStart) >= 4000) {
      found = true;
      sendStateToServer(rssi, dist, "FOUND");
      lastSentState = "FOUND";
      return;
    }
  }

  /* ===== 상태 서버 전송 ===== */
  if (status != lastSentState) {
    sendStateToServer(rssi, dist, status);
    lastSentState = status;
  }

  /* ===== 상태 (아랫줄) ===== */
  display.setTextSize(2);
  int xStatus = (128 - status.length() * 12) / 2;
  display.setCursor(xStatus, 16);
  display.println(status);

  display.display();
}
