/*
####################################################################
## Simple Mqtt Client Code for connecting to Eugenio.io IoT platform
####################################################################
##
##################################################
## Author: Artur Sarlo
## Copyright:
## License:
## Version: 0.0.0
## Maintainer: Artur Sarlo
## Email: artur.sarlo@la.logicalis.com
## Status: demo
##################################################
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <MQTT.h>
#include <time.h>
#include "secrets.h"
#ifdef USE_ARDUINO_JSON_LIB
#include <ArduinoJson.h>
#endif

// General connectivity and security
BearSSL::WiFiClientSecure net;
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);

// Mqtt client
PubSubClient client(net);

// Time related
time_t now;
time_t nowish = CURRENT_TIME_APROXIMATION_AS_EPOCH_SECONDS;

// Measurementsi related
unsigned long last_mueasurement_publish_in_millis = 0;

/**
 * @brief Configures time using NTP. This function is blocking while change is not applied.
 * 
 */
void NTPConnect(void){
  Serial.print("Setting up time using NTP");

  // Configure the NTP servers
  configTime(UTC_TIME_ZONE_BRAZIL_SECONDS, DST_SECONDS, NTP_SERVER_0, NTP_SERVER_1);
  
  // Wait for the current time to be updated
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(ctime(&now));
}

/**
 * @brief Prints a mqtt error code in a human readable fashion.
 * 
 * @param MQTTErr Mqtt error code
 */
void pubSubErr(int8_t MQTTErr){
  if (MQTTErr == MQTT_CONNECTION_TIMEOUT)
    Serial.print("Connection tiemout");
  else if (MQTTErr == MQTT_CONNECTION_LOST)
    Serial.print("Connection lost");
  else if (MQTTErr == MQTT_CONNECT_FAILED)
    Serial.print("Connect failed");
  else if (MQTTErr == MQTT_DISCONNECTED)
    Serial.print("Disconnected");
  else if (MQTTErr == MQTT_CONNECTED)
    Serial.print("Connected");
  else if (MQTTErr == MQTT_CONNECT_BAD_PROTOCOL)
    Serial.print("Connect bad protocol");
  else if (MQTTErr == MQTT_CONNECT_BAD_CLIENT_ID)
    Serial.print("Connect bad Client-ID");
  else if (MQTTErr == MQTT_CONNECT_UNAVAILABLE)
    Serial.print("Connect unavailable");
  else if (MQTTErr == MQTT_CONNECT_BAD_CREDENTIALS)
    Serial.print("Connect bad credentials");
  else if (MQTTErr == MQTT_CONNECT_UNAUTHORIZED)
    Serial.print("Connect unauthorized");
}

/**
 * @brief Connect the mqtt client to the broker.
 * 
 * @param nonBlocking Indicates if the connection process will retry until succeed or not
 */
void connectToMqtt(bool nonBlocking = false)
{
  Serial.print("MQTT connecting ");
  while (!client.connected())
  {
    if (client.connect(DEVICE_ID, MQTT_USER, NULL))
    {
      Serial.println("connected!");
      if (!client.subscribe(MQTT_SUB_CLOUD_TO_DEVICE_TOPIC))
        pubSubErr(client.state());
      if (!client.subscribe(MQTT_SUB_INVOKE_TOPIC))
        pubSubErr(client.state());

      
    }
    else
    {
      Serial.print("SSL Error Code: ");
      Serial.println(net.getLastSSLError());
      Serial.print("failed, reason -> ");
      pubSubErr(client.state());
      if (!nonBlocking)
      {
        Serial.println(" < try again in 5 seconds");
        delay(5000);
      }
      else
      {
        Serial.println(" <");
      }
    }
    if (nonBlocking)
      break;
  }
}

/**
 * @brief Connects to the wifi network with serial prints.
 * 
 * @param init_str Initial string to be printed befere connecting
 */
void connectToWiFiVerbosely(String init_str){
  Serial.print(init_str);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("ok!");
}

/**
 * @brief Connects to the wifi network withou serial prints.
 * 
 */
void connectToWiFiSilently(){
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
  }
}

/**
 * @brief Check wifi connection and restablish connection if disconnected. Connects the mqtt client to the broker.
 * 
 */
void checkWiFiThenMQTT(void){
  connectToWiFiVerbosely("Checking WiFi");
  connectToMqtt();
}

/**
 * @brief Publish a mqtt message in a specified topic with a specified payload.
 * 
 * @param topic Mqtt message topic
 * @param payload Mqtt message payload
 */
void mqtt_publish_with_topic(String topic, String payload=String("")){
  if (client.connected())
    if(client.publish(topic.c_str(), payload.c_str()))
      pubSubErr(client.state());
}

