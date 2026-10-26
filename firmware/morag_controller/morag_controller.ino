/*
  MORAG Mobile Lighting - ESP32 controller firmware (prototype)
  ------------------------------------------------------------
  Board   : ESP32 38-pin dev board (ESP32-WROOM-32, SKU MLE05195)
            Arduino IDE board: "ESP32 Dev Module"
  Strip   : 12V WS2815, 60 LEDs/m, 5 m = 300 LEDs (SKU MLE03783)
  Link    : Wi-Fi. The ESP32 always runs its own access point (setup network,
            http://192.168.4.1). From the app you can also connect it to the
            site's Wi-Fi; it then gets a second IP address on that network,
            which the app shows and uses.
  Portal  : Joining the device AP opens a captive portal page (phone or
            laptop) with the same steps as the app: device AP, Wi-Fi setup,
            IP address, login, then the color wheel, effects and brands.
            The same page works in any browser at the device IP.
  Library : FastLED 3.9 or later (Library Manager)
            WiFi, WebServer, DNSServer, Preferences and mbedtls come with the ESP32 core
            ("esp32 by Espressif Systems" 2.0.x or 3.x)

  Defaults (change them from the app's profile screen)
    Device AP name     : MORAG-Light
    Device AP password : morag2026    (8+ characters, WPA2)
    App login          : admin / morag1234

  Factory reset: while the controller is running, hold the BOOT button
  (GPIO 0) for 5 seconds. It restores the default AP and login,
  forgets the site Wi-Fi, then restarts.
  (Do not hold BOOT while powering on: that starts the firmware-upload mode.)

  Wiring
    GPIO 16 -> 74AHCT125 (powered at 5V) -> 330 ohm -> strip DI
    strip BI -> GND
    strip +12V / GND -> 12V 10A supply (also feed the far end)
    ESP32 5V/VIN -> 12V-to-5V buck converter, GND shared with supply

  HTTP API (POST, form-encoded; t=<token> from /api/login where marked)
    /api/info                                     -> JSON: AP, site Wi-Fi status, IP (no login)
    /api/provision ssid, pass                     join the site Wi-Fi (no login needed when
                                                  sent over the device AP; t=<token> otherwise)
    /api/login    user, pass                      -> {"token":"..."} or 401
    /api/logout   t
    /api/cmd      t, c=<command line>             -> OK,... | ERR,... | STATE,...
    /api/status   t                               -> STATE,...
    /api/account  t, old, user, pass              change login
    /api/wifi     t, ssid, pass                   change the device AP, controller restarts
    /api/scan     (no login on the device AP)     -> {"status":"scanning"} or {"networks":[...]}
    /api/brands   t                               -> brands saved on the controller (JSON)
    /api/brands/save t, data=<JSON array>         save brands (max 3800 bytes)
    /             the portal page; any other address on the device AP redirects to it

  Commands (same text format as the USB serial monitor)
    POWER,1 | POWER,0
    COLOR,#RRGGBB
    BRIGHT,0-100
    EFFECT,<solid|breathe|wipe|chase|gradient|rainbow>,<speed 1-100>
    PRESET,<name>,<effect>,<speed>,<bright>,#c1[,#c2[,#c3]]
    STATUS
    PING
  STATE reply: STATE,<power>,<bright>,<effect>,<speed>,#c1,#c2,#c3,<name>
*/

#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>
#include "portal_html.h"     // the captive portal page (keep this file in the same folder)

// ---------------- Settings ----------------
#define DATA_PIN        16
#define NUM_LEDS        300          // 5 m x 60 LEDs/m
#define PSU_VOLTS       12
#define PSU_MILLIAMPS   8000         // 10A supply, kept at 80%
#define SAVE_DELAY_MS   2000         // wait before saving to flash (limits wear)
#define MAX_LINE        160
#define RESET_PIN       0            // BOOT button
#define SESSION_MS      (12UL * 60 * 60 * 1000)   // a login lasts 12 hours
#define MAX_FAILS       5            // wrong passwords before a lockout
#define LOCKOUT_MS      30000        // lockout length

