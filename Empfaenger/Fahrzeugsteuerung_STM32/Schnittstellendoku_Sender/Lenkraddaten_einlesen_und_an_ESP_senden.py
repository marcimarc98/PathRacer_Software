import time
import struct
import pygame
import serial

# ---- SETTINGS ----
COM_PORT = "COM17"
BAUD = 460800
SEND_HZ = 1000
# ------------------

HEADER_1 = 0xAA
HEADER_2 = 0x55

def pedal_norm(raw: float) -> float:
    v = (1.0 - raw) * 0.5

    if v < 0.0:
        v = 0.0

    if v > 1.0:
        v = 1.0

    return v

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def steer_to_i16(steer: float) -> int:
    # -1.0 .. +1.0 -> -1000 .. +1000
    steer = clamp(steer, -1.0, 1.0)
    return int(steer * 1000.0)

def pedal_to_u16(x: float) -> int:
    # 0.0 .. 1.0 -> 0 .. 1000
    x = clamp(x, 0.0, 1.0)
    return int(x * 1000.0)

def buttons_to_u16(js) -> int:
    value = 0
    max_btn = min(13, js.get_numbuttons())

    for i in range(max_btn):
        if js.get_button(i):
            value |= (1 << i)

    return value

def make_packet(steer_i16: int, throttle_u16: int, brake_u16: int, buttons_u16: int) -> bytes:
    # < = little endian
    # B B h H H H
    payload_wo_crc = struct.pack(
        "<BBhHHH",
        HEADER_1,
        HEADER_2,
        steer_i16,
        throttle_u16,
        brake_u16,
        buttons_u16
    )

    checksum = 0

    for b in payload_wo_crc:
        checksum ^= b

    return payload_wo_crc + struct.pack("<B", checksum)

def precise_wait_until(target_time: float) -> None:
    # Windows sleep tends to overshoot.
    # Deshalb schlafen wir den groben Rest und warten am Ende kurz aktiv.
    while True:
        now = time.perf_counter()
        remaining = target_time - now

        if remaining <= 0:
            return

        if remaining > 0.002:
            time.sleep(remaining - 0.001)
        else:
            while time.perf_counter() < target_time:
                pass
            return

pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    raise SystemExit("Kein Lenkrad gefunden.")

js = pygame.joystick.Joystick(0)
js.init()

print("Wheel:", js.get_name())
print("Axes:", js.get_numaxes())
print("Buttons:", js.get_numbuttons())

ser = serial.Serial(COM_PORT, BAUD, timeout=0, write_timeout=0)
time.sleep(1.0)

print("Serial connected:", COM_PORT)
print("Baud:", BAUD)
print("Send rate:", SEND_HZ, "Hz")

dt = 1.0 / SEND_HZ
next_send = time.perf_counter()

while True:
    pygame.event.pump()

    steer = float(js.get_axis(0))
    brake_raw = float(js.get_axis(1))
    throttle_raw = float(js.get_axis(2))

    brake = pedal_norm(brake_raw)
    throttle = pedal_norm(throttle_raw)

    steer_i16 = steer_to_i16(steer)
    throttle_u16 = pedal_to_u16(throttle)
    brake_u16 = pedal_to_u16(brake)
    buttons_u16 = buttons_to_u16(js)

    packet = make_packet(
        steer_i16,
        throttle_u16,
        brake_u16,
        buttons_u16
    )

    ser.write(packet)

    next_send += dt
    now = time.perf_counter()

    if next_send > now:
        precise_wait_until(next_send)
    else:
        next_send = now