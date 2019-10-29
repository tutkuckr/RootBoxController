#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "DHT.h"

#define DHTTYPE DHT22 //DHT 22(AM2302) - Heat and Humidity Sensor
#define DHTPIN 4 //DHT sensor pin on ESP   
#define LEDPIN 2 //LED Pin on ESP
#define FANPIN 15 //FAN Pin on ESP that comes from relay module
const int fogger[]  = {18, 19, 21}; //FOGGER Pins

//Farm name will be sent by pi and then pi will choose the foggers to work
String foggerNumber[] = {"fogger1", "fogger2", "fogger3"};
String farmName = "noName";

//WIFI information - name, password and server 
const char* wifi_name = "Eden AT";
const char* wifi_password = "rootsandfruits";
const char* mqtt_server = "192.168.1.146";
char* topic;
//WiFiClient creates client that can connect to a specified internet IP address and port
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//Initialized DHT sensor, created object called dht
DHT dht(DHTPIN, DHTTYPE); 

//Temp & Hum will give data in every 15 seconds
bool switching = false;
int interval = 15000;
unsigned long currentTime;
unsigned long previousTime = 0;

char farmNameBuffer[70];
char fanBuffer[50];
char foggerBuffer[50];

void setup() 
{ 
  Serial.begin(9600);
  
  pinMode(DHTPIN, OUTPUT); //4
  pinMode(LEDPIN, OUTPUT); //2
  pinMode(FANPIN, OUTPUT); //15
  pinMode(fogger[0], OUTPUT); //18
  pinMode(fogger[1], OUTPUT);  //19
  pinMode(fogger[2], OUTPUT); //21

  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

//WIFI
void setup_wifi()
{
  delay(10);
  WiFi.begin(wifi_name, wifi_password);

  Serial.print("Connecting to ");
  Serial.println(wifi_name);
 
  while(WiFi.status()!= WL_CONNECTED)
  {
    Serial.println(".");
    delay(200);
  }

  Serial.print("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
}

//CALLBACK
void callback(char* topic, byte* message, unsigned int length) 
{
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("MESSAGE:    ");
  
  String messagetemp; //message that arrived from pi
  
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)message[i]);
    messagetemp += (char)message[i];
  }
  
  Serial.println();
  if(String(topic) == "greenHouse/farmDevice/name")
  {
    farmName = messagetemp;
    client.unsubscribe("greenHouse/farmDevice/#");
    //client.disconnect();
    //char *n = const_cast<char*>(farmName.c_str());
    //client.connect(n);
    int y = sprintf(farmNameBuffer, "greenHouse/%s/foggers/#", farmName);
    
    client.subscribe(farmNameBuffer);
    client.subscribe(fanBuffer);
    
  }
  
 for (int i = 0; i < 3; i++) 
 { 
   int x = sprintf(foggerBuffer, "greenHouse/%s/foggers/%s", farmName, foggerNumber[i]);

   if (String(topic) == foggerBuffer) 
   { 
     if(messagetemp == "on")
     {
       digitalWrite(fogger[i], LOW);
       digitalWrite(LEDPIN, HIGH);
     }
     else if(messagetemp == "off")
     {
       digitalWrite(fogger[i], HIGH);
       digitalWrite(LEDPIN, LOW);
     }
   }
 }
  int fan = sprintf(fanBuffer, "greenHouse/%s/fan", farmName);
    
  if (String(topic) == fanBuffer) 
  { 
    if(messagetemp == "on")
    {
      digitalWrite(FANPIN, LOW);
    }
    else if(messagetemp == "off")
    {
      digitalWrite(FANPIN, HIGH);
    }
  }
}

//TRIES TO RE/CONNECT MQTT, SUBSCRIBES 
void reconnect() 
{
  // Loops until reconnection
  while (!client.connected()) 
  {
    Serial.println("Attempting MQTT connection...");
    
    if (client.connect("farm2")) 
    {
      if(farmName == "noName")
      {
        //changes 'noName' and then sets a new farmName
        client.subscribe("greenHouse/farmDevice/name");
        client.publish("greenHouse/farmDevice", "NEW_CONNECTION");
      }
      
      else
      {
        client.subscribe(farmNameBuffer);
        client.subscribe(fanBuffer);
      }
    } 
    
    else 
    {
      Serial.print("failed, rc= ");
      Serial.println(client.state());
      Serial.println("Trying again in 2 seconds..");
      delay(2000);
    }
  }
}

//READS FROM TEMPERATURE & HUMIDITY SENSOR AND SENDS THE READINGS
void read_from_DHTsensor()
{
  currentTime = millis();
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);

  //if statement that checks if the sensor returned valid t and h
  if (isnan(h) || isnan(t) || isnan(f)) 
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    delay(500);
    return;
  }

  char tempString[8]; //convert the value to char array 
  char humString[8]; //convert the value to a char array
  char sensorBuffer[50];
  
  dtostrf(t, 1, 2, tempString);
  dtostrf(h, 1, 2, humString);
  
  //gets temperature and humidity values
  if(switching == true)
  {
    Serial.println("Temperature: " + (String)tempString + "°C ");
    Serial.println("Humidity: " + (String)humString + "% ");
    
    int n = sprintf(sensorBuffer, "greenHouse/%s/sensors/temp", farmName);
    client.publish(sensorBuffer, tempString);
    
    n = sprintf(sensorBuffer, "greenHouse/%s/sensors/humidity", farmName);
    client.publish(sensorBuffer , humString);
    switching = !switching;
  }
  
  if(currentTime >= previousTime + interval)
  {
    switching = !switching;
    previousTime = currentTime;
  }
  //Serial.print(F("Humidity: "));
  //Serial.print(h);
  //Serial.print(f);
  //Serial.print(F("°F" ));
}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }

  //Listens to inbound MQTT messages
  client.loop();

  if ((WiFi.status() != WL_CONNECTED))
  {
  client.publish("greenHouse/farmDevice/#", "CONNECTION_LOST");
  delay(500);
  }
  read_from_DHTsensor();
}
