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
#include "handlers/globals.h"  // per sanitizeText, P_*, CDEvent, PAGES
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

extern bool g_show[PAGES];
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

  const char* t_title = it ? "Impostazioni" : "Settings";
  const char* t_saved = it ? "Impostazioni salvate. Ricarico…" : "Settings saved. Reloading…";

  const char* t_general = it ? "Generale" : "General";
  const char* t_city = it ? "Città (meteo / aria)" : "City (weather / air)";
  const char* t_lang = it ? "Lingua interfaccia (it/en)" : "UI language (it/en)";
  const char* t_ics = it ? "URL calendario ICS" : "ICS calendar URL";
  const char* t_pageint = it ? "Intervallo cambio pagina (secondi)" : "Page switch interval (seconds)";

  const char* t_qod = it ? "Frase del giorno" : "Quote of the Day";
  const char* t_qoddesc = it ? "Se imposti la chiave OpenAI userò GPT." : "If OpenAI key is set, GPT will be used.";
  const char* t_oa_key = "OpenAI API Key";
  const char* t_oa_topic = it ? "Argomento frase" : "Quote topic";
  const char* t_force = it ? "Richiedi nuova frase ora" : "Get new quote now";

  const char* t_btc = "Bitcoin";
  const char* t_btc_amt = it ? "Quantità di BTC posseduti" : "Owned BTC amount";

  const char* t_rss = "RSS News";
  const char* t_rss_label = "RSS feed URL";

  const char* t_pages = it ? "Pagine visibili" : "Visible pages";

  const char* t_count = it ? "Countdown (max 8)" : "Countdowns (max 8)";
  const char* t_name = it ? "Nome #" : "Name #";
  const char* t_time = it ? "Data/ora #" : "Date/time #";

  const char* t_savebtn = it ? "Salva impostazioni" : "Save settings";
  const char* t_home = it ? "Dashboard" : "Home";
  const char* t_ha = "Home Assistant";

  String notice;
  if (saved) notice = "<div class='alert ok'>" + String(t_saved) + "</div>";
  else if (msg.length()) notice = "<div class='alert warn'>" + msg + "</div>";

  uint32_t page_s = PAGE_INTERVAL_MS / 1000;

  // Nomi pagine
static const char* names_it[PAGES] = {
  "Meteo",
  "Aria",
  "Orologio",
  "Orologio Binario",
  "Calendario ICS",
  "Bitcoin",
  "Frase del giorno",
  "Info",
  "Countdown",
  "Valute",
  "Temp 7 giorni",
  "Ore di luce",
  "News",
  "Home Assistant",
  "Sistema Solare",
  "Post-it",
  "Chronos"
};

