#!/usr/bin/env python3
"""
STM32F103RCT6 UART 부트로더 업로더

사용법:
    pip install pyserial
    python uart_flash.py COM5 app.bin
    python uart_flash.py /dev/ttyUSB0 app.bin --baud 115200 --no-run
"""
import argparse
import struct
import sys
import time
import zlib

import serial

SOF = 0xA5

CMD_PING = 0x01
CMD_ERASE = 0x02
CMD_WRITE = 0x03
CMD_VERIFY = 0x04
CMD_RUN = 0x05

STATUS = {
    0x00: "OK",
    0x01: "BAD_CRC",
    0x02: "BAD_CMD",
    0x03: "BAD_ARG",
    0x04: "FLASH_ERR",
    0x05: "VERIFY_FAIL",
    0x06: "NO_APP",
}


def crc16(data: bytes, crc: int = 0xFFFF) -> int:
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class BootloaderError(Exception):
    pass


class Cancelled(BootloaderError):
    pass


class Bootloader:
    def __init__(self, port: str, baud: int):
        self.ser = serial.Serial(port, baud, timeout=0.02)

    def close(self):
        self.ser.close()

    def _send(self, cmd: int, payload: bytes = b""):
        body = bytes([cmd]) + struct.pack("<H", len(payload)) + payload
        self.ser.write(bytes([SOF]) + body + struct.pack("<H", crc16(body)))

    def _read_exact(self, n: int, deadline: float) -> bytes:
        buf = bytearray()
        while len(buf) < n:
            if time.monotonic() > deadline:
                return bytes(buf)
            buf += self.ser.read(n - len(buf))
        return bytes(buf)

    def _recv(self, timeout: float):
        """(cmd, status, data) 또는 None"""
        deadline = time.monotonic() + timeout
        while True:
            b = self._read_exact(1, deadline)
            if not b:
                return None
            if b[0] == SOF:
                break
        hdr = self._read_exact(4, deadline)
        if len(hdr) < 4:
            return None
        cmd, status, length = hdr[0], hdr[1], struct.unpack("<H", hdr[2:4])[0]
        rest = self._read_exact(length + 2, deadline)
        if len(rest) < length + 2:
            return None
        data, crc = rest[:length], struct.unpack("<H", rest[length:])[0]
        if crc16(hdr + data) != crc:
            return None
        return cmd, status, data

    def request(self, cmd: int, payload: bytes = b"", timeout: float = 1.0, retries: int = 3) -> bytes:
        last = "응답 없음"
        for _ in range(retries):
            self.ser.reset_input_buffer()
            self._send(cmd, payload)
            resp = self._recv(timeout)
            if resp is None:
                continue
            rcmd, status, data = resp
            if status == 0x01:          # 전송 중 깨짐 -> 재시도
                last = "BAD_CRC"
                continue
            if rcmd != cmd:
                last = f"엉뚱한 응답 cmd=0x{rcmd:02X}"
                continue
            if status != 0x00:
                raise BootloaderError(f"cmd 0x{cmd:02X} 실패: {STATUS.get(status, hex(status))}")
            return data
        raise BootloaderError(f"cmd 0x{cmd:02X} 실패: {last}")

    def sync(self, wait_s: float, cancel=lambda: False) -> dict:
        """보드 리셋 직후의 대기 시간 안에 PING 을 맞추기 위해 계속 보낸다."""
        deadline = time.monotonic() + wait_s
        while time.monotonic() < deadline:
            if cancel():
                raise Cancelled("취소됨")
            self.ser.reset_input_buffer()
            self._send(CMD_PING)
            resp = self._recv(0.05)
            if resp and resp[0] == CMD_PING and resp[1] == 0:
                time.sleep(0.1)                 # 연달아 보낸 PING 의 응답을 버림
                self.ser.reset_input_buffer()
                d = resp[2]
                ver_maj, ver_min, app_start, app_max, max_chunk = struct.unpack("<BBIIH", d[:12])
                return {
                    "version": f"{ver_maj}.{ver_min}",
                    "app_start": app_start,
                    "app_max": app_max,
                    "max_chunk": max_chunk,
                }
        raise BootloaderError("부트로더 응답 없음 (포트/배선/보드 리셋을 확인하세요)")


