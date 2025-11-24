/*
===============================================================================
  SquaredCoso – Captive Portal & WebUI (versione riorganizzata / modernizzata)
===============================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "handlers/globals.h"   // per sanitizeText, P_*, CDEvent, PAGES
#define DNS_PORT 53

// ---------------------------------------------------------------------------
// EXTERN dal core
// ---------------------------------------------------------------------------
extern WebServer web;
extern DNSServer dnsServer;
extern Preferences prefs;

extern bool isHttpOk(int code);
extern void handleSettings();
extern void handleForceQOD();

extern uint32_t PAGE_INTERVAL_MS;

extern String g_city, g_lang, g_ics, g_fiat, g_rss_url;
extern String g_oa_key, g_oa_topic;
extern String g_from_station, g_to_station;
extern String g_ha_ip, g_ha_token;

extern double g_btc_owned;

extern bool   g_show[PAGES];
extern CDEvent cd[8];

// ---------------------------------------------------------------------------
// HTTP GET
// ---------------------------------------------------------------------------
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

// case-insensitive search
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

/* ---------------------------------------------------------------------------
   CAPTIVE PORTAL — AP MODE
--------------------------------------------------------------------------- */

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

/* ---------------------------------------------------------------------------
   CHECKBOX helper
--------------------------------------------------------------------------- */
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

