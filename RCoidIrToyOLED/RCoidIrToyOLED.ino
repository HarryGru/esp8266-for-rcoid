/*
  RCoidIrToy mit OLED und Funkmodul

  Board: "NodeMCU 1.0"
  Benötigte Bibiotheken:
    "ESP8266 and ESP32 Oled Driver for SSD1306 display"
    "IRremoteESP8266"
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <EEPROM.h>
#include <SSD1306.h>
#include "FS.h"




#define RF_PORT D9
#define IR_PORT D8
#define IR_PORT_INVERT false
#define IR_RECEIVER_PORT D4
#define STATUS_LED D0
#define SPK_PORT D3

#define PUSH_PORT D3
#define UP_PORT D7
#define ENTER_PORT D5
#define DOWN_PORT D6

boolean waitForRelease = true;

// Netzwerkinformationen für Accesspoint
// Im AP-Modus ist der ESP8266 unter der IP 192.168.0.1 erreichbar
const char* ssidAP = "ESP8266 for RCoid Access Point";
const char* passwordAP = "passpass";  //Muss mindestens 8 Zeichen haben

char puffer[1024] = {'0'};
unsigned int tones[20] = {0, 0};
String htmlcontent;
ESP8266WebServer server(80);

IRrecv irReceiver(IR_RECEIVER_PORT);
decode_results ir_Decoded;
int irDecodedIndexShow = -1;
boolean pushJson = false;

SSD1306  display(0x3c, D1, D2);

unsigned long timer;
#define DISPLAY_TIMEOUT 20000
#define RESET_TIMEOUT 300000  //Wenn sich im AP-Modus kein Client verbindet, wird nach dieser Zeit ein Reset ausgelöst. Sinnvoll, wenn nach Stromausfall der Router noch nicht neu gestartet hat.

int fileCounter;


/*
   sendet ein RCoid-RF-Signal
   z.B.:

*/
void handleRf()
{
  serial_print_HttpInfo();

  int repeats = getArgValue("repeats");
  if (repeats == -1)
    repeats = 1;
  int gap = getArgValue("gap");
  if (gap == -1)
    gap = 10000;

  if (server.arg("code") != "")
  {
    digitalWrite(STATUS_LED, LOW);
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, "RF-Signal");
    display.drawString(30, 30, "was sended.");
    display.setFont(ArialMT_Plain_10);
    display.display();

    timer = millis();

    (server.arg("code") + ",0").toCharArray(puffer, 1024);
    for (int i = 0; i <= repeats; i++)
    {
      sendRf(puffer);
      if (i != repeats)
      {
        digitalWrite(STATUS_LED, HIGH);
        delayMicroseconds(gap);
        digitalWrite(STATUS_LED, LOW);
      }
      ESP.wdtFeed();
    }
  }
  else
  {
    handleNotFound();
    return;
  }
  htmlcontent = "OK";
  server.send(200, "text/plain", htmlcontent);

  digitalWrite(STATUS_LED, HIGH);
}

void sendRf(char* p_RCoidRfCode)
{
  char *p = 0; //Zeiger im Array

  bool burst = true; //wir beginnen mit RF-Signal
  unsigned int burstTime = strtol(p_RCoidRfCode, &p, 10);
  while (burstTime != 0)
  {
    digitalWrite(RF_PORT, burst ? HIGH : LOW);


    delayMicroseconds(burstTime);  //Warten

    burst = !burst;
    p++; //Komma im String wird übersprungen
    burstTime = strtol(p, &p, 10);
  }
  digitalWrite(RF_PORT, LOW); //Am Ende IR immer AUS
}

/*
   Gibt die CPU Takte zurück, die seit dem Neustart vergangen sind.
   läuft ca. alle 53 Sekunden über
*/
#define RSR_CCOUNT(r)     __asm__ __volatile__("rsr %0,ccount":"=a" (r))
static inline uint32_t get_ccount()
{
  uint32_t ccount;
  RSR_CCOUNT(ccount);
  return ccount;
}

