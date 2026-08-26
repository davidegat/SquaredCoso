# SquaredCoso b1.3.0 (Italiano / English)

## Indice / Table of contents

- [🇮🇹 Italiano](#-italiano)
  - [Introduzione](#introduzione)
  - [Hardware supportato](#hardware-supportato)
  - [Funzionamento](#funzionamento)
  - [Touch Menu](#touch-menu)
  - [Descrizione delle pagine](#descrizione-delle-pagine)
  - [Struttura del codice](#struttura-del-codice)
  - [Troubleshooting](#troubleshooting)
  - [Licenza](#licenza)
- [🇬🇧 English](#-english)
  - [Introduction](#introduction)
  - [Supported hardware](#supported-hardware)
  - [Operation](#operation)
  - [Touch Menu](#touch-menu-1)
  - [Page overview](#page-overview)
  - [Code layout](#code-layout)
  - [Troubleshooting](#troubleshooting-1)
  - [License](#license)

<img width="480" height="480" alt="cosino" src="https://github.com/user-attachments/assets/382ad3ab-e1f3-4cf2-bc89-c8cf43fca65d" />

***

# 🇮🇹 Italiano

## Introduzione

**SquaredCoso** è un firmware per ESP32-S3 progettato per il pannello RGB 480×480 ST7701 (Panel-4848S040). Trasforma il display in una **bacheca informativa autonoma**, con pagine modulari che si alternano automaticamente e che sono completamente configurabili tramite interfaccia web.

### Novità principali di b1.3.0

* **Rotazione completa del display** a 0° / 90° / 180° / 270°, con touch sempre coerente con l'orientamento visuale.
* **Cambio pagina manuale dal display**: tap sul bordo sinistro per tornare indietro, tap sul bordo destro per avanzare, tap centrale per aprire il menu.
* **WebUI rinnovata**: dashboard più chiara, impostazioni riorganizzate e test rapidi delle integrazioni principali.

Il dispositivo può mostrare:

* Meteo e previsioni
* Qualità dell'aria
* Orologio digitale
* Orologio binario
* Visualizzazioni temporali avanzate (Chronos)
* Calendario ICS
* Grafico temperatura 7 giorni
* Ore di luce, twilight, UV e fase lunare
* Bitcoin e variazione giornaliera
* Cambi valutari
* Countdown multipli
* Note veloci in stile post-it
* News RSS
* Frase del giorno (OpenAI o ZenQuotes)
* Stato Home Assistant
* Vista sistema solare
* Informazioni di sistema

Ogni pagina è indipendente, ottimizzata per memoria e velocità, e può essere attivata o disattivata tramite WebUI o direttamente dal touch screen.

***

## Hardware supportato

Il firmware è progettato per il pannello **ESP32-S3 Panel-4848S040**:

* Display IPS 480×480
* Controller ST7701 (RGB parallelo)
* Touch GT911
* microSD via bus FSPI

Repository hardware collegato:
[https://github.com/davidegat/ESP32-4848S040-Fun](https://github.com/davidegat/ESP32-4848S040-Fun)

### Pinout

| Funzione            | Pin                                   |
| ------------------- | ------------------------------------- |
| Touch I²C           | SDA 19 — SCL 45                       |
| Backlight PWM       | 38                                    |
| ST7701 (SWSPI init) | CS 39 — SCK 48 — MOSI 47              |
| RGB Timing          | DE 18 — VSYNC 17 — HSYNC 16 — PCLK 21 |
| R                   | 11, 12, 13, 14, 0                     |
| G                   | 8, 20, 3, 46, 9, 10                   |
| B                   | 4, 5, 6, 7, 15                        |
| SD FSPI             | CS 42 — MOSI 47 — MISO 41 — SCK 48    |

### Configurazione Arduino IDE

* ESP32 package 2.0.16 / 2.0.17
* PSRAM abilitata (OPI)
* Partition scheme: Huge APP (3MB No OTA)
* Librerie richieste: Arduino_GFX_Library ≥1.6.0, TAMC_GT911, WiFi, WebServer, DNSServer, HTTPClient, Preferences

***

## Funzionamento

Alla prima accensione, il dispositivo attiva una **rete Wi-Fi personale** (modalità AP). Chi si collega viene reindirizzato al **captive portal**, utile esclusivamente per configurare la rete Wi-Fi domestica.

Dopo la configurazione, il dispositivo passa in modalità **STA** e diventa raggiungibile tramite il proprio indirizzo IP. Da qui è disponibile la **WebUI completa**, che permette di impostare:

* Città e lingua
* Coordinate o geocoding automatico
* Intervallo di rotazione delle pagine
* Orientamento display 0° / 90° / 180° / 270° con touch coerente
* Regola fuso orario POSIX (default Europa centrale con ora legale)
* Feed ICS
* Feed RSS
* Countdown
* Impostazioni FX e BTC
* Chiave OpenAI e argomento frase del giorno
* Testo e colore per i post-it
* IP/token Home Assistant e lista opzionale di entity_id da monitorare
* Pagine da visualizzare e parametri grafici

Le impostazioni vengono salvate in NVS e persistono ai riavvii.

Il firmware gestisce in autonomia:

* sincronizzazione NTP
* recupero dati dalle API con sanitizzazione testuale e helper JSON condivisi
* aggiornamento contenuti con controllo di stato e splash di versione all'avvio
* riconnessioni Wi-Fi
* transizioni grafiche e backlight PWM

***

## Touch Menu

Il dispositivo integra un **menu touch** che permette di gestire le pagine attive direttamente dallo schermo, senza dover accedere alla WebUI.

### Interazione durante la visualizzazione

* **Tap sul margine destro**: passa alla pagina attiva successiva, saltando quelle disabilitate.
* **Tap sul margine sinistro**: torna alla pagina attiva precedente, saltando quelle disabilitate.
* **Tap nella zona centrale**: apre il menu touch delle pagine.

La navigazione laterale è circolare e segue l'orientamento corrente del display (0° / 90° / 180° / 270°).

### Struttura del menu

* **Header**: mostra il titolo "Gestione Pagine", il contatore delle pagine attive (es. "12/17 attive") e l'indicatore di pagina corrente del menu (es. "1/3").
* **Griglia pulsanti**: ogni pagina del firmware è rappresentata da un pulsante. Un indicatore circolare mostra lo stato: pieno e colorato se la pagina è attiva, vuoto se disattivata.
* **Paginazione**: se le pagine sono più di 8, il menu si divide in più schermate. Le frecce laterali in basso permettono di navigare tra le pagine del menu.
* **Pulsanti azione**: in basso al centro ci sono due pulsanti:
  * **ANNULLA** — chiude il menu senza salvare
  * **OK** — salva le modifiche in NVS e riavvia il dispositivo

### Funzionamento

Tocca un pulsante per attivare o disattivare la relativa pagina. Il contatore nell'header si aggiorna in tempo reale. Le etichette dei pulsanti cambiano automaticamente in base alla lingua configurata (italiano o inglese).

### Note tecniche

Il touch screen utilizza il controller GT911 su bus I²C (SDA 19, SCL 45). Il debounce è impostato a 200 ms per evitare tocchi multipli accidentali. Le coordinate vengono rimappate internamente per compensare l'orientamento del pannello.

***

## Descrizione delle pagine

### Meteo

Dati da openmeteo, descrizioni locali, previsioni e icone RLE con particelle animate.

### Qualità dell'aria

Valori PM2.5 / PM10 / O₃ / NO₂ con colore indicativo.

### Orologio

Ora in grande formato con data.

### Orologio binario

Visualizzazione binaria di ore, minuti e secondi.

### Chronos

Conteggio del tempo rimanente di giorno, mese, anno e altre scadenze temporali.

### Calendario ICS

Mostra gli eventi della giornata dal feed ICS.

### Temperatura 

Ricostruzione settimanale tramite dati giornalieri Open-Meteo.

### Ore di luce

Alba, tramonto, twilight, mezzogiorno solare, durata giorno, UV e fase lunare.

### Bitcoin

Prezzo attuale, variazione e valore del portafoglio.

### Cambi FX

Tabella valute rispetto alla valuta base.

### Countdown

Fino a otto eventi con nome e data.

### Post-it

Nota singola con titolo, testo e scelta del colore per annotazioni veloci gestite dalla WebUI.

### News

Prime notizie dal feed RSS.

### Frase del giorno

OpenAI (se configurato) o fallback ZenQuotes.

### Home Assistant

Mostra entità rilevanti (sensori, luci, prese, batterie, temperatura, movimento).

### Sistema solare

Orbita terrestre vista dall'alto con stagioni e inclinazione.

### Info di sistema

Versione firmware e indirizzo IP del dispositivo.

***

## Struttura del codice

* `SquaredCoso.ino` — logica principale (display, Wi-Fi, NTP, rotazione, fetch)
* `SquaredWeb.ino` — captive portal e WebUI
* `handlers/` — moduli di supporto
  * `touch_menu.h` — gestione touch GT911 e menu pagine
* `pages/` — pagine del sistema
* `images/` — asset grafici RLE
* `tools/` — script di utilità

***

# Troubleshooting

## Non compila

* Controlla di usare **ESP32 core 2.0.16 o 2.0.17**.
* Assicurati che la **GFX Library for Arduino sia versione 1.6.0**.
* Verifica che tutte le altre librerie richieste siano installate e senza duplicati.
* Se ottieni errori insoliti, cancella la cartella delle build temporanee di Arduino IDE e ricompila.

## Non fa l'upload

* Verifica di aver selezionato la scheda **ESP32S3 Dev Module**.
* Controlla di aver selezionato la **porta USB corretta**.
* Se non compare alcuna porta, entra manualmente in bootloader premendo **BOOT mentre colleghi il cavo** (solo una volta).
* Se l'upload fallisce ancora, riduci la velocità da **921600** a **460800**.

## Compila e fa l'upload, ma il display rimane nero

* Controlla che lo schema partizioni sia **Huge APP (3MB No OTA)**.
* Assicurati che la **PSRAM** sia su **Enabled** oppure **OPI PSRAM**.
* Conferma di usare core ESP32 **2.0.16/2.0.17**.
* Verifica che la **GFX Library** sia esattamente la **1.6.0**.

## Perdo le impostazioni Wi-Fi o non vengono salvate

* Verifica che **Erase All Before Sketch Upload** sia **disattivato**.
* Assicurati che il tuo Arduino IDE non stia forzando una cancellazione completa della flash.

## Upload impossibile nonostante tutto

* Prova un **cavo USB migliore**: molti cavi economici trasmettono solo alimentazione.
* Cambia porta USB del PC.
* Riduci ulteriormente l'upload speed.
* Assicurati di **premere BOOT prima di fornire alimentazione** alla scheda (solo una volta).

## Il dispositivo **non si accende**

Non vedi retroilluminazione, il PC non rileva nessuna nuova porta seriale.
* Se usi un alimentatore USB, assicurati che sia almeno **5V / 1A** (meglio **5V / 2A**).
* Evita di usare hub USB non alimentati.
* Prova su un altro PC o sistema operativo per escludere driver/USB buggati.

Vedi la retroilluminazione o il logo per un momento, poi tutto si spegne e riparte. Il monitor seriale mostra riavvii frequenti o errori casuali.
* Cambia alimentazione con un alimentatore **5V / 2A** affidabile (tipo alimentatore da smartphone decente, non vecchi caricabatterie da 500 mA).
* Evita le porte USB di monitor/TV, evita di usare hub USB non alimentati.: spesso non reggono il picco di corrente del WiFi + backlight.
* Se il cavo USBè lungo o di qualità scarsa, la caduta di tensione può causare reset. Prova un cavo corto e robusto.
* Controlla che non ci siano contatti in caso tu abbia fatto modifiche sulla board (es. Aggiunta porta USB-C, Mod per l'audio, aggiunta di uno speaker...)

L'ESP32-S3 appare come porta seriale, l'upload funziona, ma il pannello resta sempre completamente buio. Il dispositivo si riavvia quando il Wi-Fi si collega o durante il cambio pagina.
* Anche se l'ESP parte, un alimentatore debole può impedire al backlight di accendersi. Prova con un **5V / 1–2A** dedicato.
* Alcuni computer limitano la corrente su certe porte. Prova un'altra porta, una porta alimentata o un alimentatore USB esterno.
* Evita di usare hub USB non alimentati.

## Il touch non risponde

* Verifica che la libreria **TAMC_GT911** sia installata correttamente.
* Controlla i pin I²C (SDA 19, SCL 45) per eventuali conflitti o saldature fredde.
* Se il menu si apre ma non registra i tocchi, potrebbe essere un problema di calibrazione o di orientamento: i valori vengono rimappati internamente.

## La frase generata dall'AI non appare

* Inserisci una **chiave API OpenAI valida**.
* Come ottenere una chiave:

  1. Accedi al tuo account OpenAI
  2. Vai in **API Keys**
  3. Crea una nuova chiave
* Assicurati di aver inserito anche un **topic** per la frase del giorno.

## Home Assistant non viene rilevato

* Inserisci l'**IP corretto** della tua istanza HA.
* Genera un token:
  **Profilo → Token a lungo termine → Crea token**.
* Assicurati che la rete locale permetta la comunicazione tra dispositivi.
* Controlla VLAN, firewall o router che potrebbero isolare i device.

## Non ricevo notizie RSS

* Verifica che l'URL del feed RSS sia **valido e raggiungibile**.
* Evita feed che richiedono login o HTTPS particolari.

## Non ricevo eventi del calendario ICS

* Controlla che l'URL ICS sia corretto e pubblico.
* Il parser gestisce gli ICS più comuni, inclusi eventi UTC, all-day e righe folded; calendari con timezone non standard possono richiedere adattamenti.

## Altri controlli utili

* Se il Wi-Fi non si connette, ricontrolla SSID/password o riapri l'AP interno.
* Se il pannello è lento, riavvia e verifica che la PSRAM funzioni.
* Se la WebUI non si apre, verifica che l'IP mostrato nella pagina INFO sia raggiungibile dalla tua rete.

***

## Licenza

Creative Commons Attribution–NonCommercial 4.0 International.

Attribuzione richiesta: citare l'autore **Davide "gat" Nasato** e il repository originale: [https://github.com/davidegat/SquaredCoso](https://github.com/davidegat/SquaredCoso)

***

# 🇬🇧 English

## Introduction

**SquaredCoso** is an ESP32-S3 firmware designed for the 480×480 ST7701 RGB panel (Panel-4848S040). It turns the display into a **stand‑alone information dashboard**, with modular pages rotating automatically and fully configurable through the WebUI.

### Main improvements in b1.3.0

* **Full display rotation** at 0° / 90° / 180° / 270°, with touch coordinates always matching the visible orientation.
* **Manual page navigation on the display**: tap the left edge for the previous page, the right edge for the next page, and the center to open the menu.
* **Refreshed WebUI** with a clearer dashboard, reorganized settings and quick tests for the main integrations.

The device can display:

* Weather and forecasts
* Air quality
* Digital clock
* Binary clock
* Chronos advanced time views
* ICS calendar
* 7 days temperature graph
* Daylight, twilight, UV, moon phase
* Bitcoin and daily change
* FX rates
* Multiple countdowns
* Sticky notes
* RSS news
* Quote of the day (OpenAI or ZenQuotes)
* Home Assistant entity status
* Solar system view
* System info

Each page is independent, optimized for memory and speed, and can be enabled or disabled from the WebUI or directly from the touch screen.

***

## Supported hardware

Designed for the **ESP32-S3 Panel-4848S040**, featuring:

* 480×480 IPS RGB display
* ST7701 controller (parallel RGB)
* GT911 touch
* microSD (FSPI)

Hardware reference:
[https://github.com/davidegat/ESP32-4848S040-Fun](https://github.com/davidegat/ESP32-4848S040-Fun)

### Pinout

(Identical to Italian section.)

### Arduino IDE setup

* ESP32 package 2.0.16 / 2.0.17
* PSRAM enabled (OPI)
* Partition scheme: Huge APP (3MB No OTA)
* Required libraries: Arduino_GFX_Library ≥1.6.0, TAMC_GT911, WiFi, WebServer, DNSServer, HTTPClient, Preferences

***

## Operation

On first boot, the device creates its own Wi‑Fi network in **AP mode**, exposing a **captive portal** used only to configure the home Wi‑Fi.

After configuration, it switches to **STA mode**, becomes reachable through its IP address and exposes the full **WebUI**, where you can configure:

* City and language
* Coordinates or automatic geocoding
* Page rotation interval
* Display orientation 0° / 90° / 180° / 270° with matching touch coordinates
* POSIX timezone rule (Central European time with DST by default)
* ICS feed
* RSS feed
* Countdown timers
* FX/BTC settings
* OpenAI key and topic
* Post-it content and color
* Home Assistant IP/token and optional entity_id list
* Enabled pages and visual parameters

Settings are saved to NVS and persist across reboots.

The firmware autonomously manages:

* NTP synchronization
* Data fetching with shared JSON helpers and text sanitization
* Page rotation with status checks and version splash at boot
* Wi‑Fi reconnection
* Graphic transitions and PWM backlight

***

## Touch Menu

The device includes a **touch menu** that allows you to manage active pages directly from the screen, without accessing the WebUI.

### Interaction during normal display

* **Tap the right edge**: moves to the next enabled page, skipping disabled pages.
* **Tap the left edge**: moves to the previous enabled page, skipping disabled pages.
* **Tap the center area**: opens the touch page menu.

Edge-tap navigation wraps around and follows the current display orientation (0° / 90° / 180° / 270°).

### Menu structure

* **Header**: shows the title "Gestione Pagine" (Page Manager), the active page counter (e.g. "12/17 attive") and the current menu page indicator (e.g. "1/3").
* **Button grid**: each firmware page is represented by a button. A circular indicator shows the status: filled and colored if the page is active, empty if disabled.
* **Pagination**: if there are more than 8 pages, the menu splits across multiple screens. The arrow buttons at the bottom allow navigation between menu pages.
* **Action buttons**: at the bottom center there are two buttons:
  * **ANNULLA** (Cancel) — closes the menu without saving
  * **OK** — saves changes to NVS and reboots the device

### Usage

Tap a button to enable or disable the corresponding page. The counter in the header updates in real time. Button labels change automatically based on the configured language (Italian or English).

### Technical notes

The touch screen uses the GT911 controller on I²C bus (SDA 19, SCL 45). Debounce is set to 200 ms to prevent accidental multiple taps. Coordinates are internally remapped to compensate for panel orientation.

***

## Page overview

### Weather

Live conditions, forecasts, localized descriptions and RLE icons, with animated particles.

### Air quality

PM2.5 / PM10 / O₃ / NO₂ with visual severity indication.

### Clock

Large digital time and date.

### Binary clock

Binary view of hours, minutes and seconds.

### Chronos

Counters for the remaining time in the day, month, year and other deadlines.

### ICS calendar

Shows today's events from the configured ICS feed.

### Temperature 

Reconstructed weekly graph using Open‑Meteo daily data.

### Daylight

Sunrise, sunset, twilight, solar noon, day length, UV, moon phase.

### Bitcoin

Current price, delta and portfolio value.

### FX rates

Currency table relative to base currency.

### Countdowns

Up to 8 programmable events.

### Sticky notes

Single note with title, body and color controls for quick reminders managed through the WebUI.

### News

First headlines from RSS feed.

### Quote of the day

OpenAI‑generated or ZenQuotes fallback.

### Home Assistant

Displays selected entities (sensors, lights, plugs, battery, temperature, motion).

### Solar system

Top‑down view of Earth orbit with seasonal position and axial tilt.

### System info

Firmware version and device IP address.

***

## Code layout

* `SquaredCoso.ino` — main logic (display, Wi-Fi, NTP, rotazione, fetch)
* `SquaredWeb.ino` — captive portal and WebUI
* `handlers/` — support modules
  * `touch_menu.h` — GT911 touch handler and page menu
* `pages/` — SquaredCoso pages files
* `images/` — RLE assets
* `tools/` — utilities

***

# Troubleshooting

## Does not compile

* Make sure you are using **ESP32 core 2.0.16 or 2.0.17**.
* Ensure that the **GFX Library for Arduino is version 1.6.0**.
* Check that all the other required libraries are installed and that there are no duplicates.
* If you get unusual errors, delete Arduino IDE's temporary build folder and recompile.

## Upload fails

* Verify that the selected board is **ESP32S3 Dev Module**.
* Check that you have selected the **correct USB port**.
* If no port appears, manually enter bootloader mode by pressing **BOOT while you plug in the cable** (only once).
* If the upload still fails, lower the speed from **921600** to **460800**.

## Compiles and uploads, but the display stays black

* Check that the partition scheme is **Huge APP (3MB No OTA)**.
* Make sure **PSRAM** is set to **Enabled** or **OPI PSRAM**.
* Confirm you are using ESP32 core **2.0.16/2.0.17**.
* Verify that the **GFX Library** is exactly **1.6.0**.

## Wi‑Fi settings are lost or not saved

* Make sure **Erase All Before Sketch Upload** is **disabled**.
* Ensure your Arduino IDE is not forcing a full flash erase.

## Upload impossible despite everything

* Try a **better USB cable**; many cheap cables only provide power.
* Change the USB port on your PC.
* Lower the upload speed further.
* Make sure you **press BOOT before powering the board** (only once).

## The device does not power on

You do not see any backlight, and the PC does not detect a new serial port.

* If you use a USB power adapter, make sure it is at least **5V / 1A** (preferably **5V / 2A**).
* Avoid using unpowered USB hubs.
* Try on another PC or operating system to rule out buggy USB/drivers.

You see the backlight or logo for a moment, then everything turns off and restarts. The serial monitor shows frequent reboots or random errors.

* Change the power source to a reliable **5V / 2A** adapter (for example a decent smartphone charger, not an old 500 mA one).
* Avoid monitor/TV USB ports and unpowered USB hubs; they often cannot handle the current spike from Wi‑Fi + backlight.
* If the USB cable is long or poor quality, voltage drop may cause resets. Try a short, good-quality cable.
* If you modified the board (e.g. added a USB‑C port, audio mod, speaker), check there are no shorts or bad contacts.

The ESP32‑S3 appears as a serial port, upload works, but the panel stays completely dark. The device reboots when Wi‑Fi connects or when pages change.

* Even if the ESP starts, a weak power supply can prevent the backlight from turning on. Try a dedicated **5V / 1–2A** adapter.
* Some computers limit current on specific ports. Try another port, a powered USB port, or an external USB power adapter.
* Avoid using unpowered USB hubs.

## Touch does not respond

* Verify that the **TAMC_GT911** library is installed correctly.
* Check the I²C pins (SDA 19, SCL 45) for conflicts or cold solder joints.
* If the menu opens but does not register taps, it might be a calibration or orientation issue: values are internally remapped.

## AI quotes do not appear

* You must enter a valid **OpenAI API key**.
* How to get one:

  1. Log into your OpenAI account
  2. Go to **API Keys**
  3. Generate a new key
* Do not forget to enter a **topic** for the quote of the day.

## Home Assistant not detected

* Enter the correct **IP address** of your HA instance.
* Generate a token:
  **Profile → Long-lived access tokens → Create token**.
* Make sure your network allows communication between devices.
* Check VLANs, firewalls, or routers that may isolate devices.

## RSS feed not loading

* Make sure the RSS feed URL is **valid and reachable**.
* Avoid feeds requiring authentication or unusual HTTPS setups.

## ICS calendar events missing

* Check that the ICS URL is correct and publicly accessible.
* The parser handles common ICS feeds, including UTC, all-day events and folded lines; unusual timezone definitions may still require adjustments.

## Additional useful checks

* If Wi-Fi won't connect, recheck SSID/password or reopen the access point.
* If the display seems slow, reboot and check PSRAM health.
* If the WebUI does not open, verify the INFO page IP is reachable on your network.

***

## License

Creative Commons Attribution–NonCommercial 4.0 International.

Attribution required: cite **Davide "gat" Nasato** and the original repository: [https://github.com/davidegat/SquaredCoso](https://github.com/davidegat/SquaredCoso)
