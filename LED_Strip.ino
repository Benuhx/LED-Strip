//Doku 1: https://www.xgadget.de/anleitung/arduino-ws2812b-leds-mit-fastled-ansteuern/
//Doku Lib: https://github.com/FastLED/FastLED
//ESP8266 Notes: https://github.com/FastLED/FastLED/wiki/ESP8266-notes

//#define FASTLED_ESP8266_NODEMCU_PIN_ORDER -> dann geht 3 statt D3
#define FASTLED_ESP8266_RAW_PIN_ORDER //-> dann geht D3 statt 3
//#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 1
#include "FastLED.h"
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

//Einstellungen
#define LED_TYPE WS2812B
#define RGB_ORDER GRB
const byte ANZAHL_LEDS = 120;
const byte PIN_LED_DATA = D3;
const byte LED_DEFAULT_HELLIGKEIT = 15;
//ENDE Einstellungen

//## WiFi
const String newLine = "\n";
const byte connectionTimeoutInSekunden = 20;
MDNSResponder mdns;
ESP8266WebServer server(80);
//## ENDE WiFi

//## EEPROM Adressen
const byte ssidLaengeAdresse = 0;
const byte passwordLaengeAdresse = 1;
const byte ssidStartAdresse = passwordLaengeAdresse + 1;
byte passwortStartAdresse;
const byte eepromUsedBytes = 195;
//##ENDE EEPROM Adressen

CRGB leds[ANZAHL_LEDS];

void setup() {
  FastLED.addLeds<LED_TYPE, PIN_LED_DATA, RGB_ORDER>(leds, ANZAHL_LEDS);
  FastLED.setBrightness(LED_DEFAULT_HELLIGKEIT);
  for (int i = 0; i < ANZAHL_LEDS; i++) {
    leds[i].setRGB(0, 0, 0);
  }
  FastLED.show();

  //WLAN Zugangsdaten aus EEPROM lesen
  byte ssidLaenge = LeseByteAusEeprom(ssidLaengeAdresse);
  byte passwordLaenge = LeseByteAusEeprom(passwordLaengeAdresse);
  byte passwortStartAdresse = ssidStartAdresse + ssidLaenge + 1;

  char ssid[ssidLaenge + 1];
  LeseStringAusEeprom(ssidStartAdresse, ssidLaenge, ssid);

  char passwort[passwordLaenge + 1];
  LeseStringAusEeprom(passwortStartAdresse, passwordLaenge, passwort);
  //ENDE WLAN Zugangsdaten aus EEPROM lesen

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, passwort);
  unsigned long timeout = millis()  + (connectionTimeoutInSekunden * 1000);
  bool wifiConfigMode = false;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    if (millis() > timeout) {
      //Wir konnten uns nicht mit dem angegebenen WLAN verbinden.
      wifiConfigMode = true;
      break;
    }
  }
  if (wifiConfigMode) {
    //Eigenes WLAN aufspannen
    WiFi.mode(WIFI_AP);
    IPAddress ip(192, 168, 0, 1);
    IPAddress mask(255, 255, 255, 0);
    WiFi.softAPConfig(ip, ip, mask);
    WiFi.softAP("LED RGB", "setupLedStrip2017");
  }

  mdns.begin("esp8266", WiFi.localIP());

  if (wifiConfigMode) {
    server.on("/", handleWlanKonfiguration);
  } else {
    server.on("/wlan", handleWlanKonfiguration);
  }

  server.begin();
}

void loop() {
  server.handleClient();
  yield();
  //RunTestmode();
}

void RunTestmode() {
  RunTestmodeWithColor(255, 0, 0);
  RunTestmodeWithColor(0, 255, 0);
  RunTestmodeWithColor(0, 0, 255);
}

