# Changelog – SquaredCoso

## [1.3.0] – 2026-08-26

Release consolidata rispetto a b1.2.1.

### Added
* Display rotation at 0° / 90° / 180° / 270°, persisted in NVS and applied before the boot splash.
* Rotation-aware GT911 touch mapping for every supported orientation.
* Manual on-device navigation: left-edge tap for the previous enabled page, center tap for the page menu, right-edge tap for the next enabled page.
* POSIX timezone configuration with automatic DST handling.
* Optional Home Assistant `entity_id` list for targeted polling.
* TLS trust anchors for the OpenAI API.
* Stellar countdown showing `-N` days to the next equinox or solstice.
* WebUI integration tests for ICS, RSS, OpenAI and Home Assistant.

### Changed
* Refreshed STA WebUI dashboard with device/network summary, compact page chips and clearer integration status.
* Reorganized settings into General, Display, Integrations, Countdowns and Post-it sections with quick navigation.
* Added direct rotation buttons, simplified timezone presets, dynamic countdown rows and unsaved-change feedback.
* QOD regeneration now runs inline from the WebUI instead of opening a raw response page.
* Startup and periodic remote-data refreshes now run through a distributed step scheduler instead of one long blocking chain.
* Clock and Binary Clock refresh independently from the automatic page-rotation interval.
* Digital Clock updates only changed fields, avoiding full-screen redraws every second.
* Open-Meteo geocoding is shared by weather, air-quality and temperature pages.
* Home Assistant polling is slower and targeted when entity IDs are configured; automatic discovery remains as fallback.
* Home Assistant numeric states preserve `unit_of_measurement`.
* Web settings no longer echo stored API keys or tokens into generated HTML.
* Calendar parsing unfolds RFC5545 lines, handles UTC and all-day timestamps, scans the full feed and keeps the three most relevant events for today.
* Device Info was simplified for stability and now shows firmware version and device IP address.
* Countdown animation uses a circular trail buffer instead of shifting the full trail every frame.
* Forex migrated to the current Frankfurter v1 endpoint and excludes the configured base currency from requested symbols.
* Stellar computes current-year equinox/solstice anchors and uses fractional time-of-day for Earth position.
* Chronos cosmic bars now explicitly represent Earth age / Universe age and Sun age / approximate main-sequence lifetime.
* Air Quality keeps its leaf animation while using a more restrained severity background.

### Fixed
* Touch-menu Cancel now truly discards pending page changes.
* The final enabled page can no longer be disabled from the touch menu.
* Touch coordinates are clamped to 0..479 and remain correct under every display rotation.
* Changing city invalidates cached coordinates, preventing weather/air/temperature data from using the previous location.
* Fixed Central European daylight-saving handling by replacing fixed offsets with timezone rules.
* Air Quality now reads current pollutant values instead of the first hourly sample and uses the six current EEA AQ categories.
* Home Assistant parsing no longer uses 16-bit offsets for large `/api/states` responses.
* Home Assistant safely handles empty states and stops targeted polling after repeated failures.
* OpenAI requests no longer use `setInsecure()` and response parsing handles escaped JSON strings.
* Paragraph rendering no longer overrides the caller's intended text color.
* RSS parsing offsets no longer wrap at 64 KiB.
* Stellar season markers, labels and interpolation are aligned to the correct equinoxes and solstices.
* Chronos century progress now respects the historical 01..00 century convention.
* Chronos universe timeline correctly marks Solar System formation instead of labelling that position as the Milky Way.
* Forex now rejects responses where no target rate was parsed.
* HTML values are escaped correctly in the WebUI, including countdown names.
* Fixed the missing countdown-row WebUI layout.
* Removed stale scheduler symbols and the obsolete `temp24_progress` reference.
* Eliminated the Clock flicker caused by unnecessary full-screen redraws.
* Fixed a crash triggered when entering the Info page.

---

## [1.2.0] – 2025-12-01

### Added
* Tons of info (separate file) on configuration and troubleshooting.
* Multipage touch menu for configuration.

### Changed
* Improved resourse usage for most pages.
* Earth drawing.
* Chronos, Clock and Bitcoin layout.
* Lighter RLE images.

### Fixed
* Critical error on Calendar page.
* Moon Phase behaviour.
* Stellar page logic.
---

## [1.1.0] – 2025-11-27

### Added

* New **Post-it Page** for quick notes with WebUI integration.
* New **Chronos Page** with enhanced time-based visualizations.
* Added improved **AI Quote-of-the-Day** generator with refined prompt logic.
* Added version info splash screen at boot.
* Added centralized **JSON Helpers module** for efficient parsing across all pages.

### Changed

* Major internal refactoring: unified JSON parsing routines into shared helper header.
* Optimized CPU, RAM and Flash usage through function unification and removal of duplicated logic.
* Improved and modernized layouts for multiple pages:

  * Digital Clock
  * Weather
  * Countdown
  * Air Quality
  * Binary Clock
  * QOD
  * Calendar
  * Solar System
* Updated and refined Web Interface for easier configuration and better readability.

### Fixed

* Corrected saving of owned Bitcoin value in settings.
* Fixed visual startup glitch on first boot.
* Fixed a critical bug in the Solar System page logic.
* Fixed a critical bug in Moon Phase calculation.
* Improved text sanitization edge cases for API-based pages.

---

## [1.0.1] – 2025-11-26

### Added

* Added version info display at boot.
* Added Post-it page.
* Added Chronos Page.
* Improved AI Quote-of-the-Day prompt.
* Updated and refined Web Interface.

### Changed

* Updated layouts for: Clock, Weather, Countdown, Air Quality, Binary Clock, QOD, Calendar, Solar System.
* Moved most shared JSON functions into helper file.

### Fixed

* Corrected saving of owned Bitcoin in settings.
* Fixed visual startup bug.
* Fixed serious bug in Solar System page.
* Fixed serious bug in Moon Phase.

---

## [1.0.0] – 2025-11-26

### Added

* Initial release of SquaredCoso.

