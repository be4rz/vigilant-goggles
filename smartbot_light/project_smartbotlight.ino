#include "string.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi
const char *ssid = "Isling iPhone"; // Enter your WiFi name
const char *password = "tumotden8";  // Enter WiFi password

//device config
const char *deviceId = "01HTQ9844VMWWP70AD8HGJ48BY";
char spaceId[30] = "01HTQ9HBANAVTXG95AQWSWGXKK";

// MQTT Broker
const char *mqtt_broker = MQTT_HOST;
const char *cmdTopic = "light_sensor";
const char *mqtt_username = deviceId;
const char *mqtt_password = MQTT_PASS;
const int mqtt_port = MQTT_PORT;
const char *pingTopic = "ping/iot_devices";

// Analog pin the photoresistor is connected to
const int photoResistorPin = A0;
unsigned long previousMillis = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWiFi();
void connectToMQTTBroker();
void mqttCallback(char *topic, byte *payload, unsigned int length);

void setup() {
    // Set software serial baud to 115200;
    Serial.begin(115200);
    
    // connecting to a WiFi network
    connectToWiFi();

    String cmdTopicS = String(spaceId) + "/commands/" + String(deviceId);
    cmdTopic = (char *)malloc(cmdTopicS.length() + 1);
    strcpy(cmdTopic, cmdTopicS.c_str());

    //connecting to a mqtt broker
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(mqttCallback);
    connectToMQTTBroker();
}


void loop() {
    if (WiFi.status() != WL_CONNECTED) {
      connectToWiFi();
    }
  
    if (!mqttClient.connected()) {
        connectToMQTTBroker();
    }

    mqttClient.loop();
    unsigned long currentMillis = millis();

    // temperature and humidity data are publish every five second
    if (currentMillis - previousMillis >= 5000) {
        previousMillis = currentMillis;
        int lightValue = analogRead(photoResistorPin);
        char *lightStatus = lightValue > 100 ? "night" : "day";

        // json serialize
        DynamicJsonDocument data(256);
        data["value"] = lightValue;
        data["status"] = lightStatus;
        // publish temperature and humidity
        char json_string[256];
        serializeJson(data, json_string);

        Serial.println(json_string);
        client.publish(cmdTopic, json_string, false);
    }
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println("\nConnected to the WiFi network");
}

void connectToMQTTBroker() {
  while (!mqttClient.connected()) {
      String clientId = "switch-client-" + String(deviceId) + "-" + String(WiFi.macAddress());
      Serial.printf("Connecting to MQTT Broker as %s.....\n", clientId.c_str());
      if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
          Serial.println("Connected to MQTT broker");
          mqttClient.subscribe(cmdTopic);
          Serial.printf("Subscribe topic %s\n", topic);
          // Publish message upon successful connection
          mqttClient.publish(pingTopic, deviceId);
      } else {
          Serial.print("Failed to connect to MQTT broker, rc=");
          Serial.print(mqttClient.state());
          Serial.println(" try again in 5 seconds");
          delay(5000);
      }
  }
}