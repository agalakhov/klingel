
/*
   Autor1: Michael Springwald
   Autor2: Hauke Holz

   Datum: Dienstag der 26.09.2017

   Für die Türklingel im KTT.
   Sendet bei einem Tastendruck ein Serielles Kommando an den Master:
   10,BD

   Um die LED'S vom Master aus zu Steuern,
   Jeweils ein Wert zwischen 0 und 255:
   10,AL,R,G,B 

   Wenn jemand Klinkelt, Bestätiung das der Wemos richtig Arbeitet
   10,ROK

   Wenn jemand oben die, Tür Öffnet kommt ein
   10,ROK2

   Der Slave und der Master Arbeiten über RS485.
*/


#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

#define cmd_AL         2 // Wenn der Status gesetzt wird
#define cmd_ROK        3 // Resiver OK
#define cmd_ROK2       4 // Wenn jemand durch dir Tür oben geht.

#define PIN 6 // für die WS2812B

const int buttonPin = 2;
const int ledPin = 13;
int ledState = HIGH;
bool buttonState;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

unsigned long previousMillis_2 = 0;
boolean IDOK = false;


int index = 0;
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


#define SSerialRX        10
#define SSerialTX        11

#define SSerialTxControl 3
#define RS485Transmit    HIGH
#define RS485Receive     LOW

bool lock = false;
bool first = true;
uint32_t OldColor = 0;
bool UpdateStatus = true;
bool SaveStatus = false;
int byteReceived;
int byteSend;
//int PIN = 3;
int totalLEDs = 11;
int ledFadeTime = 1;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(11, PIN, NEO_GRB + NEO_KHZ800);
SoftwareSerial RS485Serial(SSerialRX, SSerialTX); // RX, TX

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
//  RS485Serial.flush();
} // SendRS485

void RS485Init() {
  pinMode(SSerialTxControl, OUTPUT);
  digitalWrite(SSerialTxControl, RS485Receive);
  RS485Serial.begin(9600);
  delay(20);
  RS485Serial.flush();
  delay(1000); 
} // RS485Init


void rgbSerialOut(uint32_t colorValue) {
  uint8_t red = (colorValue & 0x00FF0000) >> 16;
  uint8_t green = (colorValue & 0x0000FF00) >> 8;
  uint8_t blue = (colorValue & 0x000000FF);
  Serial.print(red);
  Serial.print(",");
  Serial.print(green);
  Serial.print(",");
  Serial.print(blue);
  Serial.println();
}

void rgbFadeOut(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait) {
  lock = true;

  for (uint8_t b = 255; b > 0; b--) {
    for (uint8_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, red * b / 255, green * b / 255, blue * b / 255);
    }
    strip.show();
    delay(wait);
  };
  lock = false;
};

void rgbFadeIn(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait) {
  lock = true;
  for (uint8_t b = 0; b < 255; b++) {
    for (uint8_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, red * b / 255, green * b / 255, blue * b / 255);
    }

    strip.show();
    delay(wait);
  };
  lock = false;
};

void allLeds(uint32_t c, int FadeTimeIn = 0, int FadeOut = 0) {
  // lock = true;
  // if (!first)
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
  //  lock = false;
} // allLeds

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println("KTT_Klingel_26_09_2017");
  Serial.println("Von Michael Springwald und Hauke");
  Serial.println("Version 1.0");
  
  RS485Init();

  strip.begin();
  strip.setBrightness(25);
  strip.show();

  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(buttonPin, HIGH);
  buttonState=HIGH;
  lastButtonState=HIGH;

  allLeds( strip.Color(127, 0, 127), 0, 0);
  lock = false;
  SaveStatus=false;
  first=true;
    
  delay(1000);
  for (int x = 0; x < 50; x++) Data[x] = '\0';
  index=0;
}

void loop() {
  if (RS485Serial.available())  {
    while (RS485Serial.available())  {
      ch = RS485Serial.read();
      if (ch == ',' || ch == 10) {
        //Serial.println(Data);
        isEnde = ch == 10;
        if (isEnde) Data[z] = '\0';
        Command();
        if (isEnde) {
          index = 0;
          P1 = -1; P2 = -1; P3 = -1;
        }
        else
          index = index + 1;

        for (int x = 0; x < z; x++) Data[x] = '\0';
        z = 0;
      }
      else {
        if (ch > 30) {
          Data[z] = ch;
          z = z + 1;
        }
      }
    }
  }

  if (first == true) {
    first = false;
    SendRS485("10,SPS");  
    delay(1000);  
    return ;
  }    
  
  if (SaveStatus == true) {
    bool reading = digitalRead(buttonPin);
    if (reading != lastButtonState) {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;

        if (buttonState == HIGH) {
          SendRS485("10,BD");
        }
      }
    }
    lastButtonState = reading;
  }

} // loop


int GetCommandIndex(const char* aCommand) {
  int Temp = -1;
  if (strcmp(aCommand, "AL") == 0) {
    Temp = cmd_AL;
    return Temp;
  }
  if (strcmp(aCommand, "ROK") == 0) {
    Temp = cmd_ROK;
    return Temp;
  }
  if (strcmp(aCommand, "ROK2") == 0) {
    Temp = cmd_ROK2;
    return Temp;
  }
} // GetCommandIndex

//Debug();
void SetParameter () {
  switch (index)  {
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
  if (index == 0) {
    if (atoi(Data) == 10)
      IDOK = true;
    else
      IDOK = false;
  }
  else {
    if (IDOK) {
      if (index == 1)
        CommandIndex = GetCommandIndex(Data);

      switch (CommandIndex) {
        case cmd_AL: {
            SetParameter();
            if (index == 4 ) {
              lock = true;
             // Serial.println("cmd_AL");
              allLeds(strip.Color(P1, P2, P3));
              OldColor = strip.getPixelColor(1);
              lock = false;
              SaveStatus = true;
            }
            break;
          } // cmd_AL

        case cmd_ROK: {
          if (SaveStatus) {
            lock = true;

            //Serial.print(
            allLeds(0);
            for (int i = 0; i < 2; i++) {
              rgbFadeOut(127, 0, 0, ledFadeTime);
              rgbFadeIn(0, 127, 0, ledFadeTime);
            }
//            rgbSerialOut(OldColor);            
            allLeds(OldColor);
            RS485Serial.flush();
            lock = false;
          }
          else
            Serial.println("Nano: SaveStatus == false");
          break;
        } // cmd_ROK

        case cmd_ROK2: {
            if (SaveStatus) {
              //  lock = true;

              allLeds(0);
              for (int i = 0; i < 2; i++) {
                rgbFadeOut(127, 127, 0, ledFadeTime);
                rgbFadeIn(0, 127, 127, ledFadeTime);
              }
              
//              rgbSerialOut(OldColor);
              //  if (SaveStatus)
              allLeds(OldColor);
              RS485Serial.flush();
              //  lock = false;
            }
            break;
          } // cmd_ROK2
      } // switch
    }
  }
} // Command
