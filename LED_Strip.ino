// Doku 1:
// https://www.xgadget.de/anleitung/arduino-ws2812b-leds-mit-fastled-ansteuern/
// Doku Lib: https://github.com/FastLED/FastLED
// ESP8266 Notes: https://github.com/FastLED/FastLED/wiki/ESP8266-notes

//#define FASTLED_ESP8266_NODEMCU_PIN_ORDER -> dann geht 3 statt D3
#define FASTLED_ESP8266_RAW_PIN_ORDER  //-> dann geht D3 statt 3
#define FASTLED_ALLOW_INTERRUPTS 0
//#define FASTLED_INTERRUPT_RETRY_COUNT 1
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "FastLED.h"

// Einstellungen
#define LED_TYPE WS2812B
#define RGB_ORDER GRB
const byte ANZAHL_LEDS = 120;
const byte PIN_LED_DATA = D3;
const byte PIN_IP_BLINK = D7;
const byte LED_DEFAULT_HELLIGKEIT = 60;
// ENDE Einstellungen

//## WiFi
const String newLine = "\n";
const byte connectionTimeoutInSekunden = 10;
// MDNSResponder mdns;
bool serverHasBegun;
bool wifiConfigMode;
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
  serverHasBegun = false;
  ledDelay(1000);
  pinMode(PIN_IP_BLINK, INPUT);
  FastLED.addLeds<LED_TYPE, PIN_LED_DATA, RGB_ORDER>(leds, ANZAHL_LEDS);
  FastLED.setBrightness(LED_DEFAULT_HELLIGKEIT);
  resetLedArrayAndShow();

  // WLAN Zugangsdaten aus EEPROM lesen
  byte ssidLaenge = leseByteAusEeprom(ssidLaengeAdresse);
  byte passwordLaenge = leseByteAusEeprom(passwordLaengeAdresse);
  byte passwortStartAdresse = ssidStartAdresse + ssidLaenge + 1;

  char ssid[ssidLaenge + 1];
  leseStringAusEeprom(ssidStartAdresse, ssidLaenge, ssid);

  char passwort[passwordLaenge + 1];
  leseStringAusEeprom(passwortStartAdresse, passwordLaenge, passwort);
  // ENDE WLAN Zugangsdaten aus EEPROM lesen

  WiFi.mode(WIFI_STA);
  WiFi.hostname("ESP8266LedStrip");
  WiFi.begin(ssid, passwort);
  unsigned long timeout = millis() + (connectionTimeoutInSekunden * 1000);
  wifiConfigMode = false;
  byte curLed = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (curLed >= ANZAHL_LEDS) {
      curLed = 0;
    }
    leds[curLed].setRGB(0, 0, 255);
    FastLED.show();
    curLed++;
    ledDelay(250);
    if (millis() > timeout) {
      // Wir konnten uns nicht mit dem angegebenen WLAN verbinden.
      wifiConfigMode = true;
      break;
    }
  }

  if (wifiConfigMode) {
    // Eigenes WLAN aufspannen
    WiFi.mode(WIFI_AP);
    IPAddress ip(192, 168, 0, 1);
    IPAddress mask(255, 255, 255, 0);
    WiFi.softAPConfig(ip, ip, mask);
    WiFi.softAP("LED RGB", "ledConfig");
    setLedArrayAndShow(255, 0, 0);
    ledDelay(1000);
  } else {
    setLedArrayAndShow(0, 255, 0);
    ledDelay(1000);
  }
  resetLedArrayAndShow();
  // mdns.begin("esp8266", WiFi.localIP());

  if (wifiConfigMode) {
    server.on("/", handleWlanKonfiguration);
  } else {
    server.on("/", handleIndex);
    server.on("/wlan", handleWlanKonfiguration);
  }

  server.begin();
  serverHasBegun = true;
}

void loop() {
  if (serverHasBegun) {
    server.handleClient();
  }
  if (wifiConfigMode) {
    return;
  }
  runTestmode();
  ledDelay(3000);
  if (serverHasBegun) {
    server.handleClient();
  }
}

void ledDelay(int ms) {
  if (ms < 0)
    return;
  int maxMsDelay = 50;
  while (ms > 0) {
    ms = ms - maxMsDelay * 4;
    delay(maxMsDelay * 3);
  }
  if (serverHasBegun) {
    server.handleClient();
  }
}

void runTestmode() {
  runTestmodeWithColor(255, 0, 0);
  resetLedArrayAndShow();
  runTestmodeWithColor(0, 255, 0);
  resetLedArrayAndShow();
  runTestmodeWithColor(0, 0, 255);
  resetLedArrayAndShow();
}

void runTestmodeWithColor(byte r, byte g, byte b) {
  for (byte i = 0; i < ANZAHL_LEDS; i++) {
    leds[i].setRGB(r, g, b);
    FastLED.show();
    ledDelay(150);
  }
}