void RunTestmodeWithColor(byte r, byte g, byte b) {
  for (byte i = 0; i < ANZAHL_LEDS; i++) {
    if (i == 0) {
      leds[ANZAHL_LEDS - 1].setRGB(0, 0, 0);
    } else {
      leds[i - 1].setRGB(0, 0, 0);
    }
    leds[i].setRGB(r, g, b);
    FastLED.show();
    delay(300);
  }
}

void handleWlanKonfiguration() {
  if (server.args() == 2) {
    String ssid = server.arg("ssid");
    String passwort = server.arg("passwort");
    ssid.replace("+", " ");
    passwort.replace("+", " ");
    if (ssid.length() < 1 || passwort.length() < 1) {
      server.send(501, "text/plain", "SSID oder Passwort Laenge war 0!");
      return;
    }
    //SSID in EEPROM speichern
    byte laengeSsid = ssid.length();
    byte langePasswort = passwort.length();
    if (laengeSsid > 128 || langePasswort > 128) {
      server.send(501, "text/plain", "SSID oder Passwort war laenger als 128 Zeichen!");
      return;
    }
    SpechereByteInEeprom(laengeSsid, ssidLaengeAdresse);
    SpechereByteInEeprom(langePasswort, passwordLaengeAdresse);
    SpeichereStringInEeprom(ssid, ssidStartAdresse);
    byte passwortStartAdresse = ssidStartAdresse + laengeSsid + 1;
    SpeichereStringInEeprom(passwort, passwortStartAdresse);
    //Lesen zum 1zu1 ausgeben:
    byte ssidLaengeR = LeseByteAusEeprom(ssidLaengeAdresse);
    byte passwordLaengeR = LeseByteAusEeprom(passwordLaengeAdresse);
    byte passwortStartAdresseR = ssidStartAdresse + ssidLaengeR + 1;

    char ssidR[ssidLaengeR + 1];
    LeseStringAusEeprom(ssidStartAdresse, ssidLaengeR, ssidR);

    char passwortR[passwordLaengeR + 1];
    LeseStringAusEeprom(passwortStartAdresseR, passwordLaengeR, passwortR);

    //WLAN gespeichert
    String html = F("<!DOCTYPE html>");
    html += newLine;
    html += F("<html lang=\"de\">");
    html += newLine;
    html += F("<head>");
    html += newLine;
    html += F("<meta charset=\"UTF-8\">");
    html += newLine;
    html += F("<link rel=\"stylesheet\" href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\">");
    html += newLine;
    html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    html += newLine;
    html += F("<title>WLAN gespeichert</title>");
    html += newLine;
    html += F("</head>");
    html += newLine;
    html += F("<body>");
    html += newLine;
    html += F("<div class=\"pure-g\">");
    html += newLine;
    html += F("<div class=\"pure-u-1 pure-u-md-1-1\">");
    html += newLine;
    html += F("<h1>WLAN gespeichert</h1>");
    html += newLine;
    html += F("<p>SSID und Passwort wurden gespeichert.</p>");
    html += newLine;
    html += F("<p><b>Entfernen Sie für circa 5 Sekunden den Stromstecker und verbinden Sie ihn danach wieder, damit der Microcontroller neu startet!</b></p>");
    html += newLine;
    html += F("<p>Er wird sich danach mit dem angegebenen WLAN verbinden:</p>");
    html += newLine;
    html += "<p>SSID: '" + String(ssidR) + "'</p>";
    html += newLine;
    html += "<p>Passwort: '" + String(passwortR) + "'</p>";
    html += newLine;
    html += F("<p>Jeweils ohne die '-Zeichen zu Beginn / Ende)</p>");
    html += newLine;
    html += F("</div>");
    html += newLine;
    html += F("</div>");
    html += newLine;
    html += F("</body>");
    html += newLine;
    html += F("</html>");
    html += newLine;

    server.send(200, "text/html", html);
    delay(5000); //server.send bearbeiten
    while (true) {
      delayMicroseconds(1000); //Keine HTTP Anfragen mehr bearbeiten. IC soll ja neu gestartet werden
    }
  }
  //WLAN Daten eingeben
  String html = F("<!DOCTYPE html>");
  html += newLine;
  html += F("<html lang=\"de\">");
  html += newLine;
  html += F("<head>");
  html += newLine;
  html += F("<meta charset=\"UTF-8\">");
  html += newLine;
  html += F("<title>WLAN-Konfiguration</title>");
  html += newLine;
  html += F("<link rel=\"stylesheet\" href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\">");
  html += newLine;
  html += F("<link rel=\"stylesheet\" href=\"http://code.ionicframework.com/ionicons/2.0.1/css/ionicons.min.css\">");
  html += newLine;
  html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  html += newLine;
  html += F("</head>");
  html += newLine;
  html += F("<body>");
  html += newLine;
  html += F("<div class=\"pure-g\">");
  html += newLine;
  html += F("<div class=\"pure-u-1 pure-u-md-1-1\">");
  html += newLine;
  html += F("<h1>LED Strip</h1>");
  html += newLine;
  html += F("<p>Die SSID und Passwort dürfen jeweils eine Länge von 128 Zeichen nicht überschreiten.</p>");
  html += newLine;
  html += "<p>" + GetWlanNetzwerke() + "</p>";
  html += newLine;
  html += F("<fieldset>");
  html += newLine;
  html += F("<legend>WLAN Zugangsdaten eingeben</legend>");
  html += newLine;
  html += F("<form class=\"pure-form\">");
  html += newLine;
  html += F("<input type=\"text\" name=\"ssid\" required autofocus placeholder=\"SSID\">");
  html += newLine;
  html += F("<input type=\"password\" name=\"passwort\" required autofocus placeholder=\"Passwort\">");
  html += newLine;
  html += F("<br>");
  html += newLine;
  html += F("<button type=\"submit\" class=\"pure-button ion-checkmark-round\"> Bestätigen</button>");
  html += newLine;
  html += F("</form>");
  html += newLine;
  html += F("</fieldset>");
  html += newLine;
  html += F("</div>");
  html += newLine;
  html += F("</div>");
  html += newLine;
  html += F("</body>");
  html += newLine;
  html += F("</html>");
  html += newLine;


  server.send(200, "text/html", html);
}

