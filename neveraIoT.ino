#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

HardwareSerial RFID(2);

// Configuración WiFi y servidor
const char* ssid = "Codecraft Devs";
const char* password = "CODEcr4ftD3V2023!";
const char* server = "10.2.20.115";
const int port = 4888;

// Pines
const int greenLedPin = 18;
const int redLedPin = 19;
const int buzzerPin = 25;
const int relayPin = 26;

String text;
char c;
unsigned long lastReconnectAttempt = 0;

void showMessage(const String &msg, int textSize = 1, int y = 10) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, y);
  display.println(msg);
  display.display();
}

void wifiConnect() {
  if(WiFi.status() == WL_CONNECTED) return;
  
  Serial.print("Reconectando a WiFi...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    showMessage("Conectado!", 1, 0);
  } else {
    Serial.println("\nError conexion WiFi");
    showMessage("Error WiFi", 1, 0);
  }
}

void setup() {
  Serial.begin(115200);
  RFID.begin(9600, SERIAL_8N1, 5, -1);

  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Configurar pantalla primero
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Error en OLED"));
    for(;;);
  }
  showMessage("Iniciando...", 1, 0);

  // Conexión WiFi inicial
  wifiConnect();
}

void loop() {
  // Reconexión WiFi cada 10 segundos si es necesario
  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt > 10000) {
    lastReconnectAttempt = millis();
    wifiConnect();
  }

  // Lectura RFID
  while (RFID.available() > 0) {
    delay(5);
    c = RFID.read();
    text += c;
  }

  if (text.length() > 20) {
    check();
    text = "";
  }
}

void check() {
  text = text.substring(1, 11);
  Serial.print("UID leído: ");
  Serial.println(text);

  if(WiFi.status() != WL_CONNECTED) {
    showMessage("Sin conexion", 1, 0);
    digitalWrite(redLedPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
    delay(1000);
    digitalWrite(redLedPin, LOW);
    digitalWrite(buzzerPin, LOW);
    return;
  }

  String url = "http://" + String(server) + ":" + String(port) + 
              "/api/v1/employees/cardCode/" + text;
  Serial.println("Enviando a: " + url);

  HTTPClient http;
  http.begin(url);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Respuesta: " + payload);

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String firstName = doc["data"]["names"]["firstName"].as<String>();
      String lastName = doc["data"]["names"]["lastName"].as<String>();
      
      if(!firstName.isEmpty() && !lastName.isEmpty()) {
        String welcomeMsg = "Hola " + firstName + " " + lastName;
        showMessage(welcomeMsg);
        
        // Pitido de acceso (2 beeps cortos)
        for(int i=0; i<2; i++){
          digitalWrite(buzzerPin, HIGH);
          delay(100);
          digitalWrite(buzzerPin, LOW);
          delay(50);
        }
        
        accesoConcedido();
      } else {
        showMessage("Tarjeta no valida", 1, 0);
        accesoDenegado();
      }
    } else {
      Serial.print("Error parsing JSON: ");
      Serial.println(error.c_str());
      accesoDenegado();
    }
  } else {
    Serial.printf("Error HTTP: %d\n", httpCode);
    showMessage("Error servidor", 1, 0);
    accesoDenegado();
  }
  
  http.end();
}

void accesoConcedido() {
  digitalWrite(greenLedPin, HIGH);
  digitalWrite(relayPin, HIGH);
  delay(5000);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(relayPin, LOW);
  showMessage("Acerca tu tarjeta...");
}

void accesoDenegado() {
  // Pitido largo de error
  digitalWrite(buzzerPin, HIGH);
  digitalWrite(redLedPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(redLedPin, LOW);
  showMessage("Acceso denegado", 1, 0);
  delay(4000);
  showMessage("Acerca tu tarjeta...");
}