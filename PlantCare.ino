#include <Ultrasonic.h>
#include <NTPClient.h>
#include <DHT11.h>
#include <ArduinoHttpClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "settings.h"

int waterPump = A4;
int moiSensorPin = A5;
int powerPin = 2;
int waterPumpCount = 0;
int moiSensorValue = 0; 
int temperature = 0;
int humidity = 0;
int lightSensorPin = A3;
int lightSensorValue = 0;
int waterLevel = 0;
int waterVolume = 0;
int distance = 0;
int litersPerCm = 1.09;
int button = 12;

DHT11 dht11(8);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

Ultrasonic ultrasonic(4, 7);

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASSWORD;

WiFiServer server(443); 

void setup() {
  Serial.begin(9600);
  wifiSetup();
  server.begin();
  pinMode(waterPump, OUTPUT);
  digitalWrite(waterPump, LOW);
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
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

void fetchData () {
  distance = ultrasonic.read();
  waterLevel = max(44 - distance, 0); 
  waterVolume = (waterLevel * litersPerCm);
  dht11.readTemperatureHumidity(temperature, humidity);
  moiSensorValue = map(analogRead(moiSensorPin), 0, 1023, 100, 0);
  lightSensorValue = map(analogRead(lightSensorPin), 0, 1023, 1000, 0);
}

void checkWaterLevelAndPump(int duration) {
  distance = ultrasonic.read();
  delay(500);
  waterLevel = max(44 - distance, 0);

  if (waterLevel > 2) {
    digitalWrite(waterPump, HIGH);
    delay(duration);
    digitalWrite(waterPump, LOW);
    waterPumpCount++;
  }
}

void handleClient(WiFiClient client) {
  digitalWrite(powerPin, HIGH);
  delay(100);
  fetchData();
  delay(100);
  String currentLine = ""; 
  timeClient.update(); 
  int currentDay = timeClient.getDay();
  int currentHours = timeClient.getHours();
  int currentMinutes = timeClient.getMinutes();
  int currentSeconds = timeClient.getSeconds();

  String weather = "";
  if (lightSensorValue < 200) {
    weather = "M&ouml;rkt";
  } else if (lightSensorValue >= 200 && lightSensorValue < 990) {
    weather = "Skugga";
  } else if (lightSensorValue >= 990 && lightSensorValue < 995) {
    weather = "Soligt";
  } else if (lightSensorValue >= 995) {
    weather = "Mycket soligt";
  }

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
          client.println("  body { font-family: Arial, sans-serif; background-color: #cef8ff; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; flex-direction: column; min-height: 100vh; }");
          client.println("  h1 { color: #3e8e41; font-size: 28px; margin-bottom: 20px; text-align: center; }");
          client.println("  .container { width: 90%; max-width: 600px; background-color: #fff; padding: 20px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); box-sizing: border-box; }");
          client.println("  p { font-size: 16px; margin-bottom: 10px; color: #333; text-align: center; }");
          client.println("  .stat { font-weight: bold; }");
          client.println("  .value { color: #4CAF50; font-weight: normal; }");
          client.println("  .card { background-color: #f9f9f94c; padding: 15px; margin-bottom: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.3); }");
          client.println("  button { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 18px; display: block; width: 100%; margin: 10px 0; }");
          client.println("  button:hover { background-color: #45a049; }");
          client.println("  footer { margin-top: 30px; text-align: center; color: #777; font-size: 14px; }");
          client.println("</style>");
          client.println("</head>");
          client.println("<body>");
          client.println("<div class='container'>");

          client.println("<div class='card'>");
          client.println("<p><span class='stat'>V&auml;derlek:</span> <span class='value'>" + weather + " </span></p>");
          client.println("<p><span class='stat'>Temperatur:</span> <span class='value'>" + String(temperature) + " &deg;C</span></p>");
          client.println("<p><span class='stat'>Luftfuktighet:</span> <span class='value'>" + String(humidity) + " %</span></p>");
          client.println("<p><span class='stat'>Jordfuktighet:</span> <span class='value'>" + String(moiSensorValue) + " %</span></p>");
          client.println("<p><span class='stat'>Vattenvolym:</span> <span class='value'>" + String(waterVolume) + " L</span></p>");
          client.println("<p><span class='stat'>Antal vattningar denna vecka:</span> <span class='value'>" + String(waterPumpCount) + " st</span></p>");
          
          if (waterVolume < 2) {
            client.println("<p style='color: red; font-weight: bold;'>Varning: Fyll p&aring; vattentank!</p>");
          }
          client.println("<button onclick='refresh()'>Uppdatera</button>");
          client.println("</div>");
          client.println("<script>");
          client.println("function refresh() {");
          client.println("  location.reload();"); 
          client.println("}");
          client.println("window.onload = function() {");
          client.println("  setTimeout(function() {");
          client.println("    console.log('Dag: ' + " + String(currentDay) + " + ' Tid: ' + " + String(currentHours) + " + ' Solstyrka: ' + " + String(lightSensorValue) + ");");
          client.println("    console.log('Dag: ' + " + String(currentDay) + " + ' Tid: ' + " + String(currentHours) + " + ' Temperatur: ' + " + String(temperature) + ");");
          client.println("    console.log('Dag: ' + " + String(currentDay) + " + ' Tid: ' + " + String(currentHours) + " + ' Luftfuktighet: ' + " + String(humidity) + ");");
          client.println("    console.log('Dag: ' + " + String(currentDay) + " + ' Tid: ' + " + String(currentHours) + " + ' Jordfuktighet: ' + " + String(moiSensorValue) + ");");
          client.println("  }, 3000);"); 
          client.println("};");
          client.println("setTimeout(function() {");
          client.println(" location.reload();"); 
          client.println("}, 3600000);"); 
          client.println("</script>");
          client.println("<footer>");
          client.println("</footer>");
        
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
  digitalWrite(powerPin, LOW);
  client.stop(); 
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    handleClient(client); 
  }

  timeClient.update(); 
  int currentDay = timeClient.getDay();
  int currentHours = timeClient.getHours();
  int currentMinutes = timeClient.getMinutes(); 
  int currentSeconds = timeClient.getSeconds();

  if(currentDay == 1 && currentHours == 0 && currentMinutes == 0) {
    waterPumpCount = 0;
  }

  if(digitalRead(button) == HIGH) {
    digitalWrite(powerPin, HIGH);
    delay(100);
    checkWaterLevelAndPump(10000);
    digitalWrite(powerPin, LOW);
  }
  
  if ((currentHours == 7 || currentHours == 13 || currentHours == 18) && 
      currentMinutes == 0 && currentSeconds < 10) {
      digitalWrite(powerPin, HIGH);
      delay(100);

      if (currentHours == 13) {
        dht11.readTemperatureHumidity(temperature, humidity);
        delay(500);
        if (temperature >= 35) {
          checkWaterLevelAndPump(60000);
        }  
      } else {
        checkWaterLevelAndPump(120000);
      }
    digitalWrite(powerPin, LOW);
   }
}