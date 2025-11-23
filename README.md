# README (Italiano / English)

## Italiano

SquaredCoso è un firmware per ESP32-S3 con pannello RGB ST7701 480x480 (Panel-4848S040) pensato come bacheca informativa modulare. Ruota automaticamente pagine meteo, qualità dell'aria, orologio, calendario ICS, Bitcoin, cambi valutari, countdown multipli, ore di luce, feed RSS e Quote of the Day, includendo anche una pagina di info di sistema.

### Descrizione
Firmware pensato per trasformare un pannello RGB 480x480 in un cruscotto sempre connesso: offre una Web UI captive per la prima configurazione, memorizza le preferenze in NVS, integra fonti dati pubbliche e API opzionali, e gestisce in autonomia il ciclo di vita del display (accensione PWM, transizioni, recupero dati e riconnessioni Wi-Fi).

### Caratteristiche principali
- Inizializzazione completa del display RGB tramite `Arduino_GFX_Library` e gestione del retroilluminamento PWM.
- Gestione Wi-Fi in modalità AP/STA con captive portal e Web UI integrata per configurazione rapida.
- Sincronizzazione NTP e cronologia ciclica delle pagine con transizioni fade e splash screen.
- Salvataggio delle preferenze in NVS (città, lingua, intervallo cambio pagina, countdown, feed RSS, chiavi API, pagine da mostrare, ecc.).
- Geocoding automatico via Open-Meteo quando latitudine/longitudine non sono impostate, con memorizzazione su NVS.
- Supporto per più sorgenti dati: meteo e aria, tassi FX, Bitcoin (con calcolo portafoglio), feed RSS personalizzabile, ICS per eventi, contenuti generati da OpenAI (chiave/argomento configurabili).

### Requisiti hardware
- ESP32-S3 con sufficiente PSRAM.
- Pannello RGB ST7701 480x480 (Panel-4848S040) collegato secondo la piedinatura definita in `SquaredCoso.ino`.
- Sensore di alimentazione/retroilluminazione collegato al pin `GFX_BL` (38) per PWM.

### Dipendenze software
- Arduino IDE o ambiente equivalente compatibile con ESP32-S3.
- Librerie Arduino: `Arduino_GFX_Library`, `WiFi`, `WiFiClientSecure`, `WebServer`, `DNSServer`, `HTTPClient`, `Preferences`, più le librerie standard incluse nell'SDK ESP32.

### Configurazione e uso
1. Clona il repository e apri `SquaredCoso.ino` nell'IDE Arduino configurato per ESP32-S3 con PSRAM attiva.
2. Installa le librerie richieste tramite Library Manager o includile manualmente.
3. Compila e carica lo sketch sull'ESP32-S3.
4. Al primo avvio il dispositivo espone una Wi-Fi AP con captive portal: collega un dispositivo e apri la Web UI per impostare rete, città, lingua (it/en), intervallo di rotazione, feed RSS, countdown, chiavi API (OpenAI, ecc.) e le pagine da mostrare.
5. Salva le impostazioni: vengono scritte su NVS e riutilizzate ai riavvii. Se lat/lon sono vuoti, il firmware effettua il geocoding automatico.

### Struttura del codice
- `SquaredCoso.ino`: core del firmware (inizializzazioni, ciclo principale, Wi-Fi, NTP, rotazione pagine, fetch dati).
- `handlers/`: moduli di supporto per impostazioni, helper di display e variabili globali.
- `pages/`: implementazioni delle singole pagine (meteo, clock, news, aria, crypto, ecc.).
- `images/`: asset grafici compressi in RLE.
- `tools/`: script di utilità, es. `compress_h_rle.py` per generare asset.

### Licenza
Questo progetto è rilasciato sotto licenza Creative Commons Attribution-NonCommercial 4.0 International. Vedi `LICENSE.md` per i dettagli.

---

## English

SquaredCoso is an ESP32-S3 firmware for a 480x480 ST7701 RGB panel (Panel-4848S040) designed as a modular information board. It automatically rotates pages for weather, air quality, clock, ICS calendar, Bitcoin, foreign exchange rates, multiple countdowns, daylight hours, RSS news, and Quote of the Day, plus a system info page.

### Description
The firmware turns a 480x480 RGB panel into an always-connected dashboard: it provides a captive Web UI for first-time setup, stores preferences in NVS, pulls from public data sources and optional APIs, and autonomously manages the display lifecycle (PWM backlight, transitions, data fetching, and Wi-Fi reconnections).

### Key features
- Full RGB display initialization through `Arduino_GFX_Library` with PWM backlight control.
- Wi-Fi management in AP/STA modes with captive portal and built-in Web UI for quick setup.
- NTP sync and cyclic page rotation with fade transitions and splash screen.
- Preference storage in NVS (city, language, page interval, countdowns, RSS feed, API keys, enabled pages, and more).
- Automatic geocoding via Open-Meteo when latitude/longitude are missing, with cached values in NVS.
- Multiple data sources: weather and air metrics, FX rates, Bitcoin (with portfolio calculation), customizable RSS feed, ICS events, and OpenAI-generated content (configurable key/topic).

### Hardware requirements
- ESP32-S3 with sufficient PSRAM.
- ST7701 480x480 RGB panel (Panel-4848S040) wired according to the pinout defined in `SquaredCoso.ino`.
- Backlight control on pin `GFX_BL` (38) for PWM.

### Software dependencies
- Arduino IDE or a compatible environment targeting ESP32-S3.
- Arduino libraries: `Arduino_GFX_Library`, `WiFi`, `WiFiClientSecure`, `WebServer`, `DNSServer`, `HTTPClient`, `Preferences`, plus standard ESP32 SDK libraries.

### Setup and usage
1. Clone the repository and open `SquaredCoso.ino` in the Arduino IDE configured for ESP32-S3 with PSRAM enabled.
2. Install the required libraries via Library Manager or manually include them.
3. Compile and upload the sketch to the ESP32-S3.
4. On first boot the device exposes a Wi-Fi AP with captive portal: connect and open the Web UI to set network credentials, city, language (it/en), rotation interval, RSS feed, countdowns, API keys (OpenAI, etc.), and which pages to show.
5. Save settings: they are stored in NVS and reused on reboot. If lat/lon are empty, the firmware performs automatic geocoding.

### Code layout
- `SquaredCoso.ino`: firmware core (initialization, main loop, Wi-Fi, NTP, page rotation, data fetching).
- `handlers/`: support modules for settings, display helpers, and global variables.
- `pages/`: implementations for individual pages (weather, clock, news, air, crypto, etc.).
- `images/`: compressed RLE graphic assets.
- `tools/`: utility scripts, e.g., `compress_h_rle.py` for asset generation.

### License
This project is released under the Creative Commons Attribution-NonCommercial 4.0 International license. See `LICENSE.md` for details.