/**
 * @brief Publishes a mqtt message at the default event topic for this device
 * 
 * @param payload Mqtt message payload
 */
void mqtt_publish(String payload){
  if (client.connected())
    if (client.publish(MQTT_PUB_DEVICE_TO_CLOUD_TOPIC, payload.c_str()))
      pubSubErr(client.state());
}

/**
 * @brief Method used to publish the result of a previously received command
 * 
 * @param status_code REST like status code
 * @param rid Request Id used to identify witch request this response relates with
 * @param payload Mqtt message payload
 */
void invoke_reply(String status_code, String rid, String payload=String("")){
  String topic = String(MQTT_PUB_INVOKE_REPLY_TOPIC);
  topic.replace("{status_code}", status_code.c_str());
  topic.replace("{rid}", rid.c_str());

  mqtt_publish_with_topic(topic, payload);
}

/**
 * @brief This is where the job is done.
 * This method will parse and execute different commands based on the content of the received mqtt message.
 * 
 * @param topic Mqtt message topic
 * @param payload Mqtt message payload
 * @param length Mqtt message payload lenght in bytes
 */
void mqtt_client_callback(char *topic, byte *payload, unsigned int length){
  String parsed_payload = String((char *) payload).substring(0, length);
  Serial.printf("Received [%s]: %s\r\n", topic, parsed_payload.c_str());
  String parsed_topic = String(topic);
  parsed_topic.replace(MQTT_SUB_INVOKE_BASE_TOPIC, "");
  uint8_t index_of_slash = parsed_topic.indexOf('/');
  uint8_t index_of_equals = parsed_topic.indexOf('=');
  String method = parsed_topic.substring(0, index_of_slash);
  String rid = parsed_topic.substring(index_of_equals + 1, -1);
  Serial.printf("This is the method: %s\r\n", method.c_str());
  Serial.printf("This is the rid: %s\r\n", rid.c_str());
  
  String status_code;
  String resp_payload;
  if (method == "ping"){
    status_code = "200";
  }
  else if (method == "ping_verbose"){
    status_code = "200";
    resp_payload = "{"
      "\"response_ping_key_0\": \"response_ping_value_0\","
      "\"response_ping_key_1\": \"response_ping_value_1\""
    "}";
  }
  else {
    status_code = "400";
    resp_payload = "{"
      "\"error\": \"command not supported by this device\""
    "}";
  }
  invoke_reply(status_code, rid, resp_payload);
}

/**
 * @brief Publish test measurements. 
 * The used schema is test_schema. You can either use ArduinoJson lib or not.
 * Just define USE_ARDUINO_JSON_LIB at the secrets.h header file with you want so.
 * 
 */
void publish_measurements(void){
  #ifdef USE_ARDUINO_JSON_LIB
  DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(2) + 100);
  JsonObject root = jsonBuffer.to<JsonObject>();
  JsonObject payload = root.createNestedObject("payload");
  root["schema"] = "test_schema";
  payload["field0"] = "value0";
  payload["field1"] = "value1";
  char shadow[measureJson(root) + 1];
  serializeJson(root, shadow, sizeof(shadow));
  Serial.printf("Publishing measurements with payload: %s\r\n", shadow);
  String mqtt_message_payload = String(shadow);
  mqtt_publish(mqtt_message_payload);
  #else
  String payload = String(
    "{"
    "\"schema\": \"schema_name\","
    "\"payload\": {"
      "\"field0\": \"field0_value\","
      "\"field1\": \"field1_value\""
      "}"
    "}"
  );
  payload.replace("schema_name", "test_schema");
  payload.replace("field0_value", "value0");
  payload.replace("field1_value", "value1");
  Serial.printf("Publishing measurements with payload: %s\r\n", payload.c_str());
  mqtt_publish(payload);
  #endif
}

/**
 * @brief Setup routine of the device. 
 * Is called before loop routine.
 * 
 */
void setup(){
  Serial.begin(115200);
  delay(5000);
  Serial.println();
  Serial.println();
  WiFi.hostname(DEVICE_ID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  connectToWiFiVerbosely(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  NTPConnect();

  net.setInsecure();
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqtt_client_callback);

  connectToMqtt();
}

/**
 * @brief Loop routine of the device. 
 * This is called repeatedly after the setup routine. This repetition continues forever.
 * 
 */
void loop(){
  now = time(nullptr);
  if (!client.connected())
  {
    checkWiFiThenMQTT();
  }
  else
  {
    client.loop();
    if (millis() - last_mueasurement_publish_in_millis > 5000)
    {
      last_mueasurement_publish_in_millis = millis();
      publish_measurements();
    }
  }
}
