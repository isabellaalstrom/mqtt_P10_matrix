#include <TimeLib.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <time.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <NtpClientLib.h>
#include <P10_matrix.h>
#include <Fonts/TomThumb.h>

#define logging 0


MDNSResponder mdns;
ESP8266WebServer server(80);
Ticker display_ticker;

//Change these three to your own info
const char* ssid = "ssid";
const char* password = "password";
IPAddress mqtt_server(192, 168, 1, 62);
//change to this if you have an adress rather than an ip
//const char* mqtt_server = "address";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
String latestMessage;

int brightness=0;
int dimm=0;


// Pins for LED MATRIX
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_OE 2
P10_MATRIX display( P_LAT, P_OE,P_A,P_B,P_C);

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myWHITE = display.color565(255, 255, 255);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);

uint16 myCOLORS[8]={myRED,myGREEN,myBLUE,myWHITE,myYELLOW,myCYAN,myMAGENTA,myBLACK};

boolean syncEventTriggered = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent; // Last triggered event

const char INDEX_HTML[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>ESP8266 Web Form Demo</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
"</style>"
"</head>"
"<body>"
"<h1>Pixel Time - Setup</h1>"
"<FORM action=\"/\" method=\"post\">"
"<label for=\"vname\">Network (ESSID): "
"<input type=\"text\" id=\"essid\" name=\"essid\">"
"</label><br>"
"<label for=\"zname\">Network key (password): "
"<input id=\"key\" name=\"key\">"
"</label><br>"
"<label for=\"ntp\">NTP server: "
"<input id=\"ntp\" name=\"ntp\" value=\"pool.ntp.org\">"
"</label><br>"
"<INPUT type=\"submit\" value=\"OK\">"
"</FORM>"
"</body>"
"</html>";

// Handle NTP events
void processSyncEvent(NTPSyncEvent_t ntpEvent) {
  if (ntpEvent) {
    Serial.print("Time Sync error: ");
    if (ntpEvent == noResponse)
    Serial.println("NTP server not reachable");
    else if (ntpEvent == invalidAddress)
    Serial.println("Invalid NTP server address");
  }
  else {
    Serial.print("Got NTP time: ");
    Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
  }
}

void returnFail(String msg)
{
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, "text/plain", msg + "\r\n");
}
bool connect_now=false;
void handleSubmit()
{
  if (!server.hasArg("essid")) return returnFail("BAD ARGS");
  String this_essid = server.arg("essid");
  String this_key = server.arg("key");
  String this_ntp = server.arg("ntp");

  Serial.println("essid: " + this_essid);
  Serial.println("key: " + this_key);
  Serial.println("ntp: " + this_ntp);
  for (int i = 0; i < 96; ++i) { EEPROM.write(i, 0); }
  for (int i = 0; i < this_essid.length(); ++i)
  EEPROM.write(i, this_essid[i]);
  for (int i = 0; i < this_key.length(); ++i)
  EEPROM.write(i+32, this_key[i]);

  for (int i = 0; i < this_ntp.length(); ++i)
  EEPROM.write(i+64, this_ntp[i]);

  EEPROM.commit();
  connect_now=true;

  server.send ( 200, "text/html", "... connecting to wifi" );
}

void handleRoot()
{
  if (server.hasArg("essid")) {
    handleSubmit();
  }
  else {
    server.send(200, "text/html", INDEX_HTML);
  }
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}



// ISR for display refresh
void display_updater()
{

  brightness=brightness+dimm;
  if (brightness<0)
  {
    brightness=0;
    dimm=0;
  }

  if (brightness>2100)
  {
    brightness=2100;
    dimm=0;
  }
  display.display(brightness/30);
}

