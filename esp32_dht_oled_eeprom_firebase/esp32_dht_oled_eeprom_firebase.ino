#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DNSServer.h>

// ----- OLED Setup -----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ----- EEPROM Storage Configuration -----
#define eepromSize 512         // total bytes in EEPROM
#define ssidStart 0            // SSID stored at bytes 0–31
#define passwordStart 32       // Wi-Fi password at bytes 32–63
#define deviceIdStart 64       // Device ID at bytes 64–95
#define credentialMaxLength 30 // max length for SSID/password/deviceId

// ----- Firebase Setup -----
#define API_KEY "Api"
#define DATABASE_URL "databaseURL"
#define FIREBASE_PATH "/102/oled"
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData fbdo;

// ----- Web Server -----
WebServer server(80);

// ----- Global Variables -----
String ssid, pass, devid;
bool apmode = false;
bool scanComplete = false;
String scannedNetworks = "<p>Scanning networks...</p>";
unsigned long lastUpdate = 0;
const unsigned long interval = 2000;
String firebasePath = FIREBASE_PATH;
String message = "";
String updatedBy = "";

// ----- Setup -----
void setup()
{
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(0, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED init failed");
    while (true)
      ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();

  readData();

  // 5s window to detect press
  Serial.println("Hold GPIO0: short=AP, long=clear+AP");
  unsigned long start = millis();
  while (millis() - start < 5000)
  {
    if (digitalRead(0) == LOW)
    {
      unsigned long t0 = millis();
      while (digitalRead(0) == LOW)
      {
        if (millis() - t0 >= 3000)
        {
          Serial.println("Long press -> clear EEPROM");
          clearData();
          ssid = "";
          pass = "";
          devid = "";
          readData();
          scanComplete = false;
          delay(500);
          ap_mode();
          return;
        }
      }
      Serial.println("Short press -> AP mode");
      ap_mode();
      return;
    }
    delay(10);
  }

  // No press or no SSID stored
  if (ssid.length() == 0)
  {
    Serial.println("No SSID -> AP mode");
    ap_mode();
    return;
  }

  // Normal run
  testWiFi();
  initFirebase();
}

void loop()
{
  server.handleClient();

  if (apmode)
  {
    if (!scanComplete)
    {
      int n = WiFi.scanComplete();
      if (n == -2)
        WiFi.scanNetworks(true);
      else if (n >= 0)
      {
        String tbl = "<table border='1'><tr><th>SSID</th><th>Signal</th></tr>";
        for (int i = 0; i < n; i++)
          tbl += "<tr><td>" + WiFi.SSID(i) + "</td><td>" + String(WiFi.RSSI(i)) + " dBm</td></tr>";
        tbl += "</table>";
        scannedNetworks = tbl;
        scanComplete = true;
        WiFi.scanDelete();
        Serial.println("Scan done");
      }
    }
    return;
  }

  if (!Firebase.ready() && Firebase.isTokenExpired())
  {
    Serial.println("Refreshing token");
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    return;
  }
  if (!Firebase.ready())
  {
    delay(500);
    return;
  }

  // … inside loop(), after you’ve fetched message, updatedBy, lastUpdated (ms) …
  if (millis() - lastUpdate < interval)
    return;
  lastUpdate = millis();

  // … after fetching `message` & `updatedBy` …
  // Fetch 'message' from Firebase
  if (Firebase.RTDB.getString(&fbdo, firebasePath + "/message"))
  {
    if (fbdo.dataType() == "string")
    {
      message = fbdo.stringData();
    }
  }
  else
  {
    Serial.println("Failed to get message: " + fbdo.errorReason());
  }

  // Fetch 'updatedBy' from Firebase
  if (Firebase.RTDB.getString(&fbdo, firebasePath + "/updatedBy"))
  {
    if (fbdo.dataType() == "string")
    {
      updatedBy = fbdo.stringData();
    }
  }
  else
  {
    Serial.println("Failed to get updatedBy: " + fbdo.errorReason());
  }

  // 2) Clear & prep
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // 3) Line 1: Wi-Fi status + DevID
  display.setCursor(0, 0);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "WiFi ERR");
  display.print(" | ID:");
  display.print(devid);

  // 4) Line 2: Message + “By:” in one go
  display.setCursor(0, 8);
  display.print(message); // your Firebase text
  display.print(" | By:");
  display.print(updatedBy); // the name who updated it

  // 5) Finally push it to the display
  display.display();
}

