#!/usr/bin/env python3
"""
STM32F103RCT6 UART 부트로더 업로더 - GUI

사용법:
    pip install pyserial
    python uart_flash_gui.py
"""
import os
import queue
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText

import serial
from serial.tools import list_ports

from uart_flash import Bootloader, BootloaderError, Cancelled, load_image, upload

BAUDS = ["9600", "19200", "38400", "57600", "115200", "230400", "460800"]


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("STM32 UART Flasher")
        self.minsize(560, 420)

        self.events = queue.Queue()      # 작업 스레드 -> UI
        self.cancel_flag = threading.Event()
        self.worker = None

        self._build()
        self.refresh_ports()
        self.after(50, self._poll_events)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ---------------- UI ----------------
    def _build(self):
        pad = {"padx": 6, "pady": 4}
        frm = ttk.Frame(self, padding=8)
        frm.pack(fill="both", expand=True)
        frm.columnconfigure(1, weight=1)

        ttk.Label(frm, text="포트").grid(row=0, column=0, sticky="w", **pad)
        self.port_var = tk.StringVar()
        self.port_cb = ttk.Combobox(frm, textvariable=self.port_var, state="readonly")
        self.port_cb.grid(row=0, column=1, sticky="ew", **pad)
        ttk.Button(frm, text="새로고침", command=self.refresh_ports).grid(row=0, column=2, sticky="ew", **pad)

        ttk.Label(frm, text="속도").grid(row=1, column=0, sticky="w", **pad)
        self.baud_var = tk.StringVar(value="115200")
        ttk.Combobox(frm, textvariable=self.baud_var, values=BAUDS, width=10).grid(
            row=1, column=1, sticky="w", **pad)

        ttk.Label(frm, text="펌웨어").grid(row=2, column=0, sticky="w", **pad)
        self.file_var = tk.StringVar()
        ttk.Entry(frm, textvariable=self.file_var).grid(row=2, column=1, sticky="ew", **pad)
        ttk.Button(frm, text="찾아보기…", command=self.browse).grid(row=2, column=2, sticky="ew", **pad)

        self.run_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(frm, text="다운로드 후 앱 실행", variable=self.run_var).grid(
            row=3, column=1, sticky="w", **pad)

        btns = ttk.Frame(frm)
        btns.grid(row=4, column=0, columnspan=3, sticky="ew", **pad)
        btns.columnconfigure(0, weight=1)
        self.progress = ttk.Progressbar(btns, mode="determinate", maximum=100)
        self.progress.grid(row=0, column=0, sticky="ew", padx=(0, 6))
        self.start_btn = ttk.Button(btns, text="다운로드", command=self.start)
        self.start_btn.grid(row=0, column=1, padx=(0, 4))
        self.cancel_btn = ttk.Button(btns, text="취소", command=self.cancel, state="disabled")
        self.cancel_btn.grid(row=0, column=2)

        self.status_var = tk.StringVar(value="대기 중")
        ttk.Label(frm, textvariable=self.status_var).grid(row=5, column=0, columnspan=3, sticky="w", **pad)

        self.log = ScrolledText(frm, height=12, state="disabled", font=("Consolas", 9))
        self.log.grid(row=6, column=0, columnspan=3, sticky="nsew", **pad)
        frm.rowconfigure(6, weight=1)

    def refresh_ports(self):
        ports = sorted(list_ports.comports(), key=lambda p: p.device)
        self.port_items = {f"{p.device} - {p.description}": p.device for p in ports}
        names = list(self.port_items)
        self.port_cb["values"] = names
        if self.port_var.get() not in names:
            self.port_var.set(names[0] if names else "")

    def browse(self):
        path = filedialog.askopenfilename(
            title="펌웨어 선택", filetypes=[("Binary", "*.bin"), ("All files", "*.*")])
        if path:
            self.file_var.set(path)

    def append_log(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def set_busy(self, busy):
        self.start_btn.configure(state="disabled" if busy else "normal")
        self.cancel_btn.configure(state="normal" if busy else "disabled")

    # ---------------- 작업 ----------------
    def start(self):
        port = self.port_items.get(self.port_var.get())
        path = self.file_var.get().strip()
        if not port:
            messagebox.showwarning("포트", "시리얼 포트를 선택하세요.")
            return
        if not os.path.isfile(path):
            messagebox.showwarning("펌웨어", "펌웨어 .bin 파일을 선택하세요.")
            return
        try:
            baud = int(self.baud_var.get())
            image = load_image(path)
        except ValueError:
            messagebox.showwarning("속도", "통신 속도가 올바르지 않습니다.")
            return
        except (OSError, BootloaderError) as e:
            messagebox.showerror("펌웨어", str(e))
            return

        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        self.progress["value"] = 0
        self.cancel_flag.clear()
        self.set_busy(True)
        self.status_var.set("보드를 리셋하세요…")
        self.append_log(f"{os.path.basename(path)}  {len(image)} bytes  ->  {port} @ {baud}")

        self.worker = threading.Thread(
            target=self._work, args=(port, baud, image, self.run_var.get()), daemon=True)
        self.worker.start()

    def cancel(self):
        self.cancel_flag.set()
        self.status_var.set("취소 중…")

    def _work(self, port, baud, image, run):
        emit = self.events.put
        try:
            bl = Bootloader(port, baud)
        except serial.SerialException as e:
            emit(("done", False, f"포트를 열 수 없습니다: {e}"))
            return
        try:
            upload(bl, image, run=run,
                   log=lambda s: emit(("log", s)),
                   on_progress=lambda d, t: emit(("progress", d, t)),
                   cancel=self.cancel_flag.is_set)
            emit(("done", True, "완료"))
        except Cancelled as e:
            emit(("done", False, str(e)))
        except (BootloaderError, serial.SerialException) as e:
            emit(("done", False, f"오류: {e}"))
        finally:
            bl.close()

    def _poll_events(self):
        try:
            while True:
                ev = self.events.get_nowait()
                kind = ev[0]
                if kind == "log":
                    self.append_log(ev[1])
                    if ev[1].startswith("["):
                        self.status_var.set(ev[1].strip())
                elif kind == "progress":
                    done, total = ev[1], ev[2]
                    self.progress["value"] = done * 100 / total
                    self.status_var.set(f"쓰는 중 {done * 100 // total}%  ({done}/{total})")
                elif kind == "done":
                    ok, msg = ev[1], ev[2]
                    self.append_log(msg)
                    self.status_var.set(msg)
                    if ok:
                        self.progress["value"] = 100
                    self.set_busy(False)
                    if not ok and not self.cancel_flag.is_set():
                        messagebox.showerror("실패", msg)
        except queue.Empty:
            pass
        self.after(50, self._poll_events)

    def _on_close(self):
        if self.worker and self.worker.is_alive():
            if not messagebox.askyesno("종료", "다운로드 중입니다. 중단하고 종료할까요?"):
                return
            self.cancel_flag.set()
            self.worker.join(timeout=3)
        self.destroy()


if __name__ == "__main__":
    App().mainloop()