// Start Accespoint for entering WIFI info
void start_ap()
{
  WiFi.softAP("PixelTime", "");

  IPAddress myIP = WiFi.softAPIP();

  //WiFi.begin(ssid, password);
  Serial.println("");

  //  // Wait for connection
  //  while (WiFi.status() != WL_CONNECTED) {
  //    delay(500);
  //    Serial.print(".");
  //  }


  Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
  Serial.println( myIP);

  if (mdns.begin("esp8266WebForm", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.onNotFound(handleNotFound);

  server.begin();

}

void start_ota(){
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void start_wifi()
{

  Serial.println("Reading config from EEPROM");
  String esid;
  String pass;
  String ntp;
  for (int i = 0; i < 32; ++i)
  {
    esid +=  char(EEPROM.read(i));
  }

  for (int i = 32; i < 64; ++i)
  {
    pass += char(EEPROM.read(i));
  }
  for (int i = 64; i < 96; ++i)
  {
    ntp += char(EEPROM.read(i));
  }

  //Change these two to your own info
  esid="ssid";
  pass="password";
  
  ntp="0.de.pool.ntp.org";
  Serial.println("essid: " + esid);
  Serial.println("pass: " + pass);
  Serial.println("ntp: " + ntp);
  //WiFi.mode(WIFI_OFF);
  //delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(esid.c_str(), pass.c_str());
  //server.close();

  // Wait for connection
  unsigned long start_connect=millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if ((millis()-start_connect)>30000)
    ESP.restart();

  }
  Serial.println(WiFi.localIP());
  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });
  NTP.begin(ntp.c_str(), 1, true);
  NTP.setInterval(63);
}

void setup() {
  // put your setup code here, to run once:

  display.begin();
  display.clearDisplay();
  display.setTextColor(myCYAN);
  display.setCursor(2,0);
  display.print("Hello");
  display.setTextColor(myMAGENTA);
  display.setCursor(2,8);
  display.print("Isa");

  EEPROM.begin(512);
  Serial.begin(9600);


  
    start_wifi();
  start_ota();
  display_ticker.attach(0.001, display_updater);
  dimm=1;
  yield();
  delay(3000);
  //start mqtt-connection
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

//mqtt
void callback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String str;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    str += (char)payload[i];
  }
  
  latestMessage = str;
  
  Serial.println();
  Serial.print(latestMessage);
  Serial.println();

  display.setTextColor(myCYAN);
  display.setCursor(2,16);
  display.print(str);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    
    //change the username and password to those for your broker, or remove them if unauthorized
    if (client.connect("ESP8266Client", "Username", "Password")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("/ledmatrix/out/", "connected");
      // ... and resubscribe
      client.subscribe("/homeassistant/alarm_control_panel/home_alarm/state");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

union single_double{
  uint8_t two[2];
  uint16_t one;
} this_single_double;

void print_latest_message()
{
  display.setTextColor(myCYAN);
  display.setCursor(2,16);
  display.print(latestMessage);
}

// This draws the time
void draw_time ()
{
  uint8_t this_hour= NTP.getHour();
  uint8_t this_minute= NTP.getMinute();
  uint8_t this_second= NTP.getSecond();
  display.setFont(&TomThumb);
  display.setTextColor(myWHITE);
  display.setCursor(1,6);
  if (this_hour<10)
    display.println("0"+String(this_hour));
  else
    display.println(this_hour);
  display.setCursor(10,6);

  if (this_minute<10)
    display.println("0"+String(this_minute));
  else
    display.println(this_minute);

  // Dots
  display.drawPixel(8,2,myWHITE);
  display.drawPixel(8,4,myWHITE);

}

void process_ntp()
{
  static int i = 0;
  static int last = 0;

  if (syncEventTriggered) {
    processSyncEvent(ntpEvent);
    syncEventTriggered = false;
  }

  if ((millis() - last) > 5100) {
    //Serial.println(millis() - last);
    last = millis();
#ifdef logging
    Serial.print(i); Serial.print(" ");
    Serial.print(NTP.getTimeDateString()); Serial.print(" ");
    Serial.print(NTP.isSummerTime() ? "Summer Time. " : "Winter Time. ");
    Serial.print("WiFi is ");
    Serial.print(WiFi.isConnected() ? "connected" : "not connected"); Serial.print(". ");
    Serial.print("Uptime: ");
    Serial.print(NTP.getUptimeString()); Serial.print(" since ");
    Serial.println(NTP.getTimeDateString(NTP.getFirstSync()).c_str());
#endif
    i++;
  }
}

void loop() {
    if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 20000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);
  }
  
  bool ret_code;
  unsigned long this_time=millis();
  display.clearDisplay();
  draw_time ();
  print_latest_message();

  server.handleClient();
  ArduinoOTA.handle();
  process_ntp();


  delay(100);
}