// ----- WiFi Connection -----
void testWiFi()
{
  // Tear down any old AP/STA
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);

  // Go STA
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Try to join
  Serial.print("Joining “");
  Serial.print(ssid);
  Serial.print("”");
  WiFi.begin(ssid.c_str(), pass.c_str());

  // Wait ~10s (20 × 500ms)
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30)
  {
    delay(500);
    Serial.print(".");
    tries++;
  }

  // Check
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nConnected! IP = " + WiFi.localIP().toString());
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi OK");
    display.display();
  }
  else
  {
    Serial.println("\nWiFi failed. Enter AP mode.");
    ap_mode(); // drop into your config portal
  }
}

// ----- Firebase Initialization -----
void initFirebase()
{
  // Fill in your API key & database URL
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Set your Firebase Auth user credentials
  auth.user.email = "jasmine@gmail.com";
  auth.user.password = "Jasmine123";

  // Start Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Wait for the ID token to be ready
  while (auth.token.uid == "")
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nFirebase ready!");
  // OLED feedback
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Firebase OK");
  display.display();
  delay(5000);
}

// ----- Read EEPROM -----
void readData()
{
  EEPROM.begin(eepromSize);
  ssid = pass = devid = "";

  // Read SSID (bytes 0–29)
  for (int i = 0; i < credentialMaxLength; i++)
  {
    char ch = EEPROM.read(ssidStart + i);
    if (isPrintable(ch))
      ssid += ch;
  }

  // Read Password (bytes 32–61)
  for (int i = 0; i < credentialMaxLength; i++)
  {
    char ch = EEPROM.read(passwordStart + i);
    if (isPrintable(ch))
      pass += ch;
  }

  // Read Device ID (bytes 64–93)
  for (int i = 0; i < credentialMaxLength; i++)
  {
    char ch = EEPROM.read(deviceIdStart + i);
    if (isPrintable(ch))
      devid += ch;
  }

  EEPROM.end();

  // Debug output
  Serial.println("Wi-Fi SSID: " + ssid);
  Serial.println("Wi-Fi Password: " + pass);
  Serial.println("Device ID: " + devid);
}

