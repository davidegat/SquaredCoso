// STA WebUI and first-boot captive-portal routing.
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "handlers/globals.h"
#include "handlers/tls_roots.h"

#define DNS_PORT 53

extern WebServer web;
extern DNSServer dnsServer;
extern Preferences prefs;

extern bool isHttpOk(int code);
extern void handleSettings();
extern void handleForceQOD();

extern uint32_t PAGE_INTERVAL_MS;
extern const char FW_VERSION[];

extern String g_city, g_lang, g_ics, g_fiat, g_rss_url;
extern String g_tz_rule;
extern uint8_t g_rotation;
extern String g_oa_key, g_oa_topic;
extern String g_ha_ip, g_ha_token, g_ha_entities;

extern double g_btc_owned;

extern bool g_show[PAGES];
extern CDEvent cd[8];

bool httpGET(const String& url, String& out, uint32_t timeout) {
  HTTPClient http;
  http.setTimeout(timeout);

  if (!http.begin(url))
    return false;

  int code = http.GET();
  if (!isHttpOk(code)) {
    http.end();
    return false;
  }

  out = http.getString();
  http.end();
  return true;
}

int indexOfCI(const String& s, const String& pat, int from) {
  int n = s.length();
  int m = pat.length();
  if (m <= 0 || from < 0 || from >= n) return -1;

  for (int i = from; i <= n - m; ++i) {
    int j = 0;
    for (; j < m; ++j) {
      char c1 = s[i + j];
      char c2 = pat[j];
      if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
      if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
      if (c1 != c2) break;
    }
    if (j == m) return i;
  }
  return -1;
}

