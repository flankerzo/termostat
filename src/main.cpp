#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoHA.h>
#include <analogWrite.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FanController.h>
#include <secrets.h>

#ifndef BROKER_ADDR
  #define BROKER_ADDR IPAddress(192, 168, 50, 51)
#endif
#ifndef BROKER_USERNAME
  #define BROKER_USERNAME "mqtt user name" // replace with your credentials
#endif
#ifndef BROKER_PASSWORD
  #define BROKER_PASSWORD "mqtt user password"
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID "name of your wifi"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "password of your wifi"
#endif
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 25
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
int numberOfDevices;             // Number of temperature devices found
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address



unsigned int iWiFiConection = 0;
float temp = 20;
float templast = 20;
unsigned long lastUpdateAt = 0;
unsigned int lastVallDuty = 0;
byte SpeedActual = 20;
byte lastSpeed = 20;
// Fan control settings
int fanPin1 = 26;     // PWM control for fan 1
int fanPin2 = 33;     // PWM control for fan 2 THIS sdddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd
int fanTach1pin = 35; // Tachometer input for fan 1
int fanTach2pin = 32; // Tachometer input for fan 2
#define SENSOR_THRESHOLD 1000
FanController fancontrol1(fanTach1pin, SENSOR_THRESHOLD, fanPin1);
FanController fancontrol2(fanTach2pin, SENSOR_THRESHOLD, fanPin2);
unsigned int rpms1=0;
unsigned int rpms2=0;
unsigned int lastrpms1=0;
unsigned int lastrpms2=0;

// PWM duty cycle limits
byte fanMinDutyCycle = 20;
byte fanMaxDutyCycle = 100;

byte FanSpeedHot = 50;
byte FanSpeedCold = 30;

byte fan1_speed_target = 25;
byte fan2_speed_target = 25;

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device, 8);

const char UniqueID[] = "AirEQ";
char FanHAID[] = "ventilation";
char Fan1HAID[] = "Fan1RPM";
char Fan2HAID[] = "Fan2RPM";
char TemperatureHAID[] = "Temperature";

HAFan FanHA(strncat(FanHAID, UniqueID, sizeof(UniqueID)), HAFan::SpeedsFeature);
HASensorNumber Fan1RPMHA(strncat(Fan1HAID, UniqueID, sizeof(UniqueID)), HASensorNumber::PrecisionP0);
HASensorNumber Fan2RPMHA(strncat(Fan2HAID, UniqueID, sizeof(UniqueID)), HASensorNumber::PrecisionP0);
HASensorNumber TemperatureHA(strncat(TemperatureHAID, UniqueID, sizeof(UniqueID)), HASensorNumber::PrecisionP1);

bool checkBound(float newValue, float prevValue, float maxDiff)
{
  return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void lookfordevices()
{
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i))
    {
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    }
    else
    {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }
}

// put function declarations here:
void onStateCommand(bool state, HAFan *sender)
{
  Serial.print("State: ");
  Serial.println(state);
  sender->setState(state); // report state back to the Home Assistant
}

void onSpeedCommand(uint16_t speed, HAFan *sender)
{
  Serial.print("Speed: ");
  SpeedActual = speed;
  Serial.println(speed);
  sender->setSpeed(speed); // report speed back to the Home Assistant
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting...");
  fancontrol1.begin();
  fancontrol2.begin();
  fancontrol1.setDutyCycle(20);
  fancontrol2.setDutyCycle(20);
  sensors.begin();
  lookfordevices();

  // connect to wifi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("connecting");
  delay(200); // waiting for the connection
  for (size_t i = 0; i < 20; i++)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(". ");
      delay(500); // waiting for the connection
    }
    else
    {
      Serial.println("Connected to the network");
      break;
    }
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wifi connection Failed, continuing");
  }
  

  byte mac[6];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));
  device.setName(UniqueID);
  device.setSoftwareVersion("1.0.0");
  device.setManufacturer("Flankerzo");
  device.setModel("model four");
  device.enableSharedAvailability();
  device.setAvailability(true);
  

  FanHA.setName("AirTempEqC");
  FanHA.setSpeedRangeMin(fanMinDutyCycle);
  FanHA.setSpeedRangeMax(fanMaxDutyCycle);
  FanHA.onStateCommand(onStateCommand);
  FanHA.onSpeedCommand(onSpeedCommand);
  FanHA.setSpeed(FanSpeedCold);
  FanHA.turnOn();
  FanHA.setAvailability(true);
  FanHA.isOnline();
  

  Fan1RPMHA.setName("Fan1");
  Fan1RPMHA.setUnitOfMeasurement("RPM");
  Fan1RPMHA.setIcon("mdi:fan");
  
  Fan2RPMHA.setName("Fan2");
  Fan2RPMHA.setUnitOfMeasurement("RPM");
  Fan2RPMHA.setIcon("mdi:fan");

  TemperatureHA.setName("Temp");
  TemperatureHA.setIcon("mdi:thermometer");
  TemperatureHA.setUnitOfMeasurement("C");

  mqtt.begin(BROKER_ADDR, BROKER_USERNAME, BROKER_PASSWORD);

  for (size_t i = 0; i < 20; i++)
  {
    if (mqtt.isConnected() == true)
    {
      Serial.print(". ");
      delay(500); // waiting for the connection
    }
    else
    {
      Serial.println("MQTT connected<");
      break;
    }
  }

  if (mqtt.isConnected() != true)
  {
    Serial.println("MQTT not connected, continuing");
  }
}