static const char* names_en[PAGES] = {
  "Weather",
  "Air",
  "Clock",
  "Binary Clock",
  "Calendar",
  "Bitcoin",
  "Quote of the Day",
  "Info",
  "Countdown",
  "FX",
  "T24",
  "Sunlight",
  "News",
  "Home Assistant",
  "Solar System",
  "Sticky Note",
  "Chronos"
};

  const char** names = it ? names_it : names_en;

  // Costruzione pagina
  String page;
  page.reserve(9000);

  page += "<!doctype html><html><head>"
          "<meta charset='utf-8'/>"
          "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
          "<title>"
          + String(t_title) + "</title>";

  // CSS
  page +=
    "<style>"
    "body{margin:0;font-family:-apple-system,BlinkMacSystemFont,system-ui,sans-serif;"
    "background:#050814;color:#f5f5f7;}"
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

    "input[type='text'],input[type='password'],input[type='number'],input[type='datetime-local'],"
    "select{width:100%;padding:9px 10px;border-radius:10px;border:1px solid #313855;"
    "background:#070b18;color:#f5f5f7;box-sizing:border-box;font-size:.92rem;}"

    ".chk{display:flex;align-items:center;gap:8px;padding:8px 10px;border:1px solid #232949;"
    "border-radius:10px;font-size:.9rem;background:#070b18;}"
    ".chk input{width:auto;margin:0;}"

    ".footer-bar{position:fixed;left:0;right:0;bottom:0;background:#050814f2;"
    "border-top:1px solid #22284a;padding:8px 10px;backdrop-filter:blur(8px);}"
    ".footer-inner{max-width:960px;margin:0 auto;display:flex;align-items:center;"
    "justify-content:space-between;gap:8px;}"

    ".btn-primary{border:none;border-radius:999px;padding:9px 16px;font-size:.9rem;font-weight:600;"
    "background:linear-gradient(135deg,#ffdf40,#ff7a40);color:#111;cursor:pointer;}"
    ".btn-secondary{border-radius:999px;padding:8px 14px;font-size:.85rem;border:1px solid #3b436e;"
    "background:transparent;color:#d6dcff;text-decoration:none;}"
    "</style></head><body>";

  // Header
  page += "<header><h1>" + String(t_title) + "</h1>"
                                             "<p>Gat Multi Ticker — SquaredCoso</p></header><main>";

  page += notice;

  page += "<form method='POST' action='/settings'>"
          "<div class='grid-2'>";

  // --- CARD GENERALE -------------------------------------------------------
  page += "<div class='card'><h3>" + String(t_general) + "</h3>";

  page += "<label class='field'>" + String(t_city) + "</label><input name='city' type='text' value='" + sanitizeText(g_city) + "'/>";

  page += "<label class='field'>" + String(t_lang) + "</label><input name='lang' type='text' value='" + sanitizeText(g_lang) + "'/>";

  page += "<label class='field'>" + String(t_ics) + "</label><input name='ics' type='text' value='" + sanitizeText(g_ics) + "'/>";

  page += "<label class='field'>" + String(t_pageint) + "</label><input name='page_s' type='number' min='5' max='600' value='" + String(page_s) + "'/>";

  page += checkbox("splash_enabled", g_splash_enabled,
                   it ? "Mostra immagini splash" : "Enable splash images");

  page += "</div>";  // fine CARD GENERALE


  // --- PAGINE VISIBILI -----------------------------------------------------
  page += "<div class='card'><h3>" + String(t_pages) + "</h3>";

  /* --- Pulsante seleziona/deseleziona tutto --- */
  page +=
    "<button type='button' id='toggleAll' "
    "style='margin:6px 0 10px;padding:6px 12px;border-radius:8px;"
    "background:#232949;border:1px solid #3b436e;color:#d6dcff;"
    "font-size:.82rem;cursor:pointer;'>"
    + String(it ? "Seleziona / Deseleziona tutto" : "Select / Deselect all")
    + "</button>";

  /* --- Script JS per togglare tutte le checkbox delle pagine --- */
  page +=
    "<script>"
    "document.getElementById('toggleAll').onclick=function(){"
    "const boxes=document.querySelectorAll('.grid-pages input[type=checkbox]');"
    "let allChecked=true;"
    "boxes.forEach(b=>{ if(!b.checked) allChecked=false; });"
    "boxes.forEach(b=>{ b.checked=!allChecked; });"
    "};"
    "</script>";

  /* --- Griglia con le checkbox --- */
  page += "<div class='grid-pages'>";

  page += checkbox("p_WEATHER", g_show[P_WEATHER], it ? "Meteo" : "Weather");
  page += checkbox("p_AIR", g_show[P_AIR], it ? "Qualità aria" : "Air quality");
  page += checkbox("p_CLOCK", g_show[P_CLOCK], it ? "Orologio" : "Clock");
  page += checkbox("p_BINARY", g_show[P_BINARY], "Binary Clock");
  page += checkbox("p_CAL", g_show[P_CAL], it ? "Calendario ICS" : "Calendar");
  page += checkbox("p_BTC", g_show[P_BTC], "Bitcoin");
  page += checkbox("p_QOD", g_show[P_QOD], it ? "Frase del giorno" : "Quote of the Day");
  page += checkbox("p_INFO", g_show[P_INFO], "Info");
  page += checkbox("p_COUNT", g_show[P_COUNT], "Countdown");
  page += checkbox("p_FX", g_show[P_FX], it ? "Valute" : "FX");
  page += checkbox("p_T24", g_show[P_T24], "Temp 7 days");
  page += checkbox("p_SUN", g_show[P_SUN], it ? "Ore di luce" : "Sunlight");
  page += checkbox("p_NEWS", g_show[P_NEWS], "News");
  page += checkbox("p_HA", g_show[P_HA], "Home Assistant");
  page += checkbox("p_STELLAR", g_show[P_STELLAR], it ? "Sistema Solare" : "Solar System");
  page += checkbox("p_NOTES", g_show[P_NOTES], it ? "Post-it" : "Sticky Note");
  page += checkbox("p_CHRONOS", g_show[P_CHRONOS], "Chronos");


  page += "</div></div></div>";  // chiude grid colonne principali


  // --- QOD + BTC + HA + RSS ------------------------------------------------
  page += "<div class='grid-2'>";

  // QOD
  page += "<div class='card'><h3>" + String(t_qod) + "</h3>"
                                                     "<p class='desc'>"
          + String(t_qoddesc) + "</p>";

  page += "<label class='field'>" + String(t_oa_key) + "</label>"
                                                       "<input name='openai_key' type='password' value='"
          + sanitizeText(g_oa_key) + "'/>";

  page += "<label class='field'>" + String(t_oa_topic) + "</label>"
                                                         "<input name='openai_topic' type='text' value='"
          + sanitizeText(g_oa_topic) + "'/>";

  page += "<p style='margin-top:10px'>"
          "<button class='btn-secondary' formaction='/force_qod' formmethod='POST'>"
          + String(t_force) + "</button></p>";

  page += "</div>";

  // Colonna destra
  page += "<div>";

  // BTC
  page += "<div class='card'><h3>" + String(t_btc) + "</h3>";
  page += "<label class='field'>" + String(t_btc_amt) + "</label>"
                                                        "<input name='btc_owned' type='number' step='0.00000001' value='";
  if (!isnan(g_btc_owned)) page += String(g_btc_owned, 8);
  page += "'/></div>";

  // HA
  page += "<div class='card'><h3>" + String(t_ha) + "</h3>"
                                                    "<label class='field'>IP</label>"
                                                    "<input name='ha_ip' type='text' value='"
          + sanitizeText(g_ha_ip) + "'/>"
                                    "<label class='field'>Token</label>"
                                    "<input name='ha_token' type='password' value='"
          + sanitizeText(g_ha_token) + "'/>"
                                       "</div>";

  // RSS
  page += "<div class='card'><h3>" + String(t_rss) + "</h3>"
                                                     "<label class='field'>"
          + String(t_rss_label) + "</label>"
                                  "<input name='rss_url' type='text' value='"
          + sanitizeText(g_rss_url) + "'/>"
                                      "</div>";

  page += "</div></div>";  // fine grid-2 QOD/BTC/HA/RSS


  // --- COUNTDOWN -----------------------------------------------------------
  page += "<div class='card'><h3>" + String(t_count) + "</h3>";

  for (int i = 0; i < 8; i++) {
    page += "<div class='row'><div>"
            "<label class='field'>"
            + String(t_name) + String(i + 1) + "</label>"
                                               "<input name='cd"
            + String(i + 1) + "n' type='text' value='" + sanitizeText(cd[i].name) + "'/></div>";

    page += "<div><label class='field'>" + String(t_time) + String(i + 1) + "</label>"
                                                                            "<input name='cd"
            + String(i + 1) + "t' type='datetime-local' value='" + sanitizeText(cd[i].whenISO) + "'/></div></div>";
  }

  page += "</div>";  // fine CARD COUNTDOWN
  // --- NOTE (POST-IT) -------------------------------------------------------
  page += "<div class='card'><h3>";
  page += (it ? "Post-it" : "Sticky Note");
  page += "</h3>";

  page += "<label class='field'>";
  page += (it ? "Testo del post-it" : "Note text");
  page += "</label>";

  page += "<textarea name='note' rows='3' style='width:100%;padding:8px;";
  page += "border-radius:10px;border:1px solid #313855;background:#070b18;";
  page += "color:#f5f5f7;box-sizing:border-box;font-size:.92rem;'>";
  page += sanitizeText(g_note);
  page += "</textarea>";

  page += "<p class='desc' style='font-size:.8rem;color:#a9afff;margin-top:6px;'>";
  page += (it ? "Lascia vuoto per cancellare il post-it."
              : "Leave empty to remove the sticky note.");
  page += "</p>";

  page += "</div>";  // fine card Post-it


  // FOOTER
  page += "</main>"
          "<div class='footer-bar'><div class='footer-inner'><span>"
          + String(it ? "Le modifiche vengono applicate dopo il salvataggio."
                      : "Changes apply after saving.")
          + "</span><div><button class='btn-primary' type='submit'>"
          + String(t_savebtn)
          + "</button> <a class='btn-secondary' href='/'>"
          + String(t_home)
          + "</a></div></div></div></form></body></html>";

  return page;
}



