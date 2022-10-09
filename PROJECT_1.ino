#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

//uint64_t uS_TO_S_FACTOR = 1000000; // this is the code to set the time for which esp will sleep and conversion factor to micro seconds 
//uint64_t TIME_TO_SLEEP = 300;

const char* ssid = "TLOT";
const char* password = "janjanyove";

RTC_DATA_ATTR int sensor_data = 0; // variable that will store the number/amount of data being collected (E.G 0,1,2) and will be saved in the RTC memory of the ESP32
String Data; // This will hold the data that will be logged onto the microSD
String Date;
String dday;
String TTime;

const int ONE_WIRE_BUS = 15;

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);


//#define sensor_data_pin 4
//OneWire oneWire(sensor_data_pin);
//DallasTemperature sensors(&oneWire);

float temperature; // this will holdsensor reading from DS18B20

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); // These two lines request for the time stamp


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

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

  File file = SD.open("/temperature_readings.txt"); //we open the text file on the sd card and since it does not yet extis then we create 
  if(!file) {
    Serial.println("File does not exist");
    Serial.println("Creating file...");
    writeFile(SD, "/temperature_readings.txt", "Reading Number, Date, Hour, Temperature \r\n");
  }
  else {
    Serial.println("File exists");  
  }
  file.close();

  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); //This is the code for timer wake up 
  sensors.begin(); 
  obtainReadings();
  obtain_Date_Time();
  data_logging();

  sensor_data++;
  
  Serial.println("Sensor data logged successfully! Going to sleep");
  //esp_deep_sleep_start(); //function to put esp to sleep  
}

void loop() {

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
  day = Date.substring(0, split);
  Serial.println(day);
  Time = Date.substring(split+1, Date.length()-1);
  Serial.println(Time);
}

void data_logging() {
  Data = String(sensor_data) + "," + String(day) + "," + String(Time) + "," + 
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
