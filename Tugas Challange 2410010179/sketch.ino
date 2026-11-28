#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>


const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;


#define DHTPIN 15
#define DHTTYPE DHT22
#define LED_PIN 2
#define BUZZER_PIN 4
#define SERVO_PIN 13


const char* TOPIC_SUHU          = "iot/2410010179/sensor/suhu";
const char* TOPIC_ALARM         = "iot/2410010179/alarm/suhu";
const char* TOPIC_LED_CMD       = "iot/2410010179/aktuator/led";
const char* TOPIC_LED_STATUS    = "iot/2410010179/status/led";
const char* TOPIC_PWM_CMD       = "iot/2410010179/aktuator/pwm";
const char* TOPIC_MODE_CMD      = "iot/2410010179/mode";
const char* TOPIC_MODE_STATUS   = "iot/2410010179/status/mode";
const char* TOPIC_BUZZER_CMD    = "iot/2410010179/aktuator/buzzer";
const char* TOPIC_BUZZER_STATUS = "iot/2410010179/status/buzzer";
const char* TOPIC_SERVO_CMD     = "iot/2410010179/aktuator/servo";
const char* TOPIC_SERVO_STATUS  = "iot/2410010179/status/servo";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
Servo myServo;

unsigned long lastMsg = 0;
String currentMode = "MANUAL"; 
bool ledState = false;
int pwmValue = 0;

void setup_wifi() {
  delay(10);
  Serial.print("Menghubungkan ke WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung! IP: " + WiFi.localIP().toString());
}

void setBuzzer(bool state) {
  if (state) {
    tone(BUZZER_PIN, 1000); // 
    client.publish(TOPIC_BUZZER_STATUS, "ON");
  } else {
    noTone(BUZZER_PIN);
    client.publish(TOPIC_BUZZER_STATUS, "OFF");
  }
}

void setServoAngle(int angle) {
  angle = constrain(angle, 0, 180);
  myServo.write(angle);
  client.publish(TOPIC_SERVO_STATUS, String(angle).c_str());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();
  String strTopic = String(topic);

  Serial.println("[TERIMA] Topik: " + strTopic + " | Pesan: " + message);


  if (strTopic == TOPIC_LED_CMD) {
    if (message == "ON") {
      ledState = true;
      analogWrite(LED_PIN, 255);
      client.publish(TOPIC_LED_STATUS, "ON");
    } else if (message == "OFF") {
      ledState = false;
      analogWrite(LED_PIN, 0);
      client.publish(TOPIC_LED_STATUS, "OFF");
    }
  }
  
  else if (strTopic == TOPIC_PWM_CMD) {
    pwmValue = constrain(message.toInt(), 0, 255);
    analogWrite(LED_PIN, pwmValue);
    if (pwmValue > 0) {
      ledState = true;
      client.publish(TOPIC_LED_STATUS, ("ON (" + String(pwmValue) + ")").c_str());
    } else {
      ledState = false;
      client.publish(TOPIC_LED_STATUS, "OFF");
    }
  }
 
  else if (strTopic == TOPIC_MODE_CMD) {
    if (message == "AUTO" || message == "MANUAL") {
      currentMode = message;
      client.publish(TOPIC_MODE_STATUS, currentMode.c_str());
      Serial.println("[MODE] Berubah ke: " + currentMode);
    }
  }
  
  else if (strTopic == TOPIC_BUZZER_CMD) {
    if (currentMode == "MANUAL") {
      setBuzzer(message == "ON");
    } else {
      Serial.println("[INFO] Abaikan perintah Buzzer manual karena mode AUTO aktif.");
    }
  }
  
  else if (strTopic == TOPIC_SERVO_CMD) {
    if (currentMode == "MANUAL") {
      setServoAngle(message.toInt());
    } else {
      Serial.println("[INFO] Abaikan perintah Servo manual karena mode AUTO aktif.");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke EMQX Broker...");
    String clientId = "ESP32_Wokwi_2410010179_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Terhubung!");
      client.subscribe(TOPIC_LED_CMD);
      client.subscribe(TOPIC_PWM_CMD);
      client.subscribe(TOPIC_MODE_CMD);
      client.subscribe(TOPIC_BUZZER_CMD);
      client.subscribe(TOPIC_SERVO_CMD);
      
      // Kirim status awal
      client.publish(TOPIC_MODE_STATUS, currentMode.c_str());
      client.publish(TOPIC_LED_STATUS, "OFF");
      client.publish(TOPIC_BUZZER_STATUS, "OFF");
      client.publish(TOPIC_SERVO_STATUS, "0");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi dalam 3 detik");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  dht.begin();
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  
  if (now - lastMsg >= 5000) {
    lastMsg = now;
    float t = dht.readTemperature();

    if (isnan(t)) {
      Serial.println("Gagal membaca sensor DHT22!");
      return;
    }

    String suhuStr = String(t, 1);
    client.publish(TOPIC_SUHU, suhuStr.c_str());
    Serial.println("[PUBLISH] Suhu: " + suhuStr + " °C | Mode: " + currentMode);

    
    if (t > 35.0) {
      String alarmMsg = "BAHAYA! Suhu tinggi: " + suhuStr + " °C";
      client.publish(TOPIC_ALARM, alarmMsg.c_str());
      
      if (currentMode == "AUTO") {
        setBuzzer(true);
        setServoAngle(90); 
      }
    } else {
      client.publish(TOPIC_ALARM, "NORMAL");
      
      if (currentMode == "AUTO") {
        setBuzzer(false);
        setServoAngle(0);  
      }
    }
  }
}