/*
   Diese Webseite wird angezeigt, wenn der ESP im WLAN mit seiner lokalen IP abgerufen wird.
*/
void handleRoot()
{
  htmlcontent = "<html><head></head><body style='font-family: sans-serif; font-size: 12px'>ESP8266 f&uuml;r RCoid";
  htmlcontent += "<p>";
  htmlcontent += "<a href='/receiveir'>Receive Infrared Signal</a>";
  htmlcontent += "</p>";
  htmlcontent += "<p>";
  htmlcontent += "<a href='/deletepass'>WLAN Zugangsdaten l&ouml;schen</a>";
  htmlcontent += "</p>";
  htmlcontent += "<p>";
  htmlcontent += "<a href='/files'>Alle Daten anzeigen</a>";
  htmlcontent += "</p>";
  htmlcontent += "<p>";
  htmlcontent += "<a href='/formatspiffs'>Dateisystem formatieren</a>";
  htmlcontent += "</p>";

  htmlcontent += "</body></html>";
  server.send(200, "text/html", htmlcontent);
}

/*
   Diese Webseite wird angezeigt, wenn der ESP im AP-Modus mit seiner IP 192.168.0.1 abgerufen wird.
   Hier kann man dann die SSID und das Password des WLAN eingeben und den ESP neu starten.
*/
void handleAPRoot()
{

  IPAddress ip = WiFi.softAPIP();
  htmlcontent = "<html><head></head><body style='font-family: sans-serif; font-size: 12px'>ESP8266 f&uuml;r RCoid mit aktivem Access Point (IP: " +  String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]) + ")";
  htmlcontent += "<p>";
  int n = WiFi.scanNetworks();
  if (n > 0)
  {
    htmlcontent += "<ol>";
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      htmlcontent += "<li>";
      if (WiFi.SSID(i) == WiFi.SSID())
        htmlcontent += "<b>";
      htmlcontent += WiFi.SSID(i);
      htmlcontent += " (";
      htmlcontent += WiFi.RSSI(i);
      htmlcontent += ")";
      htmlcontent += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
      if (WiFi.SSID(i) == WiFi.SSID())
      {
        IPAddress ip = WiFi.localIP();
        htmlcontent += "</b>  verbunden mit lokaler IP " + String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      }
      htmlcontent += "</li>";
    }
    htmlcontent += "</ol>";
  }
  else
  {
    htmlcontent += "Es konnten keine Netzwerke gefunden werden.";
  }
  htmlcontent += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid'><input name='pass'><input type='submit'></form><br><br>";
  htmlcontent += "<a href='/reset'>ESP8266 neu starten";
  if (WiFi.status() == WL_CONNECTED)
  {
    htmlcontent += " und AP deaktivieren";
  }
  htmlcontent += ".</a>";
  htmlcontent += "<p>";
  htmlcontent += "<a href='/receiveir'>Receive Infrared Signal</a>";
  htmlcontent += "</p>";
  htmlcontent += "<p>";
  htmlcontent += "<a href='/files'>Alle Daten anzeigen</a>";
  htmlcontent += "</p>";
  htmlcontent += "<p>";
  htmlcontent += "<a href='/formatspiffs'>Dateisystem formatieren</a>";
  htmlcontent += "</p>";

  htmlcontent += "</body></html>";
  server.send(200, "text/html", htmlcontent);
}

/*
   diese Funktion löscht die Zugangsdaten aus dem EEPROM
*/
void handleDeletePass()
{
  htmlcontent = "<!DOCTYPE HTML>\r\n<html>";
  htmlcontent += "<p>Clearing the EEPROM</p></html>";
  server.send(200, "text/html", htmlcontent);
  Serial.println("clearing eeprom");
  EEPROM.write(0, 0);
  EEPROM.write(1, 0);

  EEPROM.commit();

  ESP.restart();
}

/*
   Diese Webseite wird angezeigt, wenn eine unbekannte URL abgerufen wird.
*/
void handleNotFound() {
  htmlcontent = "File Not Found\n\n";
  htmlcontent += "URI: ";
  htmlcontent += server.uri();
  htmlcontent += "\nMethod: ";
  htmlcontent += (server.method() == HTTP_GET) ? "GET" : "POST";
  htmlcontent += "\nArguments: ";
  htmlcontent += server.args();
  htmlcontent += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    htmlcontent += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", htmlcontent);
}

/*
   Gibt den Interger-Wert einers Argumentes zurück.
   oder -1. falls das Argument nicht existiert
*/
int getArgValue(String name)
{
  for (uint8_t i = 0; i < server.args(); i++)
    if (server.argName(i) == name)
      return server.arg(i).toInt();
  return -1;
}

/*
   Zeigt Informationen zur HTTP-Anfrage im Serial-Monitor an
*/
void serial_print_HttpInfo()
{
  String message = "\n\n";
  message += "Time: " + String(millis(), DEC) + "\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  Serial.println(message);
}