// ----- Clear EEPROM -----
void clearData()
{
  EEPROM.begin(eepromSize);
  for (int i = 0; i < deviceIdStart + credentialMaxLength; i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("EEPROM cleared");
}

// ----- Write EEPROM -----
void writeData(const String &newSsid, const String &newPass, const String &newDevid)
{
  // Clear old data in those regions
  clearData();

  // Write new fields
  EEPROM.begin(eepromSize);
  Serial.println("Writing to EEPROM...");
  for (int i = 0; i < credentialMaxLength; i++)
  {
    EEPROM.write(ssidStart + i, i < newSsid.length() ? newSsid[i] : 0);
    EEPROM.write(passwordStart + i, i < newPass.length() ? newPass[i] : 0);
    EEPROM.write(deviceIdStart + i, i < newDevid.length() ? newDevid[i] : 0);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Write successful");
}

// ----- Start configuration access point -----
void ap_mode()
{
  apmode = true;
  digitalWrite(2, HIGH); // LED ON in AP mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32_eeprom", ""); // open AP
  Serial.println("AP Mode. Please connect to http://192.168.4.1 to configure");
  WiFi.scanNetworks(true);         // Start scanning for WiFi networks
  Serial.println(WiFi.softAPIP()); // Print AP Ip address
  launchWeb(0);
}

void launchWeb(int webtype)
{
  createWebServer(webtype);
  server.begin();
}

void createWebServer(int webtype)
{
  if (webtype == 0)
  {
    server.on("/", []()
              {
      String html = R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
        <meta charset="UTF-8"/>
        <meta name="viewport" content="width=device-width,initial-scale=1"/>
        <title>ESP32 Configuration</title>
        <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;700&display=swap" rel="stylesheet">
        <style>
          body {
            margin:0; padding:0; font-family:'Roboto', Arial, sans-serif;
            min-height:100vh;
            background:linear-gradient(120deg, #fcb1b1 0%, #a1c4fd 100%);
          }
          .container {
            max-width:900px; margin:48px auto; padding:32px 32px 28px 32px;
            background:rgba(255,255,255,0.98); border-radius:22px;
            box-shadow:0 8px 32px rgba(161,196,253,0.18);
            position:relative;
          }
          .main-flex {
            display: flex;
            gap: 36px;
            flex-wrap: wrap;
            justify-content: space-between;
          }
          .form-col, .networks-col {
            background: #fff;
            border-radius: 16px;
            box-shadow: 0 4px 18px rgba(252,177,177,0.10);
            padding: 28px 24px 22px 24px;
            margin: 0;
            flex: 1 1 340px;
            min-width: 280px;
            max-width: 420px;
            display: flex;
            flex-direction: column;
          }
          .networks-col {
            max-width: 480px;
            min-width: 320px;
            align-items: flex-start;
          }
          @media (max-width: 950px) {
            .container { padding: 18px 2vw; }
            .main-flex { gap: 18px; }
            .form-col, .networks-col { padding: 18px 8px; }
          }
          @media (max-width: 800px) {
            .main-flex { flex-direction: column; gap: 0; }
            .networks-col { margin-top: 24px; max-width: 100%; }
          }
          .logo {
            display:flex; justify-content:center; align-items:center; margin-bottom:18px;
          }
          .logo-icon {
            font-size:2.5rem; color:#a1c4fd; margin-right:8px;
          }
          h1 { text-align:center; color:#5f0a87; margin-bottom:18px; font-weight:800; letter-spacing:1px; font-size:2.1em; }
          .current {
            background:rgba(161,196,253,0.13); padding:14px 16px; border-radius:8px;
            margin-bottom:22px; box-shadow:0 2px 8px rgba(252,177,177,0.07);
          }
          .current p {
            margin:7px 0; font-size:1em; color:#5f0a87;
          }
          label { display:block; margin-top:16px; color:#a4508b; font-weight:600; letter-spacing:0.2px; }
          input {
            width:100%; padding:12px; margin-top:7px;
            border:1.5px solid #a1c4fd; border-radius:7px;
            box-sizing:border-box; font-size:1em; background:#f8fafd;
            transition:border 0.2s, box-shadow 0.2s;
          }
          input:focus { border:1.5px solid #fcb1b1; outline:none; background:#fff; box-shadow:0 0 0 2px #a1c4fd33; }
          .buttons {
            display:flex; justify-content:space-between; margin-top:28px;
          }
          .buttons button {
            flex:1; margin:0 6px; padding:13px 0;
            border:none; border-radius:7px; font-size:1.1em; color:#fff;
            cursor:pointer; font-weight:700; letter-spacing:0.5px;
            transition:background 0.2s, box-shadow 0.2s;
            box-shadow:0 2px 8px rgba(161,196,253,0.10);
          }
          .save-btn, .clear-btn {
            background: #f76d6d;
            color: #fff;
            box-shadow: 0 2px 8px rgba(247,109,109,0.10);
          }
          .save-btn:hover, .clear-btn:hover {
            background: #e05555;
            color: #fff;
          }
          .networks { margin-top:0; }
          .networks h2 { margin-bottom:10px; color:#5f0a87; font-size:1.1em; font-weight:700; }
          .networks table {
            width:100%; min-width:320px; border-collapse:collapse; margin-top:8px; background:#f8fafd; border-radius:8px; overflow:hidden; box-shadow:0 2px 8px rgba(161,196,253,0.08);
          }
          .networks th, .networks td {
            padding:8px; text-align:left; border-bottom:1px solid #e0e0e0; font-size:0.98em;
          }
          .networks th { background:#fcb1b1; color:#5f0a87; }
          .networks tr:hover td { background:#a1c4fd22; transition:background 0.2s; }
          /* Modal */
          .modal {
            display:none; position:fixed; z-index:1000; left:0; top:0; width:100vw; height:100vh;
            background:rgba(252,177,177,0.13); justify-content:center; align-items:center;
          }
          .modal-content {
            background:#fff; padding:32px 28px; border-radius:14px; box-shadow:0 8px 32px rgba(161,196,253,0.13);
            text-align:center; min-width:260px;
          }
          .modal-content h2 { color:#fcb1b1; margin-bottom:12px; }
          .modal-content p { color:#5f0a87; margin-bottom:22px; }
          .modal-content button {
            background:linear-gradient(45deg, #a1c4fd, #fcb1b1); color:#5f0a87; border:none; border-radius:5px; padding:12px 32px; font-size:1em; font-weight:700; cursor:pointer;
            transition:background 0.2s;
          }
          .modal-content button:hover { background:linear-gradient(45deg, #fcb1b1, #a1c4fd); color:#fff; }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>ESP32 WiFi Configuration</h1>
          <!-- Current settings display -->
          <div class="current">
            <p><strong>Current SSID:</strong> )rawliteral"
                    + ssid + R"rawliteral(</p>
            <p><strong>Current Password:</strong> )rawliteral"
                    + pass + R"rawliteral(</p>
            <p><strong>Current Device ID:</strong> )rawliteral"
                    + devid + R"rawliteral(</p>
          </div>
          <!-- Flex row for form and networks -->
          <div class="main-flex">
            <div class="form-col">
              <form id="configForm">
                <label for="ssidInput">Wi-Fi SSID</label>
                <input id="ssidInput" name="ssid" type="text" value=")rawliteral"
                        + ssid + R"rawliteral(" required/>
                <label for="passInput">Wi-Fi Password</label>
                <div style="position:relative;">
                  <input id="passInput" name="pass" type="password" placeholder="••••••••" value=")rawliteral"
                        + pass + R"rawliteral(" style="padding-right:38px;" />
                  <button type="button" id="togglePass" style="position:absolute;right:8px;top:0;bottom:0;height:100%;display:flex;align-items:center;background:none;border:none;cursor:pointer;padding:0;outline:none;">
                    <svg id="eyeIcon" xmlns="http://www.w3.org/2000/svg" width="22" height="22" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"/><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z"/></svg>
                  </button>
                </div>
                <label for="devidInput">Device ID</label>
                <input id="devidInput" name="devid" type="text" value=")rawliteral"
                        + devid + R"rawliteral(" required/>
                <div class="buttons">
                  <button type="submit" class="save-btn">Save Changes</button>
                  <button type="button" class="clear-btn" id="clearBtn">Clear Data</button>
                </div>
              </form>
            </div>
            <div class="networks-col">
              <div class="networks">
                <h2>Available Networks</h2>
                )rawliteral"
                        + scannedNetworks + R"rawliteral(
              </div>
            </div>
          </div>
        </div>
        <!-- Modal for success/clear -->
        <div class="modal" id="modal">
          <div class="modal-content" id="modalContent">
            <h2 id="modalTitle"></h2>
            <p id="modalMsg"></p>
            <button onclick="location.href='/'">Back to Home</button>
          </div>
        </div>
        <script>
          const form = document.getElementById('configForm'),
                toast = document.getElementById('toast'),
                modal = document.getElementById('modal'),
                modalTitle = document.getElementById('modalTitle'),
                modalMsg = document.getElementById('modalMsg');
          form.addEventListener('submit', async e => {
            e.preventDefault();
            const ss = encodeURIComponent(ssidInput.value),
                  pw = encodeURIComponent(passInput.value),
                  dv = encodeURIComponent(devidInput.value);
            try {
              const res = await fetch(`/save?ssid=${ss}&pass=${pw}&devid=${dv}`);
              if (res.ok) {
                modalTitle.textContent = 'Configuration Successful!';
                modalMsg.textContent = 'Your ESP32 has been configured. The device will reboot.';
                modal.style.display = 'flex';
                setTimeout(()=>location.reload(),4000);
              } else {
                modalTitle.textContent = 'Save Failed';
                modalMsg.textContent = 'Could not save configuration.';
                modal.style.display = 'flex';
              }
            } catch {
              modalTitle.textContent = 'Network Error';
              modalMsg.textContent = 'Could not reach device.';
              modal.style.display = 'flex';
            }
          });
          document.getElementById('clearBtn').onclick = () => {
            if (confirm('Clear all data & reboot?')) {
              fetch('/clear').then(()=>{
                modalTitle.textContent = 'Data Cleared!';
                modalMsg.textContent = 'EEPROM has been reset. The device will reboot.';
                modal.style.display = 'flex';
                setTimeout(()=>location.reload(),4000);
              });
            }
          };
          // Password eye toggle
          const passInput = document.getElementById('passInput');
          const togglePass = document.getElementById('togglePass');
          const eyeIcon = document.getElementById('eyeIcon');
          let passVisible = false;
          togglePass.onclick = function() {
            passVisible = !passVisible;
            passInput.type = passVisible ? 'text' : 'password';
            eyeIcon.innerHTML = passVisible
              ? '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M13.875 18.825A10.05 10.05 0 0112 19c-4.478 0-8.268-2.943-9.542-7a9.956 9.956 0 012.042-3.292m3.1-2.727A9.956 9.956 0 0112 5c4.478 0 8.268 2.943 9.542 7a9.973 9.973 0 01-4.293 5.411M15 12a3 3 0 11-6 0 3 3 0 016 0z"/><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M3 3l18 18"/>'
              : '<path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z"/><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M2.458 12C3.732 7.943 7.523 5 12 5c4.478 0 8.268 2.943 9.542 7-1.274 4.057-5.064 7-9.542 7-4.477 0-8.268-2.943-9.542-7z"/>';
          };
        </script>
      </body>
      </html>
    )rawliteral";
      server.send(200, "text/html", html); });

    // /save handler with modal
    server.on("/save", []()
              {
      String newSsid = server.arg("ssid");
      String newPass = server.arg("pass");
      String newDevid = server.arg("devid");
      writeData(newSsid, newPass, newDevid);
      String html = R"rawliteral(
      <!DOCTYPE html><html><head><meta charset=\"UTF-8\"/>
      <title>Settings Saved</title>
      <style>
        body { background:#f6f8fa; font-family:'Roboto',Arial,sans-serif; }
        .modal { display:flex; justify-content:center; align-items:center; height:100vh; }
        .modal-content { background:#fff; padding:36px 32px; border-radius:12px; box-shadow:0 8px 32px rgba(60,188,141,0.13); text-align:center; min-width:260px; }
        .modal-content h2 { color:#3CBC8D; margin-bottom:12px; }
        .modal-content p { color:#444; margin-bottom:22px; }
        .modal-content button { background:#3CBC8D; color:#fff; border:none; border-radius:5px; padding:12px 32px; font-size:1em; font-weight:600; cursor:pointer; transition:background 0.2s; }
        .modal-content button:hover { background:#2fa97a; }
      </style>
      </head><body>
        <div class=\"modal\"><div class=\"modal-content\">
          <h2>Configuration Successful!</h2>
          <p>Your ESP32 has been configured. The device will reboot.</p>
          <button onclick=\"location.href='/'\">Back to Home</button>
        </div></div>
      </body></html>
    )rawliteral";
      server.send(200, "text/html", html);
      delay(1000);
      ESP.restart(); });

    // /clear handler with modal
    server.on("/clear", []()
              {
                clearData();
      String html = R"rawliteral(
      <!DOCTYPE html><html><head><meta charset=\"UTF-8\"/>
      <title>Data Cleared</title>
      <style>
        body { background:#f6f8fa; font-family:'Roboto',Arial,sans-serif; }
        .modal { display:flex; justify-content:center; align-items:center; height:100vh; }
        .modal-content { background:#fff; padding:36px 32px; border-radius:12px; box-shadow:0 8px 32px rgba(231,76,60,0.13); text-align:center; min-width:260px; }
        .modal-content h2 { color:#E74C3C; margin-bottom:12px; }
        .modal-content p { color:#444; margin-bottom:22px; }
        .modal-content button { background:#3CBC8D; color:#fff; border:none; border-radius:5px; padding:12px 32px; font-size:1em; font-weight:600; cursor:pointer; transition:background 0.2s; }
        .modal-content button:hover { background:#2fa97a; }
      </style>
      </head><body>
        <div class=\"modal\"><div class=\"modal-content\">
          <h2>Data Cleared!</h2>
          <p>EEPROM has been reset. The device will reboot.</p>
          <button onclick=\"location.href='/'\">Back to Home</button>
        </div></div>
      </body></html>
    )rawliteral";
      server.send(200, "text/html", html);
      delay(2000);
      ESP.restart(); });
  }
}
