#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>  //https://arduinojson.org v6
#include <ESP8266WiFi.h> 
#include <arduino-timer.h> //https://github.com/contrem/arduino-timer v2
#include "DHTesp.h"

#define DHT_PIN 16
#define LCDADDR 0x3F
#define RELE_PIN 9
#define SONDA_PIN 2
#define STATUS_LED_G 12
#define STATUS_LED_R 14
#define CHK_TIME 15000
#define ONLINE_PIN 13

WiFiClient client;

LiquidCrystal_I2C lcd(LCDADDR, 16, 2);
DHTesp dht;
const int oneWireBus = SONDA_PIN;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

//Timer<2> timer;
auto timer = timer_create_default();
auto task = timer.size();
bool heater_status = false; //false == off
bool go_online = true;
byte altern = 1;
float target_t = 20.5;

void typewriting(String messaggio){
  int lunghezza = messaggio.length();
  for(int i = 0; i < lunghezza; i++){
    lcd.print(messaggio[i]);
    delay(25);
  }
}

bool thermal_chk(void *){
  digitalWrite(STATUS_LED_G, HIGH);
  digitalWrite(STATUS_LED_R, HIGH);
  //reads the target temperature
  
  //target_t = r.getPosition()*pow(10, -1);
  //if current temp >= target temp cool
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  if(temperatureC >= target_t){
    /*
    Serial.println("Coolin' down");
    Serial.print(temperatureC);
    Serial.print(">=");
    Serial.println(target_t);
    */
    if(heater_status){ //do it only if it was true, aka ON
      digitalWrite(RELE_PIN, LOW);
      heater_status = false;
      //and send the update
    }
  }else{
    //else heat
    /*
    Serial.println("Heatin' up");
    Serial.print(temperatureC);
    Serial.print("<");
    Serial.println(target_t);
    */
    if(!heater_status){ //do it only if it was false, aka OFF
      digitalWrite(RELE_PIN, HIGH);
      heater_status = true;
      //and send the update
    }
  }
  if(go_online)
    uploader(0);
  
  lcd.setCursor(0, 0);
  String row = "Attuale: ";
  typewriting(row + String(temperatureC, 2));
  lcd.setCursor(0, 1);
  TempAndHumidity newValues = dht.getTempAndHumidity();
  clrLCD(1);
  switch (altern) {
    case 1:
      row="Target: ";
      typewriting(row + String(target_t, 1));
      altern++;
      break;
    case 2:
      
      row="Ambiente: ";
      typewriting(row + String(newValues.temperature, 0));
      altern++;
      break;
    default:

      row="Umidita': ";
      typewriting(row + String(newValues.humidity, 1));
      altern = 1;
      break;
  }    

  lcd.print(" ");
  if(heater_status){
    lcd.print("+");
  }else{
    lcd.print("-");
  }
  return true;
}