/*
   Zeigt eine Liste mit verfügbaren Netzwerken im Serial-Monitor an
*/
void serial_print_Networks()
{
  int n = WiFi.scanNetworks();

  Serial.println("\nScan abgeschossen");
  if (n == 0)
    Serial.println("Kein WLAN gefunden");
  else
  {
    for (int i = 0; i < n; ++i)
    {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "" : " (verschlüsselt)");
    }
  }
  Serial.println("");
}

/*
   sendet ein RCoid-IR-Signal
   z.B.:
   http://ip.for.your.device/ir?code=38000,342,171,21,21,21,21,21,21,21,64,21,21,21,21,21,64,21,21,21,21,21,21,21,64,21,64,21,21,21,64,21,21,21,21,21,21,21,64,21,64,21,21,21,21,21,64,21,64,21,64,21,64,21,21,21,21,21,64,21,64,21,21,21,21,21,21,21,1
*/
void handleIr()
{
  serial_print_HttpInfo();

  if (server.argName(0).equals("code"))
  {
    irReceiver.disableIRIn();
    digitalWrite(STATUS_LED, LOW);
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, "IR-Signal");
    display.drawString(30, 30, "was sended.");
    display.setFont(ArialMT_Plain_10);
    display.display();
    timer = millis();
    (server.arg(0) + ",0").toCharArray(puffer, 1024);
    sendIr(puffer);
  }
  else
  {
    handleNotFound();
    return;
  }
  irReceiver.enableIRIn();
  htmlcontent = "OK";
  server.send(200, "text/plain", htmlcontent);

  digitalWrite(STATUS_LED, HIGH);
}


void sendIr(char* p_RCoidIrCode)
{
  char *p = 0; //Zeiger im Array
  unsigned int frequence = strtol(p_RCoidIrCode, &p, 10);
  if (frequence == 0)
    return;
  p++; //Komma im String wird übersprungen
  unsigned int pulses = strtol(p, &p, 10);

  bool burst = true; //wir beginnen mit IR Licht

  unsigned int startTicks;
  unsigned int halfPeriodTicks = 40000000 / frequence;
  while (pulses != 0)
  {
    RSR_CCOUNT(startTicks);
    for (unsigned int i = 0 ; i < pulses * 2; i++)
    {
      if (IR_PORT_INVERT)
        digitalWrite(IR_PORT, (((i & 1) == 1) && burst) ? LOW : HIGH);
      else
        digitalWrite(IR_PORT, (((i & 1) == 1) && burst) ? HIGH : LOW);
      while (get_ccount() < startTicks + i * halfPeriodTicks) {} //Warten
    }
    burst = !burst;
    p++; //Komma im String wird übersprungen
    pulses = strtol(p, &p, 10);
  }
  digitalWrite(IR_PORT, IR_PORT_INVERT ? HIGH : LOW); //Am Ende IR immer AUS
}



/**
   Schaltete einen beliebigen GPIO Pin.
   z.B.: http://ip.of.your.device/out?port=0&value=1     //schaltet GPIO0 an
         http://ip.of.your.device/out?port=5&value=t     //wechselt den Zustand von GPIO5
*/
void handleOut()
{
  serial_print_HttpInfo();

  if (server.arg("port").length() == 0
      || server.arg("value").length() == 0)
  {
    handleNotFound();
    return;
  }

  int port = getArgValue("port");
  Serial.print("Port ");
  Serial.println(port);

  if (port < 0 || port > 15)
  {
    Serial.println("Port out of range. Abort!");
    htmlcontent = "Port out of range.";
    server.send(400, "text/plain", htmlcontent);
    return;
  }

  Serial.print("Value ");

  if (server.arg("value") == "t")
  {
    Serial.println("t (Toggle)");
    if (port != -1)
    {
      pinMode(port, OUTPUT);
      digitalWrite(port, !digitalRead(port));
    }
  }
  else
  {
    int value = getArgValue("value");
    Serial.println(value);

    if (port != -1 && value != -1)
    {
      pinMode(port, OUTPUT);
      digitalWrite(port, value == 0 ? LOW : HIGH);
    }
  }

  htmlcontent = "OK";
  server.send(200, "text/plain", htmlcontent);

}

/*
   setzt den ESP zurück, damit er sich neu verbindet (nur im AP- und AP_STA-Modus)
   Wenn sich der ESP dann im WLAN einwählen konnte, startet er im STA-Modus

   Achtung: funktioniert das erste mal nach dem Flashen nicht! Der ESP muss dann mit dem Reset-Taster neu gestartet werden.
*/
void handleReset()
{
  Serial.println("ESP wird neu gestartet!");
  ESP.restart();
}

