/**************************************************************
 *
 * For this example, you need to install PubSubClient library:
 *   https://github.com/knolleary/pubsubclient/releases/latest
 *   or from http://librarymanager/all#PubSubClient
 *
 * TinyGSM Getting Started guide:
 *   http://tiny.cc/tiny-gsm-readme
 *
 **************************************************************
 * Use Mosquitto client tools to work with MQTT
 *   Ubuntu/Linux: sudo apt-get install mosquitto-clients
 *
 * Subscribe for messages:
 *   mosquitto_sub -h mqtt.igeek.top -p 8005 -t AutoShare/autoStatus -q 1
 * Toggle led:
 *   mosquitto_pub -h mqtt.igeek.top -p 8005 -t AutoShare/lock -q 1 -m "toggle"
 *
 * You can use Node-RED for wiring together MQTT-enabled devices
 *   https://nodered.org/
 * Also, take a look at these additional Node-RED modules:
 *   node-red-contrib-blynk-websockets
 *   node-red-dashboard
 *
 **************************************************************
 * PIN 8,9 to sim800l
 * PIN 10,11 to gps
 * PIN 13 to output for mqtt "toggle"
 **************************************************************/

// Select your modem:
#define TINY_GSM_MODEM_SIM800
//#define TINY_GSM_MODEM_SIM900
//#define TINY_GSM_MODEM_A6
//#define TINY_GSM_MODEM_M590

//#include <StreamDebugger.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>


// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[]  = "CMNET";
const char user[] = "";
const char pass[] = "";

// Use Hardware Serial on Mega, Leonardo, Micro
//#define SerialAT Serial1
//#define SerialMon Serial

// or Software Serial on Uno, Nano
#define simRx 8
#define simTx 9
#define gpsRx 10
#define gpsTx 11
SoftwareSerial SerialAT = SoftwareSerial(simRx, simTx); // RX, TX
SoftwareSerial SerialGPS = SoftwareSerial(gpsRx, gpsTx); // RX, TX
#define SP_RATE 9600

//StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

const char* broker = "mqtt.igeek.top";

const char* topicLock = "AutoShare/lock";
const char* topicInit = "AutoShare/onService";
const char* topicAutoStatus = "AutoShare/autoStatus";

#define LOCK_PIN 13
int lockStatus = LOW;

long lastReconnectAttempt = 0;

void setup() {
  pinMode(LOCK_PIN, OUTPUT);

  // Set console baud rate
  Serial.begin(SP_RATE);
  delay(10);

  // Set GPS module baud rate
  pinMode(gpsRx, INPUT);
  pinMode(gpsTx, OUTPUT);
  SerialGPS.begin(SP_RATE);
  delay(3000);

  // Set GSM module baud rate
  pinMode(simRx, INPUT);
  pinMode(simTx, OUTPUT);
  SerialAT.begin(SP_RATE);
  delay(3000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();
  delay(10000);

  // Unlock your SIM card with a PIN
  //modem.simUnlock("1234");

  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println(" fail");
    //while (true);
  }
  Serial.println(" OK");
  delay(10000);

  Serial.print("Connecting to ");
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    //while (true);
  }
  Serial.println(" OK");
  delay(10000);

  // MQTT Broker setup
  mqtt.setServer(broker, 8005);
  mqtt.setCallback(mqttCallback);
}

boolean mqttConnect() {
  Serial.print("Connecting to ");
  Serial.print(broker);
  if (!mqtt.connect("AutoClient")) {
    Serial.println(" fail");
    return false;
  }
  Serial.println(" OK");
  mqtt.publish(topicInit, "Auto started service.");
  mqtt.subscribe(topicLock);
  return mqtt.connected();
}

void loop() {

  if (mqtt.connected()) {
    mqtt.loop();
  } else {
    // Reconnect every 10 seconds
    unsigned long t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();

  // Only proceed if incoming message's topic matches
  if (String(topic) == topicLock) {
    lockStatus = !lockStatus;
    digitalWrite(LOCK_PIN, lockStatus);
    mqtt.publish(topicAutoStatus, lockStatus ? "1" : "0");
  }
}

