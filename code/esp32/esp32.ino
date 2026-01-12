#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

/* =========================
   WiFi / Server 설정
   ========================= */
const char* ssid     = "embA";
const char* password = "embA1234";

const char* server_ip = "10.10.14.75";
const int   server_port = 5000;

WiFiClient client;

/* =========================
   BLE Scan 설정
   ========================= */
BLEScan* pBLEScan;

/* =========================
   RSSI 평균 필터 설정
   ========================= */
#define MAX_BEACONS       10
#define RSSI_AVG_WINDOW   5

struct BeaconRSSI {
  String uuid;
  int    rssiBuf[RSSI_AVG_WINDOW];
  int    idx;
  int    count;
};

BeaconRSSI beacons[MAX_BEACONS];
int beaconCount = 0;

/* =========================
   Beacon 관리 함수
   ========================= */
BeaconRSSI* getBeacon(String uuid) {
  for (int i = 0; i < beaconCount; i++) {
    if (beacons[i].uuid == uuid)
      return &beacons[i];
  }

  if (beaconCount < MAX_BEACONS) {
    beacons[beaconCount].uuid = uuid;
    beacons[beaconCount].idx = 0;
    beacons[beaconCount].count = 0;
    return &beacons[beaconCount++];
  }
  return nullptr;
}

int calcAvgRSSI(BeaconRSSI* b, int newRssi) {
  b->rssiBuf[b->idx] = newRssi;
  b->idx = (b->idx + 1) % RSSI_AVG_WINDOW;
  if (b->count < RSSI_AVG_WINDOW) b->count++;

  int sum = 0;
  for (int i = 0; i < b->count; i++) {
    sum += b->rssiBuf[i];
  }
  return sum / b->count;
}

/* =========================
   iBeacon Callback
   ========================= */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {

      if (!advertisedDevice.haveManufacturerData()) return;

      String mData = advertisedDevice.getManufacturerData();
      const uint8_t* payload = (uint8_t*)mData.c_str();
      int length = mData.length();
      if (length < 25) return;

      if (payload[0] == 0x4C && payload[1] == 0x00 &&
          payload[2] == 0x02 && payload[3] == 0x15) {

        char uuid_c[37];
        sprintf(uuid_c,
                "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                payload[4], payload[5], payload[6], payload[7],
                payload[8], payload[9],
                payload[10], payload[11],
                payload[12], payload[13],
                payload[14], payload[15], payload[16], payload[17],
                payload[18], payload[19]);

        String uuid = String(uuid_c);
        int rawRSSI = advertisedDevice.getRSSI();

        BeaconRSSI* b = getBeacon(uuid);
        if (!b) return;

        int avgRSSI = calcAvgRSSI(b, rawRSSI);

        Serial.println("📡 iBeacon");
        Serial.println("UUID     : " + uuid);
        Serial.print("RSSI Raw : "); Serial.println(rawRSSI);
        Serial.print("RSSI Avg : "); Serial.println(avgRSSI);

        if (client.connected()) {
          String msg = "[LT_SQL]BEACON@UUID@";
          msg += uuid;
          msg += "@RSSI@";
          msg += String(avgRSSI);
          msg += "\n";

          client.print(msg);
          Serial.println("➡ 서버 전송 (AVG): " + msg);
        }
        Serial.println("-------------------------");
      }
    }
};

/* =========================
   WiFi + Server 연결
   ========================= */
void connectWiFi() {
  Serial.print("WiFi 연결 중...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 연결 완료");
}

void connectServer() {
  Serial.print("iot_server 연결 중...");
  if (client.connect(server_ip, server_port)) {
    Serial.println(" 연결 성공");
    client.print("[ESP32:PASSWD]");
  } else {
    Serial.println(" 연결 실패");
  }
}

/* =========================
   Arduino setup()
   ========================= */
void setup() {
  Serial.begin(115200);
  connectWiFi();
  connectServer();

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

/* =========================
   Arduino loop()
   ========================= */
void loop() {
  if (!client.connected()) {
    connectServer();
  }

  pBLEScan->start(2, false);
  pBLEScan->clearResults();
  delay(1000);
}
