// import libraries
#include "EspMQTTClient.h"
#include "Adafruit_NeoPixel.h"
#include "time.h"

// storing all the pins used by the ESP-32 in an array to be iterated and initialised in setup
int OutputPins[] = {
  2, 13, 12, 4, 16
};

int InputPins[] = {
  0, 32, 33, 17
};

//Initialise all variables
//time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200;

struct tm timeInfo;
char timeHour[3];

//initialising the NeoPixel
Adafruit_NeoPixel np(5, 12);

//initialise MQTT Client data and connect to WiFi
EspMQTTClient client(
  //WiFi/Hotspot name
  "Turtle",
  //WiFi/Hotspot password
  "TurtlesAreGood()",
  // MQTT server URL
  "mqtt.a9i.sg",
  //username
  "RkQ3Fr",
  //password
  "RkQ3Fr",
  //Client name/identifier
  "ESP-32",
  //set port (cannot be string)
  1883
);

//Stats of plant
// watering system
int Moisture = 100;
int tMoisture = 0;

//Height system
float Distance = 0.0;
float PlantHeight;
float PotHeight = 5.0;
float standardHeight = 27.47;

//Light system
int LightColor[3] = {
  255,
  255,
  255
};

int Colors[][3] = {
  {255, 0, 0},
  {255, 128, 0},
  {255, 255, 0},
  {128, 255, 0},
  {0, 255, 0},
  {0, 255, 128},
  {0, 255, 255},
  {0, 128, 255},
  {0, 0, 255},
  {128, 0, 255},
  {255, 0, 255},
  {225, 0, 128},
};

bool StandBy = false;

String LightColorS = "";

bool alwaysOnLights = true;
int tLightLevel = 10;
int LightLevel = 100.0;


void GetTime(){
    getLocalTime(&timeInfo);

  if(!getLocalTime(&timeInfo)){
    Lights(true);
    Serial.println("Failed to obtain time");
    return;
  }
}

void Lights(bool on){
  if(on){
    np.fill(np.Color(LightColor[0], LightColor[1], LightColor[2]));
    np.show();
  }
  else{
    np.clear();
    np.show();
  }
}
// used to check if lights should be on which is controlled by the light level
void checkLight(){

  if(LightLevel < tLightLevel){
    Lights(true);
  }
  else{
    Lights(false);
  }
}

void StandByLights(){
  static int colorID = 0;

  for(int i = 0; i < 3; i++){
    LightColor[i] = Colors[colorID][i];
  }

  colorID += 1;

  if(colorID > 11){
    colorID = 0;
  }

  Lights(true);
}

void GetDist() {
  digitalWrite(16, LOW);
  delayMicroseconds(2);

  digitalWrite(16, HIGH);
  delayMicroseconds(10);
  digitalWrite(16, LOW);

  long duration = pulseIn(17, HIGH);
  
  Distance = ((duration * 0.0343) / 2);
}

void Debug(){

  String DebugItems[] = {
    "Time: " + String(timeInfo.tm_hour) + " " + String(timeInfo.tm_min) + " " + String(timeInfo.tm_sec),
    "Moisture Sensor: " + String(analogRead(32)), 
    "Light Sensor: " + String(analogRead(33)),
    "Moisture: " + String(Moisture),
    "Light Level: " + String(LightLevel),
    "Distance: " + String(Distance),
    "Light colour: " + String(LightColor[0]) + String(LightColor[1]) + String(LightColor[2]),
    "Plant Height: " + String(PlantHeight),
    "raw color data: " + LightColorS,
    "TMoisture: " + String(tMoisture),

  };

  String publish = "";
  for(String x: DebugItems){
    publish = publish + x + "\n";
  }

  client.publish("RkQ3Fr/debug", publish);
}

void UpdateVariables(){
  Moisture = ReturnPercent(analogRead(32));
  LightLevel = ReturnPercent(analogRead(33));
  GetDist();

  client.publish("RkQ3Fr/moisture", String(Moisture));
  client.publish("RkQ3Fr/lightLvl", String(LightLevel));
  client.publish("RkQ3Fr/currentHeight", String(PlantHeight));
}

