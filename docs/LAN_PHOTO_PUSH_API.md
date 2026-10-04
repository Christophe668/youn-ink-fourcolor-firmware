# LAN Gallery Push API

This document describes the HTTP API the 2BP gallery firmware exposes in LAN Wi-Fi mode, so a NAS, script or local service can push images to the device on a schedule.

## Prerequisites

The device must be connected to your Wi-Fi network, and `LAN service` must be turned on in the device's Settings page. Once enabled, Settings shows the device's current LAN IP, for example:

```text
192.168.110.238
```

All endpoints below use that IP:

```text
http://192.168.110.238
```

With the LAN service on, open `http://<device-ip>/` in a browser to get the image management page. A NAS or script can also call the API below directly.

## Image format

The `/upload` endpoint does not accept common image files such as JPG, PNG or WEBP. It takes raw screen pixel data that has already been converted.

The screen size is fixed at:

```text
400 x 300
```

Two upload formats are supported:

| format | Meaning | File size |
| --- | --- | --- |
| `1bpp` | Black/white, 1 bit per pixel | `15000 bytes` |
| `bwry2bpp` or `2bpp` | Black/white/yellow/red, 2 bits per pixel | `30000 bytes` |

To push ordinary images from a NAS, convert the JPG/PNG to one of these bin formats on the NAS first, then call `/upload`.

## Check service status

```bash
curl "http://192.168.110.238/status"
```

Example response:

```json
{
  "status": "ready",
  "mode": "lan",
  "ip": "192.168.110.238",
  "url": "http://192.168.110.238/"
}
```

`mode=lan` means the LAN HTTP service is running; `mode=ap` means hotspot upload mode.

## Upload an image

Upload a 2BP four-color image:

```bash
curl -X POST \
  "http://192.168.110.238/upload?format=bwry2bpp" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@/path/to/image_400x300_2bpp.bin"
```

Upload a 1BP black/white image:

```bash
curl -X POST \
  "http://192.168.110.238/upload?format=1bpp" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@/path/to/image_400x300_1bpp.bin"
```

Example success response:

```json
{
  "success": true,
  "id": "ap12345678901"
}
```

Common failure causes:

| Cause | Symptom |
| --- | --- |
| Wrong file size | Returns `Expected 400x300 2bpp four-color data` or `Expected 400x300 1bpp data` |
| Device HTTP service not enabled | The NAS can't connect to the device IP |
| IP changed | Re-read the LAN IP shown in the device's Settings page |
| Gallery full | The device fails to save |

## List images

```bash
curl "http://192.168.110.238/photos"
```

Example response:

```json
{
  "photos": [
    {
      "id": "ap12345678901",
      "title": "WiFi color image",
      "date": "2026-05-21",
      "location": "WiFi AP",
      "body": "Phone WiFi upload · 2 BP color",
      "width": 400,
      "height": 300,
      "size": 30000,
      "format": "bwry2bpp"
    }
  ]
}
```

Fields:

| Field | Meaning |
| --- | --- |
| `id` | Image ID, used for download, delete and edit |
| `title` | Image title |
| `date` | Date string |
| `location` | Location |
| `body` | Description |
| `width` / `height` | Image size |
| `size` | Raw data size |
| `format` | `1bpp` or `bwry2bpp` |

## Download raw image data

```bash
curl \
  "http://192.168.110.238/photo?id=ap12345678901" \
  --output image.bin
```

Returns the raw bin data the image was saved with.

## Delete an image

```bash
curl -X DELETE \
  "http://192.168.110.238/photo?id=ap12345678901"
```

Success response:

```json
{"success":true}
```

## Update image info

```bash
curl -X POST "http://192.168.110.238/photo/meta" \
  -H "Content-Type: application/json" \
  -d '{
    "id": "ap12345678901",
    "title": "Daily photo",
    "date": "2026-05-21",
    "location": "NAS",
    "body": "Pushed daily by the NAS"
  }'
```

