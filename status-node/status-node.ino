/*
  Autor1: Manuel ...
  Autor2: Michael Springwald
  Autor3: Hauke Holz

  Verarbeitet zwei Eregeinisse die über MQTT kommen:
  Einmal den Space Status und einmal ob jemand oben die Tür Öffnen.
  
  Später soll das durch ein Button und eine LED ersetzt werden.
  Die LED soll Blau Leuchten, wenn jemand unten Geklingkelt hat und 
  wieder ausgehen, wenn jemand oben den Button drückt.
*/

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

#include "settings.h"
#include "credentials.h"

#define SSerialRX        D6  //Serial Receive pin
#define SSerialTX        D4  //Serial Transmit pin
#define SSerialTxControl D3   //RS485 Direction control

#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define cmd_BD               2 // Button Status
#define cmd_currSpaceStatus  3 // Aktueller SpaceStatus
#define InterupPin  D5
//#define cmd_ROK        3 // Resiver OK

boolean IDOK = false;
int ParamIndex = 0;
int z = 0;
boolean isEnde = false;

char Data[50]; // Funktion
char ch;
int CommandIndex = -1;
int P1 = -1;
int P2 = -1;
int P3 = -1;
int P4 = -1;
int P5 = -1;

int byteReceived;
int byteSend;
bool First = true;
byte CurrStatus = 0;

SoftwareSerial RS485Serial(SSerialRX, SSerialTX); // RX, TX

ESP8266WiFiMulti wifiMulti;
WiFiClientSecure wclient;
PubSubClient client(mqttHost, mqttPort, wclient);

enum status {
  opened,
  closed,
  closing,
  unknown
};
status space_status = unknown;

void SendRS485(char* Buffer) {
  RS485Serial.flush(); 
  delay(20);  
  digitalWrite(SSerialTxControl, RS485Transmit);
  RS485Serial.println();
  RS485Serial.print(Buffer);  
  RS485Serial.println();  
  delay(10);
  digitalWrite(SSerialTxControl, RS485Receive);
  delay(20); 
  //RS485Serial.flush();    
} // SendRS485

void RS485Init() {
  pinMode(SSerialTxControl, OUTPUT);
  digitalWrite(SSerialTxControl, RS485Receive);
  RS485Serial.begin(9600);
  delay(20);
  RS485Serial.flush();
  delay(1000); 
} // SendRS485


void SendSpaceStaus(byte aValue = 0) {
  switch(aValue) {
    case 1: { // Open
      SendRS485("10,AL,0,255,0");
      CurrStatus=1;       
      break;     
    }

    case 2: { // Geschlossen
      SendRS485("10,AL,255,0,0");   
      CurrStatus=2;  
      break;     
    }    
  } // switch
} //  SendSpaceStaus

void open_space() {
  space_status = opened;
  Serial.println("Space is open."); 
  SendSpaceStaus(1); 
} // open_space

void close_space() {
  space_status = closed;
  Serial.println("Space closes.");
  SendSpaceStaus(2);
} // close_space

void closing_space_soon() {
  space_status = closing;
  Serial.println("Space is closing.");
} // closing_space_soon

void unknown_status() {
  space_status = unknown;
  Serial.println("Status is unknown.");
  if (!First) {   
    SendRS485("10,AL,0,0,255");
  }
} // unknown_status

// callback for subscribed topics
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print(topic);
  Serial.print(" => ");

  String status = String((char *)payload);
  status = status.substring(0, length);
  Serial.println(status);

  // Ignore anything but the space state
  if (String((char *)topic) == statusTopic) {
    Serial.println("Topic matches!");
    // Change space status, if necessary
    if (status == "open" || status == "open+") {
      if ( space_status != opened) {
        open_space();
      }
    } else if (status == "none") {
      if (space_status != closed) {
        close_space();
      }
    } else if (status == "closing") {
      if (space_status != closing) {
        closing_space_soon();
      }
    } else {
      if (space_status != unknown)
        unknown_status();
    }
  }

  if (String((char *)topic) == mainDoorTopic) {
    if (status == "0") {
      if (!First) {
        SendRS485("10,ROK2");  
      }
    }
  }
  
} // callback

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("status-node");
  Serial.println("Von Manuel, Michael Springwald und Hauke");
  Serial.println("Version 1.0");  
  
  RS485Init();

  // Initialize client
  client.setServer(mqttHost, mqttPort);
  client.setCallback(callback);

  wifiMulti.addAP(ssid0, ssid0Password);
  wifiMulti.addAP(ssid1, ssid1Password);
 // pinMode(InterupPin,OUTPUT);
 // digitalWrite(InterupPin,HIGH);
  
  // Wait till connected.
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi connection failed, retrying.");
    delay(500);
  }
 // ArduinoOTA.setHostname(signalHostname);
  //ArduinoOTA.setPassword(otaPassword);
  //ArduinoOTA.begin();
  for (int x = 0; x < 50; x++) Data[x] = '\0';
  ParamIndex=0; z=0;
} // setup