/* ---------------------------------------------------------------------------
   HOME STA — DASHBOARD COMPLETA
--------------------------------------------------------------------------- */
static String htmlHome() {
  const bool it = (g_lang == "it");

  // --------- Testi localizzati ---------
  const char* t_title = "Gat Multi Ticker";
  const char* t_profile = it ? "Profilo pannello" : "Panel profile";
  const char* t_city = it ? "Città" : "City";
  const char* t_lang_label = it ? "Lingua interfaccia" : "UI language";
  const char* t_fiat = it ? "Valuta base" : "Base currency";
  const char* t_ics = it ? "Calendario ICS" : "ICS calendar URL";

  const char* t_pages_card = it ? "Rotazione pagine" : "Page rotation";
  const char* t_pages_on = it ? "Pagine attive" : "Active pages";
  const char* t_pages_off = it ? "Pagine disattivate" : "Disabled pages";
  const char* t_pages_int = it ? "Intervallo cambio pagina" : "Page switch interval";

  const char* t_data = it ? "Dati & integrazioni" : "Data & integrations";
  const char* t_qod_label = it ? "Frase del giorno" : "Quote of the Day";
  const char* t_qod_src_ai = it ? "OpenAI (tema: " : "OpenAI (topic: ";
  const char* t_qod_src_zen = it ? "ZenQuotes (predefinito)" : "ZenQuotes (default)";
  const char* t_btc_label = "Bitcoin";
  const char* t_btc_amt = it ? "BTC posseduti" : "Owned BTC";
  const char* t_rss_label = "RSS";
  const char* t_rss_url = it ? "Feed RSS" : "RSS feed URL";
  const char* t_ha_label = "Home Assistant";
  const char* t_ha_ip = "IP";
  const char* t_ha_token = "Token";
  const char* t_ha_missing = it ? "non impostato" : "not set";

  const char* t_cd_card = it ? "Countdown configurati" : "Configured countdowns";
  const char* t_cd_none = it ? "Nessun countdown attivo" : "No active countdowns";
  const char* t_cd_count = it ? "Totale countdown" : "Total countdowns";
  const char* t_cd_list = it ? "Nomi" : "Names";

  const char* t_settings = it ? "Apri impostazioni" : "Open settings";

  // --------- Intervallo pagine ----------
  uint32_t s = PAGE_INTERVAL_MS / 1000;

  // --------- Nomi pagine (it/en) --------
  static const char* names_it[PAGES] = {
    "Meteo",          // 0
    "Aria",           // 1
    "Orologio",       // 2
    "Orologio Binario",//3
    "Calendario ICS", // 4
    "Bitcoin",        // 5
    "Frase del giorno",//6
    "Info",           // 7
    "Countdown",      // 8
    "Valute",         // 9
    "T24",            // 10
    "Ore di luce",    // 11
    "News",           // 12
    "Home Assistant", // 13
    "Sistema Solare", // 14
    "Post-it",        // 15
    "Chronos"         // 16
  };

  static const char* names_en[PAGES] = {
    "Weather",        // 0
    "Air",            // 1
    "Clock",          // 2
    "Binary Clock",   // 3
    "Calendar",       // 4
    "Bitcoin",        // 5
    "Quote of the Day",//6
    "Info",           // 7
    "Countdown",      // 8
    "FX",             // 9
    "T24",            // 10
    "Sunlight",       // 11
    "News",           // 12
    "Home Assistant", // 13
    "Solar System",   // 14
    "Sticky Note",    // 15
    "Chronos"         // 16
  };

  const char** names = it ? names_it : names_en;

  // Liste pagine attive / disattive
  String enabledList;
  String disabledList;
  int enabledCount = 0;

  for (int i = 0; i < PAGES; i++) {
    if (g_show[i]) {
      if (enabledList.length()) enabledList += ", ";
      enabledList += names[i];
      enabledCount++;
    } else {
      if (disabledList.length()) disabledList += ", ";
      disabledList += names[i];
    }
  }

  // --------- Countdown ---------
  int cdCount = 0;
  String cdNames;

  for (int i = 0; i < 8; i++) {
    if (cd[i].name.length() == 0 && cd[i].whenISO.length() == 0) continue;
    cdCount++;
    if (cdNames.length()) cdNames += ", ";

    String nn = cd[i].name;
    if (nn.length() > 24) nn.remove(24);
    cdNames += sanitizeText(nn);
  }

  // --------- HTML ----------
  String page;
  page.reserve(4096);

  page += "<!doctype html><html><head>"
          "<meta charset='utf-8'/>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
          "<title>" + String(t_title) + "</title>"
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
          ".kv{margin:2px 0;}"
          ".kv b{display:inline-block;min-width:130px;}"
          "</style></head><body><main>";

  page += "<h1>" + String(t_title) + "</h1>";

  // ------------------------------------------------------------------
  // CARD 1 — PROFILO PANNELLO
  // ------------------------------------------------------------------
  page += "<div class='card'><div class='label'>" + String(t_profile) +
          "</div><p class='value'>";

  page += "<div class='kv'><b>" + String(t_city) + ":</b> " + sanitizeText(g_city) + "</div>";
  page += "<div class='kv'><b>" + String(t_lang_label) + ":</b> " + sanitizeText(g_lang) + "</div>";
  page += "<div class='kv'><b>" + String(t_fiat) + ":</b> " + sanitizeText(g_fiat) + "</div>";
  page += "<div class='kv'><b>" + String(t_ics) + ":</b> " +
          (g_ics.length() ? sanitizeText(g_ics) : String("-")) + "</div>";

  page += "</p></div>";

  // ------------------------------------------------------------------
  // CARD 2 — ROTAZIONE PAGINE
  // ------------------------------------------------------------------
  page += "<div class='card'><div class='label'>" + String(t_pages_card) +
          "</div><p class='value'>";

  page += "<div class='kv'><b>" + String(t_pages_int) + ":</b> " + String(s) + " s</div>";

  page += "<div class='kv'><b>" + String(t_pages_on) + ":</b> " +
          (enabledList.length() ? enabledList : String("-")) + "</div>";

  page += "<div class='kv'><b>" + String(t_pages_off) + ":</b> " +
          (disabledList.length() ? disabledList : String("-")) + "</div>";

  page += "<div class='kv'><b>" + String(it ? "Totale pagine attive" : "Total active pages") +
          ":</b> " + String(enabledCount) + " / " + String(PAGES) + "</div>";

  page += "</p></div>";

  // ------------------------------------------------------------------
  // CARD 3 — DATI & INTEGRAZIONI
  // ------------------------------------------------------------------
  page += "<div class='card'><div class='label'>" + String(t_data) +
          "</div><p class='value'>";

  // QOD
  page += "<div class='kv'><b>" + String(t_qod_label) + ":</b> ";
  if (g_oa_key.length())
    page += String(t_qod_src_ai) + sanitizeText(g_oa_topic) + ")";
  else
    page += String(t_qod_src_zen);
  page += "</div>";

  // BTC
  page += "<div class='kv'><b>" + String(t_btc_label) + ":</b> ";
  if (!isnan(g_btc_owned))
    page += String(g_btc_owned, 8) + " BTC";
  else
    page += "-";
  page += "</div>";

  // RSS
  page += "<div class='kv'><b>" + String(t_rss_label) + ":</b> " +
          (g_rss_url.length() ? sanitizeText(g_rss_url) : String("-")) + "</div>";

  // HA
  page += "<div class='kv'><b>" + String(t_ha_label) + ":</b> ";
  if (g_ha_ip.length() || g_ha_token.length()) {
    page += "<br/><b>" + String(t_ha_ip) + ":</b> " +
            (g_ha_ip.length() ? sanitizeText(g_ha_ip) : String("-"));
    page += "<br/><b>" + String(t_ha_token) + ":</b> " +
            (g_ha_token.length() ? (it ? "impostato" : "set") : String(t_ha_missing));
  } else {
    page += String(t_ha_missing);
  }
  page += "</div>";

  page += "</p></div>";

  // ------------------------------------------------------------------
  // CARD 4 — COUNTDOWN
  // ------------------------------------------------------------------
  page += "<div class='card'><div class='label'>" + String(t_cd_card) +
          "</div><p class='value'>";

  if (cdCount == 0) {
    page += t_cd_none;
  } else {
    page += "<div class='kv'><b>" + String(t_cd_count) +
            ":</b> " + String(cdCount) + " / 8</div>";

    page += "<div class='kv'><b>" + String(t_cd_list) + ":</b> " +
            cdNames + "</div>";
  }

  page += "</p></div>";

  // link impostazioni
  page += "<p style='margin-top:16px'><a href='/settings'>" + String(t_settings) + "</a></p>";

  page += "</main></body></html>";

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
  web.on("/", HTTP_GET, handleRootAP);
  web.on("/save", HTTP_POST, handleSave);
  web.on("/reboot", HTTP_GET, handleReboot);
  web.onNotFound(handleRootAP);
  web.begin();
}

/* ---------------------------------------------------------------------------
   STA MODE — ROUTES
--------------------------------------------------------------------------- */
static void startSTAWeb() {
  web.on("/", HTTP_GET, handleRootSTA);
  web.on("/settings", HTTP_ANY, handleSettings);
  web.on("/force_qod", HTTP_POST, handleForceQOD);
  web.onNotFound(handleRootSTA);
  web.begin();
}
