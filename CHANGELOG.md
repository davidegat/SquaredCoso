# Changelog – SquaredCoso

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

