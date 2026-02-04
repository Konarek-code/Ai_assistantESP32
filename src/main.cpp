#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "config.h"
#include "secrets.h"

// ===== STATE =====
bool ledState = false;
AsyncWebServer server(80);

// ===== OLED =====
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0,
    U8X8_PIN_NONE);

// ===== OLED HELPERS =====
void showMessage(const String &msg)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(0, 20, msg.c_str());
  u8g2.sendBuffer();
}

void showMessage2(const String &msg1, const String &msg2 = "")
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(0, 20, msg1.c_str());
  if (msg2.length() > 0)
    u8g2.drawStr(0, 45, msg2.c_str());
  u8g2.sendBuffer();
}

void showWrapped2Lines(const String &text)
{
  const int maxLineLen = 16;

  if ((int)text.length() <= maxLineLen)
  {
    showMessage(text);
    return;
  }

  int split = text.lastIndexOf(' ', maxLineLen);
  if (split < 0)
    split = maxLineLen;

  String l1 = text.substring(0, split);
  String l2 = text.substring(split);
  l2.trim();

  if ((int)l2.length() > maxLineLen)
  {
    l2 = l2.substring(0, maxLineLen - 1) + "…";
  }

  showMessage2(l1, l2);
}
String deviceId()
{
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "ESP32-" + mac;
}

void executeAction(JsonObject a)
{
  const char *type = a["type"] | "";

  if (strcmp(type, "led") == 0)
  {
    const char *value = a["value"] | "";
    if (strcmp(value, "on") == 0)
    {
      digitalWrite(LED_PIN, HIGH);
      ledState = true;
    }
    else if (strcmp(value, "off") == 0)
    {
      digitalWrite(LED_PIN, LOW);
      ledState = false;
    }
    return;
  }

  if (strcmp(type, "oled") == 0)
  {
    const char *text = a["text"] | "";
    showWrapped2Lines(String(text));
    return;
  }

  if (strcmp(type, "delay_ms") == 0)
  {
    int ms = a["ms"] | 0;
    if (ms > 0)
      delay((uint32_t)ms);
    return;
  }

  Serial.printf("[AI] Unknown action type: %s\n", type);
}

// ===== BACKEND CALL =====
String deviceId()
{
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  return "ESP32-" + mac;
}

void executeAction(JsonObject a)
{
  const char *type = a["type"] | "";

  if (strcmp(type, "led") == 0)
  {
    const char *value = a["value"] | "";
    if (strcmp(value, "on") == 0)
    {
      digitalWrite(LED_PIN, HIGH);
      ledState = true;
    }
    else if (strcmp(value, "off") == 0)
    {
      digitalWrite(LED_PIN, LOW);
      ledState = false;
    }
    return;
  }

  if (strcmp(type, "oled") == 0)
  {
    const char *text = a["text"] | "";
    showWrapped2Lines(String(text));
    return;
  }

  if (strcmp(type, "delay_ms") == 0)
  {
    int ms = a["ms"] | 0;
    if (ms > 0)
      delay((uint32_t)ms);
    return;
  }

  Serial.printf("[AI] Unknown action type: %s\n", type);
}
String askBackend(const String &cmd)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WiFi] Not connected");
    showMessage("WiFi ERR");
    return "WiFi not connected";
  }

  // Request JSON
  StaticJsonDocument<384> req;
  req["text"] = cmd;
  req["device_id"] = deviceId();

  String payload;
  serializeJson(req, payload);

  for (uint8_t attempt = 0; attempt <= HTTP_RETRIES; attempt++)
  {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.begin(AI_URL);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(payload);
    String body = http.getString();
    http.end();

    Serial.printf("[HTTP] attempt=%u code=%d\n", attempt, code);
    Serial.println("[HTTP] payload: " + payload);
    Serial.println("[HTTP] body: " + body);

    if (code <= 0)
    {
      if (attempt == HTTP_RETRIES)
      {
        showMessage("Backend ERR");
        return "Backend error";
      }
      delay(150 * (attempt + 1));
      continue;
    }

    // Parse response JSON
    StaticJsonDocument<1536> res;
    DeserializationError err = deserializeJson(res, body);
    if (err)
    {
      Serial.printf("[JSON] parse error: %s\n", err.c_str());
      showMessage("JSON ERR");
      return "JSON parse error";
    }

    const char *requestId = res["request_id"] | "";
    const char *response = res["response"] | "";

    Serial.printf("[AI] request_id=%s\n", requestId);
    Serial.printf("[AI] response='%s'\n", response);

    // Execute actions[]
    JsonArray actions = res["actions"].as<JsonArray>();
    if (!actions.isNull())
    {
      for (JsonObject a : actions)
      {
        executeAction(a);
      }
    }

    bool hadOled = false;
    if (!actions.isNull())
    {
      for (JsonObject a : actions)
      {
        const char *t = a["type"] | "";
        if (strcmp(t, "oled") == 0)
        {
          hadOled = true;
          break;
        }
      }
    }
    if (!hadOled)
      showWrapped2Lines(String(response));

    return String(response);
  }

  showMessage("Backend ERR");
  return "Backend error";
}

// ===== SETUP =====
void setup()
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(21, 22);
  u8g2.begin();

  showMessage("Boot...");

  // WiFi connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi OK");
    Serial.println(WiFi.localIP());
    showMessage("WiFi OK");
  }
  else
  {
    Serial.println("\nWiFi FAIL");
    showMessage("WiFi FAIL");
  }

  // ===== WEB UI =====
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<body style="background:#111;color:#0f0;text-align:center;font-family:monospace">
<h1>ESP32 AI Assistant</h1>
<input id="cmd" placeholder="np: led on" style="width:260px"/>
<button onclick="send()">Send</button>
<pre id="log" style="white-space:pre-wrap;margin-top:16px"></pre>
<script>
async function send() {
  const el = document.getElementById("cmd");
  const c = el.value;
  if (!c) return;
  const r = await fetch('/cmd?text=' + encodeURIComponent(c));
  const t = await r.text();
  document.getElementById("log").innerText = t;
  el.value = "";
  el.focus();
}
</script>
</body>
</html>
)rawliteral"); });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              if (request->hasParam("text"))
              {
                String cmd = request->getParam("text")->value();
                Serial.println("CMD: " + cmd);
                String aiText = askBackend(cmd);
                request->send(200, "text/plain", aiText);
              }
              else
              {
                request->send(400, "text/plain", "Missing ?text=");
              } });

  server.begin();
  Serial.println("Server started");
}

void loop() {}