//Declare a local array of all the interval indecies coreesponding to all the preMiliss indecies
int intervals[] = {
  1000,
  2500,
};
unsigned long prevMilliss[] = {
  0,
  0,
};
bool Interval(int interval) {

  //Getting the index for current interval
  int i = 0;
  for(int x: intervals){
    if(x == interval){
      break;
    }
  }

  //checking for interval
  unsigned long prevMillis = prevMilliss[i];
  unsigned long currentMillis = millis();

  if (currentMillis - prevMillis >= interval) {
    prevMilliss[i] = currentMillis;
    return true;
  }

  return false;
}

float ReturnPercent(int value){
  return 100 - map(value, 0, 4096, 0, 100);
}

//all the IoT code
void onConnectionEstablished() {
  //light system                                                                                   
  //always on lights
  client.subscribe("RkQ3Fr/light", [](const String& msg) {
    if(msg == "on"){
      alwaysOnLights = true;
    } 
    else{
      alwaysOnLights = false;
    }

  });
  //target light level
  client.subscribe("RkQ3Fr/tLightLvl", [](const String& msg) {
    tLightLevel = msg.toInt();
  });
  //light level
  client.subscribe("RkQ3Fr/lightLvl", [](const String& msg) {
    ;
  });
  //light colour
  client.subscribe("RkQ3Fr/lightColor", [](const String& msg) {
    //get get colors and map each color channel to an index in the LightColor array
    LightColorS = msg;
    int i = 0;
    String value = "";

    for (int x = 0; x < msg.length(); x++) {
      if(msg[x] == ','){
        LightColor[i] = value.toInt();
        value = "";
        i++;
      }
      else{
        value += msg[x];
      }
    }

    if(!value.isEmpty()){
      LightColor[i] = value.toInt();
    }

  });
  //Watering system
  //target moisture
  client.subscribe("RkQ3Fr/tMoisture", [](const String& msg) {
    tMoisture = msg.toInt();
  });
  //moisture
  client.subscribe("RkQ3Fr/moisture", [](const String& msg) {
    ;
  });
  //height system
  //pot height
  client.subscribe("RkQ3Fr/potHeight", [](const String& msg) {
    if(msg == "r"){
      PotHeight = standardHeight - Distance;
    }
  });
  //plant Height
  client.subscribe("RkQ3Fr/currentHeight", [](const String& msg) {
    ;
  });
  //plant height
  client.subscribe("RkQ3Fr/updateGrowth", [](const String& msg) {
    if(msg == "r"){
      PlantHeight = standardHeight - Distance - PotHeight;
    }
  });
  //Stanby
  client.subscribe("RkQ3Fr/standby", [](const String& msg) {
    if(msg == "on"){
      StandBy = true;
    } 
    else{
      StandBy = false;
    }
  });
  //debug
  client.subscribe("RkQ3Fr/debug", [](const String& msg) {
    ;
  });
}

void setup() {
  // initialise all the pins
  for(int element: OutputPins){
    pinMode(element, OUTPUT);
  }

  for(int element: InputPins){
    pinMode(element, INPUT);
  }

  //initialise NeoPixel
  np.begin();

  //initialise time
  configTime(gmtOffset_sec, 3600, ntpServer);
  strftime(timeHour,3, "%H", &timeInfo);

  Serial.begin(115200);

}

//mainloop
void loop() {

  client.loop();

  if(StandBy){
    if(Interval(2500)){
      StandByLights();
    }
  }
  else{
      //check for moisture and water
    if(Moisture < tMoisture){
      digitalWrite(13, LOW);
    }
    else{
      digitalWrite(13, HIGH);
    }

    //Control lights
    if(alwaysOnLights == false && timeInfo.tm_hour < 19){
      checkLight();
    }
    else{
      checkLight();
    }

    //Every 1 Second
    if(Interval(1000)){
      //GetTime
      GetTime();
      //Do I have to explain this?!
      UpdateVariables();
      //update the MQTT topic with the debug variables
      Debug();
    }
  }
}

