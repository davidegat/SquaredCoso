# SquaredCoso 1.0.0 (Italiano / English)

<img width="480" height="480" alt="cosino" src="https://github.com/user-attachments/assets/382ad3ab-e1f3-4cf2-bc89-c8cf43fca65d" />

---

# 🇮🇹 Italiano

## Introduzione

**SquaredCoso** è un firmware per ESP32-S3 progettato per il pannello RGB 480×480 ST7701 (Panel-4848S040). Trasforma il display in una **bacheca informativa autonoma**, con pagine modulari che si alternano automaticamente e che sono completamente configurabili tramite interfaccia web.

Il dispositivo può mostrare:

* Meteo e previsioni
* Qualità dell’aria
* Orologio digitale
* Orologio binario
* Calendario ICS
* Grafico temperatura 24h
* Ore di luce, twilight, UV e fase lunare
* Bitcoin e variazione giornaliera
* Cambi valutari
* Countdown multipli
* News RSS
* Frase del giorno (OpenAI o ZenQuotes)
* Stato Home Assistant
* Vista sistema solare
* Informazioni di sistema

Ogni pagina è indipendente, ottimizzata per memoria e velocità, e può essere attivata o disattivata tramite WebUI.

---

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

---

## Funzionamento

Alla prima accensione, il dispositivo attiva una **rete Wi-Fi personale** (modalità AP). Chi si collega viene reindirizzato al **captive portal**, utile esclusivamente per configurare la rete Wi-Fi domestica.

Dopo la configurazione, il dispositivo passa in modalità **STA** e diventa raggiungibile tramite il proprio indirizzo IP. Da qui è disponibile la **WebUI completa**, che permette di impostare:

* Città e lingua
* Coordinate o geocoding automatico
* Intervallo di rotazione delle pagine
* Feed ICS
* Feed RSS
* Countdown
* Impostazioni FX e BTC
* Chiave OpenAI e argomento frase del giorno
* IP/token Home Assistant
* Pagine da visualizzare

Le impostazioni vengono salvate in NVS e persistono ai riavvii.

Il firmware gestisce in autonomia:

* sincronizzazione NTP
* recupero dati dalle API
* aggiornamento contenuti
* riconnessioni Wi-Fi
* transizioni grafiche e backlight PWM

---

## Descrizione delle pagine

### Meteo

Dati da openmeteo, descrizioni locali, previsioni e icone RLE con particelle animate.

### Qualità dell’aria

Valori PM2.5 / PM10 / O₃ / NO₂ con colore indicativo.

### Orologio

Ora in grande formato con data.

### Orologio binario

Visualizzazione binaria di ore, minuti e secondi.

### Calendario ICS

Mostra gli eventi della giornata dal feed ICS.

### Temperatura 24h

Ricostruzione oraria tramite dati giornalieri Open-Meteo.

### Ore di luce

Alba, tramonto, twilight, mezzogiorno solare, durata giorno, UV e fase lunare.

### Bitcoin

Prezzo attuale, variazione e valore del portafoglio.

### Cambi FX

Tabella valute rispetto alla valuta base.

### Countdown

Fino a otto eventi con nome e data.

### News

Prime notizie dal feed RSS.

### Frase del giorno

OpenAI (se configurato) o fallback ZenQuotes.

### Home Assistant

Mostra entità rilevanti (sensori, luci, prese, batterie, temperatura, movimento).

### Sistema solare

Orbita terrestre vista dall’alto con stagioni e inclinazione.

### Info di sistema

Uptime, rete, memoria, CPU, pagine attive.

---

## Struttura del codice

* `SquaredCoso.ino` — logica principale (display, Wi-Fi, NTP, rotazione, fetch)
* `SquaredWeb.ino` — captive portal e WebUI
* `handlers/` — moduli di supporto
* `pages/` — pagine del sistema
* `images/` — asset grafici RLE
* `tools/` — script di utilità

---

## Licenza

Creative Commons Attribution–NonCommercial 4.0 International.

Attribuzione richiesta: citare l’autore **Davide "gat" Nasato** e il repository originale: [https://github.com/davidegat/SquaredCoso](https://github.com/davidegat/SquaredCoso)

---

# 🇬🇧 English

## Introduction

**SquaredCoso** is an ESP32-S3 firmware designed for the 480×480 ST7701 RGB panel (Panel-4848S040). It turns the display into a **stand‑alone information dashboard**, with modular pages rotating automatically and fully configurable through the WebUI.

The device can display:

* Weather and forecasts
* Air quality
* Digital clock
* Binary clock
* ICS calendar
* 24h temperature graph
* Daylight, twilight, UV, moon phase
* Bitcoin and daily change
* FX rates
* Multiple countdowns
* RSS news
* Quote of the day (OpenAI or ZenQuotes)
* Home Assistant entity status
* Solar system view
* System info

Each page is independent, optimized for memory and speed, and can be enabled or disabled from the WebUI.

---

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

---

## Operation

On first boot, the device creates its own Wi‑Fi network in **AP mode**, exposing a **captive portal** used only to configure the home Wi‑Fi.

After configuration, it switches to **STA mode**, becomes reachable through its IP address and exposes the full **WebUI**, where you can configure:

* City and language
* Coordinates or automatic geocoding
* Page rotation interval
* ICS feed
* RSS feed
* Countdown timers
* FX/BTC settings
* OpenAI key and topic
* Home Assistant IP/token
* Enabled pages

Settings are saved to NVS and persist across reboots.

The firmware autonomously manages:

* NTP synchronization
* Data fetching
* Page rotation
* Wi‑Fi reconnection
* Graphic transitions and PWM backlight

---

## Page overview

### Weather

Live conditions, forecasts, localized descriptions and RLE icons, with animated particles.

### Air quality

PM2.5 / PM10 / O₃ / NO₂ with visual severity indication.

### Clock

Large digital time and date.

### Binary clock

Binary view of hours, minutes and seconds.

### ICS calendar

Shows today's events from the configured ICS feed.

### Temperature 24h

Reconstructed hourly graph using Open‑Meteo daily data.

### Daylight

Sunrise, sunset, twilight, solar noon, day length, UV, moon phase.

### Bitcoin

Current price, delta and portfolio value.

### FX rates

Currency table relative to base currency.

### Countdowns

Up to 8 programmable events.

### News

First headlines from RSS feed.

### Quote of the day

OpenAI‑generated or ZenQuotes fallback.

### Home Assistant

Displays selected entities (sensors, lights, plugs, battery, temperature, motion).

### Solar system

Top‑down view of Earth orbit with seasonal position and axial tilt.

### System info

Uptime, network details, memory, CPU, active pages.

---

## Code layout

Same structure as the Italian section.

---

## License

Creative Commons Attribution–NonCommercial 4.0 International.

Attribution required: cite **Davide "gat" Nasato** and the original repository: [https://github.com/davidegat/SquaredCoso](https://github.com/davidegat/SquaredCoso)
