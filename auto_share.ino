/**************************************************************

   For this example, you need to install PubSubClient library:
     https://github.com/knolleary/pubsubclient/releases/latest
     or from http://librarymanager/all#PubSubClient

   TinyGSM Getting Started guide:
     http://tiny.cc/tiny-gsm-readme

 **************************************************************
   Use Mosquitto client tools to work with MQTT
     Ubuntu/Linux: sudo apt-get install mosquitto-clients

   Subscribe for messages:
     mosquitto_sub -h mqtt.igeek.top -p 8005 -t AutoShare/autoStatus -q 1
   Toggle led:
     mosquitto_pub -h mqtt.igeek.top -p 8005 -t AutoShare/lock -q 1 -m "toggle"


 **************************************************************
   PIN 8,9 to sim800l
   PIN 10,11 to gps
   PIN 13 to output for mqtt "toggle"
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
//#include <MsTimer2.h>

char auto_status[64];

// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[]  = "CMNET";
const char user[] = "";
const char pass[] = "";

// GPS info
char nmeaSentence[68];
String latitude;    //纬度
String longitude;   //经度
String lndSpeed;    //速度
String gpsTime;     //UTC时间，本初子午线经度0度的时间，和北京时间差8小时
String beiJingTime;   //北京时间

// OBD info
char obd2Sentence[100];


// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1
#define SerialGPS Serial2
#define SerialOBD Serial3
//#define SerialMon Serial

// or Software Serial on Uno, Nano
//#define simRx 8
//#define simTx 9
//#define gpsRx 10
//#define gpsTx 11
//SoftwareSerial SerialAT = SoftwareSerial(simRx, simTx); // RX, TX
//SoftwareSerial SerialGPS = SoftwareSerial(gpsRx, gpsTx); // RX, TX
#define SP_RATE 115200

//StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

const char* broker = "mqtt.igeek.top";

const char* topicLock = "AutoShare/lock";
const char* topicInit = "AutoShare/onService";
const char* topicAutoStatus = "AutoShare/autoStatus";
const char* topicDebug = "AutoShare/debug";

#define LOCK_PIN 13
int lockStatus = LOW;

long lastReconnectAttempt = 0;

void setup() {
  pinMode(LOCK_PIN, OUTPUT);

  // Set console baud rate
  Serial.begin(SP_RATE);
  delay(10);


  // Set GPS module baud rate
  //  pinMode(gpsRx, INPUT);
  //  pinMode(gpsTx, OUTPUT);
  SerialGPS.begin(9600);
  delay(3000);

  // Set GSM module baud rate
  //  pinMode(simRx, INPUT);
  //  pinMode(simTx, OUTPUT);
  SerialAT.begin(SP_RATE);
  delay(3000);

  // Set OBD module baud rate
  SerialOBD.begin(SP_RATE);
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

  //MsTimer2 set
//  MsTimer2::set(10000,update_auto_status);
//  MsTimer2::start();

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

// *********************************************
// * GPS model
// *********************************************

String getBeiJingTime(String s)
{
  int hour = s.substring(0, 2).toInt();
  int minute = s.substring(2, 4).toInt();
  int second = s.substring(4, 6).toInt();

  hour += 8;

  if (hour > 24)
    hour -= 24;
  s = String(hour) + ":" + String(minute) + ":" + String(second);
  return s;
}

//Parse GPRMC NMEA sentence data from String
//String must be GPRMC or no data will be parsed
//Return Latitude
String parseGprmcLat(String s)
{
  int pLoc = 0; //paramater location pointer
  int lEndLoc = 0; //lat parameter end location
  int dEndLoc = 0; //direction parameter end location
  String lat;
  /*make sure that we are parsing the GPRMC string.
    Found that setting s.substring(0,5) == "GPRMC" caused a FALSE.
    There seemed to be a 0x0D and 0x00 character at the end. */
  if (s.substring(0, 4) == "GPRM")
  {
    //Serial.println(s);
    for (int i = 0; i < 5; i++)
    {
      if (i < 3)
      {
        pLoc = s.indexOf(',', pLoc + 1);
        /*Serial.print("i < 3, pLoc: ");
          Serial.print(pLoc);
          Serial.print(", ");
          Serial.println(i);*/
      }
      if (i == 3)
      {
        lEndLoc = s.indexOf(',', pLoc + 1);
        lat = s.substring(pLoc + 1, lEndLoc);
        /*Serial.print("i = 3, pLoc: ");
          Serial.println(pLoc);
          Serial.print("lEndLoc: ");
          Serial.println(lEndLoc);*/
      }
      else
      {
        dEndLoc = s.indexOf(',', lEndLoc + 1);
        lat = lat + " " + s.substring(lEndLoc + 1, dEndLoc);
        /*Serial.print("i = 4, lEndLoc: ");
          Serial.println(lEndLoc);
          Serial.print("dEndLoc: ");
          Serial.println(dEndLoc);*/
      }
    }
    return lat;
  }
}