Success response:

```json
{"success":true}
```

## Reorder images

Move up one position:

```bash
curl -X POST "http://192.168.110.238/photos/move" \
  -H "Content-Type: application/json" \
  -d '{"id":"ap12345678901","delta":-1}'
```

Move down one position:

```bash
curl -X POST "http://192.168.110.238/photos/move" \
  -H "Content-Type: application/json" \
  -d '{"id":"ap12345678901","delta":1}'
```

## Slideshow interval

Read the current settings:

```bash
curl "http://192.168.110.238/settings"
```

Example response:

```json
{
  "success": true,
  "slideshow_interval": 5,
  "service_running": true,
  "mode": "lan",
  "ip": "192.168.110.238",
  "url": "http://192.168.110.238/"
}
```

Set the slideshow interval:

```bash
curl -X POST "http://192.168.110.238/settings" \
  -H "Content-Type: application/json" \
  -d '{"slideshow_interval":5}'
```

Stop the local HTTP service:

```bash
curl -X POST "http://192.168.110.238/settings" \
  -H "Content-Type: application/json" \
  -d '{"service_enabled":false}'
```

Stop the service, turn off Wi-Fi and go to sleep immediately:

```bash
curl -X POST "http://192.168.110.238/settings" \
  -H "Content-Type: application/json" \
  -d '{"service_enabled":false,"wifi_enabled":false,"sleep":true}'
```

Supported values:

| Value | Meaning |
| --- | --- |
| `0` | Slideshow off |
| `5` | 5 minutes |
| `10` | 10 minutes |
| `30` | 30 minutes |

## Scheduled NAS push example

This directory includes a reusable converter script:

```text
docs/inkscreen_image_converter.js
```

It is extracted from the conversion algorithm in the device's management HTML page and turns an ordinary image into the raw bin the `/upload` endpoint expects. The command-line mode uses `sharp` to decode and resize images:

```bash
npm install sharp
node docs/inkscreen_image_converter.js input.jpg daily_400x300_2bpp.bin bwry2bpp
```

It can also produce 1BP black/white:

```bash
node docs/inkscreen_image_converter.js input.jpg daily_400x300_1bpp.bin 1bpp
```

If the NAS has already produced `daily_400x300_2bpp.bin`, cron can push it once a day:

```bash
#!/bin/sh
DEVICE="192.168.110.238"
BIN="/volume1/photo/daily_400x300_2bpp.bin"

curl -fsS -X POST \
  "http://${DEVICE}/upload?format=bwry2bpp" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@${BIN}"
```

To add a description after uploading, parse the returned `id` and then call `/photo/meta`. For example:

```bash
#!/bin/sh
DEVICE="192.168.110.238"
BIN="/volume1/photo/daily_400x300_2bpp.bin"

RESP=$(curl -fsS -X POST \
  "http://${DEVICE}/upload?format=bwry2bpp" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@${BIN}")

ID=$(printf "%s" "$RESP" | sed -n 's/.*"id":"\([^"]*\)".*/\1/p')

if [ -n "$ID" ]; then
  TODAY=$(date +%F)
  curl -fsS -X POST "http://${DEVICE}/photo/meta" \
    -H "Content-Type: application/json" \
    -d "{\"id\":\"${ID}\",\"title\":\"Daily photo\",\"date\":\"${TODAY}\",\"location\":\"NAS\",\"body\":\"Pushed by the NAS\"}"
fi
```

## Suggested future improvements

The API already supports scheduled NAS pushes, but letting the NAS send JPG/PNG directly would need a new conversion step on the firmware or NAS side.

Options:

1. Convert on the NAS: a script on the NAS turns JPG/PNG into a `400x300 bwry2bpp bin`, then calls `/upload`. This uses the least device memory.
2. Add `/upload-image` to the firmware: the device accepts JPG/PNG directly and converts it. Easier to use, but costs more memory and decoding work on the ESP32.

Option 1 is currently recommended.