void resetLedArrayAndShow() {
  setLedArrayAndShow(0, 0, 0);
}

void setLedArray(byte r, byte g, byte b) {
  for (byte i = 0; i < ANZAHL_LEDS; i++) {
    leds[i].setRGB(r, g, b);
  }
}

void setLedArrayAndShow(byte r, byte g, byte b) {
  setLedArray(r, g, b);
  FastLED.show();
}

void checkIpBlink() {
  /*if(digitalRead(PIN_IP_BLINK) == LOW) return;
IPAddress addr  = WiFi.localIP();
for(byte i = 0; i < 4; i++) {
  unsigned int curNum[i] = addr[i];
  //blinkCurIpDigit(i, curNum);
}*/
}

void blinkCurIpDigit(byte pos, unsigned int num) {
  for (byte i = 0; i < num; i++) {
    resetLedArrayAndShow();
    leds[pos].setRGB(0, 255, 0);
    FastLED.show();
    ledDelay(500);
  }
  resetLedArrayAndShow();
  FastLED.show();
  ledDelay(1000);
}

void handleIndex() {
  String html =
      F("<!DOCTYPE html> <html lang=\"de\"> <head> <meta charset=\"UTF-8\"> "
        "<title>Benedikts LED Konfiguration</title> <link rel=\"stylesheet\" "
        "href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\"> <link "
        "rel=\"stylesheet\" "
        "href=\"http://code.ionicframework.com/ionicons/2.0.1/css/"
        "ionicons.min.css\"> <meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1\"> </head> <body> "
        "<h1>Benedikts LED Konfiguration</h1> <div class=\"pure-g\"> <div "
        "class=\"pure-u-1 pure-u-md-1-1\"> <h2>Farbe ändern</h2> <fieldset> "
        "<legend>Vordefinierte Farbe zeigen</legend> <form "
        "class=\"pure-form\"> <select name=\"sColor\"> <option name=\"sAus\" "
        "value=\"300300300\">LEDs ausschalten</option> <option name=\"sRot\" "
        "value=\"255300300\">Rot</option> <option name=\"sGruen\" "
        "value=\"300255300\">Grün</option> <option name=\"sBlau\" "
        "value=\"300300255\">Blau</option> <option name=\"sWeiss\" "
        "value=\"255255255\">Weiß</option> <option name=\"sOrange\" "
        "value=\"255165300\">Orange</option> </select> <br> <input "
        "type=\"number\" min=\"0\" max=\"100\" name=\"sHelligkeit\" id=\"1\" "
        "autocomplete=\"off\" placeholder=\"Helligkeit in Prozent\" required> "
        "<br> <button type=\"submit\" class=\"pure-button "
        "ion-ios-pulse-strong\"> LEDs schalten</button> </form> </fieldset> "
        "<br> <fieldset> <legend>RGB-Farbe definieren</legend> <form "
        "class=\"pure-form\"> <input type=\"number\" name=\"rot\" "
        "autocomplete=\"off\" placeholder=\"Rot\" min=\"0\" max=\"255\" "
        "required> <input type=\"number\" name=\"gruen\" autocomplete=\"off\" "
        "placeholder=\"Grün\" min=\"0\" max=\"255\" required> <input "
        "type=\"number\" name=\"blau\" autocomplete=\"off\" "
        "placeholder=\"Blau\" min=\"0\" max=\"255\" required> <br> <button "
        "type=\"submit\" class=\"pure-button ion-ios-pulse-strong\"> LEDs "
        "schalten</button> </form> </fieldset> </div> <div class=\"pure-u-1 "
        "pure-u-md-1-1\"> <h2>Einstellungen</h2> <p> <a href=\"/wlan\" "
        "class=\"pure-button\"> <i class=\"ion-wifi\"></i> WLAN </a> </p> "
        "</div> <div class=\"pure-u-1 pure-u-md-1-1\"> <h2>Status</h2> <p>");
  html += String(ESP.getFreeHeap());
  html += F(" freier Heap</p> </div> </div> </body> </html>");

  server.send(200, "text/html", html);
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
    // SSID in EEPROM speichern
    byte laengeSsid = ssid.length();
    byte langePasswort = passwort.length();
    if (laengeSsid > 128 || langePasswort > 128) {
      server.send(501, "text/plain",
                  "SSID oder Passwort war laenger als 128 Zeichen!");
      return;
    }
    spechereByteInEeprom(laengeSsid, ssidLaengeAdresse);
    spechereByteInEeprom(langePasswort, passwordLaengeAdresse);
    speichereStringInEeprom(ssid, ssidStartAdresse);
    byte passwortStartAdresse = ssidStartAdresse + laengeSsid + 1;
    speichereStringInEeprom(passwort, passwortStartAdresse);
    // Lesen zum 1zu1 ausgeben:
    byte ssidLaengeR = leseByteAusEeprom(ssidLaengeAdresse);
    byte passwordLaengeR = leseByteAusEeprom(passwordLaengeAdresse);
    byte passwortStartAdresseR = ssidStartAdresse + ssidLaengeR + 1;

    char ssidR[ssidLaengeR + 1];
    leseStringAusEeprom(ssidStartAdresse, ssidLaengeR, ssidR);

    char passwortR[passwordLaengeR + 1];
    leseStringAusEeprom(passwortStartAdresseR, passwordLaengeR, passwortR);

    // WLAN Restart
    String html = F(
        "<!DOCTYPE html> <html lang=\"de\"> <head> <meta charset=\"UTF-8\"> "
        "<link rel=\"stylesheet\" "
        "href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\"> <meta "
        "name=\"viewport\" content=\"width=device-width, initial-scale=1\"> "
        "<title>WLAN gespeichert</title> </head> <body> <div class=\"pure-g\"> "
        "<div class=\"pure-u-1 pure-u-md-1-1\"> <h1>WLAN gespeichert</h1> "
        "<p>SSID und Passwort wurden gespeichert.</p> <p><b>Entfernen Sie für "
        "circa 5 Sekunden den Stromstecker und verbinden Sie ihn danach "
        "wieder, damit der Microcontroller neu startet!</b></p> <p>Er wird "
        "sich danach mit dem angegebenen WLAN verbinden:</p> <p>SSID: '");
    html += String(ssidR);
    html += F("'</p> <p>Passwort: '");
    html += String(passwortR);
    html +=
        F("'</p> <p>Jeweils ohne die '-Zeichen zu Beginn / Ende)</p> </div> "
          "</div> </body> </html>");

    server.send(200, "text/html", html);
    delay(5000);  // server.send bearbeiten
    while (true) {
      delayMicroseconds(1000);  // Keine HTTP Anfragen mehr bearbeiten. IC soll
                                // ja neu gestartet werden
    }
  }
  // WLAN Daten eingeben
  String html =
      F("<!DOCTYPE html> <html lang=\"de\"> <head> <meta charset=\"UTF-8\"> "
        "<title>WLAN-Konfiguration</title> <link rel=\"stylesheet\" "
        "href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\"> <link "
        "rel=\"stylesheet\" "
        "href=\"http://code.ionicframework.com/ionicons/2.0.1/css/"
        "ionicons.min.css\"> <meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1\"> </head> <body> <div "
        "class=\"pure-g\"> <div class=\"pure-u-1 pure-u-md-1-1\"> <h1>LED WLAN "
        "Konfiguration</h1> <p>Die SSID und Passwort dürfen jeweils eine Länge "
        "von 128 Zeichen nicht überschreiten.</p> <fieldset> <legend>WLAN "
        "Zugangsdaten eingeben</legend> <form class=\"pure-form\"> <p>");
  html += getWlanNetzwerke();
  html +=
      F("</p> <input type=\"password\" name=\"passwort\" required autofocus "
        "placeholder=\"Passwort\"> <br> <button type=\"submit\" "
        "class=\"pure-button ion-checkmark-round\"> Bestätigen</button> "
        "</form> </fieldset> </div> </div> </body> </html>");

  server.send(200, "text/html", html);
}