//Parse GPRMC NMEA sentence data from String
//String must be GPRMC or no data will be parsed
//Return Longitude
String parseGprmcLon(String s)
{
  int pLoc = 0; //paramater location pointer
  int lEndLoc = 0; //lat parameter end location
  int dEndLoc = 0; //direction parameter end location
  String lon;

  /*make sure that we are parsing the GPRMC string.
    Found that setting s.substring(0,5) == "GPRMC" caused a FALSE.
    There seemed to be a 0x0D and 0x00 character at the end. */
  if (s.substring(0, 4) == "GPRM")
  {
    //Serial.println(s);
    for (int i = 0; i < 7; i++)
    {
      if (i < 5)
      {
        pLoc = s.indexOf(',', pLoc + 1);
        /*Serial.print("i < 3, pLoc: ");
          Serial.print(pLoc);
          Serial.print(", ");
          Serial.println(i);*/
      }
      if (i == 5)
      {
        lEndLoc = s.indexOf(',', pLoc + 1);
        lon = s.substring(pLoc + 1, lEndLoc);
        /*Serial.print("i = 3, pLoc: ");
          Serial.println(pLoc);
          Serial.print("lEndLoc: ");
          Serial.println(lEndLoc);*/
      }
      else
      {
        dEndLoc = s.indexOf(',', lEndLoc + 1);
        lon = lon + " " + s.substring(lEndLoc + 1, dEndLoc);
        /*Serial.print("i = 4, lEndLoc: ");
          Serial.println(lEndLoc);
          Serial.print("dEndLoc: ");
          Serial.println(dEndLoc);*/
      }
    }
    return lon;
  }
}

//Parse GPRMC NMEA sentence data from String
//String must be GPRMC or no data will be parsed
//Return Longitude
String parseGprmcSpeed(String s)
{
  int pLoc = 0; //paramater location pointer
  int lEndLoc = 0; //lat parameter end location
  int dEndLoc = 0; //direction parameter end location
  String lndSpeed;

  /*make sure that we are parsing the GPRMC string.
    Found that setting s.substring(0,5) == "GPRMC" caused a FALSE.
    There seemed to be a 0x0D and 0x00 character at the end. */
  if (s.substring(0, 4) == "GPRM")
  {
    //Serial.println(s);
    for (int i = 0; i < 8; i++)
    {
      if (i < 7)
      {
        pLoc = s.indexOf(',', pLoc + 1);
        /*Serial.print("i < 8, pLoc: ");
          Serial.print(pLoc);
          Serial.print(", ");
          Serial.println(i);*/
      }
      else
      {
        lEndLoc = s.indexOf(',', pLoc + 1);
        lndSpeed = s.substring(pLoc + 1, lEndLoc);
        /*Serial.print("i = 8, pLoc: ");
          Serial.println(pLoc);
          Serial.print("lEndLoc: ");
          Serial.println(lEndLoc);*/
      }
    }
    return lndSpeed;
  }
}


//Parse GPRMC NMEA sentence data from String
//String must be GPRMC or no data will be parsed
//Return Longitude
String parseGprmcTime(String s)
{
  int pLoc = 0; //paramater location pointer
  int lEndLoc = 0; //lat parameter end location
  int dEndLoc = 0; //direction parameter end location
  String gpsTime;

  /*make sure that we are parsing the GPRMC string.
    Found that setting s.substring(0,5) == "GPRMC" caused a FALSE.
    There seemed to be a 0x0D and 0x00 character at the end. */
  if (s.substring(0, 4) == "GPRM")
  {
    //Serial.println(s);
    for (int i = 0; i < 2; i++)
    {
      if (i < 1)
      {
        pLoc = s.indexOf(',', pLoc + 1);
        /*Serial.print("i < 8, pLoc: ");
          Serial.print(pLoc);
          Serial.print(", ");
          Serial.println(i);*/
      }
      else
      {
        lEndLoc = s.indexOf(',', pLoc + 1);
        gpsTime = s.substring(pLoc + 1, lEndLoc);
        /*Serial.print("i = 8, pLoc: ");
          Serial.println(pLoc);
          Serial.print("lEndLoc: ");
          Serial.println(lEndLoc);*/
      }
    }
    return gpsTime;
  }
}

