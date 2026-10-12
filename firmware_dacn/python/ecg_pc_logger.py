#!/usr/bin/env python3
"""
Nhan goi du lieu ECG tu STM32 qua UART (moi 10 giay), ve do thi va luu hinh PNG + CSV.

Cai thu vien:   pip install pyserial matplotlib
Chay:           python ecg_pc_logger.py COM5
                python ecg_pc_logger.py /dev/ttyUSB0 -b 115200 -o ecg_images

Dinh dang goi tin (little-endian):
  AA 55 | fs u16 | n u16 | bpm u16 | flags u16 | n x u16 (ADC 12 bit) | chk u8
  chk = tong cac byte tu "fs" den het du lieu (8 bit thap)
  flags: bit0 = roi dien cuc, bit1 = dang canh bao


   KHÔNG NỐI 3.3V CỦA STM32 VỚI MÁY TÍNH, CHỈ NỐI GND VÀ TX/RX QUA USB-TO-TLL 

"""
import argparse
import csv
import datetime
import os
import struct

import serial
import matplotlib

matplotlib.use("Agg")  # chi luu file, khong mo cua so
import matplotlib.pyplot as plt

VREF = 3.3
ADC_MAX = 4095


def wait_sync(ser):
    """Doc tung byte cho den khi gap AA 55."""
    state = 0
    while True:
        b = ser.read(1)
        if not b:
            continue  # het thoi gian cho, tiep tuc doi
        if state == 0:
            state = 1 if b[0] == 0xAA else 0
        else:
            if b[0] == 0x55:
                return
            state = 1 if b[0] == 0xAA else 0


def read_exact(ser, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return bytes(buf)


def save_outputs(out_dir, samples, fs, bpm, flags):
    os.makedirs(out_dir, exist_ok=True)
    now = datetime.datetime.now()
    stamp = now.strftime("%Y%m%d_%H%M%S")
    png_path = os.path.join(out_dir, f"ecg_{stamp}.png")
    csv_path = os.path.join(out_dir, f"ecg_{stamp}.csv")

    volts = [s * VREF / ADC_MAX for s in samples]
    t = [i / fs for i in range(len(samples))]

    notes = []
    if flags & 1:
        notes.append("ROI DIEN CUC")
    if flags & 2:
        notes.append("CANH BAO")
    note = " | ".join(notes) if notes else "BINH THUONG"
    bpm_txt = f"{bpm} BPM" if bpm else "-- BPM"

    fig, ax = plt.subplots(figsize=(12, 4))
    ax.plot(t, volts, linewidth=0.9, color="green")
    ax.set_xlabel("Thoi gian (s)")
    ax.set_ylabel("Dien ap ADC (V)")
    ax.set_title(f"ECG {now:%Y-%m-%d %H:%M:%S}   {bpm_txt}   [{note}]")
    ax.grid(True, alpha=0.4)
    ax.set_xlim(0, t[-1])
    fig.tight_layout()
    fig.savefig(png_path, dpi=100)
    plt.close(fig)

    with open(csv_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_s", "adc_raw", "volt"])
        for ti, raw, v in zip(t, samples, volts):
            w.writerow([f"{ti:.4f}", raw, f"{v:.4f}"])

    return png_path


def main():
    ap = argparse.ArgumentParser(description="Nhan va luu du lieu ECG tu STM32")
    ap.add_argument("port", help="Cong COM, vi du COM5 hoac /dev/ttyUSB0")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("-o", "--out", default="ecg_images", help="Thu muc luu hinh")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=2)
    print(f"Dang nghe {args.port} @ {args.baud}. Nhan Ctrl+C de dung.")

    try:
        while True:
            wait_sync(ser)

            hdr = read_exact(ser, 8)
            if hdr is None:
                continue
            fs, n, bpm, flags = struct.unpack("<HHHH", hdr)
            if fs == 0 or n == 0 or n > 20000:
                continue

            body = read_exact(ser, n * 2 + 1)
            if body is None:
                print("Goi tin bi thieu, bo qua.")
                continue

            data_bytes, chk = body[:-1], body[-1]
            if ((sum(hdr) + sum(data_bytes)) & 0xFF) != chk:
                print("Sai checksum, bo qua.")
                continue

            samples = struct.unpack(f"<{n}H", data_bytes)
            path = save_outputs(args.out, samples, fs, bpm, flags)
            print(f"Da luu: {path}  (BPM={bpm}, flags={flags})")
    except KeyboardInterrupt:
        print("\nDung.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()