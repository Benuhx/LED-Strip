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
#define LED_TYPE WS2811
#define RGB_ORDER GRB
const byte ANZAHL_LEDS = 119;
const byte PIN_LED_DATA = D3;
const byte PIN_HALL_SENSOR = A0;
const byte LED_DEFAULT_HELLIGKEIT = 60;
const byte ANZAHL_HALL_MESSUNGEN = 3;
// ENDE Einstellungen

// Hall Sensor
int kalibrierterHallWert;
int aktuellerHallWert;
byte hallDiff;
//

//## WiFi
const String newLine = "\n";
const byte connectionTimeoutInSekunden = 15;
bool serverHasBegun;
bool wifiConfigMode;
const byte MAX_LAENGE_SSID_PASS = 128;
char ssid[MAX_LAENGE_SSID_PASS];
char passwort[MAX_LAENGE_SSID_PASS];
IPAddress ipAdresse;
ESP8266WebServer server(80);
//## ENDE WiFi

//## RGB Slots
byte slot1r;
byte slot1g;
byte slot1b;

byte slot2r;
byte slot2g;
byte slot2b;

byte slot3r;
byte slot3g;
byte slot3b;
// ENDE RGB Slots

//## EEPROM Adressen
const byte ssidLaengeAdresse = 0;
const byte passwordLaengeAdresse = 1;
const byte ssidStartAdresse = passwordLaengeAdresse + 1;
byte passwortStartAdresse;

int slot1rAdresse;
int slot1gAdresse;
int slot1bAdresse;

int slot2rAdresse;
int slot2gAdresse;
int slot2bAdresse;

int slot3rAdresse;
int slot3gAdresse;
int slot3bAdresse;

int hallDiffAdresse;
const byte eepromUsedBytes = 195;
//##ENDE EEPROM Adressen

CRGB leds[ANZAHL_LEDS];

void setup() {
  Serial.begin(9600);
  Serial.println("Hallo :)");
  serverHasBegun = false;
  ledDelay(1000);
  pinMode(PIN_HALL_SENSOR, INPUT);
  FastLED.addLeds<LED_TYPE, PIN_LED_DATA, RGB_ORDER>(leds, ANZAHL_LEDS);
  FastLED.setBrightness(LED_DEFAULT_HELLIGKEIT);
  resetLedArrayAndShow();

  // WLAN Zugangsdaten aus EEPROM lesen
  byte ssidLaenge = leseByteAusEeprom(ssidLaengeAdresse);
  byte passwordLaenge = leseByteAusEeprom(passwordLaengeAdresse);

  byte passwortStartAdresse = ssidStartAdresse + ssidLaenge + 1;
  leseStringAusEeprom(ssidStartAdresse, ssidLaenge, ssid);

  leseStringAusEeprom(passwortStartAdresse, passwordLaenge, passwort);
  // ENDE WLAN Zugangsdaten aus EEPROM lesen

  // u.a. RGB Slots lesen
  berechneWeitereEepromAdressen(passwortStartAdresse + passwordLaenge + 1);
  leseRgbWerteAusSlots();
  // Ende RGB Slots
  hallDiff = leseByteAusEeprom(hallDiffAdresse);
  connectToWiFi();
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
    server.on("/ledTestModus", handleLedTestModus);
    server.on("/farbeSpeichern", handleFarbeSpeichern);
  }

  server.begin();
  serverHasBegun = true;
  MDNS.begin("led", WiFi.localIP());
  kalibrierterHallWert = leseHallWert();
  Serial.println(WiFi.localIP().toString());
  ledDelay(300);
  Serial.end();
}

void loop() {
  if (serverHasBegun) {
    server.handleClient();
  }
  if (wifiConfigMode) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    yield();
    connectToWiFi();
    yield();
    delay(3000);
    yield();
    if (WiFi.status() == WL_CONNECTED) {
      server.begin();
    }
  }

  aktuellerHallWert = leseHallWert();
  hallAktionAusfuehren();

  ledDelay(3000);
  if (serverHasBegun) {
    server.handleClient();
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname("ESP8266LedStrip");
  WiFi.begin(ssid, passwort);
}

int leseHallWert() {
  int mittelwert = 0;
  for (byte i = 1; i <= ANZAHL_HALL_MESSUNGEN; i++) {
    mittelwert += analogRead(PIN_HALL_SENSOR);
    ledDelay(100);
  }
  return mittelwert / ANZAHL_HALL_MESSUNGEN;
}