/*
   gibt die IP als Klartext zurück (nur im AP- und AP_STA-Modus)
   wird von RCoid abgefragt um festzustellen, ob der ESP mit dem WLAN verbunden ist
   wird von RCoid automatisch in der App eingetragen
*/
void handleGetIp()
{
  IPAddress ip = WiFi.localIP();
  htmlcontent = ip.toString();;
  server.send(200, "text/plain", htmlcontent);
  Serial.println("Get IP = " + htmlcontent);
  if (!ip.toString().equals("0.0.0.0"))
  {
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, "Local IP");
    display.drawString(0, 30, ip.toString());
    display.setFont(ArialMT_Plain_10);
    display.display();
    timer = millis();
  }
}

/*
  wartet eine Zeit ab, bis am Receiver ein IR Signal decodiert wurde
  blockiert den ESP
*/
void handleReceiveIr()
{
  if (pushJson)
  {
    push_JSON();
    return;
  }
  digitalWrite(STATUS_LED, LOW);
  irReceiver.enableIRIn();  // Start the receiver
  unsigned long start = millis();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(10, 10, "Wait for");
  display.drawString(40, 30, "IR-Signal");
  display.setFont(ArialMT_Plain_10);
  display.display();


  while (millis() < start + 30000)
  {
    if (irReceiver.decode(&ir_Decoded))
    {
      digitalWrite(STATUS_LED, LOW);
      irDecodedIndexShow = fileCounter;
      displayIRdecoded(ir_Decoded);

      htmlcontent = getJSON();
      Serial.println(htmlcontent);
      server.send(200, "application/json", htmlcontent);

      saveJSON();
      playTone();
      irReceiver.resume();  // Receive the next value
      digitalWrite(STATUS_LED, HIGH);

      return;
    }
    delay(100);
  }
  digitalWrite(STATUS_LED, HIGH);
  server.send(408, "text/plain", "No IR Signal received!");
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 15, "No IR Signal");
  display.drawString(40, 35, "received!");
  display.setFont(ArialMT_Plain_10);
  display.display();

  timer = millis();
}

void push_JSON()
{
  File file = SPIFFS.open("/IR-Decoded" + String(irDecodedIndexShow) + ".json", "r");
  if (!file)
  {
    Serial.println("JSON file open failed");
    display.drawString(0, 0, "JSON file open failed");
  }
  else
  {
    htmlcontent = "";
    //   while (file.available())
    //   {
    htmlcontent += file.readString();
    //   }


    Serial.println(htmlcontent);

    server.send(200, "application/json", htmlcontent);
  }

  irReceiver.enableIRIn();  // Start the receiver
  digitalWrite(STATUS_LED, HIGH);
  pushJson = false;
  timer = millis();
  displayJSON(irDecodedIndexShow);

}

/*
    liefert den JSON Inhalt vom decodierten Signal
    als String zurück
*/
String getJSON()
{
  String str = "{\n";
  str += "  \"Protocol\" : ";
  str += "\"";
  str += typeToString(ir_Decoded.decode_type, false);
  str += "\",\n";
  str += "  \"Value\" : \"";
  str += uint64ToString(ir_Decoded.value, HEX);
  str += "\",\n";
  str += "  \"Length\" : \"";
  str += ir_Decoded.rawlen;
  str += "\",\n";
  str += "  \"Address\" : \"";
  str += ir_Decoded.address;
  str += "\",\n";
  str += "  \"Command\" : \"";
  str += ir_Decoded.command;
  str += "\",\n";
  str += "  \"RCoid IR Code\" : \"";
  str += getIrCode();
  str += "\"\n";
  str += "}";
  return str;
}

String getIrCode()
{
  String str = "";
  int freq = 38000;
  if (typeToString(ir_Decoded.decode_type, false).equals("SONY"))
    freq = 40000;
  str += freq;
  for (int i = OFFSET_START; i < ir_Decoded.rawlen; i++)
  {
    str += ",";
    str += (int)(((ir_Decoded.rawbuf[i] * RAWTICK) * freq) / 1000000);
  }
  if (ir_Decoded.rawlen % 2 == 0)
  {
    str += ",1";
  }
  (str + ",0").toCharArray(puffer, 1024);
  return str;
}