const char *DEFAULT_SSID = "MORAG-Light";
const char *DEFAULT_WIFI_PASS = "morag2026";
const char *DEFAULT_USER = "admin";
const char *DEFAULT_PASS = "morag1234";

CRGB leds[NUM_LEDS];
WebServer server(80);
DNSServer dns;
Preferences prefs;

enum Effect : uint8_t { FX_SOLID, FX_BREATHE, FX_WIPE, FX_CHASE, FX_GRADIENT, FX_RAINBOW, FX_COUNT };
const char *EFFECT_NAMES[FX_COUNT] = { "solid", "breathe", "wipe", "chase", "gradient", "rainbow" };

struct State {
  bool    power;
  uint8_t brightPct;     // 0-100
  uint8_t effect;        // Effect
  uint8_t speed;         // 1-100
  CRGB    c[3];
  char    name[32];
} st;

bool     dirty = false;
uint32_t dirtyAt = 0;
uint32_t frame = 0;      // effect frame counter, advanced by speed

// Login session (one phone at a time for the prototype)
String   sessionToken;
uint32_t sessionAt = 0;
uint8_t  failCount = 0;
uint32_t lockedUntil = 0;

// ---------------- Helpers ----------------
bool parseHex(const String &s, CRGB &out) {
  String h = s;
  h.trim();
  if (h.startsWith("#")) h = h.substring(1);
  if (h.length() != 6) return false;
  for (unsigned i = 0; i < 6; i++) if (!isxdigit((unsigned char)h[i])) return false;
  long v = strtol(h.c_str(), nullptr, 16);
  out = CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
  return true;
}

String toHex(const CRGB &c) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
  return String(buf);
}

int effectFromName(const String &n) {
  String s = n;
  s.trim();
  s.toLowerCase();
  for (int i = 0; i < FX_COUNT; i++) if (s == EFFECT_NAMES[i]) return i;
  return -1;
}

// Split "A,B,C" into parts. Returns the number of parts.
int splitCsv(const String &line, String parts[], int maxParts) {
  int count = 0, start = 0;
  while (count < maxParts) {
    int comma = line.indexOf(',', start);
    if (comma < 0) { parts[count++] = line.substring(start); break; }
    parts[count++] = line.substring(start, comma);
    start = comma + 1;
  }
  for (int i = 0; i < count; i++) parts[i].trim();
  return count;
}

void markDirty() { dirty = true; dirtyAt = millis(); }

void applyBrightness() {
  FastLED.setBrightness(st.power ? map(st.brightPct, 0, 100, 0, 255) : 0);
}

uint32_t packColor(const CRGB &c) { return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }

String sha256Hex(const String &text) {
  uint8_t out[32];
  mbedtls_sha256((const unsigned char *)text.c_str(), text.length(), out, 0);
  char hex[65];
  for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", out[i]);
  return String(hex);
}

// Passwords are stored as SHA-256(salt + password), never as plain text.
String hashPassword(const String &salt, const String &pass) { return sha256Hex(salt + ":" + pass); }

String randomHex(int bytes) {
  String s;
  char b[3];
  for (int i = 0; i < bytes; i++) { snprintf(b, sizeof(b), "%02x", (unsigned)(esp_random() & 0xFF)); s += b; }
  return s;
}

// Compare without leaking timing.
bool safeEquals(const String &a, const String &b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0;
  for (unsigned i = 0; i < a.length(); i++) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}

// ---------------- Storage ----------------
void loadState() {
  prefs.begin("morag", true);
  st.power     = prefs.getBool("power", true);
  st.brightPct = prefs.getUChar("bright", 60);
  st.effect    = prefs.getUChar("effect", FX_SOLID);
  st.speed     = prefs.getUChar("speed", 50);
  uint32_t c0  = prefs.getUInt("c0", 0xFFFFFF);
  uint32_t c1  = prefs.getUInt("c1", 0x000000);
  uint32_t c2  = prefs.getUInt("c2", 0x000000);
  String name  = prefs.getString("name", "Custom");
  prefs.end();

  if (st.effect >= FX_COUNT) st.effect = FX_SOLID;
  st.brightPct = constrain(st.brightPct, 0, 100);
  st.speed     = constrain(st.speed, 1, 100);
  st.c[0] = CRGB(c0); st.c[1] = CRGB(c1); st.c[2] = CRGB(c2);
  strlcpy(st.name, name.c_str(), sizeof(st.name));
}

