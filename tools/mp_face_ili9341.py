"""
Pi Camera (CSI) + MediaPipe 얼굴 인식 → ILI9341 240x320 SPI 디스플레이 출력.

전제:
  - Pi Camera Module (CSI 리본)
  - ILI9341 SPI 디스플레이: DC=GPIO24, RST=GPIO25, /dev/spidev0.0
  - LVGL 앱이 SPI 점유 중이면 먼저 종료: sudo killall lvgl_app

설치 (가상환경 mp 안):
  sudo apt install -y python3-picamera2 python3-spidev python3-libgpiod
  pip install opencv-python mediapipe numpy

실행:
  source ~/.venvs/mp/bin/activate
  python tools/mp_face_ili9341.py
  (Ctrl+C 로 종료)
"""

import time
import numpy as np
import cv2
import spidev
import gpiod
import mediapipe as mp
from picamera2 import Picamera2
from libcamera import Transform

# ─── 디스플레이 설정 ───
DISPLAY_W, DISPLAY_H = 240, 320
SPI_BUS, SPI_DEV = 0, 0
SPI_HZ = 16_000_000      # 안정적 기본값. 32_000_000 도 시도 가능 (케이블 길이에 따름)
DC_GPIO, RST_GPIO = 24, 25  # BCM, C 드라이버(ili9341.c)와 일치

# ─── 카메라 방향 (리본이 위로 갈 때 보통 1,1 / 아래면 0,0) ───
CAMERA_HFLIP = 1
CAMERA_VFLIP = 1


def display_init():
    spi = spidev.SpiDev()
    spi.open(SPI_BUS, SPI_DEV)
    spi.max_speed_hz = SPI_HZ
    spi.mode = 0

    chip = gpiod.Chip("gpiochip0")
    dc = chip.get_line(DC_GPIO)
    rst = chip.get_line(RST_GPIO)
    dc.request(consumer="ili9341_dc", type=gpiod.LINE_REQ_DIR_OUT, default_val=1)
    rst.request(consumer="ili9341_rst", type=gpiod.LINE_REQ_DIR_OUT, default_val=1)

    # 하드웨어 리셋
    rst.set_value(1); time.sleep(0.01)
    rst.set_value(0); time.sleep(0.01)
    rst.set_value(1); time.sleep(0.15)

    def cmd(c, data=None):
        dc.set_value(0); spi.xfer2([c])
        if data is not None:
            dc.set_value(1)
            spi.xfer2(list(data) if isinstance(data, (bytes, bytearray)) else [data])

    cmd(0x01); time.sleep(0.15)   # SWRESET
    cmd(0x11); time.sleep(0.5)    # SLPOUT
    cmd(0x3A, 0x55)               # COLMOD: 16bit/pixel RGB565
    cmd(0x36, 0x48)               # MADCTL: portrait, BGR
    cmd(0x29); time.sleep(0.1)    # DISPON
    return spi, dc


def push_full_frame(spi, dc, pixels_be: bytes):
    # 풀스크린 윈도우 지정
    dc.set_value(0); spi.xfer2([0x2A])
    dc.set_value(1); spi.xfer2([0, 0, 0, DISPLAY_W - 1])
    dc.set_value(0); spi.xfer2([0x2B])
    dc.set_value(1); spi.xfer2([0, 0, 0, DISPLAY_H - 1])
    dc.set_value(0); spi.xfer2([0x2C])
    dc.set_value(1)
    # 픽셀 청크 전송
    CHUNK = 4096
    for i in range(0, len(pixels_be), CHUNK):
        spi.xfer2(list(pixels_be[i:i + CHUNK]))


def rgb_to_rgb565_be(frame_rgb: np.ndarray) -> bytes:
    r = (frame_rgb[..., 0].astype(np.uint16) >> 3) << 11
    g = (frame_rgb[..., 1].astype(np.uint16) >> 2) << 5
    b = frame_rgb[..., 2].astype(np.uint16) >> 3
    rgb565 = (r | g | b).astype(np.uint16)
    out = np.empty(rgb565.shape + (2,), dtype=np.uint8)
    out[..., 0] = (rgb565 >> 8) & 0xFF
    out[..., 1] = rgb565 & 0xFF
    return out.tobytes()


def main():
    print("[1/3] 디스플레이 초기화...")
    spi, dc = display_init()

    print("[2/3] 카메라 초기화...")
    picam = Picamera2()
    cfg = picam.create_preview_configuration(
        main={"size": (DISPLAY_W, DISPLAY_H), "format": "RGB888"},
        transform=Transform(rotation=90, hflip=CAMERA_HFLIP, vflip=CAMERA_VFLIP),
    )
    picam.configure(cfg)
    picam.start()
    time.sleep(0.5)

    print("[3/3] MediaPipe 로드...")
    mp_fd = mp.solutions.face_detection
    face = mp_fd.FaceDetection(model_selection=0, min_detection_confidence=0.5)
    print("실행 중. Ctrl+C 로 종료.")

    t0 = time.time()
    frames = 0
    try:
        while True:
            frame = picam.capture_array()           # (H, W, 3) RGB, 240x320
            res = face.process(frame)
            n = 0
            if res.detections:
                for det in res.detections:
                    bb = det.location_data.relative_bounding_box
                    x = int(bb.xmin * DISPLAY_W)
                    y = int(bb.ymin * DISPLAY_H)
                    w = int(bb.width * DISPLAY_W)
                    h = int(bb.height * DISPLAY_H)
                    cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                    s = det.score[0]
                    cv2.putText(frame, f"{s:.2f}", (x, max(y - 4, 12)),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
                    n += 1

            pixels = rgb_to_rgb565_be(frame)
            push_full_frame(spi, dc, pixels)

            frames += 1
            now = time.time()
            if now - t0 >= 1.0:
                print(f"  faces={n}  fps~{frames / (now - t0):.1f}")
                frames = 0; t0 = now
    except KeyboardInterrupt:
        pass
    finally:
        face.close()
        picam.close()
        spi.close()
        print("종료.")


if __name__ == "__main__":
    main()