String GetWlanNetzwerke() {  
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    return "<b>Keine Netzwerke gefunden</b><br>";
  }
  String n = "<b>" + String(numSsid) + " Netzwerke gefunden </b><br>";
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    n += String(WiFi.SSID(thisNet)) + "<br>";
  }
  return n;
}

bool SpeichereStringInEeprom(String value, int startAdresse) {
  EEPROM.begin(eepromUsedBytes);
  int curAdresse = startAdresse;
  for (int i = 0; i < value.length(); i++) {
    char curChar = value[i];
    EEPROM.write(curAdresse, curChar);
    curAdresse++;
  }
  bool success =  EEPROM.commit();
  EEPROM.end();
  return success;
}

void LeseStringAusEeprom(int startAdresse, int laenge, char *outResultChar) { //outResultChar Länge = laenge + 1 !!!!
  EEPROM.begin(eepromUsedBytes);
  outResultChar[laenge] = '\0'; //Nullterminierung: Wenn laenge 4 ist, geht Array Index von 0-4 (laenge + 1). Dabei ist Index 0-3 EEPROM, Index 4 null-Terminierung
  for (int i = 0; i < laenge; i++) {
    outResultChar[i] = EEPROM.read(startAdresse + i);
  }
}

bool SpechereByteInEeprom(byte wert, int adresse) {
  EEPROM.begin(eepromUsedBytes);
  EEPROM.write(adresse, wert);
  bool success =  EEPROM.commit();
  EEPROM.end();
  return success;
}

byte LeseByteAusEeprom(int adresse) {
  EEPROM.begin(eepromUsedBytes);
  return EEPROM.read(adresse);
}
