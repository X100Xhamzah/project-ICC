/**
 * ESP32 data transfer met HTTP
 *
 * Doel:
 * - Leest temperatuur (BME280) en trillingen
 * - Bouwt een JSON payload met node_id + seq
 * - Verstuurt via HTTP POST naar Node-red
 *
 * eisen:
 * - Queue/buffering: berichten worden lokaal bewaart
 * - Retry & reconnect: bij netwerk disconnects blijft het bericht in een queue
 * - Backoff: wachttijd tussen retries worden groter bij meerdere falen
 * - Data wordt niet duplicatie meegeven: elk bericht heeft een oplopende seq
 */

/* ================== INCLUDES ================== */
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

/* ================== WIFI CONFIG ================== */
const char* ssid     = "rinkydinkyahh internet";
const char* password = "GrotePepernoot/0/";

/* ================== HTTP CONFIG ================== */
/**
 * Node-RED endpoint:
 * - HTTP In node: POST /esp/data
 */
const char* http_url = "http://192.168.68.105:1880/esp/data";

/* ================== DEVICE CONFIG ================== */
const char* node_id = "esp01";

/* ================== PIN CONFIG ================== */
#define PIEZO_PIN 35
#define GREEN_LED 19
#define RED_LED   18

/* ================== THRESHOLDS (DREMPELS) ================== */
#define TEMP_LOW        15.0
#define TEMP_HIGH       30.0
#define VIBRATION_HIGH  2000

/* ================== QUEUE / BUFFER CONFIG ================== */
#define QUEUE_SIZE 20             // Max aantal berichten in lokale buffer
#define FLUSH_INTERVAL_MS 1000    // Min interval tussen flush-pogingen

/* ================== DATA STRUCTURES ================== */
/**
 * Msg:
 * - seq: unieke oplopende message-id 
 * - created_ms: millis() timestamp (debug / trace)
 */
struct Msg {
  uint32_t seq;
  float temperature;
  int vibration;
  bool error;
  uint32_t created_ms;
};

/* Ringbuffer (FIFO) */
Msg queueBuf[QUEUE_SIZE];
int qHead = 0;   // Index van het oudste item
int qTail = 0;   // Index waar het nieuwe item wordt toegevoegd
int qCount = 0;  // Aantal items in de queue

/* ================== STATE / TIMING ================== */
uint32_t nextSeq = 1;                 // Wordt lokaal opgeslagen
unsigned long lastFlush = 0;
unsigned long backoffMs = 1000;       // Start backoff (1s)
const unsigned long backoffMax = 30000; // Max backoff (30s)

const unsigned long MEASURE_INTERVAL_MS = 5000;
unsigned long lastMeasure = 0;

/* ================== OBJECTS ================== */
Adafruit_BME280 bme;
Preferences prefs;

/* ================== QUEUE HELPERS ================== */

bool queueIsFull() { return qCount >= QUEUE_SIZE; }

bool queueIsEmpty() { return qCount == 0; }

void queuePush(const Msg& m) {
  if (queueIsFull()) {
    // Drop oudste om ruimte te maken (we behouden zo de meest recente data)
    qHead = (qHead + 1) % QUEUE_SIZE;
    qCount--;
  }
  queueBuf[qTail] = m;
  qTail = (qTail + 1) % QUEUE_SIZE;
  qCount++;
}

Msg* queuePeek() {
  if (queueIsEmpty()) return nullptr;
  return &queueBuf[qHead];
}

void queuePop() {
  if (queueIsEmpty()) return;
  qHead = (qHead + 1) % QUEUE_SIZE;
  qCount--;
}

/* ================== WIFI CONNECT ================== */
// Zorgt dat WiFi verbonden is. Probeert te verbinden als er geen verbinding is.
//Time-out: ~15s per connect-poging.
 
void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Verbinden met WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi verbonden, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi nog niet verbonden (later opnieuw).");
  }
}

/* ================== PERSISTENCE (NVS) ================== */

// Laadt persistent state uit buffer nextSeq.
// Hiermee voorkom je dat seq na een reboot opnieuw bij 1 begint.

void loadState() {
  prefs.begin("esp_http", false);
  nextSeq = prefs.getUInt("nextSeq", 1);
  prefs.end();
}


