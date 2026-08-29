"""
Giao dien CAI DAT NANG CAO - chan GPIO, so xung/vong, dao chieu truc...
Yeu cau: pip install pyserial

File nay TACH RIENG khoi gcode_gui_control.pyw (file su dung hang ngay) vi
day la cai dat lien quan truc tiep PHAN CUNG / FIRMWARE - chi nguoi lap dat
may hoac sua driver moi can dung, khong dung trong luc van hanh cat hang ngay.

Cach hoat dong: gui lenh CFG;... qua Serial xuong ESP32. ESP32 luu cau hinh
vao NVS (bo nho flash noi bo), KHONG mat khi mat dien / rut usb.
  - Thay doi CHAN GPIO   -> can bam "Luu vao flash" RIENG "Khoi dong lai ESP32"
    (vi lien quan gpio_config()/ngat phan cung, doi giua luc chay se khong an toan)
  - Thay doi HIEU CHUAN (xung/vong, mm/vong) va DAO CHIEU -> ap dung NGAY,
    khong can khoi dong lai, nhung van nen bam "Luu vao flash" de giu lai
    sau khi mat dien.
"""

import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time

COM_PORT_MAC_DINH = "COM3"
BAUD_RATE = 115200

# Ten chan hien thi (nhan) -> ten dung trong lenh CFG;PIN;<TEN>;<so>
CAC_CHAN = [
    ("PUL_KEO_A",       "Xung (PUL) truc keo - dong co A"),
    ("DIR_KEO_A",       "Chieu (DIR) truc keo - dong co A"),
    ("PUL_KEO_B",       "Xung (PUL) truc keo - dong co B"),
    ("DIR_KEO_B",       "Chieu (DIR) truc keo - dong co B"),
    ("PUL_XOAY",        "Xung (PUL) truc xoay"),
    ("DIR_XOAY",        "Chieu (DIR) truc xoay"),
    ("RELAY_PLASMA",    "Relay bat/tat mo cat plasma"),
    ("PLC_IN_START",    "Ngo vao PLC - START"),
    ("PLC_IN_STOP",     "Ngo vao PLC - STOP"),
    ("PLC_IN_EMG",      "Ngo vao PLC - EMG (dung khan cap)"),
    ("PLC_IN_LIMIT",    "Ngo vao PLC - LIMIT (cong tac hanh trinh)"),
    ("PLC_OUT_READY",   "Ngo ra PLC - READY"),
    ("PLC_OUT_RUNNING", "Ngo ra PLC - RUNNING"),
    ("PLC_OUT_DONE",    "Ngo ra PLC - DONE"),
    ("PLC_OUT_FAULT",   "Ngo ra PLC - FAULT"),
]


class SettingsApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Cai dat nang cao - May Cat Ong (CHAN GPIO / HIEU CHUAN)")
        self.root.geometry("760x820")
        self.root.minsize(700, 600)

        self.ser = None
        self.dang_ket_noi = False
        self.dang_doc = False
        self.entry_chan = {}

        self._xay_dung_giao_dien()

    # ---------------------------------------------------------
    def _xay_dung_giao_dien(self):
        pad = {"padx": 8, "pady": 6}

        canh_bao = tk.Label(
            self.root,
            text="⚠ CANH BAO: chi doi cau hinh khi may DUNG HAN, khong dang cat.\n"
                 "Doi sai chan GPIO co the lam mat tin hieu EMG/LIMIT hoac dieu khien nham dong co.",
            font=("Segoe UI", 9, "bold"), fg="#d9534f", justify="left", wraplength=720)
        canh_bao.pack(fill="x", **pad)

        # ----- Ket noi -----
        khung_ketnoi = ttk.LabelFrame(self.root, text="Ket noi Serial")
        khung_ketnoi.pack(fill="x", **pad)

        ttk.Label(khung_ketnoi, text="Cong COM:").grid(row=0, column=0, padx=5, pady=5, sticky="w")
        self.combo_port = ttk.Combobox(khung_ketnoi, width=12, values=self._danh_sach_cong())
        self.combo_port.set(COM_PORT_MAC_DINH)
        self.combo_port.grid(row=0, column=1, padx=5, pady=5)

        ttk.Button(khung_ketnoi, text="Lam moi", command=self._lam_moi_cong).grid(row=0, column=2, padx=5, pady=5)

        self.btn_ketnoi = ttk.Button(khung_ketnoi, text="Ket noi", command=self._toggle_ket_noi)
        self.btn_ketnoi.grid(row=0, column=3, padx=5, pady=5)

        self.lbl_trangthai = ttk.Label(khung_ketnoi, text="Chua ket noi", foreground="red")
        self.lbl_trangthai.grid(row=0, column=4, padx=15, pady=5, sticky="w")

        ttk.Button(khung_ketnoi, text="Doc cau hinh hien tai (CFG;GET)",
                   command=lambda: self._gui_qua_serial("CFG;GET")).grid(row=0, column=5, padx=5, pady=5)

        # ----- Chan GPIO -----
        khung_chan = ttk.LabelFrame(
            self.root, text="Chan GPIO (doi xong bam Luu+Khoi dong lai o duoi cung de ap dung)")
        khung_chan.pack(fill="both", expand=False, **pad)

        for i, (ten, mo_ta) in enumerate(CAC_CHAN):
            ttk.Label(khung_chan, text=mo_ta, width=34, anchor="w").grid(
                row=i, column=0, padx=5, pady=2, sticky="w")
            ttk.Label(khung_chan, text=ten, width=16, font=("Consolas", 9), anchor="w").grid(
                row=i, column=1, padx=5, pady=2, sticky="w")
            entry = ttk.Entry(khung_chan, width=6)
            entry.grid(row=i, column=2, padx=5, pady=2)
            self.entry_chan[ten] = entry
            ttk.Button(khung_chan, text="Gui", width=6,
                       command=lambda t=ten: self._gui_1_chan(t)).grid(row=i, column=3, padx=5, pady=2)

        ttk.Button(khung_chan, text="Gui TAT CA chan o tren cung luc",
                   command=self._gui_tat_ca_chan).grid(row=len(CAC_CHAN), column=0, columnspan=4,
                                                        padx=5, pady=8, sticky="we")

        # ----- Hieu chuan -----
        khung_hc = ttk.LabelFrame(self.root, text="Hieu chuan (ap dung ngay, khong can khoi dong lai)")
        khung_hc.pack(fill="x", **pad)

        ttk.Label(khung_hc, text="So xung / vong dong co (vi buoc x buoc/vong):").grid(
            row=0, column=0, padx=5, pady=5, sticky="w")
        self.entry_microstep = ttk.Entry(khung_hc, width=10)
        self.entry_microstep.insert(0, "1600")
        self.entry_microstep.grid(row=0, column=1, padx=5, pady=5)
        ttk.Button(khung_hc, text="Gui", command=self._gui_microstep).grid(row=0, column=2, padx=5, pady=5)

        ttk.Label(khung_hc, text="mm / vong truc keo (vi du 0.2 vong=1mm => 5.0):").grid(
            row=1, column=0, padx=5, pady=5, sticky="w")
        self.entry_mmvong = ttk.Entry(khung_hc, width=10)
        self.entry_mmvong.insert(0, "5.0")
        self.entry_mmvong.grid(row=1, column=1, padx=5, pady=5)
        ttk.Button(khung_hc, text="Gui", command=self._gui_mmvong).grid(row=1, column=2, padx=5, pady=5)

        # ----- Dao chieu -----
        khung_dao = ttk.LabelFrame(self.root, text="Dao chieu truc (ap dung ngay - dung khi lap motor nguoc chieu)")
        khung_dao.pack(fill="x", **pad)

        self.bien_dao = {}
        for i, (ten_hien_thi, ma) in enumerate([
            ("Dong co KEO A", "KEOA"), ("Dong co KEO B", "KEOB"), ("Dong co XOAY", "XOAY")
        ]):
            var = tk.BooleanVar(value=False)
            self.bien_dao[ma] = var
            cb = ttk.Checkbutton(khung_dao, text=f"Dao chieu {ten_hien_thi}", variable=var,
                                  command=lambda m=ma, v=var: self._gui_dao(m, v))
            cb.grid(row=0, column=i, padx=15, pady=5, sticky="w")

        # ----- Luu / Reset / Reboot -----
        khung_luu = ttk.Frame(self.root)
        khung_luu.pack(fill="x", **pad)

        tk.Button(khung_luu, text="💾  LUU VAO FLASH (CFG;SAVE)", font=("Segoe UI", 11, "bold"),
                  bg="#5cb85c", fg="white",
                  command=lambda: self._gui_qua_serial("CFG;SAVE")).pack(side="left", fill="x", expand=True, padx=(0, 4))

        tk.Button(khung_luu, text="🔄  KHOI DONG LAI ESP32 (CFG;REBOOT)", font=("Segoe UI", 11, "bold"),
                  bg="#5bc0de", fg="white", command=self._xac_nhan_reboot).pack(side="left", fill="x", expand=True, padx=4)

        tk.Button(khung_luu, text="⚠  VE MAC DINH (CFG;RESET)", font=("Segoe UI", 11, "bold"),
                  bg="#d9534f", fg="white", command=self._xac_nhan_reset).pack(side="left", fill="x", expand=True, padx=(4, 0))

        # ----- Log -----
        khung_log = ttk.LabelFrame(self.root, text="Phan hoi tu ESP32")
        khung_log.pack(fill="both", expand=True, **pad)
        self.text_log = tk.Text(khung_log, height=10, state="disabled", bg="#111", fg="#0f0", font=("Consolas", 9))
        scroll_log = ttk.Scrollbar(khung_log, orient="vertical", command=self.text_log.yview)
        self.text_log.configure(yscrollcommand=scroll_log.set)
        self.text_log.pack(side="left", fill="both", expand=True, padx=(5, 0), pady=5)
        scroll_log.pack(side="right", fill="y", padx=(0, 5), pady=5)

    # ---------------------------------------------------------
    # KET NOI SERIAL
    # ---------------------------------------------------------
    def _danh_sach_cong(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def _lam_moi_cong(self):
        self.combo_port["values"] = self._danh_sach_cong()

    def _toggle_ket_noi(self):
        if not self.dang_ket_noi:
            self._ket_noi()
        else:
            self._ngat_ket_noi()

    def _ket_noi(self):
        cong = self.combo_port.get().strip()
        try:
            self.ser = serial.Serial(cong, BAUD_RATE, timeout=1)
            time.sleep(2)
            self.dang_ket_noi = True
            self.btn_ketnoi.config(text="Ngat ket noi")
            self.lbl_trangthai.config(text=f"Da ket noi {cong} @ {BAUD_RATE}", foreground="green")
            self._ghi_log(f"[He thong] Da ket noi toi {cong}")

            self.dang_doc = True
            threading.Thread(target=self._doc_serial_lien_tuc, daemon=True).start()
            self.root.after(300, lambda: self._gui_qua_serial("CFG;GET"))
        except Exception as loi:
            messagebox.showerror("Loi ket noi", f"Khong the ket noi toi {cong}:\n{loi}")

    def _ngat_ket_noi(self):
        self.dang_doc = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.dang_ket_noi = False
        self.btn_ketnoi.config(text="Ket noi")
        self.lbl_trangthai.config(text="Chua ket noi", foreground="red")
        self._ghi_log("[He thong] Da ngat ket noi")

    def _doc_serial_lien_tuc(self):
        while self.dang_doc and self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting:
                    dong = self.ser.readline().decode(errors="ignore").strip()
                    if dong:
                        self.root.after(0, self._xu_ly_dong_tu_esp32, dong)
            except Exception:
                break
            time.sleep(0.02)

    def _xu_ly_dong_tu_esp32(self, dong):
        self._ghi_log(f"[ESP32] {dong}")
        self._thu_doc_cfg(dong)

    def _thu_doc_cfg(self, dong):
        # Doc cac dong "CFG: ten=gia_tri ten2=gia_tri2 ..." ma cau_hinh_in_ra() gui ve
        if not dong.startswith("CFG:"):
            return
        noi_dung = dong[len("CFG:"):].strip()
        for cap in noi_dung.split():
            if "=" not in cap:
                continue
            ten, gia_tri = cap.split("=", 1)
            self._dat_gia_tri_tu_cfg(ten.strip(), gia_tri.strip())

    def _dat_gia_tri_tu_cfg(self, ten, gia_tri):
        anh_xa_chan = {
            "pul_keo_a": "PUL_KEO_A", "dir_keo_a": "DIR_KEO_A",
            "pul_keo_b": "PUL_KEO_B", "dir_keo_b": "DIR_KEO_B",
            "pul_xoay": "PUL_XOAY", "dir_xoay": "DIR_XOAY",
            "relay_plasma": "RELAY_PLASMA",
            "plc_in_start": "PLC_IN_START", "plc_in_stop": "PLC_IN_STOP",
            "plc_in_emg": "PLC_IN_EMG", "plc_in_limit": "PLC_IN_LIMIT",
            "plc_out_ready": "PLC_OUT_READY", "plc_out_running": "PLC_OUT_RUNNING",
            "plc_out_done": "PLC_OUT_DONE", "plc_out_fault": "PLC_OUT_FAULT",
        }
        if ten in anh_xa_chan:
            entry = self.entry_chan.get(anh_xa_chan[ten])
            if entry is not None:
                entry.delete(0, "end")
                entry.insert(0, gia_tri)
        elif ten == "microstep_moi_vong":
            self.entry_microstep.delete(0, "end")
            self.entry_microstep.insert(0, gia_tri)
        elif ten == "mm_moi_vong_truc_x":
            self.entry_mmvong.delete(0, "end")
            self.entry_mmvong.insert(0, gia_tri)
        elif ten in ("dao_keo_a", "dao_keo_b", "dao_xoay"):
            ma = {"dao_keo_a": "KEOA", "dao_keo_b": "KEOB", "dao_xoay": "XOAY"}[ten]
            self.bien_dao[ma].set(gia_tri == "1")

    # ---------------------------------------------------------
    # GUI LENH
    # ---------------------------------------------------------
    def _gui_qua_serial(self, chuoi_lenh):
        if not self.dang_ket_noi or not self.ser or not self.ser.is_open:
            messagebox.showwarning("Chua ket noi", "Vui long ket noi Serial truoc.")
            return False
        try:
            self.ser.write((chuoi_lenh + "\n").encode())
            self._ghi_log(f"[Gui] {chuoi_lenh}")
            return True
        except Exception as loi:
            messagebox.showerror("Loi gui lenh", str(loi))
            return False

    def _gui_1_chan(self, ten):
        gia_tri = self.entry_chan[ten].get().strip()
        if not gia_tri.isdigit():
            messagebox.showwarning("Sai du lieu", f"So GPIO cho {ten} phai la so nguyen >= 0.")
            return
        self._gui_qua_serial(f"CFG;PIN;{ten};{gia_tri}")

    def _gui_tat_ca_chan(self):
        for ten in self.entry_chan:
            gia_tri = self.entry_chan[ten].get().strip()
            if gia_tri.isdigit():
                self._gui_qua_serial(f"CFG;PIN;{ten};{gia_tri}")
                time.sleep(0.03)

    def _gui_microstep(self):
        try:
            gia_tri = float(self.entry_microstep.get())
            if gia_tri <= 0:
                raise ValueError
        except ValueError:
            messagebox.showwarning("Sai du lieu", "So xung/vong phai la so > 0.")
            return
        self._gui_qua_serial(f"CFG;CAL;MICROSTEP;{gia_tri}")

    def _gui_mmvong(self):
        try:
            gia_tri = float(self.entry_mmvong.get())
            if gia_tri <= 0:
                raise ValueError
        except ValueError:
            messagebox.showwarning("Sai du lieu", "mm/vong phai la so > 0.")
            return
        self._gui_qua_serial(f"CFG;CAL;MMVONG;{gia_tri}")

    def _gui_dao(self, ma, var):
        self._gui_qua_serial(f"CFG;DAO;{ma};{1 if var.get() else 0}")

    def _xac_nhan_reboot(self):
        if messagebox.askyesno("Xac nhan khoi dong lai",
                                "Khoi dong lai ESP32 ngay bay gio?\n\n"
                                "Chi lam dieu nay khi may DANG DUNG HAN, khong dang cat."):
            self._gui_qua_serial("CFG;REBOOT")

    def _xac_nhan_reset(self):
        if messagebox.askyesno("Xac nhan ve mac dinh",
                                "Xoa TOAN BO cau hinh da luu, ve dung chan GPIO va "
                                "hieu chuan MAC DINH cua firmware?\n\n"
                                "Hanh dong nay khong hoan tac duoc."):
            self._gui_qua_serial("CFG;RESET")

    # ---------------------------------------------------------
    # LOG
    # ---------------------------------------------------------
    def _ghi_log(self, dong_chu):
        self.text_log.config(state="normal")
        self.text_log.insert("end", dong_chu + "\n")
        self.text_log.see("end")
        self.text_log.config(state="disabled")

    def dong_ung_dung(self):
        self.dang_doc = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = SettingsApp(root)
    root.protocol("WM_DELETE_WINDOW", app.dong_ung_dung)
    root.mainloop()