void saveState() {
  prefs.begin("morag", false);
  prefs.putBool("power", st.power);
  prefs.putUChar("bright", st.brightPct);
  prefs.putUChar("effect", st.effect);
  prefs.putUChar("speed", st.speed);
  prefs.putUInt("c0", packColor(st.c[0]));
  prefs.putUInt("c1", packColor(st.c[1]));
  prefs.putUInt("c2", packColor(st.c[2]));
  prefs.putString("name", st.name);
  prefs.end();
  dirty = false;
}

// Account and Wi-Fi live in their own namespace.
void ensureAccount(bool forceDefaults) {
  prefs.begin("morag-auth", false);
  if (forceDefaults || !prefs.isKey("user")) {
    String salt = randomHex(8);
    prefs.putString("user", DEFAULT_USER);
    prefs.putString("salt", salt);
    prefs.putString("hash", hashPassword(salt, DEFAULT_PASS));
    prefs.putString("ssid", DEFAULT_SSID);
    prefs.putString("wpass", DEFAULT_WIFI_PASS);
    prefs.putString("sta_ssid", "");
    prefs.putString("sta_pass", "");
    Serial.println("Account and Wi-Fi set to defaults");
  }
  prefs.end();
}

String authGet(const char *key) {
  prefs.begin("morag-auth", true);
  String v = prefs.getString(key, "");
  prefs.end();
  return v;
}

// ---------------- Commands ----------------
String statusLine() {
  String s = "STATE,";
  s += st.power ? "1" : "0";
  s += ","; s += String(st.brightPct);
  s += ","; s += EFFECT_NAMES[st.effect];
  s += ","; s += String(st.speed);
  s += ","; s += toHex(st.c[0]);
  s += ","; s += toHex(st.c[1]);
  s += ","; s += toHex(st.c[2]);
  s += ","; s += st.name;
  return s;
}

// Runs one command line and returns the reply.
String handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return "ERR,empty";
  if (line.length() > MAX_LINE) return "ERR,too long";

  String p[9];
  int n = splitCsv(line, p, 9);
  String cmd = p[0];
  cmd.toUpperCase();

  if (cmd == "PING") return "PONG";
  if (cmd == "STATUS") return statusLine();

  if (cmd == "POWER" && n >= 2) {
    st.power = (p[1] == "1" || p[1].equalsIgnoreCase("ON"));
    applyBrightness(); markDirty(); return "OK,POWER";
  }

  if (cmd == "BRIGHT" && n >= 2) {
    st.brightPct = constrain(p[1].toInt(), 0, 100);
    applyBrightness(); markDirty(); return "OK,BRIGHT";
  }

  if (cmd == "COLOR" && n >= 2) {
    CRGB c;
    if (!parseHex(p[1], c)) return "ERR,bad color";
    st.c[0] = c; st.effect = FX_SOLID; st.power = true;
    strlcpy(st.name, "Custom", sizeof(st.name));
    applyBrightness(); markDirty(); return "OK,COLOR";
  }

  if (cmd == "EFFECT" && n >= 2) {
    int fx = effectFromName(p[1]);
    if (fx < 0) return "ERR,unknown effect";
    st.effect = fx;
    if (n >= 3) st.speed = constrain(p[2].toInt(), 1, 100);
    frame = 0; markDirty(); return "OK,EFFECT";
  }

  // PRESET,<name>,<effect>,<speed>,<bright>,#c1[,#c2[,#c3]]
  if (cmd == "PRESET" && n >= 6) {
    int fx = effectFromName(p[2]);
    if (fx < 0) return "ERR,unknown effect";
    CRGB cols[3] = { CRGB::Black, CRGB::Black, CRGB::Black };
    int given = 0;
    for (int i = 0; i < 3 && 5 + i < n; i++) {
      if (p[5 + i].length() == 0) continue;
      if (!parseHex(p[5 + i], cols[i])) return "ERR,bad color";
      given++;
    }
    if (given == 0) return "ERR,no color";
    strlcpy(st.name, p[1].c_str(), sizeof(st.name));
    st.effect    = fx;
    st.speed     = constrain(p[3].toInt(), 1, 100);
    st.brightPct = constrain(p[4].toInt(), 0, 100);
    for (int i = 0; i < 3; i++) st.c[i] = cols[i];
    st.power = true;
    frame = 0;
    applyBrightness(); markDirty(); return "OK,PRESET";
  }

  return "ERR,unknown command";
}

