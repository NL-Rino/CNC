"""
Giao dien CAI DAT NANG CAO - chan GPIO, so xung/vong, dao chieu truc...
Yeu cau: pip install pyserial

File nay TACH RIENG khoi gcode_gui_control.pyw (file su dung hang ngay) vi
day la cai dat lien quan truc tiep PHAN CUNG / FIRMWARE - chi nguoi lap dat
may hoac sua driver moi can dung, khong dung trong luc van hanh cat hang ngay.

Bo cuc: chia THEO TAB cho de nhin va khong tran man hinh
  Tab 1 "Che do"        : chon 1 trong 3 che do lam viec + duong kinh ong
  Tab 2 "Truc & Driver" : chan PUL/DIR cua 3 dong co + relay plasma + dao chieu
  Tab 2 "Bang dieu khien tay" : EMG/LIMIT + 7 nut bam + den bao
  Tab 3 "Hieu chuan"    : so xung/vong, mm/vong truc keo

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
import queue
import time

COM_PORT_MAC_DINH = "COM3"
BAUD_RATE = 115200

# Cac nhom chan: (ten dung trong lenh CFG;PIN;<TEN>;<so>, mo ta hien thi)
NHOM_TRUC = [
    ("PUL_KEO_A", "Xung (PUL) truc keo - dong co A"),
    ("DIR_KEO_A", "Chieu (DIR) truc keo - dong co A"),
    ("PUL_KEO_B", "Xung (PUL) truc keo - dong co B"),
    ("DIR_KEO_B", "Chieu (DIR) truc keo - dong co B"),
    ("PUL_XOAY",  "Xung (PUL) truc xoay"),
    ("DIR_XOAY",  "Chieu (DIR) truc xoay"),
]

NHOM_PLASMA = [
    ("RELAY_PLASMA", "Relay bat/tat mo cat plasma"),
]

NHOM_AN_TOAN = [
    ("PLC_IN_EMG",   "EMG - dung khan cap"),
    ("PLC_IN_LIMIT", "LIMIT - cong tac h.trinh"),
]

NHOM_NUT_DI_CHUYEN = [
    ("NUT_X_TIEN",   "Nut X+  (keo ong ra)"),
    ("NUT_X_LUI",    "Nut X-  (keo ong vao)"),
    ("NUT_A_THUAN",  "Nut A+  (xoay thuan)"),
    ("NUT_A_NGHICH", "Nut A-  (xoay nghich)"),
]

NHOM_NUT_LENH = [
    ("NUT_START", "Nut START (= chay tiep)"),
    ("NUT_STOP",  "Nut STOP (= tam dung)"),
    ("NUT_NHICH", "Nut NHICH (giu de nhich)"),
]

NHOM_DEN = [
    ("DEN_SAN_SANG",  "Den SAN SANG"),
    ("DEN_DANG_CHAY", "Den DANG CHAY"),
    ("DEN_XONG",      "Den XONG"),
    ("DEN_LOI",       "Den LOI"),
]

# Anh xa ten khoa trong dong "CFG: ..." tu ESP32 -> ten chan trong GUI
ANH_XA_CHAN = {
    "pul_keo_a": "PUL_KEO_A", "dir_keo_a": "DIR_KEO_A",
    "pul_keo_b": "PUL_KEO_B", "dir_keo_b": "DIR_KEO_B",
    "pul_xoay": "PUL_XOAY", "dir_xoay": "DIR_XOAY",
    "relay_plasma": "RELAY_PLASMA",
    "plc_in_emg": "PLC_IN_EMG", "plc_in_limit": "PLC_IN_LIMIT",
    "nut_x_tien": "NUT_X_TIEN", "nut_x_lui": "NUT_X_LUI",
    "nut_a_thuan": "NUT_A_THUAN", "nut_a_nghich": "NUT_A_NGHICH",
    "nut_start": "NUT_START", "nut_stop": "NUT_STOP", "nut_nhich": "NUT_NHICH",
    "den_san_sang": "DEN_SAN_SANG", "den_dang_chay": "DEN_DANG_CHAY",
    "den_xong": "DEN_XONG", "den_loi": "DEN_LOI",
}


class SettingsApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Cai dat nang cao - May Cat Ong (CHAN GPIO / HIEU CHUAN)")
        self.root.geometry("990x730")
        self.root.minsize(975, 640)

        self.ser = None
        self.dang_ket_noi = False
        self.dang_doc = False
        self.entry_chan = {}
        self.bien_dao = {}
        # Luong nen doc serial KHONG duoc dung cham vao giao dien Tkinter - no chi
        # bo dong doc duoc vao hang doi nay, luong giao dien tu lay ra
        self.hang_doi_su_kien = queue.Queue()

        self._xay_dung_giao_dien()
        self.root.after(50, self._lay_su_kien_tu_hang_doi)

    # ---------------------------------------------------------
    # DUNG GIAO DIEN
    # ---------------------------------------------------------
    def _xay_dung_giao_dien(self):
        pad = {"padx": 8, "pady": 3}

        tk.Label(
            self.root,
            text="⚠ Chi doi cau hinh khi may DUNG HAN. Doi sai chan GPIO co the lam mat "
                 "tin hieu EMG/LIMIT hoac dieu khien nham dong co.",
            font=("Segoe UI", 8, "bold"), fg="#d9534f", justify="left", wraplength=970
        ).pack(fill="x", padx=8, pady=(6, 2))

        self._dung_khung_ket_noi(pad)

        # ----- Notebook chia tab -----
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, **pad)

        self._dung_tab_che_do()
        self._dung_tab_truc()
        self._dung_tab_plc()
        self._dung_tab_hieu_chuan()

        self._dung_khung_luu(pad)
        self._dung_khung_log(pad)

    def _dung_khung_ket_noi(self, pad):
        khung = ttk.LabelFrame(self.root, text="Ket noi Serial")
        khung.pack(fill="x", **pad)

        ttk.Label(khung, text="Cong COM:").grid(row=0, column=0, padx=5, pady=5, sticky="w")
        self.combo_port = ttk.Combobox(khung, width=10, values=self._danh_sach_cong())
        self.combo_port.set(COM_PORT_MAC_DINH)
        self.combo_port.grid(row=0, column=1, padx=5, pady=5)

        ttk.Button(khung, text="Lam moi", command=self._lam_moi_cong).grid(row=0, column=2, padx=4, pady=5)

        self.btn_ketnoi = ttk.Button(khung, text="Ket noi", command=self._toggle_ket_noi)
        self.btn_ketnoi.grid(row=0, column=3, padx=4, pady=5)

        self.lbl_trangthai = ttk.Label(khung, text="Chua ket noi", foreground="red")
        self.lbl_trangthai.grid(row=0, column=4, padx=10, pady=5, sticky="w")

        ttk.Button(khung, text="Doc cau hinh (CFG;GET)",
                   command=lambda: self._gui_qua_serial("CFG;GET")).grid(row=0, column=5, padx=5, pady=5)

    # ----- Tab: CHE DO LAM VIEC -----
    def _dung_tab_che_do(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Che do  ")

        khung = ttk.LabelFrame(tab, text="Chon che do lam viec (ap dung ngay)")
        khung.pack(fill="x", padx=8, pady=6)

        self.bien_che_do = tk.IntVar(value=1)

        cac_che_do = [
            (1, "Che do 1 - MM va DO",
             "Truc X nhap bang mm, truc A nhap bang do. F la toc do VONG/PHUT cua\n"
             "dong co. Thoi gian di chuyen lay theo truc cham nhat.\n"
             "KHONG can khai bao duong kinh ong. Day la che do goc."),
            (2, "Che do 2 - MM va DO, giu deu toc do mo cat",
             "Van nhap X bang mm va A bang do NHUNG phai khai bao duong kinh ong.\n"
             "F la toc do MO CAT LUOT TREN MAT ONG, don vi mm/phut.\n"
             "He thong quy doi goc xoay ra chieu dai cung that roi tinh thoi gian\n"
             "theo quang duong that => 2 truc phoi hop dung ty le, mo cat luot voi\n"
             "toc do khong doi du duong cat cheo bao nhieu."),
            (3, "Che do 3 - FULL MM",
             "Nhu che do 2 nhung truc A cung nhap bang MM (chieu dai cung tren mat\n"
             "ong, kieu 'trai phang'), khong phai do. Hop voi file CAM xuat ra dang\n"
             "trai phang. Cung can khai bao duong kinh ong."),
        ]
        for gia_tri, ten, mo_ta in cac_che_do:
            ttk.Radiobutton(khung, text=ten, value=gia_tri, variable=self.bien_che_do,
                            command=self._gui_che_do).pack(anchor="w", padx=8, pady=(6, 0))
            ttk.Label(khung, text=mo_ta, foreground="#666", justify="left",
                      font=("Segoe UI", 8)).pack(anchor="w", padx=30, pady=(0, 2))

        khung_dk = ttk.LabelFrame(tab, text="Duong kinh ong (bat buoc cho che do 2 va 3)")
        khung_dk.pack(fill="x", padx=8, pady=6)

        ttk.Label(khung_dk, text="Duong kinh ngoai ong (mm):").grid(
            row=0, column=0, padx=8, pady=8, sticky="w")
        self.entry_duong_kinh = ttk.Entry(khung_dk, width=10)
        self.entry_duong_kinh.insert(0, "60")
        self.entry_duong_kinh.grid(row=0, column=1, padx=6, pady=8)
        ttk.Button(khung_dk, text="Gui", width=8,
                   command=self._gui_duong_kinh).grid(row=0, column=2, padx=6, pady=8)
        self.lbl_chu_vi = ttk.Label(khung_dk, text="", foreground="#666")
        self.lbl_chu_vi.grid(row=0, column=3, padx=12, pady=8, sticky="w")
        self._cap_nhat_chu_vi()
        self.entry_duong_kinh.bind("<KeyRelease>", lambda e: self._cap_nhat_chu_vi())

    def _cap_nhat_chu_vi(self):
        try:
            d = float(self.entry_duong_kinh.get())
            if d <= 0:
                raise ValueError
            import math
            self.lbl_chu_vi.config(
                text=f"chu vi = {math.pi * d:.2f} mm   (1 do = {math.pi * d / 360:.4f} mm cung)")
        except ValueError:
            self.lbl_chu_vi.config(text="")

    def _gui_che_do(self):
        self._gui_qua_serial(f"CFG;MODE;{self.bien_che_do.get()}")

    def _gui_duong_kinh(self):
        try:
            gia_tri = float(self.entry_duong_kinh.get())
            if gia_tri <= 0:
                raise ValueError
        except ValueError:
            messagebox.showwarning("Sai du lieu", "Duong kinh ong phai la so > 0.")
            return
        self._gui_qua_serial(f"CFG;DUONGKINH;{gia_tri}")
        self._cap_nhat_chu_vi()

    # ----- Tab 1: truc dong co -----
    def _dung_tab_truc(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Truc & Driver  ")

        khung_chan = ttk.LabelFrame(tab, text="Chan xung / chieu cua 3 dong co")
        khung_chan.pack(fill="x", padx=8, pady=6)
        self._them_nhom_2_cot(khung_chan, NHOM_TRUC)

        khung_plasma = ttk.LabelFrame(tab, text="Mo cat plasma")
        khung_plasma.pack(fill="x", padx=8, pady=6)
        for i, (ten, mo_ta) in enumerate(NHOM_PLASMA):
            self._them_hang_chan(khung_plasma, i, ten, mo_ta)

        khung_dao = ttk.LabelFrame(tab, text="Dao chieu truc (ap dung ngay - dung khi lap motor nguoc chieu)")
        khung_dao.pack(fill="x", padx=8, pady=6)
        for i, (ten_hien_thi, ma) in enumerate([
            ("dong co KEO A", "KEOA"), ("dong co KEO B", "KEOB"), ("dong co XOAY", "XOAY")
        ]):
            var = tk.BooleanVar(value=False)
            self.bien_dao[ma] = var
            ttk.Checkbutton(khung_dao, text=f"Dao chieu {ten_hien_thi}", variable=var,
                            command=lambda m=ma, v=var: self._gui_dao(m, v)
                            ).grid(row=0, column=i, padx=12, pady=6, sticky="w")

        ttk.Button(tab, text="Gui tat ca chan trong tab nay",
                   command=lambda: self._gui_nhom_chan(NHOM_TRUC + NHOM_PLASMA)
                   ).pack(fill="x", padx=8, pady=(2, 8))

    # ----- Tab 2: PLC I/O -----
    def _dung_tab_plc(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Bang dieu khien tay  ")

        khung_at = ttk.LabelFrame(tab, text="Ngo vao AN TOAN")
        khung_at.pack(fill="x", padx=8, pady=4)
        self._them_nhom_2_cot(khung_at, NHOM_AN_TOAN)

        khung_dc = ttk.LabelFrame(tab, text="4 nut di chuyen (GIU la chay lien tuc)")
        khung_dc.pack(fill="x", padx=8, pady=4)
        self._them_nhom_2_cot(khung_dc, NHOM_NUT_DI_CHUYEN)

        khung_l = ttk.LabelFrame(tab, text="Nut lenh")
        khung_l.pack(fill="x", padx=8, pady=4)
        self._them_nhom_2_cot(khung_l, NHOM_NUT_LENH)

        khung_d = ttk.LabelFrame(tab, text="Den bao (dat -1 de TAT - ESP32 het chan nen mac dinh TAT)")
        khung_d.pack(fill="x", padx=8, pady=4)
        self._them_nhom_2_cot(khung_d, NHOM_DEN)

        ttk.Button(tab, text="Gui tat ca chan trong tab nay",
                   command=lambda: self._gui_nhom_chan(
                       NHOM_AN_TOAN + NHOM_NUT_DI_CHUYEN + NHOM_NUT_LENH + NHOM_DEN)
                   ).pack(fill="x", padx=8, pady=(2, 8))

    # ----- Tab 3: hieu chuan -----
    def _dung_tab_hieu_chuan(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Hieu chuan  ")

        khung = ttk.LabelFrame(tab, text="Hieu chuan (ap dung ngay, khong can khoi dong lai)")
        khung.pack(fill="x", padx=8, pady=6)

        ttk.Label(khung, text="So xung / vong dong co:").grid(row=0, column=0, padx=6, pady=8, sticky="w")
        self.entry_microstep = ttk.Entry(khung, width=10)
        self.entry_microstep.insert(0, "1600")
        self.entry_microstep.grid(row=0, column=1, padx=6, pady=8)
        ttk.Button(khung, text="Gui", width=8, command=self._gui_microstep).grid(row=0, column=2, padx=6, pady=8)
        ttk.Label(khung, text="= vi buoc driver x so buoc/vong dong co\n   (vi du 1/8 x 200 = 1600)",
                  foreground="#666").grid(row=0, column=3, padx=6, pady=8, sticky="w")

        ttk.Label(khung, text="mm / vong truc keo:").grid(row=1, column=0, padx=6, pady=8, sticky="w")
        self.entry_mmvong = ttk.Entry(khung, width=10)
        self.entry_mmvong.insert(0, "5.0")
        self.entry_mmvong.grid(row=1, column=1, padx=6, pady=8)
        ttk.Button(khung, text="Gui", width=8, command=self._gui_mmvong).grid(row=1, column=2, padx=6, pady=8)
        ttk.Label(khung, text="quay 1 vong thi ong di duoc bao nhieu mm\n   (vi du 0.2 vong = 1mm  =>  5.0)",
                  foreground="#666").grid(row=1, column=3, padx=6, pady=8, sticky="w")

        khung_tay = ttk.LabelFrame(tab, text="Bang dieu khien tay (ap dung ngay)")
        khung_tay.pack(fill="x", padx=8, pady=6)

        ttk.Label(khung_tay, text="Toc do khi GIU nut di chuyen (RPM):").grid(
            row=0, column=0, padx=6, pady=5, sticky="w")
        self.entry_toc_do_tay = ttk.Entry(khung_tay, width=10)
        self.entry_toc_do_tay.insert(0, "30")
        self.entry_toc_do_tay.grid(row=0, column=1, padx=6, pady=5)
        ttk.Button(khung_tay, text="Gui", width=8,
                   command=lambda: self._gui_so("CFG;TAY;TOCDO", self.entry_toc_do_tay)
                   ).grid(row=0, column=2, padx=6, pady=5)

        ttk.Label(khung_tay, text="Nhich truc X moi lan bam (mm):").grid(
            row=1, column=0, padx=6, pady=5, sticky="w")
        self.entry_nhich_mm = ttk.Entry(khung_tay, width=10)
        self.entry_nhich_mm.insert(0, "1.0")
        self.entry_nhich_mm.grid(row=1, column=1, padx=6, pady=5)
        ttk.Button(khung_tay, text="Gui", width=8,
                   command=lambda: self._gui_so("CFG;TAY;NHICHMM", self.entry_nhich_mm)
                   ).grid(row=1, column=2, padx=6, pady=5)

        ttk.Label(khung_tay, text="Nhich truc A moi lan bam (do):").grid(
            row=2, column=0, padx=6, pady=5, sticky="w")
        self.entry_nhich_do = ttk.Entry(khung_tay, width=10)
        self.entry_nhich_do.insert(0, "1.0")
        self.entry_nhich_do.grid(row=2, column=1, padx=6, pady=5)
        ttk.Button(khung_tay, text="Gui", width=8,
                   command=lambda: self._gui_so("CFG;TAY;NHICHDO", self.entry_nhich_do)
                   ).grid(row=2, column=2, padx=6, pady=5)

        khung_ramp = ttk.LabelFrame(tab, text="Tang toc khi CAT (ap dung ngay)")
        khung_ramp.pack(fill="x", padx=8, pady=6)

        self.bien_ramp_cat = tk.BooleanVar(value=False)
        ttk.Checkbutton(
            khung_ramp,
            text="Cho doan CAT tang toc dan (mac dinh TAT)",
            variable=self.bien_ramp_cat, command=self._gui_ramp_cat
        ).pack(anchor="w", padx=8, pady=(6, 2))

        ttk.Label(
            khung_ramp,
            text="TAT (khuyen dung): doan cat chay dung toc do ngay tu xung dau, mep cat dep.\n"
                 "BAT: chi khi dong co bi RU / MAT BUOC luc vao cat (doi lai mep dau xau hon).",
            justify="left", foreground="#666"
        ).pack(anchor="w", padx=8, pady=(0, 8))

        ttk.Label(
            tab,
            text="Kiem tra: ZERO -> JOG X 100mm -> do thuoc. Neu lech, sua 'mm / vong truc keo':\n"
                 "     gia_tri_moi = gia_tri_cu x (quang duong that / 100)",
            justify="left", foreground="#444"
        ).pack(anchor="w", padx=14, pady=10)

    def _dung_khung_luu(self, pad):
        khung = ttk.Frame(self.root)
        khung.pack(fill="x", **pad)

        tk.Button(khung, text="💾  LUU VAO FLASH", font=("Segoe UI", 10, "bold"),
                  bg="#5cb85c", fg="white",
                  command=lambda: self._gui_qua_serial("CFG;SAVE")
                  ).pack(side="left", fill="x", expand=True, padx=(0, 4))

        tk.Button(khung, text="🔄  KHOI DONG LAI ESP32", font=("Segoe UI", 10, "bold"),
                  bg="#5bc0de", fg="white", command=self._xac_nhan_reboot
                  ).pack(side="left", fill="x", expand=True, padx=4)

        tk.Button(khung, text="⚠  VE MAC DINH", font=("Segoe UI", 10, "bold"),
                  bg="#d9534f", fg="white", command=self._xac_nhan_reset
                  ).pack(side="left", fill="x", expand=True, padx=(4, 0))

    def _dung_khung_log(self, pad):
        khung = ttk.LabelFrame(self.root, text="Phan hoi tu ESP32")
        khung.pack(fill="both", expand=True, **pad)
        self.text_log = tk.Text(khung, height=6, state="disabled", bg="#111", fg="#0f0", font=("Consolas", 9))
        scroll = ttk.Scrollbar(khung, orient="vertical", command=self.text_log.yview)
        self.text_log.configure(yscrollcommand=scroll.set)
        self.text_log.pack(side="left", fill="both", expand=True, padx=(5, 0), pady=5)
        scroll.pack(side="right", fill="y", padx=(0, 5), pady=5)

    def _them_hang_chan(self, parent, hang, ten, mo_ta, cot=0):
        """Them 1 dong: mo ta | TEN_CHAN | o nhap GPIO | nut Gui.

        cot=0 dat o nua trai, cot=1 dat o nua phai (de xep 2 cot cho gon).
        """
        c = cot * 4
        ttk.Label(parent, text=mo_ta, width=27, anchor="w").grid(
            row=hang, column=c, padx=(6, 2), pady=2, sticky="w")
        ttk.Label(parent, text=ten, width=14, font=("Consolas", 8),
                  foreground="#666", anchor="w").grid(
            row=hang, column=c + 1, padx=2, pady=2, sticky="w")
        entry = ttk.Entry(parent, width=5)
        entry.grid(row=hang, column=c + 2, padx=2, pady=2)
        self.entry_chan[ten] = entry
        ttk.Button(parent, text="Gui", width=5,
                   command=lambda t=ten: self._gui_1_chan(t)).grid(
            row=hang, column=c + 3, padx=(2, 8), pady=2)

    def _them_nhom_2_cot(self, parent, danh_sach):
        """Xep danh sach chan thanh 2 cot cho do cao cua so."""
        nua = (len(danh_sach) + 1) // 2
        for i, (ten, mo_ta) in enumerate(danh_sach):
            if i < nua:
                self._them_hang_chan(parent, i, ten, mo_ta, cot=0)
            else:
                self._them_hang_chan(parent, i - nua, ten, mo_ta, cot=1)

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
            self.lbl_trangthai.config(text=f"Da ket noi {cong}", foreground="green")
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
                        self.hang_doi_su_kien.put(dong)
            except Exception:
                break
            time.sleep(0.02)

    def _lay_su_kien_tu_hang_doi(self):
        """Chay o LUONG GIAO DIEN - lay cac dong luong nen doc duoc va hien thi."""
        try:
            while True:
                self._xu_ly_dong_tu_esp32(self.hang_doi_su_kien.get_nowait())
        except queue.Empty:
            pass
        self.root.after(50, self._lay_su_kien_tu_hang_doi)

    def _xu_ly_dong_tu_esp32(self, dong):
        self._ghi_log(f"[ESP32] {dong}")
        self._thu_doc_cfg(dong)

    def _thu_doc_cfg(self, dong):
        # Doc cac dong "CFG: ten=gia_tri ten2=gia_tri2 ..." ma cau_hinh_in_ra() gui ve
        if not dong.startswith("CFG:"):
            return
        for cap in dong[len("CFG:"):].strip().split():
            if "=" in cap:
                ten, gia_tri = cap.split("=", 1)
                self._dat_gia_tri_tu_cfg(ten.strip(), gia_tri.strip())

    def _dat_gia_tri_tu_cfg(self, ten, gia_tri):
        if ten in ANH_XA_CHAN:
            entry = self.entry_chan.get(ANH_XA_CHAN[ten])
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
        elif ten == "ramp_khi_cat":
            self.bien_ramp_cat.set(gia_tri == "1")
        elif ten == "che_do":
            try:
                self.bien_che_do.set(int(gia_tri))
            except ValueError:
                pass
        elif ten == "duong_kinh_ong":
            self.entry_duong_kinh.delete(0, "end")
            self.entry_duong_kinh.insert(0, gia_tri)
            self._cap_nhat_chu_vi()
        elif ten == "toc_do_tay_rpm":
            self.entry_toc_do_tay.delete(0, "end"); self.entry_toc_do_tay.insert(0, gia_tri)
        elif ten == "nhich_mm":
            self.entry_nhich_mm.delete(0, "end"); self.entry_nhich_mm.insert(0, gia_tri)
        elif ten == "nhich_do":
            self.entry_nhich_do.delete(0, "end"); self.entry_nhich_do.insert(0, gia_tri)

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

    @staticmethod
    def _la_so_chan(chuoi):
        """Chan hop le: 0..39, hoac -1 de TAT (khong lap thiet bi do)."""
        try:
            so = int(chuoi)
        except ValueError:
            return False
        return so == -1 or 0 <= so <= 39

    def _gui_1_chan(self, ten):
        gia_tri = self.entry_chan[ten].get().strip()
        if not self._la_so_chan(gia_tri):
            messagebox.showwarning("Sai du lieu",
                                   f"So GPIO cho {ten} phai trong khoang 0-39, "
                                   f"hoac -1 de TAT.")
            return
        self._gui_qua_serial(f"CFG;PIN;{ten};{gia_tri}")

    def _gui_nhom_chan(self, nhom):
        for ten, _ in nhom:
            gia_tri = self.entry_chan[ten].get().strip()
            if self._la_so_chan(gia_tri):
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

    def _gui_so(self, lenh, o_nhap):
        """Gui 1 thong so dang so qua Serial, kiem tra hop le truoc."""
        try:
            gia_tri = float(o_nhap.get())
            if gia_tri <= 0:
                raise ValueError
        except ValueError:
            messagebox.showwarning("Sai du lieu", "Gia tri phai la so > 0.")
            return
        self._gui_qua_serial(f"{lenh};{gia_tri}")

    def _gui_ramp_cat(self):
        self._gui_qua_serial(f"CFG;RAMP;CAT;{1 if self.bien_ramp_cat.get() else 0}")

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