/* ---------------------------------------------------------------------------
   SETTINGS PAGE — STA MODE
--------------------------------------------------------------------------- */
String htmlSettings(bool saved, const String& msg) {

  const bool it = (g_lang == "it");

  const char* t_title   = it ? "Impostazioni" : "Settings";
  const char* t_saved   = it ? "Impostazioni salvate. Ricarico…" : "Settings saved. Reloading…";

  const char* t_general = it ? "Generale" : "General";
  const char* t_city    = it ? "Città (meteo / aria)" : "City (weather / air)";
  const char* t_lang    = it ? "Lingua interfaccia (it/en)" : "UI language (it/en)";
  const char* t_ics     = it ? "URL calendario ICS" : "ICS calendar URL";
  const char* t_pageint = it ? "Intervallo cambio pagina (secondi)" : "Page switch interval (seconds)";

  const char* t_qod     = it ? "Frase del giorno" : "Quote of the Day";
  const char* t_qoddesc = it ? "Se imposti la chiave OpenAI userò GPT."
                             : "If OpenAI key is set, GPT will be used.";
  const char* t_oa_key   = "OpenAI API Key";
  const char* t_oa_topic = it ? "Argomento frase" : "Quote topic";
  const char* t_force    = it ? "Richiedi nuova frase ora" : "Get new quote now";

  const char* t_btc      = "Bitcoin";
  const char* t_btc_amt  = it ? "Quantità di BTC posseduti" : "Owned BTC amount";

  const char* t_rss      = "RSS News";
  const char* t_rss_label= "RSS feed URL";

  const char* t_pages    = it ? "Pagine visibili" : "Visible pages";

  const char* t_count    = it ? "Countdown (max 8)" : "Countdowns (max 8)";
  const char* t_name     = it ? "Nome #" : "Name #";
  const char* t_time     = it ? "Data/ora #" : "Date/time #";

  const char* t_savebtn  = it ? "Salva impostazioni" : "Save settings";
  const char* t_home     = it ? "Dashboard" : "Home";

  const char* t_ha       = "Home Assistant";

  // Notice
  String notice;
  if (saved) {
    notice = "<div class='alert ok'>" + String(t_saved) + "</div>";
  } else if (msg.length()) {
    notice = "<div class='alert warn'>" + msg + "</div>";
  }

  uint32_t page_s = PAGE_INTERVAL_MS / 1000;

  String page;
  page.reserve(8192);

  // header + layout base
  page += "<!doctype html><html><head>"
          "<meta charset='utf-8'/>"
          "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
          "<title>";
  page += t_title;
  page += "</title>"
          "<style>"
          "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,system-ui,"
          "sans-serif;background:#050814;color:#f5f5f7;}"
          "header{position:sticky;top:0;z-index:10;background:linear-gradient(135deg,#0b1020,#071537);"
          "padding:14px 18px 10px;border-bottom:1px solid #20274a;}"
          "header h1{margin:0;font-size:1.35rem;color:#ffdf40;}"
          "header p{margin:2px 0 0;font-size:.8rem;color:#b3b7ff;}"
          "main{max-width:960px;margin:0 auto;padding:14px 10px 80px;}"
          ".alert{border-radius:10px;padding:10px 12px;font-size:.85rem;margin:10px 4px 14px;}"
          ".alert.ok{background:#102a1b;border:1px solid #1f8b3b;color:#b4f3c4;}"
          ".alert.warn{background:#2a1710;border:1px solid #e26f3b;color:#ffd2b6;}"
          ".card{background:#0b1020;border:1px solid #191f3b;border-radius:16px;"
          "padding:16px 16px 14px;margin:10px 4px;box-shadow:0 10px 26px rgba(0,0,0,.45);}"
          ".card h3{margin:0 0 8px;font-size:1rem;color:#ffdf40;}"
          ".card p.desc{margin:4px 0 10px;font-size:.8rem;color:#a9afff;}"
          ".grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:10px;}"
          ".grid-pages{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:8px;}"
          "label.field{display:block;margin:8px 0 4px;font-size:.8rem;color:#b3b8ff;}"
          "input[type='text'],input[type='password'],input[type='number'],"
          "input[type='datetime-local']{width:100%;padding:9px 10px;border-radius:10px;"
          "border:1px solid #313855;background:#070b18;color:#f5f5f7;box-sizing:border-box;"
          "font-size:.92rem;}"
          "input:focus{outline:none;border-color:#5b7cff;box-shadow:0 0 0 1px #5b7cff55;}"
          ".row{display:flex;gap:12px;flex-wrap:wrap;margin:4px 0;}"
          ".row>div{flex:1 1 180px;}"
          ".chk{display:flex;align-items:center;gap:8px;padding:8px 10px;border:1px solid #232949;"
          "border-radius:10px;font-size:.9rem;background:#070b18;}"
          ".chk input{width:auto;margin:0;}"
          ".chk span{flex:1;color:#e3e6ff;}"
          ".footer-bar{position:fixed;left:0;right:0;bottom:0;background:#050814f2;"
          "border-top:1px solid #22284a;padding:8px 10px;backdrop-filter:blur(8px);}"
          ".footer-inner{max-width:960px;margin:0 auto;display:flex;align-items:center;"
          "justify-content:space-between;gap:8px;}"
          ".footer-inner span{font-size:.8rem;color:#9fa4ff;}"
          ".btn-primary{border:none;border-radius:999px;padding:9px 16px;font-size:.9rem;"
          "font-weight:600;background:linear-gradient(135deg,#ffdf40,#ff7a40);color:#111;cursor:pointer;}"
          ".btn-secondary{border-radius:999px;padding:8px 14px;font-size:.85rem;"
          "border:1px solid #3b436e;background:transparent;color:#d6dcff;text-decoration:none;}"
          ".btn-primary:active{transform:scale(.98);}"
          "a{color:#ffdf40;text-decoration:none;}"
          "a:hover{text-decoration:underline;}"
          "@media (max-width:600px){header{padding:12px 12px 8px;}main{padding:12px 8px 76px;}}"
          "</style></head><body>";

  page += "<header><h1>";
  page += t_title;
  page += "</h1><p>Gat Multi Ticker &mdash; SquaredCoso</p></header><main>";
  page += notice;

  // form
  page += "<form method='POST' action='/settings'>";

  // === BLOCCO SUPERIORE: GENERALE + PAGINE VISIBILI ===
  page += "<div class='grid-2'>";

  // --- GENERALE ---
  page += "<div class='card'><h3>";
  page += t_general;
  page += "</h3>";

  page += "<label class='field'>";
  page += t_city;
  page += "</label><input name='city' type='text' value='";
  page += sanitizeText(g_city);
  page += "'/>";

  page += "<label class='field'>";
  page += t_lang;
  page += "</label><input name='lang' type='text' value='";
  page += sanitizeText(g_lang);
  page += "'/>";

  page += "<label class='field'>";
  page += t_ics;
  page += "</label><input name='ics' type='text' value='";
  page += sanitizeText(g_ics);
  page += "'/>";

  page += "<label class='field'>";
  page += t_pageint;
  page += "</label><input name='page_s' type='number' min='5' max='600' value='";
  page += String(page_s);
  page += "'/>";

  page += "</div>";

  // --- PAGINE VISIBILI ---
  page += "<div class='card'><h3>";
  page += t_pages;
  page += "</h3><div class='grid-pages'>";

  page += checkbox("p_WEATHER",  g_show[P_WEATHER],  it ? "Meteo"           : "Weather");
  page += checkbox("p_AIR",      g_show[P_AIR],      it ? "Qualità aria"    : "Air quality");
  page += checkbox("p_CLOCK",    g_show[P_CLOCK],    it ? "Orologio"        : "Clock");
  page += checkbox("p_CAL",      g_show[P_CAL],      it ? "Calendario ICS"  : "Calendar");
  page += checkbox("p_BTC",      g_show[P_BTC],      "Bitcoin");
  page += checkbox("p_QOD",      g_show[P_QOD],      it ? "Frase del giorno": "Quote of the Day");
  page += checkbox("p_INFO",     g_show[P_INFO],     "Info");
  page += checkbox("p_COUNT",    g_show[P_COUNT],    "Countdown");
  page += checkbox("p_FX",       g_show[P_FX],       it ? "Valute"          : "FX");
  page += checkbox("p_T24",      g_show[P_T24],      "T24");
  page += checkbox("p_SUN",      g_show[P_SUN],      it ? "Ore di luce"     : "Sunlight");
  page += checkbox("p_NEWS",     g_show[P_NEWS],     "News");
  page += checkbox("p_STELLAR",  g_show[P_STELLAR],  it ? "Sistema Solare"  : "Solar System");
  page += checkbox("p_HA",       g_show[P_HA],       "Home Assistant");

  page += "</div></div></div>"; // chiusura grid-2

  // === BLOCCO DATI ESTERNI: QOD + BTC + HA + RSS ===
  page += "<div class='grid-2'>";

  // --- QOD ---
  page += "<div class='card'><h3>";
  page += t_qod;
  page += "</h3><p class='desc'>";
  page += t_qoddesc;
  page += "</p>";

  page += "<label class='field'>";
  page += t_oa_key;
  page += "</label><input name='openai_key' type='password' value='";
  page += sanitizeText(g_oa_key);
  page += "' autocomplete='off'/>";

  page += "<label class='field'>";
  page += t_oa_topic;
  page += "</label><input name='openai_topic' type='text' value='";
  page += sanitizeText(g_oa_topic);
  page += "'/>";

  page += "<p style='margin-top:10px'><button class='btn-secondary' "
          "formaction='/force_qod' formmethod='POST'>";
  page += t_force;
  page += "</button></p>";

  page += "</div>";

  // --- BTC + HA + RSS nello stesso blocco verticale ---
  page += "<div>";

  // BTC
  page += "<div class='card'><h3>";
  page += t_btc;
  page += "</h3>";

  page += "<label class='field'>";
  page += t_btc_amt;
  page += "</label><input name='btc_owned' type='number' step='0.00000001' value='";
  if (!isnan(g_btc_owned))
    page += String(g_btc_owned, 8);
  page += "'/></div>";

  // Home Assistant
  page += "<div class='card'><h3>";
  page += t_ha;
  page += "</h3>";

  page += "<label class='field'>IP</label><input name='ha_ip' type='text' value='";
  page += sanitizeText(g_ha_ip);
  page += "'/>";

  page += "<label class='field'>Token</label><input name='ha_token' type='password' value='";
  page += sanitizeText(g_ha_token);
  page += "' autocomplete='off'/></div>";

  // RSS
  page += "<div class='card'><h3>";
  page += t_rss;
  page += "</h3>";

  page += "<label class='field'>";
  page += t_rss_label;
  page += "</label><input name='rss_url' type='text' value='";
  page += sanitizeText(g_rss_url);
  page += "'/></div>";

  page += "</div></div>"; // fine grid-2 blocco dati esterni

  // === COUNTDOWN ===
  page += "<div class='card'><h3>";
  page += t_count;
  page += "</h3>";

  for (int i = 0; i < 8; i++) {
    page += "<div class='row'>";

    page += "<div><label class='field'>";
    page += t_name;
    page += String(i + 1);
    page += "</label><input name='cd";
    page += String(i + 1);
    page += "n' type='text' value='";
    page += sanitizeText(cd[i].name);
    page += "'/></div>";

    page += "<div><label class='field'>";
    page += t_time;
    page += String(i + 1);
    page += "</label><input name='cd";
    page += String(i + 1);
    page += "t' type='datetime-local' value='";
    page += sanitizeText(cd[i].whenISO);
    page += "'/></div>";

    page += "</div>";
  }

  page += "</div>"; // card countdown

  page += "</main>";

  // FOOTER BAR (salva + home)
  page += "<div class='footer-bar'><div class='footer-inner'>"
          "<span>";
  page += it ? "Le modifiche vengono applicate dopo il salvataggio."
             : "Changes apply after saving.";
  page += "</span>"
          "<div>"
          "<button class='btn-primary' type='submit'>";
  page += t_savebtn;
  page += "</button> "
          "<a class='btn-secondary' href='/'>";
  page += t_home;
  page += "</a></div></div></div>";

  page += "</form></body></html>";

  return page;
}

