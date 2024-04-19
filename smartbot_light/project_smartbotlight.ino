#include "string.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include "DHT.h"

// WiFi
const char *ssid = "AnDK"; // Enter your WiFi name
const char *password = "123456789";  // Enter WiFi password

//device config
const char *deviceId = "01HVAJ1N63VW5BAA6SDDWNB88Q";
char spaceId[30] = "01HTQ9HBANAVTXG95AQWSWGXKK";

// MQTT Broker
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_HOST "mqtt.dev.isling.me"
#define MQTT_PORT 1883
#define MQTT_PASS ""
#define DEVICE_ID "01HVAJ1N63VW5BAA6SDDWNB88Q"

const char *mqtt_broker = MQTT_HOST;
char *cmdTopic;
const char *mqtt_username = deviceId;
const char *mqtt_password = MQTT_PASS;
const int mqtt_port = MQTT_PORT;
const char *pingTopic = "ping/iot_devices";

// Analog pin the photoresistor is connected to
const int photoResistorPin = A0;
unsigned long previousMillis = 0;
const int DHTPIN = 2;
#define DHTTYPE DHT11 // DHT 11

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void connectToWiFi();
void connectToMQTTBroker();

void setup() {

    // Set software serial baud to 115200;
    Serial.begin(115200);
    
    // connecting to a WiFi network
    connectToWiFi();

    String cmdTopicS = String(spaceId) + "/commands/" + String(deviceId);
    cmdTopic = (char *)malloc(cmdTopicS.length() + 1);
    strcpy(cmdTopic, cmdTopicS.c_str());

    //Set the maximum MQTT packet size
    mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    //connecting to a mqtt broker
    mqttClient.setServer(mqtt_broker, mqtt_port);
    connectToMQTTBroker();

    dht.begin();
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
        float temp = dht.readTemperature();
        float hum = dht.readHumidity();

        // // json serialize
        // DynamicJsonDocument data(512);
        // data["iotDeviceId"]=deviceId;
        // data["timestamp"]="4/13/2024 12:22:00";
        
        // JsonArray measurementsArray  = data.createNestedArray("measurements");

        // //Add temperature measurement
        // JsonObject temperatureMeasurement = measurementsArray.createNestedObject();
        // temperatureMeasurement["type"] = "temperature";
        // temperatureMeasurement["value"] = temp;

        // // Add humidity measurement
        // JsonObject humidityMeasurement = measurementsArray.createNestedObject();
        // humidityMeasurement["type"] = "humidity";
        // humidityMeasurement["value"] = hum;

        // // Add light measurement
        // JsonObject lightMeasurement = measurementsArray.createNestedObject();
        // lightMeasurement["type"] = "light";
        // lightMeasurement["value"] = lightValue;

        // // publish temperature and humidity
        // char json_string[512];
        // serializeJson(data, json_string);

        // Clear previous measurements

        JSONVar measurements;

        // Add measurements to the JSONVar object
        measurements["iotDeviceId"] = deviceId;
        measurements["timestamp"] = "2024-04-08T02:38:00Z";

        JSONVar measurementsArray;
        JSONVar temperatureMeasurement;
        temperatureMeasurement["type"] = "temperature";
        temperatureMeasurement["value"] = temp;
        measurementsArray[0]=temperatureMeasurement;

        JSONVar humidityMeasurement;
        humidityMeasurement["type"] = "humidity";
        humidityMeasurement["value"] = hum;
        measurementsArray[1]=humidityMeasurement;

        JSONVar lightMeasurement;
        lightMeasurement["type"] = "light";
        lightMeasurement["value"] = lightValue;
        measurementsArray[2]=lightMeasurement;

        measurements["measurements"] = measurementsArray;

        // Serialize the JSONVar object
        String json_string = JSON.stringify(measurements);

        Serial.println(json_string);
        boolean result = mqttClient.publish(cmdTopic, json_string.c_str());
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
          Serial.printf("Subscribe topic %s\n", cmdTopic);
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