def load_image(path: str) -> bytes:
    with open(path, "rb") as f:
        image = f.read()
    if len(image) % 2:
        image += b"\xFF"
    if len(image) < 8:
        raise BootloaderError("이미지가 너무 작습니다")
    return image


def upload(bl: Bootloader, image: bytes, chunk: int = 1024, run: bool = True, wait: float = 15.0,
           log=print, on_progress=None, cancel=lambda: False):
    """CLI 와 GUI 가 같이 쓰는 다운로드 절차. 실패/취소 시 BootloaderError."""
    log(f"[1/5] 부트로더 접속 중... 지금 보드를 리셋하세요 (최대 {wait:.0f}초)")
    info = bl.sync(wait, cancel)
    log(f"      v{info['version']}  app=0x{info['app_start']:08X}  max={info['app_max']} bytes")

    if len(image) > info["app_max"]:
        raise BootloaderError(f"이미지({len(image)}B)가 앱 영역({info['app_max']}B)보다 큽니다")

    # 첫 워드(초기 SP)로 .elf 를 잘못 변환했는지 대략 확인
    sp, reset = struct.unpack("<II", image[:8])
    if not (0x20000000 < sp <= 0x20010000) or not (
        info["app_start"] <= (reset & ~1) < info["app_start"] + info["app_max"]
    ):
        log(f"      경고: 벡터 테이블이 이상합니다 (SP=0x{sp:08X}, Reset=0x{reset:08X}).")
        log(f"            앱이 0x{info['app_start']:08X} 에 링크됐는지 확인하세요.")

    log(f"[2/5] 지우는 중 ({len(image)} bytes)")
    bl.request(CMD_ERASE, struct.pack("<I", len(image)), timeout=15.0)

    log("[3/5] 쓰는 중")
    chunk = min(chunk, info["max_chunk"]) & ~1
    t0 = time.monotonic()
    for off in range(0, len(image), chunk):
        if cancel():
            raise Cancelled("취소됨 (앱은 무효 상태로 남아 부트로더에 머뭅니다)")
        part = image[off:off + chunk]
        bl.request(CMD_WRITE, struct.pack("<I", off) + part, timeout=2.0, retries=5)
        if on_progress:
            on_progress(off + len(part), len(image))
    log(f"      {time.monotonic() - t0:.1f}초")

    log("[4/5] 검증 중")
    crc = zlib.crc32(image) & 0xFFFFFFFF
    bl.request(CMD_VERIFY, struct.pack("<II", len(image), crc), timeout=5.0)
    log(f"      CRC32 0x{crc:08X} OK")

    if run:
        log("[5/5] 앱 실행")
        bl.request(CMD_RUN, timeout=1.0, retries=1)
    else:
        log("[5/5] 앱 실행 건너뜀")
    log("완료")


def progress(done: int, total: int):
    pct = done * 100 // total
    bar = "#" * (pct // 2)
    sys.stdout.write(f"\r  [{bar:<50}] {pct:3d}% ({done}/{total})")
    sys.stdout.flush()


def main():
    ap = argparse.ArgumentParser(description="STM32F103 UART bootloader uploader")
    ap.add_argument("port", help="시리얼 포트 (예: COM5, /dev/ttyUSB0)")
    ap.add_argument("image", help="앱 바이너리 (.bin, 0x08004000 기준으로 링크된 것)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--wait", type=float, default=15.0, help="부트로더 접속 대기 시간(초)")
    ap.add_argument("--chunk", type=int, default=1024, help="WRITE 패킷당 바이트 수")
    ap.add_argument("--no-run", action="store_true", help="다운로드 후 앱 실행하지 않음")
    args = ap.parse_args()

    try:
        image = load_image(args.image)
    except (OSError, BootloaderError) as e:
        sys.exit(f"오류: {e}")

    def cli_progress(done, total):
        progress(done, total)
        if done == total:
            print()

    bl = Bootloader(args.port, args.baud)
    try:
        upload(bl, image, chunk=args.chunk, run=not args.no_run, wait=args.wait,
               on_progress=cli_progress)
    except BootloaderError as e:
        print(f"\n오류: {e}")
        sys.exit(1)
    finally:
        bl.close()


if __name__ == "__main__":
    main()