/*
   ////////////////////////////////////   SETUP   ///////////////////////////////////////////
*/
void setup(void) {

  //  tone(10,NOTE_C4,1000);
  Serial.begin(115200);

  pinMode(UP_PORT, INPUT_PULLUP);
  pinMode(DOWN_PORT, INPUT_PULLUP);
  pinMode(ENTER_PORT, INPUT_PULLUP);
  pinMode(PUSH_PORT, INPUT_PULLUP);
  pinMode(IR_PORT, OUTPUT);
  pinMode(RF_PORT, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(IR_PORT, IR_PORT_INVERT ? HIGH : LOW);
  digitalWrite(STATUS_LED, HIGH);

  display.init();

  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();

  irReceiver.enableIRIn();  // Start the receiver


  EEPROM.begin(512);


  delay(1);
  SPIFFS.begin();
  // This function have to be done ONLY ONCE!
  //formatSPIFFS();
  File file = SPIFFS.open("/counter.txt", "r");
  if (!file)
  {
    formatSPIFFS();
    fileCounter = 0;
    file = SPIFFS.open("/counter.txt", "w");
    if (!file)
    {
      Serial.println("counter file open failed");
    }
    else
    {
      file.print(String(fileCounter));
      file.close();
      Serial.println("counter.txt saved");
    }
  }
  else
  {
    String line = file.readStringUntil('\n');
    fileCounter = line.toInt();
  }
  irDecodedIndexShow = fileCounter - 1;
  Serial.print("fileCounter = ");
  Serial.println(fileCounter);


  // read eeprom for ssid and pass
  Serial.println("\n\nReading EEPROM");
  String essid = "";
  byte ssidSize = EEPROM.read(0);
  byte passSize = EEPROM.read(1);

  for (int i = 2; i < 2 + ssidSize; i++)
  {
    essid += char(EEPROM.read(i));
  }
  Serial.print("SSID: ");
  Serial.println(essid);
  String epass = "";
  for (int i = 2 + ssidSize; i < 2 + ssidSize + passSize; i++)
  {
    epass += char(EEPROM.read(i));
  }
  //Serial.print("PASS: ");
  //Serial.println(epass);


  if (essid.length() > 1)
  {

    WiFi.mode(WIFI_STA);
    WiFi.begin(essid.c_str(), epass.c_str());  //Starte WIFI mit den Zugangsdaten aus dem EEPROM
    display.drawString(0, 0, "Connect to");
    display.drawString(0, 10, essid.c_str());
    display.display();

    Serial.println("");

    if (WaitForConnection(10))
    {
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(essid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      display.drawString(0, 20, "success");
      display.drawString(0, 30, "Local IP: " + WiFi.localIP().toString());
      display.display();
      timer = millis();

      server.on("/", handleRoot);
      server.on("/ir", handleIr);
      server.on("/out", handleOut);
      server.on("/deletepass", handleDeletePass);
      server.on("/receiveir", handleReceiveIr);
      server.on("/files", handleFiles);
      server.on("/formatspiffs", handleFormatSpiffs);
      server.on("/rf", handleRf);

      server.onNotFound(handleNotFound);

      server.begin();
      Serial.println("HTTP server started");
      return;
    }
  }
  else
  {
    display.drawString(0, 0, "No WLAN Settings saved.");
    display.drawString(0, 10, "Connect to WLAN");
  }

  display.drawString(0, 20, "failed! Start Access Point.");
  display.display();

  setupAP();  //wenn keine Verbindung zum WLAN hergestellt werden konnte, wird der Accespoint aufgespannt.
}

/*
   Zeigt eine Webseite mit allen Dateien im SPIFFS an
   z.B.: http://ip.of.your.device/files

   oder zeigt eine bestimmte Datei an
   z.B.: http://ip.of.your.device/files?file=/IR-Decoded10.json
*/
void handleFiles()
{
  if (server.arg("file").length() != 0)
  {
    File file = SPIFFS.open(server.arg("file"), "r");
    if (!file)
    {
      Serial.println("file open failed");
      server.send(400, "text/html", "File not found.");
    }
    else
    {
      htmlcontent = file.readString();
      if (String(file.name()).endsWith(".json"))
        server.send(200, "application/json", htmlcontent);
      else
        server.send(200, "text/html", htmlcontent);
    }
  }
  else //listet alle Files auf
  {
    Dir dir = SPIFFS.openDir("/");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);

    htmlcontent = "<html><head></head><body style='font-family: sans-serif; font-size: 12px'><h1>ESP8266 Files</h1>";
    htmlcontent += "<p>";

    server.send(200, "text/html", htmlcontent);

    while (dir.next()) {
      htmlcontent = "<a href='/files?file=" + dir.fileName() + "'>" + dir.fileName() + "</a>";
      htmlcontent += " (";
      htmlcontent += dir.fileSize();
      htmlcontent += " bytes)";
      htmlcontent += "<br>";
      server.sendContent(htmlcontent);
    }
    htmlcontent = "</p>";
    htmlcontent += "</body></html>";
    server.sendContent(htmlcontent);
    server.client().stop();
  }
}

void handleFormatSpiffs()
{
  formatSPIFFS();
  fileCounter = 0;
  irDecodedIndexShow = -1;
  File file = SPIFFS.open("/counter.txt", "w");
  if (!file)
  {
    Serial.println("counter file open failed");
  }
  else
  {
    file.print(String(fileCounter));
    file.close();
    Serial.println("counter.txt saved");
  }

  server.send(200, "text/html", "OK");
}

// Formatiert des Speicher
void formatSPIFFS()
{
  display.clear();
  Serial.println("Please wait 30 secs for SPIFFS to be formatted");
  display.drawString(0, 0, "Please wait 30 secs");
  display.drawString(0, 10, "for SPIFFS to be formatted");
  display.display();

  SPIFFS.format();

  display.drawString(0, 25, "Spiffs formatted");
  Serial.println("Spiffs formatted");
  display.display();
  delay(1000);
  display.clear();
}

/*
   Wartet die gegebene Zeit t(in s)ab, bis die Verbindung zum WLAN besteht.
   gibt "true" zurück, wenn die Verbindung besteht.
   Anderenfalls wird "false" zurück gegeben.
*/
bool WaitForConnection(int t)
{
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    counter++;
    if (counter > t * 2)
      return false;
    delay(500);
    Serial.print(".");
    display.drawString(50 + counter * 3, 0, ".");
    display.display();

  }
  return true;
}