// ---------------- HTTP API ----------------
void sendText(int code, const String &body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "text/plain", body);
}

// True when the request carries the current login token (sends nothing).
bool tokenValid() {
  String t = server.arg("t");
  return sessionToken.length() > 0 && safeEquals(t, sessionToken) && millis() - sessionAt <= SESSION_MS;
}

// True when the phone is talking to us over the device AP (192.168.4.x).
bool fromDeviceAp() {
  IPAddress ip = server.client().remoteIP();
  IPAddress ap = WiFi.softAPIP();
  return ip[0] == ap[0] && ip[1] == ap[1] && ip[2] == ap[2];
}

String jsonEscape(const String &in) {
  String out;
  for (unsigned i = 0; i < in.length(); i++) {
    char ch = in[i];
    if (ch == '"' || ch == '\\') { out += '\\'; out += ch; }
    else if ((uint8_t)ch < 0x20) out += ' ';
    else out += ch;
  }
  return out;
}

String staStatusText() {
  if (authGet("sta_ssid").length() == 0) return "none";
  switch (WiFi.status()) {
    case WL_CONNECTED:      return "connected";
    case WL_NO_SSID_AVAIL:  return "not_found";
    case WL_CONNECT_FAILED: return "failed";
    default:                return "connecting";
  }
}

void connectSiteWifi() {
  String ssid = authGet("sta_ssid");
  if (ssid.length() == 0) return;
  WiFi.begin(ssid.c_str(), authGet("sta_pass").c_str());
}

bool authorized() {
  String t = server.arg("t");
  if (sessionToken.length() == 0 || !safeEquals(t, sessionToken)) { sendText(401, "ERR,login required"); return false; }
  if (millis() - sessionAt > SESSION_MS) { sessionToken = ""; sendText(401, "ERR,session expired"); return false; }
  return true;
}

void apiLogin() {
  uint32_t now = millis();
  if ((int32_t)(lockedUntil - now) > 0) {
    sendText(429, "ERR,too many attempts, wait " + String((lockedUntil - now) / 1000 + 1) + " s");
    return;
  }
  String user = server.arg("user");
  String pass = server.arg("pass");
  bool ok = safeEquals(user, authGet("user")) &&
            safeEquals(hashPassword(authGet("salt"), pass), authGet("hash"));
  if (!ok) {
    if (++failCount >= MAX_FAILS) { lockedUntil = now + LOCKOUT_MS; failCount = 0; }
    sendText(401, "ERR,wrong username or password");
    return;
  }
  failCount = 0;
  sessionToken = randomHex(16);       // a new login replaces the old one
  sessionAt = now;
  server.send(200, "application/json", "{\"token\":\"" + sessionToken + "\"}");
}

void apiLogout() {
  if (!authorized()) return;
  sessionToken = "";
  sendText(200, "OK,LOGOUT");
}

void apiCmd() {
  if (!authorized()) return;
  String reply = handleCommand(server.arg("c"));
  sendText(reply.startsWith("ERR") ? 400 : 200, reply);
}

void apiStatus() {
  if (!authorized()) return;
  sendText(200, statusLine());
}

