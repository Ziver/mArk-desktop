# Task Viewer — CrowPanel 5.0" ESP32-S3

A daily task viewer that pulls events and tasks from Google Calendar, iCal feeds, and self-hosted Vikunja instances, displayed on an
Elecrow CrowPanel 5.0" touchscreen (800×480). Touch-only interface.

## Hardware

| Component | Details |
|---|---|
| Display | CrowPanel 5.0" ESP32-S3 (800×480, RGB parallel) |
| MCU | ESP32-S3 |
| Touch | Capacitive (GT911) |

## Setup

### 1. Credentials

Copy the example file and fill in your values:

```bash
cp src/credentials.h.example src/credentials.h
```

Edit `src/credentials.h`:

```c
#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define GCAL_API_KEY  "your-google-calendar-api-key"
```

`credentials.h` is gitignored and never committed.

### 2. Build & Flash

```bash
pio run -t upload
pio device monitor
```

## Project Structure

```
esp32-project/
├── platformio.ini              # PlatformIO config
├── partitions.csv              # Flash partition table
└── src/
    ├── main.cpp                # Entry point
    ├── display_driver.h        # RGB display driver (LovyanGFX)
    ├── touch.h                 # GT911 touch input
    ├── wifi_manager.h          # WiFi + NTP
    ├── task_store.h            # NVS task provider config persistence
    ├── task_fetch.h            # Task fetch coordinator
    ├── task_common.h           # Shared structures, cache and helpers
    ├── provider_gcal.h         # Google Calendar task provider implementation
    ├── provider_ical.h         # iCal / ICS feed parser implementation
    ├── provider_vikunja.h      # Vikunja task provider (fetch + completion sync)
    ├── streak_store.h          # NVS streak/level persistence
    ├── web_settings.h          # Local web portal for task provider management
    ├── ui_taskviewer.h         # LVGL UI (800×480, dark mode, touch)
    ├── lv_conf.h               # LVGL configuration
    ├── credentials.h           # Your secrets — gitignored, not committed
    ├── credentials.h.example   # Template — copy to credentials.h
    └── font_swedish_*.c        # Custom font assets
```

## Features

- Multiple task provider integrations:
  - **Google Calendar**: Integrates with the official calendar events list API
  - **iCal / ICS**: Directly downloads and parses `.ics` feeds
  - **Vikunja**: Connects to self-hosted Vikunja instances via REST API with two-way done/status sync
- Touch navigation: swipe left/right to browse days, tap to mark tasks done
- Streak counter with level system (Starter → Legend → Titan)
- Dark mode
- Local web portal for managing task provider sources (no reflash needed)
- NTP time sync

## Troubleshooting

- **Blank screen**: Check display driver config matches your CrowPanel 5.0" model
- **No WiFi**: Verify credentials.h has the correct SSID/password
- **No events**: Check your Google Calendar API key and that the calendar/provider ID is correct
- **Touch not working**: Verify GT911 driver matches your panel revision

