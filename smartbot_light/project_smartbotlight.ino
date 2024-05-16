#include "string.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Arduino_JSON.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "DHT.h"
#include "config.h"

// mode: setup home WiFi
const char* apSsid = AP_WIFI_SSID;
const char* apPassword = AP_WIFI_PASS;
const int port = 80;
WiFiServer server(port);
IPAddress local_IP(192, 168, 4, 1);

// mode: connect home WiFi config
char ssid[64] = WIFI_SSID;
char password[30] = WIFI_PASS;

//device config
const char *deviceId = DEVICE_ID;
char spaceId[30] = SPACE_ID;

const char *mqtt_broker = MQTT_HOST;
const char *mqtt_username = deviceId;
const char *mqtt_password = MQTT_PASS;
const int mqtt_port = MQTT_PORT;
char *cmdTopic;
char *statusTopic;
char *scheduleTopic;
const char *pingTopic = "ping/iot_devices";
const char *registerTopic = "system/iot_device/register";

// Analog pin the photoresistor is connected to
const int photoResistorPin = A0;
unsigned long previousMillis = 0;
const int DHTPIN = 2;
#define DHTTYPE DHT11 // DHT 11

int isNotRegister = 1;

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void connectToWiFi();
void connectToMQTTBroker();

bool connectToHomeWifi(const char* ssid, const char* password, int attempt);
void setupHomeWifi();
void setupAfterConnectWifi();
bool needSetupHomeWiFi(){
  return strcmp(ssid, "") == 0;
}

void configWiFiAP() {
  Serial.println("Setting up access point...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(local_IP, local_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsid, apPassword);
  Serial.print("Access point created with SSID: ");
  Serial.println(apSsid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
}

void setup() {

    // Set software serial baud to 115200;
    Serial.begin(115200);

    // setup WiFi AP
    configWiFiAP();

    server.begin();
    Serial.println("Server started");

    String cmdTopicS = String(spaceId) + "/commands/" + String(deviceId);
    cmdTopic = (char *)malloc(cmdTopicS.length() + 1);
    strcpy(cmdTopic, cmdTopicS.c_str());

    //Set the maximum MQTT packet size
    mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
    //connecting to a mqtt broker
    mqttClient.setServer(mqtt_broker, mqtt_port);

    dht.begin();
}

void loop() {
  if (needSetupHomeWiFi()) {
    setupHomeWiFi();
    delay(1);

    return;
  }
  
  if (!mqttClient.connected()) {
      connectToMQTTBroker();
  }

  if (isNotRegister) {
    registerDevice();
    isNotRegister = 0;
    Serial.println("registered device successfully");
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

void setupHomeWiFi() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected");
    while (client.connected()) {
      if (!client.available()) { continue; }

      String request = client.readStringUntil('{');
      Serial.print("request: "); Serial.println(request);

      if (request.indexOf("OPTIONS") != -1) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println("Access-Control-Allow-Origin: *");  // Optional: Allow cross-origin requests
        client.println("Access-Control-Allow-Headers: authorization, x-client-info, apikey, content-type");
        client.println("Access-Control-Request-Method: *");
        client.println();
        client.println("ok");

        break;
      }

      if (request.indexOf("/ping") != -1 && request.indexOf("GET") != -1) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println("Access-Control-Allow-Origin: *");  // Optional: Allow cross-origin requests
        client.println();
        client.println("OK");

        break;
      }

      if (request.indexOf("/config") == -1 || request.indexOf("POST") == -1) {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println("Access-Control-Allow-Origin: *");
        client.println();
        client.println("");

        break;
      }

      String payload = "{";
      while (client.available()) {
        payload += client.readStringUntil('&');
      }
      payload.trim();

      JSONVar body = JSON.parse(payload);

      Serial.print("Payload: "); Serial.println(payload);

      String homeSSID = body["ssid"];
      String homePassword = body["password"];
      String spaceIdStr = body["spaceId"];

      strcpy(spaceId, spaceIdStr.c_str());

      Serial.print("Received SSID: ");
      Serial.println(homeSSID);
      Serial.print("Received password: ");
      Serial.println(homePassword);

      // Connect to home WiFi
      if (connectToHomeWiFi(homeSSID.c_str(), homePassword.c_str(), false, 30)) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println("Access-Control-Allow-Origin: *");  // Optional: Allow cross-origin requests
        client.println();
        client.println("");

        strcpy(ssid, homeSSID.c_str());
        strcpy(password, homePassword.c_str());

        Serial.println("Setup home WiFi successfully. Exit setup WiFi state");
      } else {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println("Access-Control-Allow-Origin: *");
        client.println();
        client.println("");
      }

      break;
    }

    client.stop();
    Serial.println("Client stopped");
  }
}

bool connectToHomeWiFi(const char* ssid, const char* password, bool onlySTA, int attempt) {
  if (onlySTA) {
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to home WiFi: ");
  Serial.println(ssid);
  int connectionRetries = 0;
  while (WiFi.status() != WL_CONNECTED && (attempt == 0 || connectionRetries < attempt)) {
    delay(500);
    Serial.print(".");
    connectionRetries++;
  }

  return WiFi.status() == WL_CONNECTED;
}

void registerDevice() {
  JSONVar msg;
  msg["iotDeviceId"] = DEVICE_ID;
  msg["iotDeviceType"] = DEVICE_TYPE;
  msg["version"] = DEVICE_VERSION;
  msg["firmwareVersion"] = DEVICE_FIRMWARE_VERSION;
  msg["chipId"] = "0";
  msg["macAddress"] = "0";
  msg["state"] = "running";
  msg["connectStatus"] = "online";
  msg["spaceId"] = spaceId;

  String payload = JSON.stringify(msg);

  mqttClient.publish(registerTopic, payload.c_str());
}