void apiAccount() {
  if (!authorized()) return;
  String oldPass = server.arg("old");
  String user = server.arg("user");
  String pass = server.arg("pass");
  user.trim();
  if (!safeEquals(hashPassword(authGet("salt"), oldPass), authGet("hash"))) { sendText(403, "ERR,current password is wrong"); return; }
  if (user.length() < 3 || pass.length() < 6) { sendText(400, "ERR,username 3+ and password 6+ characters"); return; }
  String salt = randomHex(8);
  prefs.begin("morag-auth", false);
  prefs.putString("user", user);
  prefs.putString("salt", salt);
  prefs.putString("hash", hashPassword(salt, pass));
  prefs.end();
  sessionToken = "";                   // sign in again with the new login
  sendText(200, "OK,ACCOUNT");
}

void apiWifi() {
  if (!authorized()) return;
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (ssid.length() < 1 || ssid.length() > 32) { sendText(400, "ERR,Wi-Fi name 1-32 characters"); return; }
  if (pass.length() < 8 || pass.length() > 63) { sendText(400, "ERR,Wi-Fi password 8-63 characters"); return; }
  prefs.begin("morag-auth", false);
  prefs.putString("ssid", ssid);
  prefs.putString("wpass", pass);
  prefs.end();
  if (dirty) saveState();
  sendText(200, "OK,WIFI restarting");
  delay(500);
  ESP.restart();
}

void apiInfo() {
  bool up = WiFi.status() == WL_CONNECTED;
  String j = "{";
  j += "\"name\":\"MORAG controller\",";
  j += "\"ap_ssid\":\"" + jsonEscape(authGet("ssid")) + "\",";
  j += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  j += "\"via_ap\":" + String(fromDeviceAp() ? "true" : "false") + ",";
  j += "\"sta_ssid\":\"" + jsonEscape(authGet("sta_ssid")) + "\",";
  j += "\"sta_status\":\"" + staStatusText() + "\",";
  j += "\"sta_ip\":\"" + String(up ? WiFi.localIP().toString() : String("")) + "\"";
  j += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// Join (or forget, with an empty ssid) the site Wi-Fi.
void apiProvision() {
  if (!fromDeviceAp() && !tokenValid()) { sendText(401, "ERR,login required"); return; }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (ssid.length() > 32 || pass.length() > 63) { sendText(400, "ERR,Wi-Fi name up to 32, password up to 63 characters"); return; }
  prefs.begin("morag-auth", false);
  prefs.putString("sta_ssid", ssid);
  prefs.putString("sta_pass", pass);
  prefs.end();
  sendText(200, ssid.length() ? "OK,PROVISION connecting" : "OK,PROVISION forgotten");
  delay(100);                          // let the reply leave before the radio changes
  WiFi.disconnect(false);
  if (ssid.length()) connectSiteWifi();
}

// ---------------- Captive portal ----------------
void redirectToPortal() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.sendHeader("Cache-Control", "no-store");
  server.send(302, "text/plain", "");
}

void servePortal() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", PORTAL_HTML);
}

