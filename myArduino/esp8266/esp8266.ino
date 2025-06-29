#include <ESP8266WiFi.h>

const char* ssid = "MyESP-AP";
const char* password = "12345678";

WiFiClient client;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to AP...");
  }

  Serial.println("Connected to AP.");
  Serial.println(WiFi.localIP());

  if (client.connect("192.168.4.1", 1234)) {
    client.println("Hello from ESP8266!");
    delay(500);

    while (client.available()) {
      String response = client.readStringUntil('\n');
      Serial.println("Received from PicoW: " + response);
    }

    client.stop();
  } else {
    Serial.println("Failed to connect to server.");
  }
}

void loop() {
  // Do nothing
}
