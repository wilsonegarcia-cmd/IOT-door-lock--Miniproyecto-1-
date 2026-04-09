#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "ANGEL_SARANGO";
const char* password = "Suquinancy77";
const char* serverHost = "192.168.0.104";
const uint16_t serverPort = 80;
const char* serverPath = "/api.php?uid=13C652F6";
const unsigned long wifiConnectTimeoutMs = 20000;
const unsigned long httpTimeoutMs = 5000;

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

const char* authModeToString(wifi_auth_mode_t authMode) {
  switch (authMode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2_WPA3_PSK";
    default:
      return "UNKNOWN_AUTH";
  }
}

String buildTestUrl() {
  return "http://" + String(serverHost) + ":" + String(serverPort) + String(serverPath);
}

void scanNetworks() {
  int networkCount = WiFi.scanNetworks();

  if (networkCount <= 0) {
    Serial.println("No se detectaron redes WiFi");
    return;
  }

  Serial.println("Redes detectadas:");
  for (int i = 0; i < networkCount; i++) {
    String currentSsid = WiFi.SSID(i);
    wifi_auth_mode_t authMode = WiFi.encryptionType(i);

    Serial.printf("  %d) %s RSSI:%d dBm Canal:%d %s\n",
                  i + 1,
                  currentSsid.c_str(),
                  WiFi.RSSI(i),
                  WiFi.channel(i),
                  authModeToString(authMode));

    if (currentSsid.equals(ssid)) {
      Serial.println("Red objetivo encontrada con seguridad: " + String(authModeToString(authMode)));
    }
  }
}

bool connectToWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(200);

  scanNetworks();

  WiFi.begin(ssid, password);
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

void testHttp() {
  WiFiClient client;
  HTTPClient http;
  String fullUrl = buildTestUrl();

  Serial.println("Consultando URL: " + fullUrl);
  http.begin(client, fullUrl);
  http.setTimeout(httpTimeoutMs);

  int httpCode = http.GET();
  if (httpCode > 0) {
    String response = http.getString();
    response.trim();
    Serial.printf("HTTP %d -> %s\n", httpCode, response.c_str());
  } else {
    Serial.printf("Error HTTP: %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Inicio prueba WiFi");

  if (!connectToWifi()) {
    Serial.println("Error de conexion WiFi");
    return;
  }

  Serial.println("Conectado. IP: " + WiFi.localIP().toString());
  testHttp();
}

void loop() {
  delay(1000);
}