// Wi-Fi scan without blocking: first call starts it, later calls collect it.
void apiScan() {
  if (!fromDeviceAp() && !tokenValid()) { sendText(401, "ERR,login required"); return; }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); n = WIFI_SCAN_RUNNING; }
  if (n == WIFI_SCAN_RUNNING) { server.send(200, "application/json", "{\"status\":\"scanning\"}"); return; }
  String j = "{\"status\":\"done\",\"networks\":[";
  for (int i = 0; i < n && i < 30; i++) {
    if (i) j += ",";
    j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
         ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  j += "]}";
  WiFi.scanDelete();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// Brands for the portal page, saved on the controller.
const char *DEFAULT_BRANDS =
  "[{\"name\":\"Brand A\",\"colors\":[\"#E4002B\",\"#FFFFFF\"],\"effect\":\"breathe\",\"speed\":25,\"brightness\":70},"
  "{\"name\":\"Brand B\",\"colors\":[\"#0057B8\",\"#FFC72C\"],\"effect\":\"gradient\",\"speed\":50,\"brightness\":80},"
  "{\"name\":\"Brand C\",\"colors\":[\"#00A651\",\"#000000\",\"#FFFFFF\"],\"effect\":\"chase\",\"speed\":80,\"brightness\":60}]";

void apiBrands() {
  if (!authorized()) return;
  prefs.begin("morag-brands", true);
  String j = prefs.getString("json", DEFAULT_BRANDS);
  prefs.end();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void apiBrandsSave() {
  if (!authorized()) return;
  String j = server.arg("data");
  j.trim();
  if (!j.startsWith("[") || !j.endsWith("]")) { sendText(400, "ERR,bad brand list"); return; }
  if (j.length() > 3800) { sendText(400, "ERR,too many brands, remove one first"); return; }
  prefs.begin("morag-brands", false);
  prefs.putString("json", j);
  prefs.end();
  sendText(200, "OK,BRANDS");
}

// Phones and laptops probe these to detect a captive portal.
void apiNotFound() {
  if (server.uri().startsWith("/api/")) { sendText(404, "ERR,not found"); return; }
  if (fromDeviceAp()) { redirectToPortal(); return; }   // pop the portal up on the device AP
  servePortal();                                         // on the site Wi-Fi, any page shows the UI
}

// ---------------- Effects ----------------
CRGB colorB() { return st.c[1]; }
CRGB colorC() { return (st.c[2] == CRGB(CRGB::Black)) ? st.c[0] : st.c[2]; }

void renderFrame() {
  switch (st.effect) {
    case FX_SOLID:
      fill_solid(leds, NUM_LEDS, st.c[0]);
      break;

    case FX_BREATHE: {
      // Breathes the first color; blends toward the second if it is set.
      uint8_t wave = sin8((uint8_t)(frame >> 5));
      CRGB col = st.c[0];
      if (st.c[1] == CRGB(CRGB::Black)) col.nscale8(qadd8(wave, 20));
      else col = blend(st.c[0], st.c[1], wave);
      fill_solid(leds, NUM_LEDS, col);
      break;
    }

    case FX_WIPE: {
      // Fills the strip with color 1, then color 2, then repeats.
      uint32_t step = (frame >> 4) % (2 * NUM_LEDS);
      bool second = step >= NUM_LEDS;
      uint32_t pos = step % NUM_LEDS;
      CRGB fg = second ? colorB() : st.c[0];
      CRGB bg = second ? st.c[0] : colorB();
      for (int i = 0; i < NUM_LEDS; i++) leds[i] = (i <= (int)pos) ? fg : bg;
      break;
    }

    case FX_CHASE: {
      // Groups of 3 LEDs in color 1, gaps in color 2, moving along.
      uint32_t offset = (frame >> 8) % 6;
      for (int i = 0; i < NUM_LEDS; i++) leds[i] = (((i + offset) % 6) < 3) ? st.c[0] : colorB();
      break;
    }

    case FX_GRADIENT: {
      // Moving gradient: color 1 -> color 2 -> color 3 (or color 1) -> color 1.
      CRGB a = st.c[0];
      CRGB b = (st.c[1] == CRGB(CRGB::Black)) ? st.c[0] : st.c[1];
      CRGB c = colorC();
      uint8_t shift = (uint8_t)(frame >> 6);
      for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t pos = (uint8_t)((i * 256L / NUM_LEDS) + shift);
        if (pos < 85)       leds[i] = blend(a, b, pos * 3);
        else if (pos < 170) leds[i] = blend(b, c, (pos - 85) * 3);
        else                leds[i] = blend(c, a, (pos - 170) * 3);
      }
      break;
    }

    case FX_RAINBOW:
      fill_rainbow(leds, NUM_LEDS, (uint8_t)(frame >> 6), 2);
      break;
  }
}

// ---------------- Setup / loop ----------------
void readUsbSerial() {
  // USB serial accepts the same commands, handy for bench tests.
  static String usbLine;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (usbLine.length()) Serial.println(handleCommand(usbLine));
      usbLine = "";
    } else if (usbLine.length() < MAX_LINE) {
      usbLine += ch;
    }
  }
}

