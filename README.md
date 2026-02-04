# ESP32 AI Assistant (Embedded/IoT Portfolio Project)

> 🚧 Project status: **Work in progress (MVP is working)**  
> This repository is actively developed as an embedded/IoT portfolio project.

## Overview
ESP32 device acts as an edge node with a local Web UI and an OLED HUD.  
FastAPI backend works as the “brain” and returns an **action plan** (`actions[]`) which the ESP32 executes deterministically.

## Architecture
Web UI (ESP32) → ESP32 → HTTP (JSON) → FastAPI backend → action plan → ESP32 → OLED + GPIO

## Current Features (MVP)
- ✅ WiFi STA connection + boot status on OLED
- ✅ Local Web UI hosted on ESP32 (`/` + `/cmd?text=...`)
- ✅ Backend communication via HTTP POST with timeout + retry
- ✅ JSON protocol (ArduinoJson)
- ✅ Device identity (`device_id` derived from MAC)
- ✅ Backend returns `request_id` + `actions[]` (workflow-like execution)
- ✅ Supported actions:
  - `led` (`on/off`)
  - `oled` (`text`)
  - `delay_ms` (`ms`)
- ✅ FastAPI `/health` endpoint

## Supported Commands (demo)
- `hello`
- `status`
- `led on`
- `led off`

## Quick Start

### Backend (FastAPI)
```bash
cd backend
pip install -r requirements.txt
uvicorn main:app --reload --host 0.0.0.0 --port 8000