void hallAktionAusfuehren() {
  if (hallDiff == 0)
    return;
  if (aktuellerHallWert >= kalibrierterHallWert + hallDiff ||
      aktuellerHallWert <= kalibrierterHallWert - hallDiff) {
    setLedArrayAndShow(0, 0, 100);
    ledDelay(1000);
    resetLedArrayAndShow();
  }
}

void ledDelay(int ms) {
  if (ms < 0)
    return;
  delay(ms);
  if (serverHasBegun) {
    server.handleClient();
  }
}

void berechneWeitereEepromAdressen(int startAdresse) {
  slot1rAdresse = startAdresse;
  slot1gAdresse = slot1rAdresse + 1;
  slot1bAdresse = slot1gAdresse + 1;

  slot2rAdresse = slot1bAdresse + 1;
  slot2gAdresse = slot2rAdresse + 1;
  slot2bAdresse = slot2gAdresse + 1;

  slot3rAdresse = slot2bAdresse + 1;
  slot3gAdresse = slot3rAdresse + 1;
  slot3bAdresse = slot3gAdresse + 1;

  hallDiffAdresse = slot3bAdresse + 1;
}

void leseRgbWerteAusSlots() {
  slot1r = leseByteAusEeprom(slot1rAdresse);
  slot1g = leseByteAusEeprom(slot1gAdresse);
  slot1b = leseByteAusEeprom(slot1bAdresse);

  slot2r = leseByteAusEeprom(slot2rAdresse);
  slot2g = leseByteAusEeprom(slot2gAdresse);
  slot2b = leseByteAusEeprom(slot2bAdresse);

  slot3r = leseByteAusEeprom(slot3rAdresse);
  slot3g = leseByteAusEeprom(slot3gAdresse);
  slot3b = leseByteAusEeprom(slot3bAdresse);
}

void runTestmode() {
  resetLedArray();
  FastLED.setBrightness(255);
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
    ledDelay(100);
  }
}

