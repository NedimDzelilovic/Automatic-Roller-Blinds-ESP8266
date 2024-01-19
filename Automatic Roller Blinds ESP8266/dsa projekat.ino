#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "stepper.h"

#define WIFI_AP "FET"
#define WIFI_PASSWORD "tuzlatuzla"

#define TOKEN "bzEiTYqJtFWMWtxAt2Ky"

char thingsboardServer[] = "demo.thingsboard.io";

WiFiClient wifiClient;

PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;
int current_angle = 0;
int new_angle_percent = 0;
int new_angle_steps = 0;
int current_difference;
int led_pin = 14;  //D5
bool led_state[] = { false };

void setup() {
  Serial.begin(115200);

  pinMode(led_pin, OUTPUT);

  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);

  delay(10);
  InitWiFi();
  client.setServer(thingsboardServer, 1883);
  client.setCallback(on_message);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
}

// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy(json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  // method for setting new position of roller blinds
  if (methodName.equals("setAngle")) {
    new_angle_percent = data["params"];
    new_angle_steps = SCALE_FACTOR * new_angle_percent;
    Serial.print("Zeljena vrijednost pozicije je: ");
    Serial.println(new_angle_percent);

    if (new_angle_steps > current_angle) 
    {
      current_difference = new_angle_steps - current_angle;
      for (int j = 0; j < current_difference; j++) {
        for (int i = 0; i <= 3; i++) {
          stepper_motor(i);
          delay(2);
        }
      }
      current_angle = new_angle_steps;
    }

    else if (new_angle_steps < current_angle) 
    {
      current_difference = current_angle - new_angle_steps;
      for (int j = 0; j < current_difference; j++) {
        for (int i = 3; i >= 0; i--) {
          stepper_motor(i);
          delay(2);
        }
      }
      current_angle = new_angle_steps;
    }
    delay(200);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_angle_status().c_str());
    client.publish("v1/devices/me/attributes", get_angle_status().c_str());
  }

  // method for setting new state of led diode
  else if (methodName.equals("set_LED_Status")) {
    // Update GPIO status and reply
    set_led_status(data["params"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_led_status().c_str());
    client.publish("v1/devices/me/attributes", get_led_status().c_str());
  }
}

// set new state od led diode
void set_led_status(boolean state) {
  digitalWrite(led_pin, state);
  led_state[0] = state;
  Serial.print("Stanje diode je: ");
  Serial.println(led_state[0]);
}

// get current state od led diode
String get_led_status() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["Stanje LED diode"] = led_state[0] ? true : false;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

// get current position of roller blinds
String get_angle_status() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["Trenutna pozicija"] = current_angle/SCALE_FACTOR;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get angle status: ");
  Serial.println(strPayload);
  return strPayload;
}

void InitWiFi() {
  Serial.println("Connecting to AP ...");

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    status = WiFi.status();
    if (status != WL_CONNECTED) {
      WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
    }
    Serial.print("Connecting to ThingsBoard node ...");
    if (client.connect("ESP8266 Device", TOKEN, NULL)) {
      Serial.println("[DONE]");
      // Subscribing to receive RPC requests
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      Serial.print("[FAILED] [ rc = ");
      Serial.print(client.state());
      Serial.println(" : retrying in 5 seconds]");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}