/* ---------------------------------------------------------------------------
   HOME STA
--------------------------------------------------------------------------- */
static String htmlHome() {
  uint32_t s = PAGE_INTERVAL_MS / 1000;
  String enabledList;

  const char* names[PAGES] = {
    "Meteo", "Aria", "Orologio", "Calendario",
    "BTC", "Frase", "Info", "Countdown",
    "FX", "T24", "Ore di luce", "News",
    "Home Assistant", "Sistema Stellare"
  };

  for (int i = 0; i < PAGES; i++) {
    if (g_show[i]) {
      if (enabledList.length()) enabledList += ", ";
      enabledList += names[i];
    }
  }

  String page;
  page.reserve(2048);

  page += "<!doctype html><html><head>"
          "<meta charset='utf-8'/>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
          "<title>Gat Multi Ticker</title>"
          "<style>"
          "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,system-ui,sans-serif;"
          "background:#050814;color:#f5f5f7;}"
          "main{max-width:720px;margin:0 auto;padding:18px 14px 32px;}"
          "h1{margin:0 0 8px;font-size:1.4rem;color:#ffdf40;}"
          "p{margin:4px 0;font-size:.9rem;color:#d8ddff;}"
          ".card{background:#0b1020;border-radius:16px;border:1px solid #191f3b;"
          "padding:14px 16px;margin-top:12px;box-shadow:0 10px 26px rgba(0,0,0,.45);}"
          ".label{font-size:.78rem;color:#9ca2ff;text-transform:uppercase;letter-spacing:.06em;}"
          ".value{font-size:.95rem;}"
          "a{color:#ffdf40;text-decoration:none;font-weight:500;}"
          "a:hover{text-decoration:underline;}"
          "</style></head><body>"
          "<main><h1>Gat Multi Ticker</h1>";

  page += "<div class='card'>"
          "<div class='label'>Profilo corrente</div>";

  page += "<p class='value'><b>Città:</b> " + sanitizeText(g_city) + "<br>"
          "<b>Lingua:</b> " + sanitizeText(g_lang) + "<br>"
          "<b>Intervallo cambio pagina:</b> " + String(s) + " s</p></div>";

  page += "<div class='card'>"
          "<div class='label'>Pagine attive</div>"
          "<p class='value'>";
  page += enabledList.length() ? enabledList : String("-");
  page += "</p></div>";

  page += "<div class='card'>"
          "<div class='label'>Collegamento treno</div>"
          "<p class='value'>" + sanitizeText(g_from_station) + " &rarr; "
        + sanitizeText(g_to_station) + "</p></div>";

  page += "<div class='card'>"
          "<div class='label'>Quote of the Day</div><p class='value'>";

  if (g_oa_key.length()) {
    page += "OpenAI (tema: " + sanitizeText(g_oa_topic) + ")";
  } else {
    page += "ZenQuotes (default)";
  }
  page += "</p></div>";

  page += "<p style='margin-top:16px'><a href='/settings'>Apri impostazioni</a></p>"
          "</main></body></html>";

  return page;
}

/* ---------------------------------------------------------------------------
   STA — ROOT HANDLER
--------------------------------------------------------------------------- */
static void handleRootSTA() {
  web.send(200, "text/html; charset=utf-8", htmlHome());
}

/* ---------------------------------------------------------------------------
   AP MODE — ROUTES
--------------------------------------------------------------------------- */
static void startDNSCaptive() {
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

static void startAPPortal() {
  web.on("/",      HTTP_GET,  handleRootAP);
  web.on("/save",  HTTP_POST, handleSave);
  web.on("/reboot",HTTP_GET,  handleReboot);
  web.onNotFound(handleRootAP);
  web.begin();
}

/* ---------------------------------------------------------------------------
   STA MODE — ROUTES
--------------------------------------------------------------------------- */
static void startSTAWeb() {
  web.on("/",          HTTP_GET,  handleRootSTA);
  web.on("/settings",  HTTP_ANY,  handleSettings);
  web.on("/force_qod", HTTP_POST, handleForceQOD);
  web.onNotFound(handleRootSTA);
  web.begin();
}