bool uploader(void *){
  //Serial.println("PERIODIC");
  clrLCD(0);
  clrLCD(1);
  if(WiFi.status() == WL_CONNECTED){
    digitalWrite(STATUS_LED_G, HIGH);
    digitalWrite(STATUS_LED_R, LOW);
    /* Env T/H */
    //Serial.print(dht.getStatusString()); //OK
    TempAndHumidity newValues = dht.getTempAndHumidity();
    //DOPPIO? doc["amb_temp"] = newValues.temperature;
   // Serial.println(" T:" + String(newValues.temperature) + " H:" + String(newValues.humidity));
    
    /*
    float ambHum = dht.getHumidity();
    float ambTemp = dht.getTemperature();
    /* Mosto Temperature */
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);

    /* Getting out data */
    //if(srv_reachable||true){
  
    //turn on LED
    //test_status();
    /* Make JSON to post */
    DynamicJsonDocument doc(100);
    doc["temp"] = String(temperatureC, 2);
    doc["target"] = String(target_t, 2);
    doc["heater"] = heater_status;
    doc["amb_hum"] = newValues.humidity;
    doc["amb_temp"] = newValues.temperature;
    
    
    //    String dataFileName;
    //    serializeJson(doc, dataFileName);
    //    char jDataFileName[100];
    //    Serial.print((unsigned int) measureJson(doc));
    //    dataFileName.toCharArray(jDataFileName, 80);

    
    
    //client.setContentType("application/json");
    //int statusCode = client.post("/birrino", "{\"test\":123.456}");
    
    String resp = ""; // TODO
    
    /*
    int statuscode = client.connect("/birrino", jDataFileName);
    Serial.println(statuscode);
    Serial.println(resp);
    */
    if (!client.connect("192.168.1.108", 1880)) {
      Serial.println("connection failed");
    }
    if (client.connected()) {
      client.println("POST /temperatures HTTP/1.1");
      client.println("Host: 192.168.1.108:1880");
      client.println("User-Agent: Arduino/1.0");
      client.println("Content-Type: application/json; charset=UTF-8");
      client.print("Content-Length: ");
      client.println(measureJson(doc));
      client.println();
      serializeJson(doc, client);
      /*client.println("Connection: close");*/
      client.println();
    }
    
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 1000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return false;
      }
    }
    String res = "";
    //DynamicJsonDocument rcv_temp(30);
    StaticJsonDocument<32> rcv_temp;
    while (client.available()) {
      //char ch = static_cast<char>(client.read());
      //Serial.print(ch);
      res.concat(static_cast<char>(client.read()));
    }
    
    client.stop();
    //Serial.println(res.indexOf("\"tewmp\":"));
    //Serial.println(res);
    //Serial.println(res.substring(res.indexOf("{\"")+1, res.indexOf("\"")+12));
    //Serial.println(res.substring(294, 308));
    
    DeserializationError err = deserializeJson(rcv_temp, res.substring(248, 263));
    if (err) {
      digitalWrite(STATUS_LED_G, LOW);
      digitalWrite(STATUS_LED_R, HIGH);
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(err.f_str());
      clrLCD(1);
      clrLCD(0);
      lcd.setCursor(6, 1);
      typewriting("E_DES");
    }else{
      float new_temp = rcv_temp["tewmp"];
     // Serial.println(String(new_temp,2));
      target_t = new_temp;
      
    }

    /*
    client.println("POST /birrino HTTP/1.1");
    client.println("Host:  192.168.1.108:1880");
    client.println("User-Agent: Arduino/1.0");
    client.println("Connection: close");

    client.println("Connection: close");
    client.println();
  
    /* Serial.print("Status code from server: ");
    Serial.println(statusCode);
    Serial.print("Response body from server: ");
    Serial.println(response);*/
    
  }else{
    
    digitalWrite(STATUS_LED_G, LOW);
    digitalWrite(STATUS_LED_R, HIGH);
    clrLCD(1);
    clrLCD(0);
    typewriting("reConnect");
    lcd.setCursor(6, 1);
    typewriting("...");
    WiFi.begin("MyAwesome AP", "ABBA4321");
    //WiFi.begin("Valorosi", "DieciDodici61"); 
  }
  return true;
}

bool have_to_go_online(){
  go_online = digitalRead( ONLINE_PIN ); //if HIGH then connect!
  return go_online;
}

void clrLCD(unsigned int line){
  lcd.setCursor(0, line);
  typewriting("                ");
  lcd.setCursor(0, line);
}

void setup() {
  Serial.begin(9600);
  //while (!Serial) continue;
  pinMode(STATUS_LED_G, OUTPUT);
  pinMode(STATUS_LED_R, OUTPUT);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(STATUS_LED_G, HIGH);
  digitalWrite(STATUS_LED_R, LOW);
  digitalWrite(RELE_PIN, LOW);
  dht.setup(DHT_PIN, DHTesp::DHT11);
  lcd.init();
  lcd.backlight();
  // Start the DS18B20 sensor
  sensors.begin();
  delay(50);
  lcd.setCursor(3,0);
  typewriting("iTERMOSTATO");
  lcd.setCursor(6,1);
  typewriting("v1.8");
  delay(1000);
  if(have_to_go_online()){
    WiFi.mode(WIFI_STA);
    WiFi.begin("Valorosi", "DieciDodici61");
    //WiFi.begin("My Awesome AP", "ABBA4321");
  }else{
    lcd.setCursor(3,1);
    typewriting("- offline -");
  }
  //tests if server reachable, if not disalbes the data-out  
  //timer.every(8000, periodic_chk); //uploads to the cloud
  
  task = timer.every(CHK_TIME + dht.getMinimumSamplingPeriod(), thermal_chk);
  lcd.setCursor(2, 0);
  //Serial.println("START");
  thermal_chk(0);
}
void loop(){
  timer.tick();
}