// Hold BOOT for 5 seconds while running: back to default Wi-Fi and login.
void checkFactoryReset() {
  static uint32_t pressedAt = 0;
  if (digitalRead(RESET_PIN) == LOW) {
    if (pressedAt == 0) pressedAt = millis() | 1;
    if (millis() - pressedAt > 5000) {
      ensureAccount(true);
      fill_solid(leds, NUM_LEDS, CRGB::White);   // short white flash confirms the reset
      FastLED.show();
      delay(400);
      ESP.restart();
    }
  } else {
    pressedAt = 0;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  loadState();
  ensureAccount(false);
  pinMode(RESET_PIN, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);   // WS2815 uses WS2812B timing
  FastLED.setMaxPowerInVoltsAndMilliamps(PSU_VOLTS, PSU_MILLIAMPS);
  FastLED.setCorrection(TypicalLEDStrip);
  applyBrightness();
  FastLED.clear(true);

  String ssid = authGet("ssid");
  String wpass = authGet("wpass");
  WiFi.mode(WIFI_AP_STA);              // device AP + site Wi-Fi at the same time
  WiFi.setAutoReconnect(true);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (WiFi.softAP(ssid.c_str(), wpass.c_str())) {
    Serial.println("Wi-Fi ready: " + ssid + " at " + WiFi.softAPIP().toString());
  } else {
    Serial.println("Wi-Fi access point failed to start");
  }
  connectSiteWifi();

  server.on("/",                 HTTP_GET, servePortal);
  server.on("/index.html",       HTTP_GET, servePortal);
  // captive portal probes: Android, Apple, Windows
  server.on("/generate_204",     HTTP_ANY, redirectToPortal);
  server.on("/gen_204",          HTTP_ANY, redirectToPortal);
  server.on("/hotspot-detect.html", HTTP_ANY, redirectToPortal);
  server.on("/library/test/success.html", HTTP_ANY, redirectToPortal);
  server.on("/connecttest.txt",  HTTP_ANY, redirectToPortal);
  server.on("/ncsi.txt",         HTTP_ANY, redirectToPortal);
  server.on("/redirect",         HTTP_ANY, redirectToPortal);
  server.on("/fwlink",           HTTP_ANY, redirectToPortal);
  server.on("/api/scan",        HTTP_POST, apiScan);
  server.on("/api/brands",      HTTP_POST, apiBrands);
  server.on("/api/brands/save", HTTP_POST, apiBrandsSave);
  server.on("/api/info",      HTTP_ANY,  apiInfo);
  server.on("/api/provision", HTTP_POST, apiProvision);
  server.on("/api/login",   HTTP_POST, apiLogin);
  server.on("/api/logout",  HTTP_POST, apiLogout);
  server.on("/api/cmd",     HTTP_POST, apiCmd);
  server.on("/api/status",  HTTP_POST, apiStatus);
  server.on("/api/account", HTTP_POST, apiAccount);
  server.on("/api/wifi",    HTTP_POST, apiWifi);
  server.onNotFound(apiNotFound);
  server.begin();

  // Answer every name lookup on the device AP with our address (captive portal).
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
}

void loop() {
  dns.processNextRequest();
  server.handleClient();
  readUsbSerial();
  checkFactoryReset();

  static uint32_t lastFrame = 0;
  uint32_t now = millis();
  if (now - lastFrame >= 20) {                 // about 50 frames per second
    lastFrame = now;
    frame += st.speed;                         // speed 1-100 sets how fast effects move
    renderFrame();
    FastLED.show();
  }

  if (dirty && now - dirtyAt >= SAVE_DELAY_MS) saveState();

  static bool wasUp = false;
  bool up = WiFi.status() == WL_CONNECTED;
  if (up != wasUp) {
    wasUp = up;
    Serial.println(up ? "Site Wi-Fi connected, IP " + WiFi.localIP().toString() : String("Site Wi-Fi disconnected"));
  }
}