/*
    ////////////////////////////////////   SETUP Access Point  ///////////////////////////////////////////

    Startet einen AP.
    Settings:
        SSID = "ESP8266 for RCoid Access Point"
        Pass = "passpass"
        IP   = "192.168.0.1"

    Wird ausgeführt, wenn der ESP keine Verbindung zum WLAN aufbauen konnte.

*/

void setupAP(void) {
  WiFi.mode(WIFI_AP_STA);

  serial_print_Networks();

  IPAddress apIP(192, 168, 0, 1);

  display.drawString(0, 35, "Password: " + String(passwordAP));
  display.drawString(0, 47, "IP: " + apIP.toString());
  display.display();
  timer = millis();
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  WiFi.softAP(ssidAP, passwordAP, 3, false);
  delay(100);

  server.on("/", handleAPRoot);
  server.on("/setting", handleSetting);
  server.on("/ir", handleIr);
  server.on("/out", handleOut);
  server.on("/reset", handleReset);
  server.on("/deletepass", handleDeletePass);
  server.on("/getip", handleGetIp);
  server.on("/receiveir", handleReceiveIr);
  server.on("/files", handleFiles);
  server.on("/formatspiffs", handleFormatSpiffs);
  server.on("/rf", handleRf);

  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println("HTTP server started");
}



/*
   speicher die SSID und das Password in den EEPROM
   und versucht sich anschließend im WLAN einzuloggen.

   Wenn das gelingt wird RCoid die IP abrufen und den ESP neu startet
*/
void handleSetting()
{
  serial_print_HttpInfo();
  String qsid = server.arg("ssid");
  String qpass = server.arg("pass");
  if (qsid.length() > 0 && qpass.length() > 0)
  {
    EEPROM.write(0, qsid.length());
    EEPROM.write(1, qpass.length());
    for (unsigned int i = 0; i < qsid.length(); i++)
    {
      EEPROM.write(2 + i, qsid[i]);
    }
    for (unsigned int i = 0; i < qpass.length(); i++)
    {
      EEPROM.write(2 + qsid.length() + i, qpass[i]);
    }
    EEPROM.commit();

    htmlcontent = "<html><head><meta http-equiv=\"refresh\" content=\"0; URL=../\"></head><body style='font-family: sans-serif; font-size: 12px'>";
    htmlcontent += "OK";
    htmlcontent += "</body></html>";
    server.send(200, "text/html", htmlcontent);

    display.clear();
    display.drawString(0, 15, "Settings saved");
    display.drawString(0, 25, "AP_STA-Mode will start");
    display.drawString(0, 50, "Please reset the ESP");
    display.display();
    timer = millis();


    Serial.println("AP_STA-Modus wird aktiviert");

    WiFi.mode(WIFI_AP_STA);
    delay(100);

    WiFi.begin(qsid.c_str(), qpass.c_str());
  }
  else
  {
    handleNotFound();
  }

}



