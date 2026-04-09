#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

// Pines RFID (adaptados para ESP32)
#define SS_PIN    5   // SDA
#define RST_PIN   22  // RST
#define SCK_PIN   18  // SPI clock
#define MISO_PIN  19  // SPI MISO
#define MOSI_PIN  23  // SPI MOSI
#define READY_LED_PIN   25  // Sistema listo
#define GRANTED_LED_PIN 26  // Acceso concedido
#define DENIED_LED_PIN  27  // Acceso denegado
#define SERVO_PIN       32  // Senal del servomotor SG90
#define BUZZER_PIN      33  // Buzzer
#define SERVO_PWM_CHANNEL    0
#define SERVO_PWM_FREQUENCY  50
#define SERVO_PWM_RESOLUTION 16

// Datos WiFi (AP compartido)
const char* ssid     = "ANGEL_SARANGO";
const char* password = "Suquinancy77";

// Host del servidor local (evita depender de una IP fija)
// Si el laboratorio no resuelve nombres de equipo, usa una reserva DHCP o vuelve a IP fija.
const char* serverHost = "192.168.0.104";
const uint16_t serverPort = 80;
const char* serverPath = "/api.php";
const unsigned long timeout = 5000; // Tiempo de espera HTTP en ms
const unsigned int accessLedDurationMs = 1500;
const unsigned int grantedBuzzerDurationMs = 200;
const unsigned int deniedBuzzerDurationMs = grantedBuzzerDurationMs * 3;
const unsigned int servoOpenDurationMs = 1000;
const int servoRestAngle = 0;
const int servoOpenAngle = 90;
const unsigned long wifiConnectTimeoutMs = 20000;

MFRC522 rfid(SS_PIN, RST_PIN); // Inicializacion del lector RFID

const char* wifiStatusToString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE_STATUS";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETED";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

String buildServerUrl(const String& uid) {
  return "http://" + String(serverHost) + ":" + String(serverPort) + String(serverPath) + "?uid=" + uid;
}

void logRfidStatus() {
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);

  Serial.printf("RFID pins -> SS:%d RST:%d SCK:%d MISO:%d MOSI:%d\n",
                SS_PIN, RST_PIN, SCK_PIN, MISO_PIN, MOSI_PIN);

  if (version == 0x00 || version == 0xFF) {
    Serial.println("RC522 no detectado. Revisa cableado, 3.3V, GND y pines SPI");
  } else {
    Serial.printf("RC522 detectado. Version: 0x%02X\n", version);
  }
}

void logServerResolution() {
  IPAddress serverIp;

  if (WiFi.hostByName(serverHost, serverIp)) {
    Serial.println("Servidor resuelto: " + String(serverHost) + " -> " + serverIp.toString());
  } else {
    Serial.println("No se pudo resolver el host del servidor: " + String(serverHost));
    Serial.println("La red actual probablemente no publica DNS local para ese nombre");
  }
}

