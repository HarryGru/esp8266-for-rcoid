#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <EEPROM.h>

#define IR_PORT 0
#define IR_PORT_INVERT false
#define IR_RECEIVER_PORT 2


// Netzwerkinformationen für Accesspoint
// Im AP-Modus ist der ESP8266 unter der IP 192.168.0.1 erreichbar
const char* ssidAP = "ESP8266 for RCoid Access Point";
const char* passwordAP = "passpass";  //Muss mindestens 8 Zeichen haben

char ir[1024];
String htmlcontent;
ESP8266WebServer server(80);

IRrecv irReceiver(IR_RECEIVER_PORT);
decode_results irDecoded;

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

  pinMode(IR_PORT, OUTPUT);

  if (server.argName(0).equals("code"))
  {
    (server.arg(0) + ",0").toCharArray(ir, 1024);

    char *p; //Zeiger im Array
    unsigned int frequence = strtol(ir, &p, 10);
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
  else
  {
    handleNotFound();
    return;
  }
  htmlcontent = "OK";
  server.send(200, "text/plain", htmlcontent);


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
  htmlcontent = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  server.send(200, "text/plain", htmlcontent);
  Serial.println("Get IP = " + htmlcontent);
}




/*
  wartet eine Zeit ab, bis am Receiver ein IR Signal decodiert wurde
  blockiert den ESP
*/
void handleReceiveIr()
{

  irReceiver.enableIRIn();  // Start the receiver
  unsigned long start = millis();

  while (millis() < start + 30000)
  {
    if (irReceiver.decode(&irDecoded))
    {
      htmlcontent = "{\n";
      htmlcontent += "  \"Protocol\" : ";
      htmlcontent += "\"";
      htmlcontent += typeToString(irDecoded.decode_type, false);
      htmlcontent += "\",\n";
      htmlcontent += "  \"Value\" : \"";
      htmlcontent += uint64ToString(irDecoded.value, HEX);
      htmlcontent += "\",\n";
      htmlcontent += "  \"Length\" : \"";
      htmlcontent += irDecoded.rawlen;
      htmlcontent += "\",\n";
      htmlcontent += "  \"Address\" : \"";
      htmlcontent += irDecoded.address;
      htmlcontent += "\",\n";
      htmlcontent += "  \"Command\" : \"";
      htmlcontent += irDecoded.command;
      htmlcontent += "\",\n";
      htmlcontent += "  \"RCoid IR Code\" : \"";
      int freq = 38000;
      if (typeToString(irDecoded.decode_type, false).equals("SONY"))
        freq = 40000;
      htmlcontent += freq;
      for (int i = OFFSET_START; i < irDecoded.rawlen; i++)
      {
        htmlcontent += ",";
        htmlcontent += (int)(((irDecoded.rawbuf[i] * RAWTICK) * freq) / 1000000);
      }
      if (irDecoded.rawlen % 2 == 0)
      {
        htmlcontent += ",1";
      }
      htmlcontent += "\"\n";
      htmlcontent += "}";

      Serial.println(htmlcontent);

      server.send(200, "application/json", htmlcontent);

      irReceiver.resume();  // Receive the next value
      irReceiver.disableIRIn();  // Stopps the receiver

      return;
    }
    delay(100);
  };

  server.send(408, "text/plain", "No IR Signal received!");

}



/*
   ////////////////////////////////////   SETUP   ///////////////////////////////////////////
*/
void setup(void) {

  pinMode(IR_PORT, OUTPUT);
  digitalWrite(IR_PORT, IR_PORT_INVERT ? HIGH : LOW);

  EEPROM.begin(512);
  Serial.begin(115200);

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
  Serial.print("PASS: ");
  Serial.println(epass);


  if (essid.length() > 1)
  {

    WiFi.mode(WIFI_STA);
    WiFi.begin(essid.c_str(), epass.c_str());  //Starte WIFI mit den Zugangsdaten aus dem EEPROM
    Serial.println("");

    if (WaitForConnection(10))
    {
      Serial.println("");
      Serial.print("Connected to ");
      Serial.println(essid);
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      server.on("/", handleRoot);
      server.on("/ir", handleIr);
      server.on("/out", handleOut);
      server.on("/deletepass", handleDeletePass);
      server.on("/receiveir", handleReceiveIr);
      server.onNotFound(handleNotFound);

      server.begin();
      Serial.println("HTTP server started");
      return;
    }
  }
  setupAP();  //wenn keine Verbindung zum WLAN hergestellt werden konnte, wird der Accespoint aufgespannt.
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
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  WiFi.softAP(ssidAP, passwordAP, 3, false);
  delay(100);

  server.on("/", handleAPRoot);
  server.on("/setting", handleSetting);
  server.on("/ir", handleIr);
  server.on("/out", handleOut);
  server.on("/reset", handleReset);
  server.on("/getip", handleGetIp);
  server.on("/receiveir", handleReceiveIr);

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
  server.handleClient();
}