static String htmlEscape(const String &src) {
  String out;
  out.reserve(src.length() + 16);
  for (int i = 0; i < src.length(); i++) {
    char c = src[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

static String htmlAP() {
  String ip = WiFi.softAPIP().toString();

  String page;
  page.reserve(1024);

  page += "<!doctype html><html><head>"
          "<meta charset='utf-8'/>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
          "<title>Wi-Fi Setup</title>"
          "<style>"
          "body{margin:0;font-family:sans-serif;background:#050814;color:#f5f5f7;"
          "display:flex;align-items:center;justify-content:center;min-height:100vh}"
          ".card{background:#0b1020;border-radius:18px;padding:22px 20px;"
          "box-shadow:0 12px 30px rgba(0,0,0,.6);width:100%;max-width:380px}"
          "h1{margin:0 0 4px;font-size:1.4rem;color:#ffdf40}"
          "p{margin:4px 0 14px;font-size:.9rem;color:#cfd2ff}"
          "label{display:block;margin:10px 0 4px;font-size:.85rem;color:#b0b5ff}"
          "input{width:100%;padding:9px 10px;border-radius:10px;border:1px solid #313855;"
          "background:#070b18;color:#f5f5f7;box-sizing:border-box;font-size:.95rem}"
          "input:focus{outline:none;border-color:#5b7cff;box-shadow:0 0 0 1px #5b7cff55}"
          "button{margin-top:16px;width:100%;padding:10px 12px;border-radius:999px;"
          "border:none;background:linear-gradient(135deg,#ffdf40,#ff7a40);"
          "color:#111;font-weight:600;font-size:.95rem;cursor:pointer}"
          "button:active{transform:scale(.98)}"
          ".tip{margin-top:14px;font-size:.8rem;color:#9aa3ff}"
          ".ip{font-family:monospace;color:#ffdf40}"
          "</style></head><body>";

  page += "<div class='card'>"
          "<h1>Configura Wi-Fi</h1>"
          "<p>Collega il pannello alla tua rete domestica.</p>"
          "<form method='POST' action='/save'>"
          "<label>SSID</label><input name='ssid' autocomplete='off'/>"
          "<label>Password</label><input name='pass' type='password' autocomplete='off'/>"
          "<button type='submit'>Salva e connetti</button>"
          "</form>"
          "<p class='tip'>Se il popup non compare, apri <span class='ip'>http://";

  page += ip;
  page += "/</span> dal browser.</p></div></body></html>";

  return page;
}

static void handleRootAP() {
  web.send(200, "text/html; charset=utf-8", htmlAP());
}

static void handleSave() {
  if (web.hasArg("ssid") && web.hasArg("pass")) {

    prefs.begin("wifi", false);
    prefs.putString("ssid", web.arg("ssid"));
    prefs.putString("pass", web.arg("pass"));
    prefs.end();

    web.send(
      200,
      "text/html; charset=utf-8",
      "<!doctype html><html><head>"
      "<meta charset='utf-8'/>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
      "<title>Riavvio</title>"
      "<style>"
      "body{margin:0;font-family:sans-serif;background:#050814;color:#f5f5f7;"
      "display:flex;align-items:center;justify-content:center;min-height:100vh}"
      ".box{background:#0b1020;border-radius:16px;padding:20px 22px;"
      "box-shadow:0 10px 26px rgba(0,0,0,.6);max-width:320px;text-align:center}"
      "h2{margin:0 0 10px;font-size:1.3rem;color:#ffdf40}"
      "p{margin:4px 0 0;font-size:.9rem;color:#cfd2ff}"
      "</style></head><body>"
      "<div class='box'><h2>Salvato</h2>"
      "<p>Mi connetto alla rete e mi riavvio…</p></div>"
      "<script>setTimeout(()=>{fetch('/reboot')},800);</script>"
      "</body></html>");
  } else {
    web.send(400, "text/plain; charset=utf-8", "Bad Request");
  }
}

static void handleReboot() {
  web.send(200, "text/plain; charset=utf-8", "OK");
  delay(100);
  ESP.restart();
}

static String checkbox(const char* name, bool checked, const char* label) {
  String s;
  s.reserve(120);
  s += "<label class='chk'><input type='checkbox' name='";
  s += name;
  s += "' value='1' ";
  if (checked) s += "checked";
  s += "/><span>";
  s += label;
  s += "</span></label>";
  return s;
}

static String statusPill(bool configured, bool it) {
  String s = "<span class='status ";
  s += configured ? "on'>" : "off'>";
  s += configured ? (it ? "configurato" : "configured")
                  : (it ? "non configurato" : "not configured");
  s += "</span>";
  return s;
}

static String pageChip(const char *name, bool checked, const char *label) {
  String s;
  s.reserve(150);
  s += "<label class='page-chip'><input class='page-toggle' type='checkbox' name='";
  s += name;
  s += "' value='1'";
  if (checked) s += " checked";
  s += "><span>";
  s += label;
  s += "</span></label>";
  return s;
}

static String uptimeCompact() {
  uint32_t sec = millis() / 1000UL;
  uint32_t days = sec / 86400UL; sec %= 86400UL;
  uint8_t hours = sec / 3600UL; sec %= 3600UL;
  uint8_t mins = sec / 60UL;
  String out;
  if (days) { out += String(days); out += "d "; }
  if (hours || days) { out += String(hours); out += "h "; }
  out += String(mins); out += "m";
  return out;
}

static bool testOpenAIKey() {
  if (!g_oa_key.length()) return false;
  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);
  HTTPClient http;
  http.setTimeout(7000);
  if (!http.begin(client, "https://api.openai.com/v1/models")) return false;
  http.addHeader(F("Authorization"), String(F("Bearer ")) + g_oa_key);
  int code = http.GET();
  http.end();
  return code >= 200 && code < 300;
}

static void handleIntegrationTest() {
  const bool it = (g_lang == "it");
  String kind = web.arg("kind");
  bool ok = false;
  String msg;

  if (kind == "ics") {
    if (!g_ics.length()) msg = it ? "URL calendario non configurato" : "Calendar URL not configured";
    else {
      ok = fetchICS();
      msg = ok ? (it ? "Calendario raggiungibile" : "Calendar reachable")
               : (it ? "Errore nel calendario" : "Calendar test failed");
    }
  } else if (kind == "rss") {
    ok = fetchNews();
    msg = ok ? (it ? "Feed RSS raggiungibile" : "RSS feed reachable")
             : (it ? "Errore nel feed RSS" : "RSS test failed");
  } else if (kind == "ha") {
    if (!g_ha_token.length()) msg = it ? "Token Home Assistant non configurato" : "Home Assistant token not configured";
    else {
      ok = fetchHA();
      msg = ok ? (it ? "Home Assistant raggiungibile" : "Home Assistant reachable")
               : (it ? "Home Assistant non raggiungibile" : "Home Assistant test failed");
    }
  } else if (kind == "openai") {
    if (!g_oa_key.length()) msg = it ? "Chiave OpenAI non configurata" : "OpenAI key not configured";
    else {
      ok = testOpenAIKey();
      msg = ok ? (it ? "Chiave OpenAI valida" : "OpenAI key is valid")
               : (it ? "OpenAI non raggiungibile o chiave non valida" : "OpenAI unreachable or key invalid");
    }
  } else {
    web.send(400, "text/plain; charset=utf-8", it ? "Test sconosciuto" : "Unknown test");
    return;
  }

  web.send(ok ? 200 : 503, "text/plain; charset=utf-8", msg);
}

String htmlSettings(bool saved, const String& msg) {
  const bool it = (g_lang == "it");

  const char* t_title = it ? "Impostazioni" : "Settings";
  const char* t_savebtn = it ? "Salva" : "Save";
  const char* t_home = it ? "Dashboard" : "Dashboard";

  String notice;
  if (saved) {
    notice = "<div class='alert ok'>" + String(it ? "Impostazioni salvate." : "Settings saved.") + "</div>";
  } else if (msg.length()) {
    bool good = msg.startsWith("ok:");
    String shown = good ? msg.substring(3) : msg;
    notice = "<div class='alert ";
    notice += good ? "ok'>" : "warn'>";
    notice += htmlEscape(shown);
    notice += "</div>";
  }

  uint32_t page_s = PAGE_INTERVAL_MS / 1000UL;
  bool tzEurope = (g_tz_rule == "CET-1CEST,M3.5.0,M10.5.0/3");
  bool tzUTC = (g_tz_rule == "UTC0");
  bool tzCustom = !tzEurope && !tzUTC;

  String page;
  page.reserve(16000);

  page += "<!doctype html><html><head>"
          "<meta charset='utf-8'/>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
          "<title>" + String(t_title) + "</title>"
          "<style>"
          ":root{color-scheme:dark}"
          "*{box-sizing:border-box}"
          "html{scroll-behavior:smooth}"
          "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,system-ui,sans-serif;background:#050814;color:#f5f5f7}"
          "header{position:sticky;top:0;z-index:20;background:linear-gradient(135deg,#0b1020,#071537);padding:13px 16px 10px;border-bottom:1px solid #20274a}"
          "header h1{margin:0;font-size:1.35rem;color:#ffdf40}"
          "header p{margin:2px 0 0;font-size:.8rem;color:#b3b7ff}"
          "main{max-width:980px;margin:0 auto;padding:10px 10px 96px}"
          ".jump{display:flex;gap:7px;overflow:auto;padding:8px 4px 3px;white-space:nowrap;scrollbar-width:none}"
          ".jump a{padding:6px 10px;border:1px solid #2b3359;border-radius:999px;color:#dbe0ff;text-decoration:none;font-size:.78rem;background:#080d1c}"
          ".alert{border-radius:10px;padding:10px 12px;font-size:.85rem;margin:10px 4px 14px}"
          ".alert.ok{background:#102a1b;border:1px solid #1f8b3b;color:#b4f3c4}"
          ".alert.warn{background:#2a1710;border:1px solid #e26f3b;color:#ffd2b6}"
          ".card{background:#0b1020;border:1px solid #191f3b;border-radius:16px;padding:16px;margin:10px 4px;box-shadow:0 10px 26px rgba(0,0,0,.45)}"
          ".card h3{margin:0 0 5px;font-size:1rem;color:#ffdf40}.card h4{margin:16px 0 6px;font-size:.9rem;color:#dce1ff}"
          ".desc{margin:4px 0 10px;font-size:.8rem;color:#a9afff}"
          ".grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:10px}"
          ".row{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto;gap:9px;align-items:end;padding:8px 0;border-bottom:1px solid #171d38}"
          ".grid-pages{display:flex;flex-wrap:wrap;gap:8px}"
          "label.field{display:block;margin:9px 0 4px;font-size:.8rem;color:#b3b8ff}"
          "input[type=text],input[type=password],input[type=number],input[type=datetime-local],select,textarea{width:100%;padding:9px 10px;border-radius:10px;border:1px solid #313855;background:#070b18;color:#f5f5f7;font:inherit;font-size:.92rem}"
          "input:focus,select:focus,textarea:focus{outline:none;border-color:#5b7cff;box-shadow:0 0 0 1px #5b7cff55}"
          ".chk{display:flex;align-items:center;gap:8px;padding:8px 10px;border:1px solid #232949;border-radius:10px;font-size:.88rem;background:#070b18;margin-top:8px}.chk input{width:auto;margin:0}"
          ".page-chip input,.rot input{position:absolute;opacity:0;pointer-events:none}"
          ".page-chip span,.rot span{display:inline-block;padding:8px 11px;border:1px solid #30385f;border-radius:999px;background:#070b18;color:#cfd5ff;font-size:.84rem;cursor:pointer;user-select:none}"
          ".page-chip input:checked+span,.rot input:checked+span{background:#26201a;border-color:#ffb740;color:#ffdf40;box-shadow:inset 0 0 0 1px #ffdf4030}"
          ".mini-actions{display:flex;gap:7px;flex-wrap:wrap;margin:8px 0}.mini-btn,.test-btn,.remove-cd{border:1px solid #3b436e;border-radius:999px;padding:7px 11px;background:#0a1022;color:#dce1ff;font-size:.79rem;cursor:pointer}"
          ".remove-cd{border-radius:9px;padding:9px 11px;color:#ffb8b8}"
          ".page-count{font-size:.8rem;color:#9fa6dc;margin-left:4px}"
          ".rotations{display:flex;gap:7px;flex-wrap:wrap}"
          ".advanced{margin-top:8px;padding-top:4px}.advanced summary{cursor:pointer;color:#aeb6ff;font-size:.82rem}"
          ".integration{padding:11px 0;border-bottom:1px solid #171d38}.integration:last-child{border-bottom:0}.integration-head{display:flex;gap:8px;align-items:center;justify-content:space-between;flex-wrap:wrap}"
          ".integration-name{font-weight:600}.status{font-size:.72rem;border-radius:999px;padding:3px 7px}.status.on{background:#12301d;color:#aef0bd}.status.off{background:#272b3d;color:#aeb3cc}"
          ".test-result{display:block;min-height:1.1em;margin-top:5px;font-size:.78rem;color:#aeb6ff}.test-result.ok{color:#aef0bd}.test-result.bad{color:#ffb8b8}"
          ".footer-bar{position:fixed;left:0;right:0;bottom:0;z-index:30;background:#050814f2;border-top:1px solid #22284a;padding:8px 10px;backdrop-filter:blur(8px)}"
          ".footer-inner{max-width:980px;margin:0 auto;display:flex;align-items:center;justify-content:space-between;gap:10px}.dirty{font-size:.78rem;color:#9198bd}.dirty.on{color:#ffdf40}"
          ".btn-primary{border:0;border-radius:999px;padding:10px 20px;font-size:.9rem;font-weight:700;background:linear-gradient(135deg,#ffdf40,#ff7a40);color:#111;cursor:pointer}"
          ".btn-secondary{display:inline-block;border-radius:999px;padding:8px 14px;font-size:.84rem;border:1px solid #3b436e;background:transparent;color:#d6dcff;text-decoration:none;cursor:pointer}"
          ".footer-actions{display:flex;align-items:center;gap:8px}"
          "@media(max-width:620px){main{padding-left:6px;padding-right:6px}.card{margin-left:2px;margin-right:2px}.row{grid-template-columns:1fr}.remove-cd{justify-self:start}.footer-inner{flex-direction:column;align-items:stretch;gap:5px}.footer-inner .dirty{display:block;text-align:center}.footer-actions{width:100%}.footer-actions .btn-primary{flex:1}.footer-actions .btn-secondary{padding:8px 10px;font-size:.76rem}.grid-2{display:block}}"
          "</style></head><body>";

  page += "<header><h1>" + String(t_title) + "</h1><p>Gat Multi Ticker — SquaredCoso</p></header><main>";
  page += "<nav class='jump'>"
          "<a href='#general'>" + String(it ? "Generale" : "General") + "</a>"
          "<a href='#display'>Display</a>"
          "<a href='#integrations'>" + String(it ? "Integrazioni" : "Integrations") + "</a>"
          "<a href='#countdowns'>Countdown</a>"
          "<a href='#notes'>Post-it</a>"
          "</nav>";
  page += notice;

  page += "<form id='settingsForm' method='POST' action='/settings'>";

  page += "<section id='general' class='card'><h3>" + String(it ? "Generale" : "General") + "</h3>";
  page += "<div class='grid-2'><div>";
  page += "<label class='field'>" + String(it ? "Città (meteo / aria)" : "City (weather / air)") + "</label>"
          "<input name='city' type='text' value='" + htmlEscape(g_city) + "'>";
  page += "<label class='field'>" + String(it ? "Lingua interfaccia" : "UI language") + "</label><select name='lang'>"
          "<option value='it'" + String(g_lang=="it" ? " selected" : "") + ">Italiano</option>"
          "<option value='en'" + String(g_lang=="en" ? " selected" : "") + ">English</option></select>";
  page += "</div><div>";
  page += "<label class='field'>" + String(it ? "Valuta base" : "Base currency") + "</label>"
          "<input name='fiat' type='text' maxlength='3' value='" + htmlEscape(g_fiat) + "'>";
  page += "<p class='desc'>" + String(it ? "Codice ISO a 3 lettere, per esempio CHF o EUR." : "3-letter ISO code, for example CHF or EUR.") + "</p>";
  page += "</div></div></section>";

  page += "<section id='display' class='card'><h3>Display</h3><div class='grid-2'><div>";
  page += "<label class='field'>" + String(it ? "Intervallo cambio pagina (secondi)" : "Page switch interval (seconds)") + "</label>"
          "<input name='page_s' type='number' min='5' max='600' value='" + String(page_s) + "'>";
  page += "<label class='field'>" + String(it ? "Orientamento" : "Orientation") + "</label><div class='rotations'>";
  for (uint8_t r=0; r<4; r++) {
    page += "<label class='rot'><input type='radio' name='rotation' value='" + String(r) + "'";
    if (g_rotation==r) page += " checked";
    page += "><span>" + String(r*90) + "&deg;</span></label>";
  }
  page += "</div>";
  page += checkbox("splash_enabled", g_splash_enabled, it ? "Mostra immagini splash" : "Enable splash images");
  page += "</div><div>";
  page += "<label class='field'>" + String(it ? "Fuso orario" : "Timezone") + "</label><select id='tzPreset' name='tz_preset'>"
          "<option value='europe'" + String(tzEurope ? " selected" : "") + ">" + String(it ? "Europa centrale (Svizzera / Italia)" : "Central Europe (Switzerland / Italy)") + "</option>"
          "<option value='utc'" + String(tzUTC ? " selected" : "") + ">UTC</option>"
          "<option value='custom'" + String(tzCustom ? " selected" : "") + ">" + String(it ? "Personalizzato" : "Custom") + "</option></select>";
  page += "<details class='advanced'" + String(tzCustom ? " open" : "") + "><summary>" + String(it ? "Configurazione avanzata POSIX" : "Advanced POSIX configuration") + "</summary>"
          "<label class='field'>POSIX TZ</label><input id='tzCustom' name='tz_custom' type='text' value='" + htmlEscape(g_tz_rule) + "'>"
          "<p class='desc'>CET-1CEST,M3.5.0,M10.5.0/3</p></details>";
  page += "</div></div>";

  page += "<h4>" + String(it ? "Pagine visibili" : "Visible pages") + " <span id='pageCount' class='page-count'></span></h4>";
  page += "<div class='mini-actions'><button class='mini-btn' type='button' id='pagesAll'>" + String(it ? "Attiva tutte" : "Enable all") + "</button>"
          "<button class='mini-btn' type='button' id='pagesNone'>" + String(it ? "Disattiva tutte" : "Disable all") + "</button></div>";
  page += "<div class='grid-pages'>";
  page += pageChip("p_WEATHER", g_show[P_WEATHER], it ? "Meteo" : "Weather");
  page += pageChip("p_AIR", g_show[P_AIR], it ? "Qualità aria" : "Air quality");
  page += pageChip("p_CLOCK", g_show[P_CLOCK], it ? "Orologio" : "Clock");
  page += pageChip("p_BINARY", g_show[P_BINARY], "Binary Clock");
  page += pageChip("p_CAL", g_show[P_CAL], it ? "Calendario" : "Calendar");
  page += pageChip("p_BTC", g_show[P_BTC], "Bitcoin");
  page += pageChip("p_QOD", g_show[P_QOD], it ? "Frase del giorno" : "Quote");
  page += pageChip("p_INFO", g_show[P_INFO], "Info");
  page += pageChip("p_COUNT", g_show[P_COUNT], "Countdown");
  page += pageChip("p_FX", g_show[P_FX], it ? "Valute" : "FX");
  page += pageChip("p_T24", g_show[P_T24], it ? "Temp 7 giorni" : "Temp 7 days");
  page += pageChip("p_SUN", g_show[P_SUN], it ? "Ore di luce" : "Sunlight");
  page += pageChip("p_NEWS", g_show[P_NEWS], "News");
  page += pageChip("p_HA", g_show[P_HA], "Home Assistant");
  page += pageChip("p_STELLAR", g_show[P_STELLAR], it ? "Sistema Solare" : "Solar System");
  page += pageChip("p_NOTES", g_show[P_NOTES], it ? "Post-it" : "Sticky Note");
  page += pageChip("p_CHRONOS", g_show[P_CHRONOS], "Chronos");
  page += "</div><span id='pageWarning' class='test-result'></span></section>";

  page += "<section id='integrations' class='card'><h3>" + String(it ? "Integrazioni" : "Integrations") + "</h3>"
          "<p class='desc'>" + String(it ? "I pulsanti Test verificano la connessione senza salvare modifiche non ancora confermate." : "Test buttons check the currently saved connection without saving pending edits.") + "</p>";

  page += "<div class='integration'><div class='integration-head'><div><span class='integration-name'>" + String(it ? "Calendario ICS" : "ICS Calendar") + "</span> " + statusPill(g_ics.length(),it) + "</div>"
          "<button type='button' class='test-btn' data-test='ics'>Test</button></div>"
          "<label class='field'>URL ICS</label><input name='ics' type='text' value='" + htmlEscape(g_ics) + "'>"
          "<span id='test-ics' class='test-result'></span></div>";

  page += "<div class='integration'><div class='integration-head'><div><span class='integration-name'>RSS News</span> " + statusPill(g_rss_url.length(),it) + "</div>"
          "<button type='button' class='test-btn' data-test='rss'>Test</button></div>"
          "<label class='field'>RSS feed URL</label><input name='rss_url' type='text' value='" + htmlEscape(g_rss_url) + "'>"
          "<span id='test-rss' class='test-result'></span></div>";

  page += "<div class='integration'><div class='integration-head'><div><span class='integration-name'>OpenAI</span> " + statusPill(g_oa_key.length(),it) + "</div>"
          "<button type='button' class='test-btn' data-test='openai'>Test</button></div>"
          "<label class='field'>OpenAI API Key</label><input name='openai_key' type='password' value='' placeholder='" +
          String(g_oa_key.length() ? (it ? "già configurata - lascia vuoto per mantenerla" : "already set - leave blank to keep") : "") + "'>";
  if (g_oa_key.length()) page += checkbox("clear_openai_key", false, it ? "Cancella chiave OpenAI" : "Clear OpenAI key");
  page += "<label class='field'>" + String(it ? "Argomento frase" : "Quote topic") + "</label><input name='openai_topic' type='text' value='" + htmlEscape(g_oa_topic) + "'>"
          "<div class='mini-actions'><button id='forceQod' type='button' class='btn-secondary'>" + String(it ? "Richiedi nuova frase ora" : "Get new quote now") + "</button></div>"
          "<span id='qod-result' class='test-result'></span><span id='test-openai' class='test-result'></span></div>";

  page += "<div class='integration'><div class='integration-head'><div><span class='integration-name'>Home Assistant</span> " + statusPill(g_ha_token.length(),it) + "</div>"
          "<button type='button' class='test-btn' data-test='ha'>Test</button></div>"
          "<label class='field'>IP</label><input name='ha_ip' type='text' value='" + htmlEscape(g_ha_ip) + "'>"
          "<label class='field'>Token</label><input name='ha_token' type='password' value='' placeholder='" +
          String(g_ha_token.length() ? (it ? "già configurato - lascia vuoto per mantenerlo" : "already set - leave blank to keep") : "") + "'>";
  if (g_ha_token.length()) page += checkbox("clear_ha_token", false, it ? "Cancella token HA" : "Clear HA token");
  page += "<label class='field'>" + String(it ? "Entità da mostrare (CSV, opzionale)" : "Entities to show (CSV, optional)") + "</label>"
          "<input name='ha_entities' type='text' value='" + htmlEscape(g_ha_entities) + "'>"
          "<p class='desc'>" + String(it ? "Esempio: light.sala,sensor.temperatura. Vuoto = filtro automatico." : "Example: light.living_room,sensor.temperature. Empty = automatic filter.") + "</p>"
          "<span id='test-ha' class='test-result'></span></div>";

  page += "<div class='integration'><div class='integration-head'><div><span class='integration-name'>Bitcoin</span></div></div>"
          "<label class='field'>" + String(it ? "Quantità di BTC posseduti" : "Owned BTC amount") + "</label>"
          "<input name='btc_owned' type='number' step='0.00000001' value='";
  if (!isnan(g_btc_owned)) page += String(g_btc_owned,8);
  page += "'></div></section>";

  page += "<section id='countdowns' class='card'><h3>" + String(it ? "Countdown" : "Countdowns") + " <span id='cdCount' class='page-count'></span></h3>"
          "<p class='desc'>" + String(it ? "Vengono mostrati solo gli slot utilizzati. Massimo 8." : "Only used slots are shown. Maximum 8.") + "</p>"
          "<div id='countdownRows'>";
  int firstHidden = -1;
  for (int i=0;i<8;i++) {
    bool used = cd[i].name.length() || cd[i].whenISO.length();
    if (!used && firstHidden < 0) firstHidden = i;
    page += "<div class='row cd-row' data-slot='" + String(i) + "'";
    if (!used) page += " hidden";
    page += "><div><label class='field'>" + String(it ? "Nome" : "Name") + "</label>"
            "<input name='cd" + String(i+1) + "n' type='text' value='" + htmlEscape(cd[i].name) + "'></div>"
            "<div><label class='field'>" + String(it ? "Data/ora" : "Date/time") + "</label>"
            "<input name='cd" + String(i+1) + "t' type='datetime-local' value='" + htmlEscape(cd[i].whenISO) + "'></div>"
            "<button type='button' class='remove-cd' title='" + String(it ? "Rimuovi" : "Remove") + "'>&times;</button></div>";
  }
  page += "</div><div class='mini-actions'><button id='addCountdown' type='button' class='mini-btn'>+ " + String(it ? "Aggiungi countdown" : "Add countdown") + "</button></div></section>";

  page += "<section id='notes' class='card'><h3>Post-it</h3>"
          "<label class='field'>" + String(it ? "Testo del post-it" : "Note text") + "</label>"
          "<textarea name='note' rows='4'>" + htmlEscape(g_note) + "</textarea>"
          "<p class='desc'>" + String(it ? "Lascia vuoto per cancellarlo." : "Leave empty to remove it.") + "</p></section>";

  page += "</main><div class='footer-bar'><div class='footer-inner'>"
          "<span id='dirtyState' class='dirty'>" + String(it ? "Nessuna modifica" : "No changes") + "</span>"
          "<div class='footer-actions'><button class='btn-primary' type='submit'>" + String(t_savebtn) + "</button>"
          "<a class='btn-secondary' href='/'>" + String(t_home) + "</a></div></div></div></form>";

  page += "<script>(function(){"
          "const f=document.getElementById('settingsForm'),dirty=document.getElementById('dirtyState');"
          "function markDirty(){dirty.textContent='" + String(it ? "● Modifiche non salvate" : "● Unsaved changes") + "';dirty.classList.add('on')}"
          "f.addEventListener('input',markDirty);f.addEventListener('change',markDirty);"
          "const boxes=[...document.querySelectorAll('.page-toggle')],pc=document.getElementById('pageCount'),pw=document.getElementById('pageWarning');"
          "function pageCount(){const n=boxes.filter(b=>b.checked).length;pc.textContent=n+' / '+boxes.length+' " + String(it ? "attive" : "active") + "';return n}"
          "boxes.forEach(b=>b.addEventListener('change',()=>{if(pageCount()===0){b.checked=true;pageCount();pw.textContent='" + String(it ? "Deve rimanere attiva almeno una pagina." : "At least one page must stay active.") + "';pw.className='test-result bad'}else{pw.textContent=''}}));"
          "document.getElementById('pagesAll').onclick=()=>{boxes.forEach(b=>b.checked=true);pageCount();markDirty()};"
          "document.getElementById('pagesNone').onclick=()=>{boxes.forEach(b=>b.checked=false);const clock=boxes.find(b=>b.name==='p_CLOCK');if(clock)clock.checked=true;pageCount();pw.textContent='" + String(it ? "Orologio lasciato attivo: non è possibile avere zero pagine." : "Clock left enabled: zero active pages is not allowed.") + "';pw.className='test-result';markDirty()};pageCount();"
          "const rows=[...document.querySelectorAll('.cd-row')],add=document.getElementById('addCountdown'),cc=document.getElementById('cdCount');"
          "function cdCount(){const n=rows.filter(r=>!r.hidden).length;cc.textContent=n+' / 8';add.disabled=n>=8;add.style.opacity=n>=8?'.45':'1'}"
          "add.onclick=()=>{const r=rows.find(x=>x.hidden);if(r){r.hidden=false;cdCount();markDirty();const i=r.querySelector('input');if(i)i.focus()}};"
          "rows.forEach(r=>r.querySelector('.remove-cd').onclick=()=>{r.querySelectorAll('input').forEach(i=>i.value='');r.hidden=true;cdCount();markDirty()});cdCount();"
          "const tz=document.getElementById('tzPreset'),tzd=document.querySelector('.advanced');tz.addEventListener('change',()=>{if(tz.value==='custom')tzd.open=true;markDirty()});"
          "const fq=document.getElementById('forceQod');if(fq)fq.onclick=async()=>{const o=document.getElementById('qod-result');fq.disabled=true;o.className='test-result';o.textContent='" + String(it ? "Generazione in corso…" : "Generating…") + "';try{const r=await fetch('/force_qod',{method:'POST',cache:'no-store'});const t=await r.text();o.textContent=(r.ok?'✓ ':'✕ ')+t;o.className='test-result '+(r.ok?'ok':'bad')}catch(e){o.textContent='✕ " + String(it ? "Errore di rete" : "Network error") + "';o.className='test-result bad'}fq.disabled=false};"
          "document.querySelectorAll('.test-btn').forEach(btn=>btn.onclick=async()=>{const k=btn.dataset.test,o=document.getElementById('test-'+k);btn.disabled=true;o.className='test-result';o.textContent='" + String(it ? "Test in corso…" : "Testing…") + "';try{const r=await fetch('/test_integration?kind='+encodeURIComponent(k),{cache:'no-store'});const t=await r.text();o.textContent=(r.ok?'✓ ':'✕ ')+t;o.className='test-result '+(r.ok?'ok':'bad')}catch(e){o.textContent='✕ " + String(it ? "Errore di rete" : "Network error") + "';o.className='test-result bad'}btn.disabled=false});"
          "})();</script></body></html>";

  return page;
}

static String htmlHome() {
  const bool it = (g_lang == "it");

  static const char* names_it[PAGES] = {
    "Meteo","Aria","Orologio","Orologio Binario","Calendario","Bitcoin",
    "Frase del giorno","Info","Countdown","Valute","Temp 7 giorni","Ore di luce",
    "News","Home Assistant","Sistema Solare","Post-it","Chronos"
  };
  static const char* names_en[PAGES] = {
    "Weather","Air","Clock","Binary Clock","Calendar","Bitcoin",
    "Quote of the Day","Info","Countdown","FX","Temp 7 days","Sunlight",
    "News","Home Assistant","Solar System","Sticky Note","Chronos"
  };
  const char** names = it ? names_it : names_en;

  int enabledCount=0, cdCount=0;
  String pageChips, cdItems;
  for (int i=0;i<PAGES;i++) {
    if (!g_show[i]) continue;
    enabledCount++;
    pageChips += "<span class='chip'>" + htmlEscape(String(names[i])) + "</span>";
  }
  for (int i=0;i<8;i++) {
    if (!cd[i].name.length() && !cd[i].whenISO.length()) continue;
    cdCount++;
    String n=cd[i].name.length()?cd[i].name:String(it?"Senza nome":"Unnamed");
    if (n.length()>32) n.remove(32);
    cdItems += "<div class='cd'><b>" + htmlEscape(n) + "</b>";
    if (cd[i].whenISO.length()) cdItems += "<span>" + htmlEscape(cd[i].whenISO) + "</span>";
    cdItems += "</div>";
  }

  char ver[32];
  strcpy_P(ver, FW_VERSION);
  int32_t rssi=WiFi.RSSI();
  String signal = rssi >= -55 ? (it ? "ottimo" : "excellent")
                : rssi >= -67 ? (it ? "buono" : "good")
                : rssi >= -75 ? (it ? "debole" : "weak")
                              : (it ? "molto debole" : "very weak");

  String page;
  page.reserve(7000);
  page += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
          "<title>SquaredCoso</title><style>"
          ":root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;font-family:-apple-system,BlinkMacSystemFont,system-ui,sans-serif;background:#050814;color:#f5f5f7}"
          "main{max-width:820px;margin:0 auto;padding:18px 12px 34px}.hero{background:linear-gradient(135deg,#0b1020,#071537);border:1px solid #22294c;border-radius:18px;padding:18px;box-shadow:0 12px 30px #0007}"
          ".hero-top{display:flex;justify-content:space-between;gap:12px;align-items:flex-start}.hero h1{margin:0;color:#ffdf40;font-size:1.5rem}.sub{color:#aeb5e8;font-size:.82rem;margin-top:4px}.online{display:inline-flex;align-items:center;gap:6px;background:#102a1b;color:#b4f3c4;border:1px solid #1f8b3b;border-radius:999px;padding:5px 9px;font-size:.75rem}.dot{width:7px;height:7px;border-radius:50%;background:#68e287}"
          ".stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px;margin-top:15px}.stat{background:#080d1c;border:1px solid #1c2444;border-radius:12px;padding:10px}.stat small{display:block;color:#8f97c7;font-size:.7rem;text-transform:uppercase}.stat b{display:block;margin-top:3px;font-size:.9rem}"
          ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:10px;margin-top:10px}.card{background:#0b1020;border:1px solid #191f3b;border-radius:16px;padding:15px;box-shadow:0 10px 25px #0006}.card h2{margin:0 0 10px;color:#ffdf40;font-size:1rem}.kv{display:flex;justify-content:space-between;gap:10px;padding:5px 0;border-bottom:1px solid #171d38;font-size:.84rem}.kv:last-child{border-bottom:0}.kv span:first-child{color:#9ea6d3}.kv span:last-child{text-align:right}"
          ".chips{display:flex;flex-wrap:wrap;gap:6px}.chip{border:1px solid #30385f;border-radius:999px;background:#070b18;color:#dce1ff;padding:5px 8px;font-size:.75rem}.cd{display:flex;justify-content:space-between;gap:10px;padding:7px 0;border-bottom:1px solid #171d38;font-size:.82rem}.cd:last-child{border-bottom:0}.cd span{color:#aab1dd;text-align:right}"
          ".status{font-size:.71rem;border-radius:999px;padding:3px 7px}.status.on{background:#12301d;color:#aef0bd}.status.off{background:#272b3d;color:#aeb3cc}.integration-line{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:6px 0;border-bottom:1px solid #171d38;font-size:.84rem}.integration-line:last-child{border-bottom:0}"
          ".actions{margin-top:14px;display:flex;gap:8px;flex-wrap:wrap}.btn{display:inline-block;border-radius:999px;padding:9px 14px;text-decoration:none;font-size:.84rem}.primary{background:linear-gradient(135deg,#ffdf40,#ff7a40);color:#111;font-weight:700}.secondary{border:1px solid #3b436e;color:#d6dcff}"
          "@media(max-width:520px){.hero-top{display:block}.online{margin-top:10px}.kv{display:block}.kv span{display:block!important;text-align:left!important;margin-top:2px}}"
          "</style></head><body><main>";

  page += "<section class='hero'><div class='hero-top'><div><h1>SquaredCoso</h1><div class='sub'>Gat Multi Ticker · " + htmlEscape(String(ver)) + "</div></div>"
          "<span class='online'><span class='dot'></span>Online</span></div><div class='stats'>";
  page += "<div class='stat'><small>IP</small><b>" + WiFi.localIP().toString() + "</b></div>";
  page += "<div class='stat'><small>Wi-Fi</small><b>" + String(rssi) + " dBm · " + signal + "</b></div>";
  page += "<div class='stat'><small>Uptime</small><b>" + uptimeCompact() + "</b></div>";
  page += "<div class='stat'><small>Heap</small><b>" + String(ESP.getFreeHeap()/1024UL) + " KB</b></div>";
  page += "</div></section>";

  page += "<div class='grid'><section class='card'><h2>" + String(it?"Pannello":"Panel") + "</h2>"
          "<div class='kv'><span>" + String(it?"Città":"City") + "</span><span>" + htmlEscape(g_city) + "</span></div>"
          "<div class='kv'><span>" + String(it?"Lingua":"Language") + "</span><span>" + htmlEscape(g_lang) + "</span></div>"
          "<div class='kv'><span>" + String(it?"Valuta":"Currency") + "</span><span>" + htmlEscape(g_fiat) + "</span></div>"
          "<div class='kv'><span>" + String(it?"Rotazione":"Rotation") + "</span><span>" + String((g_rotation&3)*90) + "&deg;</span></div>"
          "<div class='kv'><span>" + String(it?"Cambio pagina":"Page switch") + "</span><span>" + String(PAGE_INTERVAL_MS/1000UL) + " s</span></div></section>";

  page += "<section class='card'><h2>" + String(it?"Integrazioni":"Integrations") + "</h2>"
          "<div class='integration-line'><span>Calendario ICS</span>" + statusPill(g_ics.length(),it) + "</div>"
          "<div class='integration-line'><span>RSS News</span>" + statusPill(g_rss_url.length(),it) + "</div>"
          "<div class='integration-line'><span>OpenAI</span>" + statusPill(g_oa_key.length(),it) + "</div>"
          "<div class='integration-line'><span>Home Assistant</span>" + statusPill(g_ha_token.length(),it) + "</div></section></div>";

  page += "<section class='card'><h2>" + String(it?"Pagine":"Pages") + " · " + String(enabledCount) + " / " + String(PAGES) + "</h2><div class='chips'>" + pageChips + "</div></section>";

  page += "<section class='card'><h2>Countdown · " + String(cdCount) + " / 8</h2>";
  page += cdCount ? cdItems : String("<div class='sub'>") + (it?"Nessun countdown configurato.":"No countdown configured.") + "</div>";
  page += "</section>";

  page += "<div class='actions'><a class='btn primary' href='/settings'>" + String(it?"Apri impostazioni":"Open settings") + "</a></div>";
  page += "</main></body></html>";
  return page;
}

static void handleRootSTA() {
  web.send(200, "text/html; charset=utf-8", htmlHome());
}

static void startDNSCaptive() {
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

static void startAPPortal() {
  web.on("/", HTTP_GET, handleRootAP);
  web.on("/save", HTTP_POST, handleSave);
  web.on("/reboot", HTTP_GET, handleReboot);
  web.onNotFound(handleRootAP);
  web.begin();
}

static void startSTAWeb() {
  web.on("/", HTTP_GET, handleRootSTA);
  web.on("/settings", HTTP_ANY, handleSettings);
  web.on("/force_qod", HTTP_POST, handleForceQOD);
  web.on("/test_integration", HTTP_GET, handleIntegrationTest);
  web.onNotFound(handleRootSTA);
  web.begin();
}