bool isTargetNetworkVisible() {
  int networkCount = WiFi.scanNetworks();

  if (networkCount <= 0) {
    Serial.println("No se detectaron redes WiFi durante el escaneo");
    return false;
  }

  bool networkFound = false;

  Serial.println("Redes detectadas:");
  for (int i = 0; i < networkCount; i++) {
    String currentSsid = WiFi.SSID(i);
    Serial.printf("  %d) %s RSSI:%d dBm Canal:%d %s\n",
                  i + 1,
                  currentSsid.c_str(),
                  WiFi.RSSI(i),
                  WiFi.channel(i),
                  (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "abierta" : "protegida");

    if (currentSsid.equals(ssid)) {
      networkFound = true;
    }
  }

  return networkFound;
}

bool connectToWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(200);

  bool networkVisible = isTargetNetworkVisible();
  if (!networkVisible) {
    Serial.println("La red configurada no aparece en el escaneo: " + String(ssid));
    Serial.println("Verifica que el hotspot/router este emitiendo en 2.4 GHz y que el SSID sea exacto");
  }

  if (password[0] == '\0') {
    WiFi.begin(ssid);
  } else {
    WiFi.begin(ssid, password);
  }
  Serial.print("Conectando a WiFi");

  unsigned long startTime = millis();
  wl_status_t lastStatus = WL_IDLE_STATUS;

  while (millis() - startTime < wifiConnectTimeoutMs) {
    wl_status_t currentStatus = WiFi.status();
    if (currentStatus == WL_CONNECTED) {
      Serial.println();
      return true;
    }

    if (currentStatus != lastStatus) {
      Serial.printf("\nEstado WiFi: %s\n", wifiStatusToString(currentStatus));
      lastStatus = currentStatus;
    }

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.printf("Fallo WiFi. Estado final: %s\n", wifiStatusToString(WiFi.status()));
  return false;
}

uint32_t angleToDutyCycle(int angle) {
  const uint32_t minPulseUs = 500;
  const uint32_t maxPulseUs = 2400;
  const uint32_t pwmMaxDuty = (1UL << SERVO_PWM_RESOLUTION) - 1;
  const uint32_t pulseUs = minPulseUs + ((maxPulseUs - minPulseUs) * (uint32_t)angle) / 180U;
  const uint32_t periodUs = 1000000UL / SERVO_PWM_FREQUENCY;

  return (pulseUs * pwmMaxDuty) / periodUs;
}

void setServoAngle(int angle) {
  if (angle < 0) {
    angle = 0;
  } else if (angle > 180) {
    angle = 180;
  }

  ledcWrite(SERVO_PWM_CHANNEL, angleToDutyCycle(angle));
}

void initializeServo() {
  ledcSetup(SERVO_PWM_CHANNEL, SERVO_PWM_FREQUENCY, SERVO_PWM_RESOLUTION);
  ledcAttachPin(SERVO_PIN, SERVO_PWM_CHANNEL);
  setServoAngle(servoRestAngle);
}

void activateServoLock() {
  Serial.println("Servo ACTIVADO");
  setServoAngle(servoOpenAngle);
  delay(servoOpenDurationMs);
  setServoAngle(servoRestAngle);
  Serial.println("Servo EN REPOSO");
}

void initializeOutputs() {
  pinMode(READY_LED_PIN, OUTPUT);
  pinMode(GRANTED_LED_PIN, OUTPUT);
  pinMode(DENIED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(READY_LED_PIN, HIGH);
  digitalWrite(GRANTED_LED_PIN, LOW);
  digitalWrite(DENIED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

void signalAccessResult(int ledPin, unsigned int buzzerDurationMs) {
  digitalWrite(ledPin, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(buzzerDurationMs);
  digitalWrite(BUZZER_PIN, LOW);
  delay(accessLedDurationMs);
  digitalWrite(ledPin, LOW);
}

void signalAccessGranted() {
  signalAccessResult(GRANTED_LED_PIN, grantedBuzzerDurationMs);
}

void signalAccessDenied() {
  signalAccessResult(DENIED_LED_PIN, deniedBuzzerDurationMs);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN); // Inicializacion SPI explicita para ESP32
  delay(50);
  rfid.PCD_Init();       // Inicializacion del modulo RFID
  logRfidStatus();

  initializeOutputs();
  initializeServo();

  if (!connectToWifi()) {
    Serial.println("Error de conexion WiFi");
  } else {
    Serial.println("Conectado. IP: " + WiFi.localIP().toString());
    logServerResolution();
  }
}

void loop() {
  // Verifica si hay una nueva tarjeta
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(10);
    return;
  }

  // Leer el UID de la tarjeta
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    char buffer[3];
    sprintf(buffer, "%02X", rfid.uid.uidByte[i]);
    uid += buffer;
  }

  Serial.println("UID detectado: " + uid);

  // Enviar el UID al servidor
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    // Construir la URL con el parametro GET
    String fullUrl = buildServerUrl(uid);
    Serial.println("Consultando URL: " + fullUrl);
    http.begin(client, fullUrl);
    http.setTimeout(timeout);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      response.trim();
      Serial.println("Respuesta del servidor: " + response);

      if (response.equalsIgnoreCase("GRANTED")) {
        signalAccessGranted();
        activateServoLock();
      } else {
        Serial.println("Acceso denegado");
        signalAccessDenied();
      }
    } else {
      Serial.printf("Error HTTP: %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("Sin conexion WiFi");
  }

  // Fin de la comunicacion con la tarjeta
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(200);
}