/*
   Hauptschleife
*/
void loop()
{
  if (WiFi.localIP().toString().equals("0.0.0.0") && WiFi.softAPgetStationNum() == 0)
  {
    if (millis() > timer + RESET_TIMEOUT)
    {
      handleReset();
    }
  }

  server.handleClient();
  if (irReceiver.decode(&ir_Decoded))
  {
    irDecodedIndexShow = fileCounter;
    saveJSON();
    playTone();
    displayIRdecoded(ir_Decoded);
    irReceiver.resume();
  }
  handleSwitch();
  handleTone();
  if (millis() > timer + DISPLAY_TIMEOUT)
  {
    puffer[0] = '0';
    puffer[1] = ',';
    if (pushJson)
    {
      pushJson = false;
      digitalWrite(STATUS_LED, HIGH);
      irReceiver.enableIRIn();
    }
    display.clear();
    display.setFont(ArialMT_Plain_24);
    display.drawString(20, 0, "RCoid");
    display.drawString(50, 25, "IRToy");
    display.setFont(ArialMT_Plain_10);
    IPAddress ip = WiFi.localIP();
    if (!ip.toString().equals("0.0.0.0"))
    {
      display.drawString(0, 54, "Local IP:  " + ip.toString());
    }
    display.display();
  }
}
/*
   speichert das aktuelle dekodierte Siganl als JSON Datei
*/
void saveJSON()
{
  File file = SPIFFS.open("/IR-Decoded" + String(fileCounter) + ".json", "w");
  if (!file)
  {
    Serial.println("json file open failed");
  }
  else
  {
    file.print(getJSON());
    file.close();
    Serial.println("IR-Decoded" + String(fileCounter) + ".json saved");
    fileCounter++;
    file = SPIFFS.open("/counter.txt", "w");
    if (!file)
    {
      Serial.println("counter file open failed");
    }
    else
    {
      file.print(String(fileCounter));
      file.close();
      Serial.println("counter.txt saved");
    }
  }
}

void handleSwitch()
{
  if (irDecodedIndexShow == -1 || pushJson)
    return;
  if (!digitalRead(UP_PORT) && !waitForRelease)
  {
    irDecodedIndexShow--;
    if (irDecodedIndexShow < 0)
      irDecodedIndexShow = 0;
    displayJSON(irDecodedIndexShow);
    waitForRelease = true;
    timer = millis();
    delay(50);
  }
  if (!digitalRead(DOWN_PORT) && !waitForRelease)
  {
    irDecodedIndexShow++;
    if (irDecodedIndexShow >=  fileCounter)
      irDecodedIndexShow = fileCounter - 1;
    displayJSON(irDecodedIndexShow);
    waitForRelease = true;
    timer = millis();
    delay(50);
  }
  if (!digitalRead(ENTER_PORT) && !waitForRelease)
  {
    digitalWrite(STATUS_LED, LOW);
    irReceiver.disableIRIn();
    sendIr(puffer);
    irReceiver.enableIRIn();
    digitalWrite(STATUS_LED, HIGH);
    waitForRelease = true;
    timer = millis();
    delay(50);
  }
  if (!digitalRead(PUSH_PORT) && !waitForRelease && pinIsInput(PUSH_PORT))
  {
    digitalWrite(STATUS_LED, LOW);
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(10, 10, "Wait for");
    display.drawString(40, 30, "RCoid");
    display.setFont(ArialMT_Plain_10);
    display.display();
    irReceiver.disableIRIn();
    pushJson = true;

    waitForRelease = true;
    timer = millis();
    delay(50);
  }
  if (waitForRelease && digitalRead(UP_PORT) && digitalRead(PUSH_PORT) && digitalRead(DOWN_PORT) && digitalRead(ENTER_PORT))
  {
    timer = millis();
    waitForRelease = false;
    delay(50);
  }

}



/*
   Zeigt die decodierten Informationen des IR-Signal auf dem Display an
*/
void displayIRdecoded(decode_results & decoded_results)
{
  display.clear();
  display.drawString(0, 0, "Protocol: " + typeToString(decoded_results.decode_type, false));
  display.drawString(0, 12, "Length: " + String(decoded_results.rawlen));
  display.drawString(0, 24, "Value: 0x" + uint64ToString(decoded_results.value, HEX));
  display.drawString(0, 36, "Address: " + String(decoded_results.address));
  display.drawString(0, 48, "Command: " + String(decoded_results.command));
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(127, 12, "#" + String(irDecodedIndexShow));
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  display.display();
  timer = millis();

}

