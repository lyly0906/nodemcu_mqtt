#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#define WIFI_AP ""   //ssid
#define WIFI_PASSWORD ""  // 密码

#define TOKEN ""

#define GPIO0 0
#define GPIO2 2

#define GPIO0_PIN 3
#define GPIO2_PIN 5

#define relay1 12 //继电器连接在8266的GPIO12上

SoftwareSerial DLSerial(5, 4); // 电流表数据通讯TTL RX, TX  5, 4
char shuchu[8]={1,3,0,0,0,2,196,11};
String comdata = "";
float x = 0;
float y = 0;
int stringOne = 0;
int stringTwo = 0;

char thingsboardServer[] = "192.168.2.117";

WiFiClient wifiClient;

PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;

// We assume that all GPIOs are LOW
boolean gpioState[] = {false, false};

void setup() {
  pinMode(relay1,OUTPUT);
  digitalWrite(relay1, LOW);
  Serial.begin(115200);
  DLSerial.begin(115200);
  // Set output mode for all GPIO pins
  pinMode(GPIO0, OUTPUT);
  pinMode(GPIO2, OUTPUT);
  delay(10);
  InitWiFi();
  client.setServer( thingsboardServer, 1883 );
  client.setCallback(on_message);
}

void loop() {
  if ( !client.connected() ) {
    reconnect();
  }

  client.loop();
}

// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  //Serial.print("Topic: ");
  //Serial.println(topic);
  //Serial.print("Message: ");
  //Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);
  //Serial.println(String((const char*)data["params"]["pin"]));

  if (methodName.equals("getGpioStatus")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } else if (methodName.equals("setGpioStatus")) {
    // Update GPIO status and reply
          String responseTopic = String(topic);
          responseTopic.replace("request", "response");
          client.publish(responseTopic.c_str(), get_gpio_status1().c_str());
          digitalWrite(relay1, HIGH);
          delay(2000);
          //const char* xxx = (const char*)data["params"]["pin"];
          DLSerial.write(&shuchu[0],8);
          delay(200);
          while(DLSerial.available()){
            comdata += char(DLSerial.read());
            delay(2);
          }
       
          Serial.println(comdata);          
          DLSerial.flush();
          
          if(comdata.length() > 0)             //如果comdata接收到卡号，则读出卡号
          {
              x = 0;
              y = 0;            
              for(int i=0;i<comdata.length();i++){
                  if(i == 3){
                         stringOne = String(comdata[i],DEC).toInt();  
                         x += stringOne * 256;
                    }
           
                   if(i == 4){
                       stringTwo = String(comdata[i],DEC).toInt();  
                       x += stringTwo;
                  }
                  stringOne = 0;
                  stringTwo = 0;
                  if(i == 5){
                         stringOne = String(comdata[i],DEC).toInt();  
                         y += stringOne * 256;
                    }
           
                   if(i == 6){
                       stringTwo = String(comdata[i],DEC).toInt();  
                       y += stringTwo;
                  }
              }
              x = x / 10;
              y = y / 10;
              if(x < 100 && y < 100){
                Serial.println(x);
                Serial.println(y);
              }  

              client.publish("v1/devices/me/telemetry", get_gpio_status1().c_str());
          }
          comdata = "";       
  }
}

String get_gpio_status1() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["sd"] = String(x);
  data["wd"] = String(y);
  data[String(GPIO2_PIN)] = true;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

String get_gpio_status() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["hz"] = String(x / 10);
  data[String(GPIO2_PIN)] = gpioState[1] ? true : false;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

void set_gpio_status(int pin, boolean enabled) {
  if (pin == GPIO0_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO0, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[0] = enabled;
  } else if (pin == GPIO2_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO2, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[1] = enabled;
  }
}

void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

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
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to AP");
    }
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect("ESP8266 Device", TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
      // Subscribing to receive RPC requests
      client.subscribe("v1/devices/me/rpc/request/+");
      // Sending current GPIO status
      Serial.println("Sending current GPIO status ...");
      client.publish("v1/devices/me/attributes", get_gpio_status1().c_str());
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
