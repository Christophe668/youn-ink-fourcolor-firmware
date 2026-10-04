# Youn Ink Four Color

> **English fork.** This fork translates the on-device UI, the device's image-upload web page and the documentation to English. See the [`english-ui`](https://github.com/Christophe668/youn-ink-fourcolor-firmware/tree/english-ui) branch. Upstream: [LazyYoun/youn-ink-fourcolor-firmware](https://github.com/LazyYoun/youn-ink-fourcolor-firmware).

A personal AI assistant project for ESP32-S3 e-paper devices. The main line has three parts: the ESP32 firmware, a Python backend service, and pages for managing images, to-dos and devices.

This is not a general-purpose npm package. It is a system that runs on a real e-paper device: voice chat, TTS playback, to-do sync, weather/news/calendar/e-book/gallery pages, AP image upload, OTA firmware management, and a RawDraw UI designed for four-color screens.

## 2BP four-color image pipeline

![Youn Ink Four Color 2BP BWRY architecture](README-2bp-architecture.png)

Gallery images enter the server from a PC/NAS admin client or the device's AP page, are converted to `2BP BWRY` (black, white, red, yellow), and are pushed over Wi-Fi to the ESP32-S3 four-color e-paper screen. The 2BP four-color pipeline in this repo is maintained separately from NOTE4's 4BP black-and-white grayscale gallery: the panel colors, pixel format and refresh driver are all different.

## Current status

- The backend is now the Python service under `server/`. The old root-level Node `scripts/` have been removed.
- The firmware's main UI is rendered with RawDraw, designed for the four-color screen by default, with 1bpp black-and-white compatibility kept.
- There is only one theme for now: a Nintendo-inspired four-color theme with semantic use of red, yellow, black and white.
- Image transfer supports two formats: 1bpp black/white and 2bpp four-color BWRY.
- The root `.gitignore` excludes build output, logs, pid files, databases, local config and key files.

## Directory structure

```text
.
├── firmware/        ESP-IDF firmware: RawDraw UI, page rendering, display driver, AP image upload
├── server/          Python backend: WebSocket chat, TTS, discovery, image push, OTA API
├── frontend/        Admin frontend source, with its own package/pnpm workflow
├── docs/            Historical design docs and implementation notes
├── documents/       Project materials
└── package.json     Repo-level helper commands only; no longer the entry point for the old Node service
```

Note: `firmware/scripts/` and `frontend/scripts/` are still used, for firmware and frontend tooling respectively. Only the legacy root-level `scripts/` was removed.

## Backend service

The backend entry point is `server/llmserve.py`; manage it with `server/start.sh`. Default ports:

| Port | Protocol | Purpose |
| --- | --- | --- |
| `9001` | WebSocket | ESP32 voice, LLM, TTS, sync messages |
| `8766` | UDP | Device discovery |
| `8766` | HTTP | Image push, device image management, OTA API |
| `8090` | HTTP | Standalone admin service (optional) |

### Install dependencies

```bash
cd server
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### Start the service

```bash
export DASHSCOPE_API_KEY=your_dashscope_api_key
cd server
./start.sh start
```

Common commands:

```bash
cd server
./start.sh status
./start.sh logs
./start.sh restart
./start.sh stop
```

Or from the repo root:

```bash
npm run server:start
npm run server:status
npm run server:logs
```

### Local mock device

```bash
cd server
python3 mock_client.py --server ws://127.0.0.1:9001
```

## Image and device management

The image HTTP API is mounted by `server/push_image.py` on port `8766`. It supports:

- Uploading an image file, converting it and pushing it to the device.
- Choosing `1bpp` black/white or `2bpp` four-color BWRY format.
- Listing the images on the device.
- Deleting images from the device.
- Uploading firmware and serving it for OTA download.

Common endpoints:

```bash
curl http://localhost:8766/api/status
curl http://localhost:8766/api/images
```

Upload example:

```bash
curl -X POST http://localhost:8766/api/upload_image \
  -F "image=@/path/to/photo.jpg" \
  -F "format=bwry2bpp" \
  -F "title=Photo title"
```

When the device is in AP upload mode, join its hotspot from your phone and open:

```text
http://192.168.4.1
```

To push images over your home network instead, see [docs/LAN_PHOTO_PUSH_API.md](docs/LAN_PHOTO_PUSH_API.md).

## Firmware

The firmware lives in `firmware/` and is based on ESP-IDF v6.0. It targets the ZecTrix ESP32-S3 4.2-inch e-paper device by default, supports the four-color BWRY screen, and keeps a 1bpp black-and-white configuration.

### Build

```bash
cd firmware
source ~/esp/esp-idf-v6.0/export.sh   # path to your ESP-IDF v6.0 install
idf.py set-target esp32s3             # first time only
idf.py build
```

Flash (hold BOOT and tap RESET if the port isn't detected):

```bash
idf.py -p /dev/cu.usbmodemXXXX flash
```

Repo-root helper:

```bash
npm run firmware:build
```

### Display configuration

The firmware Kconfig has a display type option:

```text
ZECTRIX_EPD_PANEL_4COLOR_SSD2683  four-color BWRY screen
ZECTRIX_EPD_PANEL_1BPP            black/white 1bpp screen
```

To go back to the old black-and-white screen, switch to `1bpp black/white EPD` in `idf.py menuconfig`, then rebuild and flash. The RawDraw theme layer degrades the red/yellow semantic colors into readable black-and-white styles.

## UI overview

The firmware UI uses the RawDraw component system. The main pages are:

- Chat: shows the user's speech, recognition status and the AI reply.
- To-do: local display, server sync, complete/delete/edit.
- Settings: volume, brightness, theme, network, sync, OTA and more.
- Gallery: thumbnail list, full-screen view, entry point for AP upload.
- Weather / weather details, news, almanac, year progress, calendar, e-book, log.
- Quick-switch overlay: for jumping between pages.

The four-color theme layer draws components through semantic styles. Avoid adding raw `RED/YELLOW/BLACK/WHITE` in page code; prefer RawDraw components and theme tokens for new UI.

## Environment variables

Common backend environment variables:

| Variable | Default | Description |
| --- | --- | --- |
| `DASHSCOPE_API_KEY` | none | Alibaba DashScope (Bailian) API key, required to start the backend |
| `LISTEN_HOST` | `0.0.0.0` | WebSocket listen address |
| `LISTEN_PORT` | `9001` | WebSocket port |
| `DISCOVERY_PORT` | `8766` | UDP discovery port |
| `PUSH_IMAGE_PORT` | `8766` | Image/OTA HTTP API port |
| `TTS_WS_CHUNK_BYTES` | `8000` | TTS push chunk size |
| `TTS_WS_CHUNK_GAP_SEC` | `0.01` | Delay between TTS chunks |

Do not commit `.env`, databases, logs, pid files, build directories or firmware artifacts.

## What to commit

Commit:

- Firmware source such as `firmware/main/`, `firmware/components/`, `firmware/partitions/`.
- `server/*.py`, `server/static/`, `server/requirements.txt`, `server/DEPLOY.md`.
- Frontend source such as `frontend/src/`, `frontend/package.json`, `frontend/pnpm-lock.yaml`.
- The root README, docs and config templates.

Do not commit:

- `firmware/build/`
- `firmware/managed_components/`
- `firmware/sdkconfig`
- `firmware/releases/`
- `server/.env`
- `server/todo.db`
- `server/*.pid`
- `server/*.log`
- `frontend/.env*`
- `frontend/dist/`
- `node_modules/`