/*
   Läd ein JSON-IR-File und zeigt die Informationen des IR-Signal auf dem Display an
*/
void displayJSON(int irDecodedIndexShow)
{
  display.clear();
  File file = SPIFFS.open("/IR-Decoded" + String(irDecodedIndexShow) + ".json", "r");
  if (!file)
  {
    Serial.println("JSON file open failed");
    display.drawString(0, 0, "JSON file open failed");
  }
  else
  {
    String temp;
    while (file.available())
    {
      temp = file.readStringUntil('"');
      if (temp.equals("Protocol"))
      {
        file.readStringUntil('"');
        display.drawString(0, 0, "Protocol: " + file.readStringUntil('"'));
      }
      else if (temp.equals("Length"))
      {
        file.readStringUntil('"');
        display.drawString(0, 12, "Length: " + file.readStringUntil('"'));
      }
      else if (temp.equals("Value"))
      {
        file.readStringUntil('"');
        display.drawString(0, 24, "Value: 0x" + file.readStringUntil('"'));
      }
      else if (temp.equals("Address"))
      {
        file.readStringUntil('"');
        display.drawString(0, 36, "Address: " + file.readStringUntil('"'));
      }
      else if (temp.equals("Command"))
      {
        file.readStringUntil('"');
        display.drawString(0, 48, "Command: " + file.readStringUntil('"'));
      }
      else if (temp.equals("RCoid IR Code"))
      {
        file.readStringUntil('"');
        (file.readStringUntil('"') + ",0").toCharArray(puffer, 1024);
        break;
      }
    }
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    display.drawString(127, 12, "#" + String(irDecodedIndexShow));
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    file.close();
  }

  display.display();
  timer = millis();
}

boolean pinIsInput(uint8_t pin)
{
  if (pin >= NUM_DIGITAL_PINS) return (false);

  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint32_t *reg = portModeRegister(port);
  if (*reg & bit)
    return false;
  return true;
}

void addTone(int f, int t)
{
  if (t == 0)
    return;
  int i = 0;
  while (tones[i + 1] != 0 && i < 20)
    i += 2;

  if (i < 19)
  {
    tones[i] = f;
    tones[i + 1] = t;
    if (i == 0)
    {
      tone(SPK_PORT, f, t);
      tones[i + 1] += millis();
    }
  }
}
void handleTone()
{

  if (tones[1] == 0)
    return;
  if (millis() > tones[1])
  {
    for (int i = 0; i < 18 ; i += 2)
    {
      tones[i] = tones[i + 2];
      tones[i + 1] = tones[i + 3];
    }
    tones[18] = 0;
    tones[19] = 0;

    if (tones[1] > 0)
    {
      tone(SPK_PORT, tones[0], tones[1]);
      tones[1] += millis();
    }
    else
    {
      pinMode(PUSH_PORT, INPUT_PULLUP);
    }
  }
}
void addToneRepeat()
{
  addTone(294, 125);
  addTone(370, 125);
  addTone(587, 250);
}
void addToneSuccess()
{
  addTone(294, 125);
  addTone(370, 250);
}
void addToneUnknown()
{
  addTone(370, 125);
  addTone(262, 250);
}

void playTone()
{
  String protocol = typeToString(ir_Decoded.decode_type, false);
  if (protocol.equals("UNKNOWN"))
    addToneUnknown();
  else
  {
    File file = SPIFFS.open("/IR-Decoded" + String(irDecodedIndexShow - 1) + ".json", "r");
    if (!file)
    {
      Serial.println("JSON file open failed");
      display.drawString(0, 0, "JSON file open failed");
    }
    else
    {
      String temp;
      while (file.available())
      {
        temp = file.readStringUntil('"');
        if (temp.equals("Protocol"))
        {
          file.readStringUntil('"');
          if (!file.readStringUntil('"').equals(protocol))
          {
            addToneSuccess();
            file.close();
            return;
          }
        }
      }
        else if (temp.equals("Value"))
        {
          file.readStringUntil('"');
          if (!file.readStringUntil('"').equals(uint64ToString(ir_Decoded.value, HEX)))
          {
            addToneSuccess();
            file.close();
            return;
          }
        }

        else if (temp.equals("RCoid IR Code"))
        {
          break;
        }
      }
      file.close();
      addToneRepeat();
    }
  }
}


