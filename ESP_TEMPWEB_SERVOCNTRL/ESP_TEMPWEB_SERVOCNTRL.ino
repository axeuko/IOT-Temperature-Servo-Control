#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Arduino.h"
#include <WiFi.h>
#include <Servo.h>
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <NTPClient.h>
#include "WiFiUdp.h"


Servo objServo;

static const int ServoGPIO = 13;


AsyncWebServer server(80);
AsyncEventSource events("/events");

JSONVar readings;

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

const char* ssid = "TLOT";
const char* password = "janjanyove";

const char* PARAM_1 = "servoangle";
const String inputParam1 = "servoangle";



RTC_DATA_ATTR int sensor_data = 0; // variable that will store the number/amount of data being collected (E.G 0,1,2) and will be saved in the RTC memory of the ESP32
String Data; // This will hold the data that will be logged onto the microSD
String Date;
String dday;
String TTime;

String header;

// Data wire is plugged into digital pin 2 on the Arduino
const int ONE_WIRE_BUS = 15;

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

float temperature;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); // These two lines request for the time stamp


String getSensorReadings(){
 sensors.requestTemperatures();
  readings["Celsius"] = String(sensors.getTempCByIndex(0));
 // readings["Farenhiet"] = String(sensors.getTempFByIndex(0)); 

  String jsonString = JSON.stringify(readings);
  return jsonString;
  }
  
void initSPIFFS() {
  if (!SPIFFS.begin()){
      Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
    Serial.println("SPIFFS mounted successfully");
  }
}
  
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  delay(10000);
}

void notFound(AsyncWebServerRequest *request){
  request->send(404,"text/plain", "Not Found");
}

void setup()
{
 
  Serial.begin(115200);
  objServo.attach(ServoGPIO);
  initWiFi();

timeClient.begin();// this initializes the NTP
timeClient.setTimeOffset(3600); //Time zone offset gotten by your time zont UTC*60*60

 if(!SD.begin()) { //THE NEXT FEW LINES BEGIN THE SD SPI CARD COMMUNICATION 
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin()) {
    Serial.println("SD card initialization failed!");
    return;    
  }

  File file = SD.open("/temperature_readings.txt"); //we open the text file on the sd card and since it does not yet exist then we create 
  if(!file) {
    Serial.println("File does not exist");
    Serial.println("Creating file...");
    writeFile(SD, "/temperature_readings.txt", "Reading Number, Date, Hour, Temperature \r\n");
  }
  else {
    Serial.println("File exists");  
  }
  file.close();

  sensors.begin();  // Start up the library
  initSPIFFS();


 
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");

  

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  // Start server
  server.begin();

}

void loop()
{ 
  obtainReadings();
  obtain_Date_Time();
  data_logging();

  sensor_data++;

  
if ((millis() - lastTime) > timerDelay) {
    // Send Events to the client with the Sensor Readings Every 10 seconds
    events.send("ping",NULL,millis());
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
    lastTime = millis();
  }


server.on("/get", HTTP_GET,  [](AsyncWebServerRequest *request){
  String inputMessage1;
  if (request ->getParam(PARAM_1)->value() !=""){
    inputMessage1 = request -> getParam(PARAM_1)->value();
    int servoAngle = inputMessage1.toInt();
    objServo.write(servoAngle);
  }
  else{
    inputMessage1 = "none";
  }
  request->send(SPIFFS, "/index.html", "text/html");
});
}


void obtainReadings(){
  sensors.requestTemperatures(); 
  temperature = sensors.getTempCByIndex(0); 
  Serial.print("Temperature: ");
  Serial.println(temperature);
}

void obtain_Date_Time() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  Date = timeClient.getFormattedDate();
  Serial.println(Date);
  
  int split = Date.indexOf("T");
  dday = Date.substring(0, split);
  Serial.println(dday);
  TTime = Date.substring(split+1, Date.length()-1);
  Serial.println(TTime);
}

void data_logging() {
  Data = String(sensor_data) + "," + String(dday) + "," + String(TTime) + "," + 
                String(temperature) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(Data);
  appendFile(SD, "/temperature_readings.txt", Data.c_str());
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void readFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path, FILE_READ);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.print("Read from file: ");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}