void loop() {
  if (wifiMulti.run() != WL_CONNECTED) {
    unknown_status();
    Serial.println("WiFi not connected!");
    delay(1000);
    return;
  }

 // ArduinoOTA.handle();

  if (!client.connected()) {
    unknown_status();
    Serial.println("Connecting to MQTT server...");
    if (client.connect(signalHostname)) {
      Serial.println("Connected to MQTT server, checking cert...");
      if (wclient.verify(mqttFingerprint, mqttHost)) {
        Serial.println("Certificate matches!");
      } else {
        Serial.println("Certificate doesn't match!");
        unknown_status();
        delay(60000);
        return;
      }
      client.subscribe(statusTopic);
      client.loop();

      client.subscribe(mainDoorTopic);
      client.loop();

    } else {
      Serial.println("Could not connect to MQTT server.");
      delay(2000);
    }
  }

  if (RS485Serial.available() ) {
    while (RS485Serial.available())  {
      ch = RS485Serial.read();
      if (ch == ',' || ch == 10) {
        isEnde = ch == 10;
        // Zum Debuggen 
        /*Serial.print("Receive Daten:");
        Serial.print("\"");
        Serial.print(Data);
        Serial.print("\"");        
        Serial.print(",");        
        Serial.print(ParamIndex);
        Serial.print(",");   
        Serial.print(z);        
        Serial.println();*/
        
        if (isEnde) 
          Data[z]='\0';

          
        Command();
        
        if (isEnde) {
          ParamIndex = 0;
          P1 = -1; P2 = -1; P3 = -1;        
        }
        else
          ParamIndex = ParamIndex + 1;

        for (int x = 0; x < z; x++) Data[x] = '\0';
        z = 0;
      }
      else {
        Data[z] = ch;
        z = z + 1;
      }
    }
  }
  
  if (client.connected()) {
    client.loop();
  }
  First=false;
} // loop

int GetCommandIndex(const char* aCommand) {
  int Temp = -1;
  if (strncmp(aCommand, "BD",2) == 0) {
    Temp = cmd_BD;
    return Temp;
  }
  
  if (strncmp(aCommand, "SPS",3) == 0) {
    Temp = cmd_currSpaceStatus;
    return Temp;
  }  
} // GetCommandIndex

//Debug();
void SetParameter () {
  switch (ParamIndex)  {
    case 2: { // X
        P1 = atoi(Data);
        break;
      }

    case 3: { // Y
        P2 = atoi(Data);
        break;
      }

    case 4: {
        P3 = atoi(Data);
        break;
      }

    case 5: {
        P4 = atoi(Data);
        break;
      }

    case 6: {
        P5 = atoi(Data);
        break;
      }
  }
} // SetParameter

void Command() {
  if (ParamIndex == 0) {
    if (atoi(Data) == 10)
      IDOK = true;
    else
      IDOK = false;
  }
  else {
    if (IDOK) {
      if (ParamIndex == 1)
        CommandIndex = GetCommandIndex(Data);
      switch (CommandIndex) {
        case cmd_BD: {                 
          //Serial.println("WemosD1: Klingel wurde gedrueckt");   
          digitalWrite(InterupPin,LOW);           
          delay(1000);
          digitalWrite(InterupPin,HIGH);
          delay(1000);                    
          SendRS485("10,ROK");  

                

          break;
        } // cmd_BD

        case cmd_currSpaceStatus: {
  //        Serial.println("cmd_currSpaceStatus");
          SendSpaceStaus(CurrStatus);
          break;
        }
      } // switch
    }
  }
} // Command