String getWlanNetzwerke() {
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    return "<b>Keine Netzwerke gefunden</b><br>";
  }
  String n = "<b>" + String(numSsid) +
             " Netzwerke gefunden </b><br><select name=\"ssid\">";
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    n += "<option>" + String(WiFi.SSID(thisNet)) + "</option>";
  }
  n += "</select>";
  return n;
}

bool speichereStringInEeprom(String value, int startAdresse) {
  EEPROM.begin(eepromUsedBytes);
  int curAdresse = startAdresse;
  for (int i = 0; i < value.length(); i++) {
    char curChar = value[i];
    EEPROM.write(curAdresse, curChar);
    curAdresse++;
  }
  bool success = EEPROM.commit();
  EEPROM.end();
  return success;
}

void leseStringAusEeprom(
    int startAdresse,
    int laenge,
    char* outResultChar) {  // outResultChar Länge = laenge + 1 !!!!
  EEPROM.begin(eepromUsedBytes);
  outResultChar[laenge] = '\0';  // Nullterminierung: Wenn laenge 4 ist, geht
                                 // Array Index von 0-4 (laenge + 1). Dabei ist
                                 // Index 0-3 EEPROM, Index 4 null-Terminierung
  for (int i = 0; i < laenge; i++) {
    outResultChar[i] = EEPROM.read(startAdresse + i);
  }
}

bool spechereByteInEeprom(byte wert, int adresse) {
  EEPROM.begin(eepromUsedBytes);
  EEPROM.write(adresse, wert);
  bool success = EEPROM.commit();
  EEPROM.end();
  return success;
}

byte leseByteAusEeprom(int adresse) {
  EEPROM.begin(eepromUsedBytes);
  return EEPROM.read(adresse);
}
