#include <Ultrasonic.h>
#include <NTPClient.h>
#include <DHT11.h>
#include <ArduinoHttpClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "settings.h"

int waterPump = A4;
int moiSensorPin = A5;
int moiPowerPin = 13; 

int waterPumpCount = 0;
int moiSensorValue = 0; 
int temperature = 0;
int humidity = 0;

DHT11 dht11(8);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

Ultrasonic ultrasonic(4, 7);

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASSWORD;

WiFiServer server(443); 

unsigned long previousMillis = 0;
const long interval = 3600000;

void setup() {
  Serial.begin(9600);
  wifiSetup();
  server.begin();
  pinMode(waterPump, OUTPUT);
  pinMode(moiPowerPin, OUTPUT); 
  digitalWrite(moiPowerPin, LOW);
  timeClient.begin();
}

void wifiSetup() {
  if (WiFi.status() != WL_CONNECTED) {  
    Serial.println("Försöker ansluta till Wi-Fi...");
    WiFi.begin(ssid, password);
    
    unsigned long startAttemptTime = millis(); 
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {  
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Ansluten till Wi-Fi!");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Kunde inte ansluta till Wi-Fi.");
    }
  }
}

void moistureSensor() {
 digitalWrite(moiPowerPin, HIGH); 
 delay(100); 
 moiSensorValue = analogRead(moiSensorPin); 
 digitalWrite(moiPowerPin, LOW); 
 moiSensorValue = map(moiSensorValue, 0, 1023, 100, 0); 
}

void handleClient(WiFiClient client) {
  String currentLine = ""; 
  int distance = ultrasonic.read();
  float litersPerCm = 1.09;
  float waterLevel = (44 - distance); 

  if(waterLevel < 0) {
    waterLevel = 0;
  }
  float waterVolume = (waterLevel * litersPerCm);

  dht11.readTemperatureHumidity(temperature, humidity);
  moistureSensor(); 

  while (client.connected()) {
    if (client.available()) {
      char c = client.read();  
      Serial.write(c);  
      currentLine += c;

      if (c == '\n') {
        if (currentLine.startsWith("GET /")) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println("Access-Control-Allow-Origin: *");
          client.println();

          client.println("<html>");
          client.println("<head><title>Plant Care+</title>");
          client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"); 
          client.println("<style>");
          client.println("  body { font-family: Arial, sans-serif; background-color: #f2f2f2; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; flex-direction: column; min-height: 100vh; }");
          client.println("  h1 { color: #3e8e41; font-size: 28px; margin-bottom: 20px; text-align: center; }");
          client.println("  .container { width: 90%; max-width: 600px; background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); box-sizing: border-box; }");
          client.println("  p { font-size: 16px; margin-bottom: 10px; color: #333; text-align: center; }");
          client.println("  .stat { font-weight: bold; }");
          client.println("  .value { color: #4CAF50; font-weight: normal; }");
          client.println("  .card { background-color: #f9f9f9; padding: 15px; margin-bottom: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }");
          client.println("  button { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 18px; display: block; width: 100%; margin: 10px 0; }");
          client.println("  button:hover { background-color: #45a049; }");
          client.println("  footer { margin-top: 30px; text-align: center; color: #777; font-size: 14px; }");
          client.println("  @media (max-width: 768px) {");
          client.println("    h1 { font-size: 24px; }");
          client.println("    .container { width: 95%; }");
          client.println("    p { font-size: 14px; }");
          client.println("    button { font-size: 16px; }");
          client.println("  }");
          client.println("  @media (max-width: 480px) {");
          client.println("    h1 { font-size: 22px; }");
          client.println("    p { font-size: 12px; }");
          client.println("  }");
          client.println("</style>");
          client.println("</head>");
          client.println("<body>");
          client.println("<div class='container'>");

          client.println("<div class='card'>");
          client.println("<p><span class='stat'>Temperatur:</span> <span class='value'>" + String(temperature) + " &deg;C</span></p>");
          client.println("<p><span class='stat'>Luftfuktighet:</span> <span class='value'>" + String(humidity) + " %</span></p>");
          client.println("<p><span class='stat'>Jordfuktighet:</span> <span class='value'>" + String(moiSensorValue) + " %</span></p>");
          client.println("<p><span class='stat'>Antal vattningar idag:</span> <span class='value'>" + String(waterPumpCount) + " st</span></p>");
          client.println("<p><span class='stat'>Vattenvolym:</span> <span class='value'>" + String(waterVolume) + " L</span></p>");
          if (waterVolume < 2) {
            client.println("<p style='color: red; font-weight: bold;'>Varning: Fyll p&aring; vattentank!</p>");
          }
          client.println("</div>");

          client.println("<button onclick='refreshData()'>Uppdatera</button>");
          client.println("</div>");

          client.println("<footer>");
          client.println("</footer>");

          client.println("<script>");
            client.println("function refreshData() {");
             client.println("location.reload();");
            client.println("}");
          client.println("</script>");

          client.println("</body>");
          client.println("</html>");
        } else {
          client.println("HTTP/1.1 404 Not Found");
          client.println("Connection: close");
          client.println();
        }
        break;
      }
    }
  }
  client.stop(); 
}

void loop() {
  unsigned long currentMillis = millis();
  WiFiClient client = server.available();

  if (client) {
    handleClient(client); 
  }

  timeClient.update(); 
  int currentHour = timeClient.getHours();
  int currentMinutes = timeClient.getMinutes();
  int distance = ultrasonic.read();
  float litersPerCm = 1.09;
  float waterLevel = 44 - distance; 

  if(waterLevel < 0) {
    waterLevel = 0;
  }

  float waterVolume = waterLevel * litersPerCm;

  if(currentHour == 0 && currentMinutes == 0) {
    waterPumpCount = 0;
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    dht11.readTemperatureHumidity(temperature, humidity);
    Serial.println("Temperature: " + String(temperature)+" °C");
    Serial.println("Air-moisture: " + String(humidity) + " %"); 
    moistureSensor(); 
    Serial.println("Soil-moisture: " + String(moiSensorValue) + " %"); 
    Serial.println("Waterpump count: " + String(waterPumpCount) + " st"); 
    Serial.println("Water level: " + String(waterLevel) + " cm");
    Serial.println("Water volume: " + String(waterVolume) + " L");
    Serial.println("Distance: " + String(distance) + " cm");

    if (waterLevel < 2) {
        digitalWrite(waterPump, LOW);
    } else if (moiSensorValue < 40) {
      delay(5000);
        digitalWrite(waterPump, HIGH);
        delay(10000);
        digitalWrite(waterPump, LOW);
        waterPumpCount++;
    }
  }
}
