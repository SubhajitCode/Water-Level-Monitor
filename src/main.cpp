#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <movingAvg.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_I2CDevice.h>
#include <credential.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1
movingAvg ultraSonic(5);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
FirebaseData dbIn;
FirebaseData dbOut;
// Global Variables Here
int tankFullLevel = 7; //cm
int tankEmptyLevel = 78;
const int trigPin = D7;
const int echoPin = D8;
const int relayPin = D5;
int waterLevel;
String path;
String childPath[3] = {"/motorStat", "/tankEmptyLevel", "/tankFullLevel"};

//Function Declairation
void streamTimeoutCallback(bool timeout);
void relayOn();
void relayOff();
int ultraSonicRead();
int calcPercent(int level);
void drawPercentbar(int x, int y, int width, int height, int progress);
void displayOled(int waterLevel);
void streamCallback(MultiPathStreamData data);

//Function Body
void streamTimeoutCallback(bool timeout)
{
  if (timeout)
  {
    //Stream timeout occurred
    Serial.println("Stream timeout, resume streaming...");
  }
}
void streamCallback(MultiPathStreamData data)
{
  if (data.get(childPath[0]))
  {
    if (data.value != "true")
    {
      relayOn();
      relayOff();
    }
  }
  if (data.get(childPath[1]))
  {
    tankEmptyLevel = data.value.toInt();
  }
  if (data.get(childPath[2]))
  {
    tankFullLevel = data.value.toInt();
  }
}

void relayOn()
{
  //Motor Cut-Off
  Serial.println("On");
  digitalWrite(relayPin, LOW);
  if (!Firebase.setBool(dbOut, path + "/motorStat", false))
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + dbOut.errorReason());
  }
  delay(3000);
}

void relayOff()
{
  Serial.println("Off");
  digitalWrite(relayPin, HIGH);
  if (!Firebase.setBool(dbOut, path + "/motorStat", true))
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + dbOut.errorReason());
  }
}

int ultraSonicRead()
{
  // ULTRASONIC *****************************************
  int avg;
  long duration;
  int distance;
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(100);
  digitalWrite(trigPin, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  // Calculating the distance
  distance = duration * 0.034 / 2;
  avg = ultraSonic.reading(distance);
  return avg;
}

int calcPercent(int level)
{
  int height = tankEmptyLevel - level;
  int maxHeight = tankEmptyLevel - tankFullLevel;
  return (height * 100 / maxHeight);
}

void drawPercentbar(int x, int y, int width, int height, int progress)
{
  progress = progress > 100 ? 100 : progress;
  progress = progress < 0 ? 0 : progress;
  float bar = ((float)(width - 4) / 100) * progress;
  display.drawRect(x, y, width, height, WHITE);
  display.fillRect(x + 2, y + 2, bar, height - 4, WHITE);
  // Display progress text
  if (height >= 15)
  {
    display.setCursor((width / 2) - 3, y + 5);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    if (progress >= 50)
      display.setTextColor(BLACK, WHITE); // 'inverted' text
    display.print(progress);
    display.print("%");
  }
}

void displayOled(int waterLevel)
{
  int val = calcPercent(waterLevel);
  display.clearDisplay();
  char buffer[50];
  sprintf(buffer, "%03d", val);
  display.drawRect(0, 0, 128, 45, WHITE);
  display.setTextSize(4);
  display.setTextColor(WHITE);
  display.setCursor(18, 9);
  display.print(buffer);
  display.println('%');
  drawPercentbar(0, 50, 128, 10, val > 100 ? 100 : val);
  display.display();
  Serial.print("value ");
  Serial.println(waterLevel);
  Serial.print("percent ");
  Serial.println(val);
  delay(100);
  if (!Firebase.setInt(dbOut, path + "/waterLevel", waterLevel))
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + dbOut.errorReason());
  }
}

//setup and loop

void setup()
{
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3D (for the 128x64)
  //display.setFont(&FreeSerif9pt7b);
  Serial.begin(9600);
  Serial.println("Communication Started \n\n");
  ultraSonic.begin();
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH); //  Sets the motorPin as an Output
  // connect to firebase
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setReadTimeout(dbIn, 1000 * 5);
  Firebase.setwriteSizeLimit(dbOut, "tiny");

  path = "/Test";
  size_t childPathSize = 3;
  Firebase.setMultiPathStreamCallback(dbIn, streamCallback, streamTimeoutCallback);
  if (!Firebase.beginMultiPathStream(dbIn, path, childPath, childPathSize))
  {
    Serial.println("Could not begin stream");
    Serial.println("REASON: " + dbIn.errorReason());
    Serial.println();
  }
}
void loop()
{
  waterLevel = ultraSonicRead();
  // Prints the distance on the Serial Monitor
  displayOled(waterLevel);
  if (waterLevel < tankFullLevel)
  {
    relayOn();
    relayOff();
    while (waterLevel < tankFullLevel)
    {
      waterLevel = ultraSonicRead();
      displayOled(waterLevel);
    }
  }
  delay(100);
}
