from __future__ import annotations

from datetime import datetime, timezone
from typing import Any, Literal, Optional
from uuid import uuid4

from fastapi import FastAPI
from pydantic import BaseModel, Field


app = FastAPI(title="ESP32 AI Assistant Backend", version="0.2.0")


# ===== Models =====

class CommandIn(BaseModel):
    text: str
    device_id: Optional[str] = None
    context: Optional[dict[str, Any]] = None


ActionType = Literal["oled", "led", "delay_ms"]


class Action(BaseModel):
    type: ActionType

    # oled
    text: Optional[str] = None

    # led
    value: Optional[Literal["on", "off"]] = None

    # delay
    ms: Optional[int] = Field(default=None, ge=0, le=60_000)


class AIResponse(BaseModel):
    request_id: str
    ts: str
    device_id: Optional[str] = None
    response: str
    actions: list[Action] = Field(default_factory=list)


# ===== Helpers =====

def utc_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def make_response(
    *,
    response: str,
    device_id: Optional[str],
    actions: list[Action] | None = None,
) -> AIResponse:
    return AIResponse(
        request_id=str(uuid4()),
        ts=utc_iso(),
        device_id=device_id,
        response=response,
        actions=actions or [],
    )


# ===== Routes =====

@app.get("/health")
def health():
    return {"status": "ok", "ts": utc_iso(), "version": app.version}


@app.post("/ai", response_model=AIResponse)
def ai(cmd: CommandIn):
    t = (cmd.text or "").strip().lower()

    LED_ON = {"led on", "ledon", "włącz led", "wlacz led", "zalacz led", "załącz led"}
    LED_OFF = {"led off", "ledoff", "wyłącz led", "wylacz led", "zgas led", "zgaś led"}

    # v1 intents 
    if any(k in t for k in ["hello", "hej", "cześć", "czesc"]):
        return make_response(
            response="Cześć! Jestem gotowy 🤖",
            device_id=cmd.device_id,
            actions=[Action(type="oled", text="Czesc! 🤖")],
        )

    if "status" in t:
        return make_response(
            response="System działa poprawnie ✅",
            device_id=cmd.device_id,
            actions=[Action(type="oled", text="Status: OK")],
        )

    if t in LED_ON:
        return make_response(
            response="Włączam LED ✅",
            device_id=cmd.device_id,
            actions=[
                Action(type="led", value="on"),
                Action(type="oled", text="LED: ON"),
            ],
        )

    if t in LED_OFF:
        return make_response(
            response="Wyłączam LED ✅",
            device_id=cmd.device_id,
            actions=[
                Action(type="led", value="off"),
                Action(type="oled", text="LED: OFF"),
            ],
        )

    return make_response(
        response="Nie rozumiem polecenia. Spróbuj: hello, status, led on, led off.",
        device_id=cmd.device_id,
        actions=[Action(type="oled", text="Nie rozumiem :(")],
    )