void saveNextSeq() {
  prefs.begin("esp_http", false);
  prefs.putUInt("nextSeq", nextSeq);
  prefs.end();
}

/* ================== JSON BUILD ================== */

String buildPayload(const Msg& m) {
  const char* statusStr = m.error ? "NOT_OK" : "OK";

  String payload = "{";
  payload += "\"node_id\":\"" + String(node_id) + "\",";
  payload += "\"seq\":" + String(m.seq) + ",";
  payload += "\"created_ms\":" + String(m.created_ms) + ",";
  payload += "\"temperature\":" + String(m.temperature, 2) + ",";
  payload += "\"vibration\":" + String(m.vibration) + ",";
  payload += "\"status\":\"" + String(statusStr) + "\"";
  payload += "}";
  return payload;
}

/* ================== HTTP SEND ================== */

bool httpSendOne(const Msg& m) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(5000);

  if (!http.begin(http_url)) {
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Node-Id", node_id);
  http.addHeader("X-Seq", String(m.seq)); // Extra header voor debug / tracing

  String payload = buildPayload(m);
  int code = http.POST(payload);

  // Server response lezen
  String resp = "";
  if (code > 0) resp = http.getString();

  http.end();

  if (code == 200 || code == 201 || code == 204) {
    Serial.print("HTTP OK voor seq=");
    Serial.print(m.seq);
    if (resp.length()) {
      Serial.print(" resp=");
      Serial.print(resp);
    }
    Serial.println();
    return true;
  }

  Serial.print("HTTP FAIL code=");
  Serial.print(code);
  Serial.print(" seq=");
  Serial.println(m.seq);
  return false;
}

/* ================== FLUSH QUEUE ================== */
void flushQueue() {
  if (queueIsEmpty()) return;

  ensureWiFi();
  if (WiFi.status() != WL_CONNECTED) return;

  Msg* m = queuePeek();
  if (!m) return;

  if (httpSendOne(*m)) {
    queuePop();
    backoffMs = 1000; // Reset backoff na succes
  } else {
    // Exponential backoff bij falen
    backoffMs = (backoffMs * 2 > backoffMax) ? backoffMax : backoffMs * 2;
  }
}

/* ================== SETUP ================== */
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);

  if (!bme.begin(0x76)) {
    Serial.println("FOUT: BME280 niet gevonden");
    digitalWrite(RED_LED, HIGH);
    while (1);
  }

  loadState();
  ensureWiFi();

  Serial.println("ESP gestart (HTTP mode) en klaar");
}

/* ================== LOOP ================== */
void loop() {
  unsigned long now = millis();

  if (now - lastFlush >= backoffMs && now - lastFlush >= FLUSH_INTERVAL_MS) {
    lastFlush = now;

    // Stuur meerdere berichten per flush-cycle (max 5),
    // stop zodra een bericht faalt (dan heeft backoff effect).
    for (int i = 0; i < 5; i++) {
      if (queueIsEmpty()) break;

      Msg* m = queuePeek();
      if (!m) break;

      if (httpSendOne(*m)) {
        queuePop();
        backoffMs = 1000;
      } else {
        backoffMs = (backoffMs * 2 > backoffMax) ? backoffMax : backoffMs * 2;
        break;
      }
    }
  }

  if (now - lastMeasure >= MEASURE_INTERVAL_MS) {
    lastMeasure = now;

    float temperature = bme.readTemperature();

    int vibration = 0;
    for (int i = 0; i < 5; i++) {
      int value = analogRead(PIEZO_PIN);
      if (value > vibration) vibration = value;
      delay(2);
    }

    bool error = false;
    if (temperature < TEMP_LOW || temperature > TEMP_HIGH) error = true;
    if (vibration > VIBRATION_HIGH) error = true;

    digitalWrite(GREEN_LED, !error);
    digitalWrite(RED_LED, error);

    Msg m;
    m.seq = nextSeq++;
    m.temperature = temperature;
    m.vibration = vibration;
    m.error = error;
    m.created_ms = now;

    queuePush(m);
    saveNextSeq();

    Serial.println("----- ESP STATUS -----");
    Serial.println(buildPayload(m));
    Serial.print("Queue count: ");
    Serial.println(qCount);
  }
}
