#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

#define ONE_WIRE_BUS 33  // GPIO pin for DS18B20 aka temperature sensor
#define MOISTURE_PIN 34 // GPIO pin for moisture data

// Security idea is that, you connect another ESP/device to this AP, so no need to have it on your (guest) wifi network, hence ok if the device gets stolen and the attacker extracts the firmware image and finds this password
const char* ssid = "MoestuinSensor";
const char* password = "changeme";
const char* mqttServer = "192.168.13.37";
const int mqttPort = 1883;
const char* mqttTopic = "sensor/data";
IPAddress local_IP(192, 168, 13, 1);  // ESP32 AP's IP Address
IPAddress gateway(192, 168, 13, 1);
IPAddress subnet(255, 255, 255, 0);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  Serial.printf("ESP32 Access Point Started! AP IP Address: %s\n", WiFi.softAPIP().toString().c_str());

  client.setServer(mqttServer, mqttPort);
  sensors.begin();
  Serial.print("Program initialized");
}

void reconnectMQTT() {
    if (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("ESP32Publisher")) {
            Serial.println("Connected to MQTT broker!");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" Retrying in 2 seconds...");
        }
    }
}

void loop() {
    // 4095 maximum reading, sensor not in soil
    // 3000+ to dry
    // 2000-3000 dry especially for moisture loving crops
    // 1000-2000 ideal for most plants
    // 0-1000 what are you doing, putting the sensor in watter? To wet for most plants

    reconnectMQTT(); 
    client.loop();
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0); // Read temperature
    int moistureValue = analogRead(MOISTURE_PIN);

    Serial.printf("Soil Moisture Level: %d\n", moistureValue);
    Serial.printf("Temperature: %.2f °C\n", tempC);

    String payload = "{ \"temperature\": " + String(tempC) + ", \"moisture\": " + String(moistureValue) + " }";
    client.publish(mqttTopic, payload.c_str());
    Serial.println("MQTT message sent!");
  delay(2000);
}