void resetLedArray() {
  setLedArray(0, 0, 0);
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

int ermittleEchteRgbWerte(int farbenCode, bool* converterErfolgreich) {
  if (farbenCode >= 100 && farbenCode <= 255) {
    // Ist schon der echte RGB-Wert
    return farbenCode;
  } else if (farbenCode == 300) {
    // Entspricht 0
    return 0;
  } else if (farbenCode > 300 && farbenCode <= 399) {
    // Echter RGB ist zwischen 1 und 99.
    return farbenCode - 300;
  }
  *converterErfolgreich = false;
  return 0;  // Fehler!
}

byte countDigits(int num) {
  byte count = 0;
  while (num) {
    num = num / 10;
    count++;
  }
  return count;
}

void handleIndex() {
  int serverArgs = server.args();
  if (serverArgs == 4) {
    byte rot = (byte)server.arg("rot").toInt();
    byte gruen = (byte)server.arg("gruen").toInt();
    byte blau = (byte)server.arg("blau").toInt();
    byte farbeAnzeigen = (byte)server.arg("b1").toInt();
    if (farbeAnzeigen == 1) {
      // Manueller RGB Wert
      FastLED.setBrightness(255);
      setLedArrayAndShow(rot, gruen, blau);
    } else {
      // Farbe speichern
      server.sendHeader(
          "Location",
          String("/farbeSpeichern?rot=" + String(rot) +
                 "&gruen=" + String(gruen) + "&blau=" + String(blau) + "&b2=1"),
          true);
      server.send(303, "text/plain", "");
    }
  } else if (serverArgs == 2) {
    // Definierter RGB Wert. Entweder Slot oder hardcoded
    int farbeCode = server.arg("sColor").toInt();
    float helligkeit = server.arg("sHelligkeit").toInt();
    // Heligkeit ist Prozentual von 0 bis 100
    // setBrightness erwartet 0 bis 255
    helligkeit = 255 * (helligkeit / 100);
    FastLED.setBrightness((byte)helligkeit);
    byte digits = countDigits(farbeCode);
    if (digits == 2) {
      // Eigener Slot. Zum Beispiel 91 für Slot 1, 92 für Slot 2....
      byte slotNummer = farbeCode % 10;
      byte r;
      byte g;
      byte b;
      switch (slotNummer) {
        case 1:
          r = slot1r;
          g = slot1g;
          b = slot1b;
          break;
        case 2:
          r = slot2r;
          g = slot2g;
          b = slot2b;
          break;
        case 3:
          r = slot3r;
          g = slot3g;
          b = slot3b;
          break;
        default:
          return;
      }
      setLedArrayAndShow(r, g, b);
    } else {
      // Vordefinierter, hardcoded Farbcode. 300300300 für 0 0 0 oder 120320340
      // für 120 20 40
      if (digits != 9) {
        server.send(501, "text/plain",
                    "Parameter sColor ist falsch. Muss 9-stellig sein!");
        return;
      }
      int rot = farbeCode / 1000000;
      int gruen = (farbeCode % 1000000) / 1000;
      int blau = farbeCode % 1000;
      bool converterErfolgreich = true;
      rot = ermittleEchteRgbWerte(rot, &converterErfolgreich);
      gruen = ermittleEchteRgbWerte(gruen, &converterErfolgreich);
      blau = ermittleEchteRgbWerte(blau, &converterErfolgreich);
      if (!converterErfolgreich) {
        server.send(501, "text/html",
                    "ermittleEchteRgbWerte ist fehlgeschlagen");
      }
      setLedArrayAndShow(rot, gruen, blau);
    }
  } else if (serverArgs == 1) {
    // Hall Schwellwert neu einstellen
    int neuerHallDiffR = server.arg("hS").toInt();
    if (neuerHallDiffR < 0 || neuerHallDiffR > 255) {
      server.send(501, "text/html",
                  "Schwellwert muss zwischen 0 und 255 liegen");
      return;
    }
    hallDiff = (byte)neuerHallDiffR;
    spechereByteInEeprom(hallDiff, hallDiffAdresse);
  }
  String html = F(
      "<!DOCTYPE html> <html lang=\"de\"> <head> <meta charset=\"UTF-8\"> "
      "<title>Benedikts LED Konfiguration</title> <link rel=\"stylesheet\" "
      "href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\"> <link "
      "rel=\"stylesheet\" "
      "href=\"http://code.ionicframework.com/ionicons/2.0.1/css/"
      "ionicons.min.css\"> <meta name=\"viewport\" "
      "content=\"width=device-width, initial-scale=1\"> </head> <body> "
      "<h1>Benedikts LED Konfiguration</h1> <div class=\"pure-g\"> <div "
      "class=\"pure-u-1 pure-u-md-1-1\"> <h2>Farbe ändern</h2> <fieldset> "
      "<legend>Vordefinierte Farbe zeigen</legend> <form class=\"pure-form\"> "
      "<select name=\"sColor\"> <option value=\"300300300\">LEDs "
      "ausschalten</option> <option value=\"255255255\">Weiß</option> <option "
      "value=\"255220360\">warmes Weiß</option> <option "
      "value=\"255300300\">Rot</option> <option "
      "value=\"300255300\">Grün</option> <option "
      "value=\"300300255\">Blau</option> <option "
      "value=\"316378139\">gemildertes Blau</option> <option "
      "value=\"365105225\">royales Blau</option> <option "
      "value=\"300300128\">navy Blau</option> <option "
      "value=\"300134139\">türkises Blau</option> <option "
      "value=\"255320300\">warmes Rot</option> <option "
      "value=\"220334301\">Ziegelstein Rot</option> <option "
      "value=\"255380300\">Orange</option> <option "
      "value=\"165342342\">Lila</option> <option value=\"255369319\">leichtes "
      "Rosa</option> <option value=\"127255340\">aqua Grün</option> <option "
      "value=\"300100300\">dunkles Grün</option> <option "
      "value=\"334150310\">Wald Grün</option> <option "
      "value=\"255220300\">Gelb</option> <option value=\"139101308\">dunkles "
      "Gelb</option> <option value=\"255215300\">Gold</option> <option "
      "value=\"91\">Slot 1 (eigene Farbe)</option> <option value=\"92\">Slot 2 "
      "(eigene Farbe)</option> <option value=\"93\">Slot 3 (eigene "
      "Farbe)</option> </select> <br> <input type=\"number\" min=\"0\" "
      "max=\"100\" name=\"sHelligkeit\" id=\"1\" autocomplete=\"off\" "
      "placeholder=\"Helligkeit in Prozent\" required> <br> <br> <button "
      "type=\"submit\" class=\"pure-button ion-ios-pulse-strong\"> LEDs "
      "schalten</button> </form> </fieldset> <br> <fieldset> <legend>Eigene "
      "RGB-Farbe definieren</legend> <form class=\"pure-form\"> <input "
      "type=\"number\" name=\"rot\" autocomplete=\"off\" placeholder=\"Rot (0 "
      "bis 255)\" min=\"0\" max=\"255\" required> <input type=\"number\" "
      "name=\"gruen\" autocomplete=\"off\" placeholder=\"Grün (0 bis 255)\" "
      "min=\"0\" max=\"255\" required> <input type=\"number\" name=\"blau\" "
      "autocomplete=\"off\" placeholder=\"Blau (0 bis 255)\" min=\"0\" "
      "max=\"255\" required> <br> <br> <button name=\"b1\" type=\"submit\" "
      "value=\"1\" class=\"pure-button ion-ios-pulse-strong\"> LEDs "
      "schalten</button> oder <button name=\"b2\" type=\"submit\" value=\"1\" "
      "class=\"pure-button ion-android-cloud-done\"> Dauerhaft "
      "speichern</button> </form> </fieldset> </div> <div class=\"pure-u-1 "
      "pure-u-md-1-1\"> <h2>Einstellungen</h2> <fieldset> <legend>Schwellwert "
      "für Magnetsensor</legend> <p>Wenn die Aktion des Magnetsensors oft "
      "unerwartet ausgeführt wird, lässt sich hier der Schwellwert erhöhen. "
      "Mit 0 lässt er sich permanent deaktivieren</p> <form "
      "class=\"pure-form\"> <input type=\"number\" name=\"hS\" "
      "autocomplete=\"off\" placeholder=\"0 bis 255\" min=\"0\" max=\"255\" "
      "required> <span>Aktueller Wert: ");
  html += String(hallDiff);
  html += F(
      ", Standard: 10</span> <br> <br> <button type=\"submit\" "
      "class=\"pure-button ion-checkmark-round\"> Speichern</button> </form> "
      "</fieldset> <br> <fieldset> <legend>Weitere Einstellungsseiten</legend> "
      "<a href=\"/wlan\" class=\"pure-button\"> <i class=\"ion-wifi\"></i> "
      "WLAN </a> <a href=\"/ledTestModus\" class=\"pure-button\"> <i "
      "class=\"ion-ios-analytics-outline\"></i> LED Testmodus </a> </fieldset> "
      "</div> <div class=\"pure-u-1 pure-u-md-1-1\"> <h2>Gespeicherte "
      "Farben</h2> <table class=\"pure-table pure-table-horizontal\"> <thead> "
      "<tr> <th>Slot</th> <th>Rot</th> <th>Grün</th> <th>Blau</th> </tr> "
      "</thead> <tbody> <tr> <td>Slot 1</td> <td>");
  html += String(slot1r);
  html += F("</td> <td>");
  html += String(slot1g);
  html += F("</td> <td>");
  html += String(slot1b);
  html += F("</td> </tr> <tr> <td>Slot 2</td> <td>");
  html += String(slot2r);
  html += F("</td> <td>");
  html += String(slot2g);
  html += F("</td> <td>");
  html += String(slot2b);
  html += F("</td> </tr> <tr> <td>Slot 3</td> <td>");
  html += String(slot3r);
  html += F("</td> <td>");
  html += String(slot3g);
  html += F("</td> <td>");
  html += String(slot3b);
  html +=
      F("</td> </tr> </tbody> </table> </div> <div class=\"pure-u-1 "
        "pure-u-md-1-1\"> <h2>Status</h2> <p>");
  html += String(ESP.getFreeHeap());
  html += F(" freier Heap</p> <p>Aktuelles magnetisches Feld: <b>");
  html += String(aktuellerHallWert);
  html += F("</b>. Der beim Booten gemessene Wert liegt bei <b>");
  html += String(kalibrierterHallWert);
  html += F("</b>. Die Aktion wird bei einem Wert von +- <b>");
  html += String(hallDiff);
  html +=
      F("</b> Differenz vom beim Booten gemessenem Wert ausgeführt</p> </div> "
        "</div> </body> </html>");

  server.send(200, "text/html", html);
}

void handleLedTestModus() {
  String html = F(
      "<!DOCTYPE html> <html lang=\"de\"> <head> <meta charset=\"UTF-8\"> "
      "<link rel=\"stylesheet\" "
      "href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\"> <link "
      "rel=\"stylesheet\" "
      "href=\"http://code.ionicframework.com/ionicons/2.0.1/css/"
      "ionicons.min.css\"> <meta name=\"viewport\" "
      "content=\"width=device-width, initial-scale=1\"> <title>LED "
      "Testmodus</title> </head> <body> <div class=\"pure-g\"> <div "
      "class=\"pure-u-1 pure-u-md-1-1\"> <h1>LED Testmodus</h1> <p>Die LEDs "
      "blinken gerade nacheinannder in jeder Farbe. Zuerst rot, dann grün und "
      "dann blau</p> <p>Nach dem Druchlauf bleiben die LEDs deaktiviert</p> <i "
      "class=\"ion-android-happy\"></i> </div> </div> </body> </html>");
  server.send(200, "text/html", html);
  ledDelay(500);
  runTestmode();
}

void handleFarbeSpeichern() {
  byte rot = (byte)server.arg("rot").toInt();
  byte gruen = (byte)server.arg("gruen").toInt();
  byte blau = (byte)server.arg("blau").toInt();
  byte farbeBestaetigen = (byte)server.arg("b2").toInt();
  byte slot = (byte)server.arg("slot").toInt();
  if (farbeBestaetigen == 0) {
    speichereRgbInEEprom(rot, gruen, blau, slot);
    server.sendHeader("Location", String("/"), true);
    server.send(303, "text/plain", "");
    return;
  }
  String html =
      F("<!DOCTYPE html> <html lang=\"de\"> <head> <meta charset=\"UTF-8\"> "
        "<link rel=\"stylesheet\" "
        "href=\"http://yui.yahooapis.com/pure/0.6.0/pure-min.css\"> <link "
        "rel=\"stylesheet\" "
        "href=\"http://code.ionicframework.com/ionicons/2.0.1/css/"
        "ionicons.min.css\"> <meta name=\"viewport\" "
        "content=\"width=device-width, initial-scale=1\"> <title>Neue Farbe "
        "speichern</title> </head> <body> <div class=\"pure-g\"> <div "
        "class=\"pure-u-1 pure-u-md-1-1\"> <h1>Neue Farbe speichern</h1> "
        "<p>Deine Farbe wird gerade angezeigt. Die Werte sind: </p> <table "
        "class=\"pure-table pure-table-horizontal\"> <thead> <tr> <th>Rot</th> "
        "<th>Grün</th> <th>Blau</th> </tr> </thead> <tbody> <tr> <td>");
  html += String(rot);
  html += F("</td> <td>");
  html += String(gruen);
  html += F("</td> <td>");
  html += String(blau);
  html +=
      F("</td> </tr> </tbody> </table> <fieldset> <form class=\"pure-form\"> "
        "<legend>Welcher Speicherslot soll genutzt werden? Falls in dem "
        "ausgewählten Slot bereits eine Farbe gespeichert ist, wird diese "
        "überschrieben</legend> <select name=\"slot\"> <option "
        "value=\"1\">Slot 1</option> <option value=\"2\">Slot 2</option> "
        "<option value=\"3\">Slot 3</option> </select> <br> <br> <input "
        "type=\"hidden\" name=\"rot\" value=\"");
  html += String(rot);
  html += F("\"> <input type=\"hidden\" name=\"gruen\" value=\"");
  html += String(gruen);
  html += F("\"> <input type=\"hidden\" name=\"blau\" value=\"");
  html += String(blau);
  html +=
      F("\"> <input type=\"hidden\" name=\"b2\" value=\"0\"> <button "
        "type=\"submit\" class=\"pure-button ion-android-cloud-done\"> Farbe "
        "speichern</button> </form> </fieldset> </div> </div> </body> </html>");
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

void speichereRgbInEEprom(byte r, byte g, byte b, byte slot) {
  int eepromR;
  int eepromG;
  int eepromB;
  switch (slot) {
    case 1:
      eepromR = slot1rAdresse;
      eepromG = slot1gAdresse;
      eepromB = slot1bAdresse;
      break;
    case 2:
      eepromR = slot2rAdresse;
      eepromG = slot2gAdresse;
      eepromB = slot2bAdresse;
      break;
    case 3:
      eepromR = slot3rAdresse;
      eepromG = slot3gAdresse;
      eepromB = slot3bAdresse;
      break;
    default:
      return;
  }
  spechereByteInEeprom(r, eepromR);
  spechereByteInEeprom(g, eepromG);
  spechereByteInEeprom(b, eepromB);
  leseRgbWerteAusSlots();
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