// Turn char[] array into String object
String charToString(char *c)
{

  String val = "";

  for (int i = 0; i <= sizeof(c); i++)
  {
    val = val + c[i];
  }

  return val;
}


void getGPS() {
  for (unsigned long start = millis(); millis() - start < 1000;)  //一秒钟内不停扫描GPS信息
  {
    while (SerialGPS.available()) //串口获取到数据开始解析
    {
      char c = SerialGPS.read();  //读取一个字节获取的数据
      switch (c)        //判断该字节的值
      {
        case '$':         //若是$，则说明是一帧数据的开始
          SerialGPS.readBytesUntil('*', nmeaSentence, 67);    //读取接下来的数据，存放在nmeaSentence字符数组中，最大存放67个字节
          Serial.println(nmeaSentence);
          latitude = parseGprmcLat(nmeaSentence); //获取纬度值
          longitude = parseGprmcLon(nmeaSentence);//获取经度值
          lndSpeed = parseGprmcSpeed(nmeaSentence);//获取速度值
          gpsTime = parseGprmcTime(nmeaSentence);//获取GPS时间
          String res = String(latitude + ",");
          res += longitude;
          res += ",";
          res.toCharArray(auto_status, 64);
          mqtt.publish(topicAutoStatus, auto_status);
      }
    }
  }
}

void update_auto_status() {
  //gps read
  for (unsigned long start = millis(); millis() - start < 1000;)  //一秒钟内不停扫描GPS信息
  {
    while (SerialGPS.available()) //串口获取到数据开始解析
    {
      char c = SerialGPS.read();  //读取一个字节获取的数据
      switch (c)        //判断该字节的值
      {
        case '$':         //若是$，则说明是一帧数据的开始
          SerialGPS.readBytesUntil('*', nmeaSentence, 67);    //读取接下来的数据，存放在nmeaSentence字符数组中，最大存放67个字节
          //Serial.println(nmeaSentence);
          latitude = parseGprmcLat(nmeaSentence); //获取纬度值
          longitude = parseGprmcLon(nmeaSentence);//获取经度值
          lndSpeed = parseGprmcSpeed(nmeaSentence);//获取速度值
          gpsTime = parseGprmcTime(nmeaSentence);//获取GPS时间
      }
    }
  }

  String obd_temp;
  //obd read
  for (unsigned long start = millis(); millis() - start < 1000;)  //一秒钟内不停扫描GPS信息
  {
    while (SerialOBD.available()) //串口获取到数据开始解析
    {
      char c = SerialOBD.read();  //读取一个字节获取的数据
      switch (c)        //判断该字节的值
      {
        case '$':         //若是$，则说明是一帧数据的开始
          SerialOBD.readBytesUntil('$', obd2Sentence, 100);    //读取接下来的数据，存放在nmeaSentence字符数组中，最大存放67个字节
          obd_temp = String(obd2Sentence);
          //Serial.println(obd_temp.substring(8));
          obd_temp = obd_temp.substring(8);
      }
    }
  }

  //data update
  String res = "3001,";
  res += latitude;
  res += ",";
  res += longitude;
  res += ",";
  res += obd_temp;
  res.toCharArray(auto_status, 100);
  mqtt.publish(topicAutoStatus, auto_status);
}
//the end of GPS

// *******************************************
// * the start on GPS
// *******************************************
void getOBD() {
  for (unsigned long start = millis(); millis() - start < 1000;)  //一秒钟内不停扫描GPS信息
  {
    while (SerialOBD.available()) //串口获取到数据开始解析
    {
      char c = SerialOBD.read();  //读取一个字节获取的数据
      switch (c)        //判断该字节的值
      {
        case '$':         //若是$，则说明是一帧数据的开始
          SerialOBD.readBytesUntil('$', obd2Sentence, 100);    //读取接下来的数据，存放在nmeaSentence字符数组中，最大存放67个字节
          Serial.println(obd2Sentence);
      }
    }
  }
}
// the end of OBD

void loop() {

  //SerialGPS.listen();
  //Serial.println("GPS windown");
  // For one second we parse GPS data and report some key values
  //getGPS();
  //getOBD();
  update_auto_status();

  //SerialAT.listen();
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

