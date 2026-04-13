# Task Viewer — CrowPanel 5.0" ESP32-S3

A daily task viewer that pulls events from Google Calendar, displayed on an
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
    ├── calendar_fetch.h        # Google Calendar API + ICS
    ├── calendar_store.h        # NVS calendar config persistence
    ├── streak_store.h          # NVS streak/level persistence
    ├── web_settings.h          # Local web portal for calendar management
    ├── ui_taskviewer.h         # LVGL UI (800×480, dark mode, touch)
    ├── lv_conf.h               # LVGL configuration
    ├── credentials.h           # Your secrets — gitignored, not committed
    ├── credentials.h.example   # Template — copy to credentials.h
    └── font_swedish_*.c        # Custom font assets
```

## Features

- Google Calendar API + ICS URL support (multiple calendars)
- Touch navigation: swipe left/right to browse days, tap to mark tasks done
- Streak counter with level system (Starter → Legend → Titan)
- Dark mode
- Local web portal for managing calendar sources (no reflash needed)
- NTP time sync

## Troubleshooting

- **Blank screen**: Check display driver config matches your CrowPanel 5.0" model
- **No WiFi**: Verify credentials.h has the correct SSID/password
- **No events**: Check your Google Calendar API key and that the calendar ID is correct
- **Touch not working**: Verify GT911 driver matches your panel revision
