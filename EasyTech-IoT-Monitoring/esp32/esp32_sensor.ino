#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

/* ================== WIFI ================== */
const char* ssid     = "iPhone 1000000000 ;)";
const char* password = "12345678";

/* ================== MQTT ================== */
const char* mqtt_server = "172.20.10.2";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "sensors/data";
const char* mqtt_client_id = "esp32_esp01";

/* ================== PIN CONFIG ================== */
#define PIEZO_PIN 35
#define GREEN_LED 19
#define RED_LED   18

/* ================== DREMPELS ================== */
#define TEMP_LOW        15.0
#define TEMP_HIGH       30.0
#define VIBRATION_HIGH  2000

/* ================== OBJECTEN ================== */
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_BME280 bme;

/* ================== WIFI CONNECT ================== */
void connectWiFi() {
  Serial.print("Verbinden met WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi verbonden, IP: ");
  Serial.println(WiFi.localIP());
}

/* ================== MQTT CONNECT ================== */
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Verbinden met MQTT... ");
    if (mqttClient.connect(mqtt_client_id)) {
      Serial.println("verbonden");
    } else {
      Serial.print("mislukt, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" opnieuw proberen...");
      delay(2000);
    }
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

  connectWiFi();

  mqttClient.setServer(mqtt_server, mqtt_port);

  Serial.println("ESP gestart en klaar");
}

/* ================== LOOP ================== */
void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  /* ===== TEMPERATUUR ===== */
  float temperature = bme.readTemperature();

  /* ===== TRILLING (max van meerdere samples) ===== */
  int vibration = 0;
  for (int i = 0; i < 5; i++) {
    int value = analogRead(PIEZO_PIN);
    if (value > vibration) vibration = value;
    delay(2);
  }

  /* ===== STATUS ===== */
  bool error = false;
  if (temperature < TEMP_LOW || temperature > TEMP_HIGH) error = true;
  if (vibration > VIBRATION_HIGH) error = true;

  /* ===== LED LOGICA ===== */
  digitalWrite(GREEN_LED, !error);
  digitalWrite(RED_LED, error);

  /* ===== JSON PAYLOAD ===== */
  String payload = "{";
  payload += "\"node_id\":\"esp01\",";
  payload += "\"temperature\":" + String(temperature, 2) + ",";
  payload += "\"vibration\":" + String(vibration) + ",";
  payload += "\"status\":\"" + String(error ? "NOT_OK" : "OK") + "\"";
  payload += "}";

  mqttClient.publish(mqtt_topic, payload.c_str());

  /* ===== DEBUG ===== */
  Serial.println("----- ESP STATUS -----");
  Serial.println(payload);

  delay(5000);
}
