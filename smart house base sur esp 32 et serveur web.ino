#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>


const char* ssid     = "Tunisie_Telecom-5G-B98E";       
const char* password = "ISLEM@000";  
WebServer server(80);


#define DHTPIN        4
#define DHTTYPE       DHT11
#define PIR_PIN       13
#define LDR_PIN       34
#define MQ2_PIN       35
#define SERVO_PIN     18
#define RELAY_PIN     16
#define BUZZER_PIN    17
#define LED_ALARM_PIN 2


const int SEUIL_LUMIERE_BAS = 1000;  
const int SEUIL_GAZ_DANGER  = 1800;  
const float SEUIL_TEMP_ELEVEE = 30.0;

// Variables d'état globales
float temp = 0.0;
float hum = 0.0;
int ldrValue = 0;
int gasValue = 0;
bool motionState = false;
bool relayStatus = false;
bool doorOpen = false;
bool gasAlert = false;

// Instance des périphériques
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
Servo myServo;


String getHTMLPage() {
  String html = "<!DOCTYPE html><html lang='fr'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Smart House Dashboard</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f4f7f6; margin: 0; padding: 20px; text-align: center; }";
  html += "h1 { color: #333; }";
  html += ".grid { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; margin-top: 20px; }";
  html += ".card { background: white; padding: 20px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); width: 160px; }";
  html += ".card h3 { margin: 0 0 10px 0; color: #666; font-size: 14px; }";
  html += ".card p { font-size: 22px; font-weight: bold; margin: 0; color: #2c3e50; }";
  html += ".btn { display: inline-block; padding: 12px 24px; font-size: 16px; font-weight: bold; color: white; border: none; border-radius: 8px; cursor: pointer; margin: 10px; text-decoration: none; }";
  html += ".btn-on { background-color: #2ecc71; } .btn-off { background-color: #e74c3c; }";
  html += ".alert { background-color: #e74c3c; color: white; padding: 15px; border-radius: 8px; font-weight: bold; margin-bottom: 20px; }";
  html += "</style>";
  html += "<script>setInterval(function() { fetch('/data').then(response => response.json()).then(data => {";
  html += "document.getElementById('temp').innerText = data.temp + ' °C';";
  html += "document.getElementById('hum').innerText = data.hum + ' %';";
  html += "document.getElementById('ldr').innerText = data.ldr;";
  html += "document.getElementById('gas').innerText = data.gas;";
  html += "document.getElementById('pir').innerText = data.pir ? 'Détecté' : 'Aucun';";
  html += "document.getElementById('alert').style.display = data.alert ? 'block' : 'none';";
  html += "}); }, 2000);</script></head><body>";

  html += "<h1>🏠 Smart House ESP32</h1>";
  html += "<div id='alert' class='alert' style='display:" + String(gasAlert ? "block" : "none") + ";'>⚠️ DANGER : GAZ / FUMÉE DÉTECTÉE !</div>";

  html += "<div class='grid'>";
  html += "<div class='card'><h3>Température</h3><p id='temp'>" + String(temp, 1) + " °C</p></div>";
  html += "<div class='card'><h3>Humidité</h3><p id='hum'>" + String((int)hum) + " %</p></div>";
  html += "<div class='card'><h3>Luminosité</h3><p id='ldr'>" + String(ldrValue) + "</p></div>";
  html += "<div class='card'><h3>Niveau Gaz</h3><p id='gas'>" + String(gasValue) + "</p></div>";
  html += "<div class='card'><h3>Mouvement</h3><p id='pir'>" + String(motionState ? "Détecté" : "Aucun") + "</p></div>";
  html += "</div><br>";

  html += "<h3>Contrôles Manuels</h3>";
  html += "<a href='/toggleRelay' class='btn " + String(relayStatus ? "btn-off" : "btn-on") + "'>" + String(relayStatus ? "Éteindre Relais" : "Allumer Relais") + "</a>";
  html += "<a href='/toggleDoor' class='btn btn-on'>" + String(doorOpen ? "Fermer Porte" : "Ouvrir Porte") + "</a>";

  html += "</body></html>";
  return html;
}


void handleRoot() {
  server.send(200, "text/html", getHTMLPage());
}

void handleData() {
  String json = "{";
  json += "\"temp\":" + String(temp, 1) + ",";
  json += "\"hum\":" + String((int)hum) + ",";
  json += "\"ldr\":" + String(ldrValue) + ",";
  json += "\"gas\":" + String(gasValue) + ",";
  json += "\"pir\":" + String(motionState ? "true" : "false") + ",";
  json += "\"alert\":" + String(gasAlert ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleToggleRelay() {
  relayStatus = !relayStatus;
  digitalWrite(RELAY_PIN, relayStatus ? LOW : HIGH); // Relais souvent actif bas
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleToggleDoor() {
  doorOpen = !doorOpen;
  myServo.write(doorOpen ? 90 : 0);
  server.sendHeader("Location", "/");
  server.send(303);
}


void setup() {
  Serial.begin(115200);

  // Configuration E/S
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_ALARM_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH); // Éteint
  digitalWrite(LED_ALARM_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Servo
  ESP32PWM::allocateTimer(0);
  myServo.setPeriodHertz(50);
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(0);

  // LCD & DHT
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("ESP32 SMART HOME");
  lcd.setCursor(0, 1);
  lcd.print("Connexion WiFi..");

  // Connexion Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connecte !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Web Server:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(3000);

  // Routes du serveur Web
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/toggleRelay", handleToggleRelay);
  server.on("/toggleDoor", handleToggleDoor);
  server.begin();
}


void loop() {
  server.handleClient(); // Traitement des requêtes Web

  // Lecture régulière des capteurs
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000) {
    lastCheck = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temp = t;
    if (!isnan(h)) hum = h;

    ldrValue = analogRead(LDR_PIN);
    gasValue = analogRead(MQ2_PIN);
    motionState = digitalRead(PIR_PIN);

    // 1. Alarme Gaz
    if (gasValue > SEUIL_GAZ_DANGER) {
      gasAlert = true;
      digitalWrite(BUZZER_PIN, HIGH);
      digitalWrite(LED_ALARM_PIN, HIGH);
    } else {
      gasAlert = false;
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(LED_ALARM_PIN, LOW);
    }

    // 2. Gestion automatique Porte (PIR)
    if (motionState && !doorOpen) {
      myServo.write(90);
    } else if (!motionState && !doorOpen) {
      myServo.write(0);
    }

    // 3. Mise à jour Écran LCD
    lcd.setCursor(0, 0);
    if (gasAlert) {
      lcd.print("!! ALARME GAZ !!");
    } else {
      lcd.print("T:");
      lcd.print((int)temp);
      lcd.print("C H:");
      lcd.print((int)hum);
      lcd.print("%  L:");
      lcd.print(ldrValue / 100); // Réduction affichage
      lcd.print(" ");
    }

    lcd.setCursor(0, 1);
    lcd.print("IP:");
    lcd.print(WiFi.localIP().toString().substring(0, 13));
  }
}