void loop()
{
  mqtt.loop();

  if ((millis() - lastUpdateAt) > 1000)
  {                                // 1000ms debounce time
    sensors.requestTemperatures(); // Send the command to get temperatures
    float tempAvg = 0;
    for (uint16_t i = 0; i < numberOfDevices; i++)
    {
      Serial.print("Temperature for the device ");
      Serial.print(i);
      Serial.print(" is ");
      Serial.println(sensors.getTempCByIndex(i));
      tempAvg = tempAvg + sensors.getTempCByIndex(i);
    }

    if (tempAvg == -127)
    {
      Serial.println("Temp Sensor FAIL");
      TemperatureHA.setAvailability(false);
    }
    else
    {
      TemperatureHA.setAvailability(true);
      temp = tempAvg;
      TemperatureHA.setValue(temp);
    }

    //if (checkBound(temp, lastVallDuty, 0.2))
   // {
      if (temp > 30)
      {
       // FanHA.setSpeed(FanSpeedHot);
        SpeedActual = FanSpeedHot;
        Serial.println(" zopnuto");
        fancontrol1.setDutyCycle(60); // nasavim striedu pre fantcontrol
      fancontrol2.setDutyCycle(60); // nasavim striedu pre fantcontrol
      
      }
      else
      {
       // FanHA.setSpeed(FanSpeedCold);
        SpeedActual = FanSpeedCold;
        Serial.println(" vypnuto");
        fancontrol1.setDutyCycle(40); // nasavim striedu pre fantcontrol
      fancontrol2.setDutyCycle(40); // nasavim striedu pre fantcontrol
      
      }

      lastVallDuty = temp;
   // }
    if (checkBound(SpeedActual, lastSpeed, 1))
    {
      //FanHA.setSpeed(SpeedActual);
      lastSpeed = SpeedActual;
    }
    // Calculate RPM for each fan
     rpms1 = fancontrol1.getSpeed();
    rpms2 = fancontrol2.getSpeed();
    
    if (checkBound(rpms1,lastrpms1,100)) {
    Fan1RPMHA.setValue(rpms1);
    lastrpms1=rpms1;
    }
    
    if (checkBound(rpms2,lastrpms2,100)) {
    Fan2RPMHA.setValue(rpms2);
    lastrpms2=rpms2;
    }
    if ((rpms1 == 0) || (rpms1 > 4000))
    {
      Serial.print("FAN1: ");
      Serial.println(" FAIL");
      Fan1RPMHA.setValue(rpms1);
      Fan1RPMHA.setAvailability(false);
    }
    else
    {
      Serial.print("FAN1: ");
      Serial.print(rpms1);
      Serial.println(" RPM");
      Fan1RPMHA.setValue(rpms1);
      Fan1RPMHA.setAvailability(true);
    }

    if ((rpms2 == 0) || (rpms1 > 4000))
    {
      Serial.print("FAN2: ");
      Serial.println(" FAIL");
      Fan1RPMHA.setValue(rpms2);
      Fan2RPMHA.setAvailability(false);
    }
    else
    {
      Serial.print("FAN2: ");
      Serial.print(rpms2);
      Serial.println(" RPM");
      Fan2RPMHA.setValue(rpms2);
      Fan2RPMHA.setAvailability(true);
    }

    lastUpdateAt = millis();
  }
}

// put function definitions here:
