"""
Giao dien SU DUNG HANG NGAY - dieu khien may cat ong bang G-code qua USB COM.
Yeu cau: pip install pyserial

Go G-code tu do hoac MO FILE .nc/.gcode co san, bam "NAP & CHAY" de gui toan bo
(PROG;BEGIN...PROG;END...RUN) mot lan xuong ESP32.

File nay CHI danh cho van hanh hang ngay (nap G-code, jog, chay/dung).
Cai dat nang cao (chan GPIO, so xung/vong, dao chieu...) nam o file rieng
"cnc_settings.pyw" vi lien quan truc tiep den firmware/phan cung.
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import threading
import time
import os

COM_PORT_MAC_DINH = "COM3"
BAUD_RATE = 115200

VI_DU_GCODE = """(VI DU: cat vat quanh ong - 2 truc chay DONG THOI)
G21              ; don vi mm
G90              ; toa do tuyet doi
G92 X0 A0        ; dat goc tai vi tri hien tai

G0 X100 F60      ; chay nhanh toi vi tri cat
M3               ; bat mo cat plasma
G4 P0.8          ; pierce delay - cho moi xuyen thung ong

(Duong cat cheo: X va A chay dong thoi -> vat goc)
G1 X110 A90 F15  ; vua keo 10mm vua xoay 90 do
G1 X100 A180 F15
G1 X110 A270 F15
G1 X100 A360 F15

M5               ; tat mo cat
G0 X0 A0 F60     ; ve goc (ca 2 truc cung luc)
M30              ; ket thuc
"""

CAC_DUOI_FILE_GCODE = [
    ("File G-code", "*.nc *.gcode *.tap *.txt"),
    ("Tat ca file", "*.*"),
]

# Mau trang thai may
MAU_TRANG_THAI = {
    "CHUA_KETNOI": ("Chua ket noi", "#888"),
    "SAN_SANG":    ("SAN SANG", "#28a745"),
    "DANG_CHAY":   ("DANG CHAY...", "#007bff"),
    "TAM_DUNG":    ("TAM DUNG", "#f0ad4e"),
    "LOI":         ("LOI / DUNG KHAN CAP", "#d9534f"),
    "DA_DUNG":     ("DA DUNG", "#6c757d"),
}


class GCodeApp:
    def __init__(self, root):
        self.root = root
        self.root.geometry("900x920")
        self.root.minsize(800, 720)

        self.ser = None
        self.dang_ket_noi = False
        self.dang_doc = False
        self.trang_thai = "CHUA_KETNOI"
        self.file_hien_tai = None
        self.da_thay_doi = False

        self._xay_dung_giao_dien()
        self._cap_nhat_tieu_de()
        self._cap_nhat_trang_thai("CHUA_KETNOI")

    # ---------------------------------------------------------
    def _xay_dung_giao_dien(self):
        pad = {"padx": 8, "pady": 6}

        # ----- Menu File -----
        thanh_menu = tk.Menu(self.root)
        menu_file = tk.Menu(thanh_menu, tearoff=0)
        menu_file.add_command(label="Mo file G-code...     Ctrl+O", command=self._mo_file)
        menu_file.add_command(label="Luu                    Ctrl+S", command=self._luu_file)
        menu_file.add_command(label="Luu thanh...       Ctrl+Shift+S", command=self._luu_file_thanh)
        menu_file.add_separator()
        menu_file.add_command(label="Thoat", command=self.dong_ung_dung)
        thanh_menu.add_cascade(label="File", menu=menu_file)
        self.root.config(menu=thanh_menu)
        self.root.bind_all("<Control-o>", lambda e: self._mo_file())
        self.root.bind_all("<Control-s>", lambda e: self._luu_file())
        self.root.bind_all("<Control-Shift-S>", lambda e: self._luu_file_thanh())
        self.root.bind_all("<Escape>", lambda e: self._gui_stop())

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

        # ----- Trang thai may (noi bat, mau theo trang thai) -----
        khung_trangthai = ttk.LabelFrame(self.root, text="Trang thai may")
        khung_trangthai.pack(fill="x", **pad)
        self.lbl_trangthai = tk.Label(khung_trangthai, text="Chua ket noi",
                                       font=("Segoe UI", 14, "bold"), fg="white", bg="#888")
        self.lbl_trangthai.pack(fill="x", padx=8, pady=6)

        # ----- Vi tri hien tai -----
        khung_vitri = ttk.LabelFrame(self.root, text="Vi tri hien tai (X = KEO tinh bang mm, A = XOAY tinh bang do)")
        khung_vitri.pack(fill="x", **pad)
        self.lbl_vitri = ttk.Label(khung_vitri, text="X = 0.00 mm    A = 0.00 do",
                                   font=("Segoe UI", 13, "bold"))
        self.lbl_vitri.pack(padx=10, pady=8, anchor="w")

        # ----- O soan thao G-code -----
        khung_gcode = ttk.LabelFrame(self.root, text="Chuong trinh G-code (go truc tiep hoac File > Mo file)")
        khung_gcode.pack(fill="both", expand=True, **pad)

        khung_text = ttk.Frame(khung_gcode)
        khung_text.pack(fill="both", expand=True, padx=5, pady=5)

        self.text_gcode = tk.Text(khung_text, height=14, font=("Consolas", 11), wrap="none", undo=True)
        self.text_gcode.insert("1.0", VI_DU_GCODE)
        self.text_gcode.edit_modified(False)
        self.text_gcode.bind("<<Modified>>", self._doi_gcode)
        scroll_y = ttk.Scrollbar(khung_text, orient="vertical", command=self.text_gcode.yview)
        self.text_gcode.configure(yscrollcommand=scroll_y.set)
        self.text_gcode.pack(side="left", fill="both", expand=True)
        scroll_y.pack(side="right", fill="y")

        self.text_gcode.tag_configure("dong_loi", background="#5c1a1a")

        # ----- Dieu khien thu cong (JOG) -----
        khung_jog = ttk.LabelFrame(self.root, text="Dieu khien thu cong (dua mo cat toi vi tri bat dau)")
        khung_jog.pack(fill="x", **pad)

        khung_jog_ts = ttk.Frame(khung_jog)
        khung_jog_ts.pack(fill="x", padx=5, pady=(5, 0))

        ttk.Label(khung_jog_ts, text="Buoc X (mm):").pack(side="left", padx=(0, 3))
        self.entry_jog_x = ttk.Entry(khung_jog_ts, width=7)
        self.entry_jog_x.insert(0, "10")
        self.entry_jog_x.pack(side="left", padx=(0, 12))

        ttk.Label(khung_jog_ts, text="Buoc A (do):").pack(side="left", padx=(0, 3))
        self.entry_jog_y = ttk.Entry(khung_jog_ts, width=7)
        self.entry_jog_y.insert(0, "15")
        self.entry_jog_y.pack(side="left", padx=(0, 12))

        ttk.Label(khung_jog_ts, text="Toc do (RPM):").pack(side="left", padx=(0, 3))
        self.entry_jog_rpm = ttk.Entry(khung_jog_ts, width=7)
        self.entry_jog_rpm.insert(0, "30")
        self.entry_jog_rpm.pack(side="left")

        khung_jog_nut = ttk.Frame(khung_jog)
        khung_jog_nut.pack(fill="x", padx=5, pady=5)

        self.nut_jog = []
        for text, truc, dau in [("◀ X-", "X", -1), ("X+ ▶", "X", 1)]:
            b = tk.Button(khung_jog_nut, text=text, font=("Segoe UI", 10, "bold"), width=8,
                          command=lambda t=truc, d=dau: self._jog(t, d))
            b.pack(side="left", padx=3)
            self.nut_jog.append(b)

        ttk.Separator(khung_jog_nut, orient="vertical").pack(side="left", fill="y", padx=10)

        for text, truc, dau in [("↺ A-", "A", -1), ("A+ ↻", "A", 1)]:
            b = tk.Button(khung_jog_nut, text=text, font=("Segoe UI", 10, "bold"), width=8,
                          command=lambda t=truc, d=dau: self._jog(t, d))
            b.pack(side="left", padx=3)
            self.nut_jog.append(b)

        self.btn_zero = tk.Button(khung_jog_nut, text="🎯  DAT GOC 0 TAI DAY (ZERO)", font=("Segoe UI", 10, "bold"),
                                   bg="#6f42c1", fg="white", command=self._dat_goc)
        self.btn_zero.pack(side="left", fill="x", expand=True, padx=(15, 0))

        # ----- Nut dieu khien chuong trinh -----
        khung_nut = ttk.Frame(self.root)
        khung_nut.pack(fill="x", **pad)

        self.btn_run = tk.Button(
            khung_nut, text="▶  NAP & CHAY", font=("Segoe UI", 12, "bold"),
            bg="#5cb85c", fg="white", command=self._nap_va_chay
        )
        self.btn_run.pack(side="left", fill="x", expand=True, padx=(0, 4))

        self.btn_pause = tk.Button(
            khung_nut, text="⏸  PAUSE", font=("Segoe UI", 12, "bold"),
            bg="#f0ad4e", fg="white", command=lambda: self._gui_qua_serial("PAUSE")
        )
        self.btn_pause.pack(side="left", fill="x", expand=True, padx=4)

        self.btn_resume = tk.Button(
            khung_nut, text="⏵  RESUME", font=("Segoe UI", 12, "bold"),
            bg="#5bc0de", fg="white", command=lambda: self._gui_qua_serial("RESUME")
        )
        self.btn_resume.pack(side="left", fill="x", expand=True, padx=4)

        self.btn_stop = tk.Button(
            khung_nut, text="⛔  STOP (Esc)", font=("Segoe UI", 12, "bold"),
            bg="#d9534f", fg="white", command=self._gui_stop
        )
        self.btn_stop.pack(side="left", fill="x", expand=True, padx=(4, 0))

        # ----- Lenh don nhanh (test 1 dong G-code, khong can Run ca chuong trinh) -----
        khung_don = ttk.LabelFrame(self.root, text="Gui 1 dong lenh nhanh (test rieng le)")
        khung_don.pack(fill="x", **pad)
        self.entry_lenh_don = ttk.Entry(khung_don, font=("Consolas", 10))
        self.entry_lenh_don.pack(side="left", fill="x", expand=True, padx=(5, 5), pady=5)
        self.entry_lenh_don.bind("<Return>", lambda e: self._gui_lenh_don())
        ttk.Button(khung_don, text="Gui", command=self._gui_lenh_don).pack(side="left", padx=(0, 5), pady=5)

        # ----- Log -----
        khung_log = ttk.LabelFrame(self.root, text="Nhat ky (log) tu ESP32")
        khung_log.pack(fill="both", expand=True, **pad)
        self.text_log = tk.Text(khung_log, height=8, state="disabled", bg="#111", fg="#0f0", font=("Consolas", 9))
        self.text_log.tag_configure("loi", foreground="#ff5555")
        self.text_log.tag_configure("ok", foreground="#55ff7f")
        self.text_log.tag_configure("he_thong", foreground="#aaaaff")
        scroll_log = ttk.Scrollbar(khung_log, orient="vertical", command=self.text_log.yview)
        self.text_log.configure(yscrollcommand=scroll_log.set)
        self.text_log.pack(side="left", fill="both", expand=True, padx=(5, 0), pady=5)
        scroll_log.pack(side="right", fill="y", padx=(0, 5), pady=5)

        self._cap_nhat_nut_theo_trang_thai()

    # ---------------------------------------------------------
    # FILE G-CODE (Mo / Luu)
    # ---------------------------------------------------------
    def _mo_file(self):
        if self.da_thay_doi:
            tra_loi = messagebox.askyesnocancel(
                "Chua luu", "Noi dung G-code hien tai chua duoc luu.\nLuu truoc khi mo file khac?")
            if tra_loi is None:
                return
            if tra_loi and not self._luu_file():
                return

        duong_dan = filedialog.askopenfilename(title="Mo file G-code", filetypes=CAC_DUOI_FILE_GCODE)
        if not duong_dan:
            return
        try:
            with open(duong_dan, "r", encoding="utf-8", errors="ignore") as f:
                noi_dung = f.read()
        except Exception as loi:
            messagebox.showerror("Loi mo file", str(loi))
            return

        self.text_gcode.delete("1.0", "end")
        self.text_gcode.insert("1.0", noi_dung)
        self.text_gcode.edit_modified(False)
        self.da_thay_doi = False
        self.file_hien_tai = duong_dan
        self._cap_nhat_tieu_de()
        self._ghi_log(f"[He thong] Da mo file: {duong_dan}", "he_thong")

    def _luu_file(self):
        if not self.file_hien_tai:
            return self._luu_file_thanh()
        try:
            with open(self.file_hien_tai, "w", encoding="utf-8") as f:
                f.write(self.text_gcode.get("1.0", "end-1c"))
        except Exception as loi:
            messagebox.showerror("Loi luu file", str(loi))
            return False
        self.da_thay_doi = False
        self.text_gcode.edit_modified(False)
        self._cap_nhat_tieu_de()
        self._ghi_log(f"[He thong] Da luu file: {self.file_hien_tai}", "he_thong")
        return True

    def _luu_file_thanh(self):
        duong_dan = filedialog.asksaveasfilename(
            title="Luu file G-code thanh...", defaultextension=".nc", filetypes=CAC_DUOI_FILE_GCODE)
        if not duong_dan:
            return False
        self.file_hien_tai = duong_dan
        return self._luu_file()

    def _doi_gcode(self, event=None):
        if self.text_gcode.edit_modified():
            self.da_thay_doi = True
            self._cap_nhat_tieu_de()
            self.text_gcode.edit_modified(False)

    def _cap_nhat_tieu_de(self):
        ten_file = os.path.basename(self.file_hien_tai) if self.file_hien_tai else "chua luu"
        dau_sao = " *" if self.da_thay_doi else ""
        self.root.title(f"Dieu khien May Cat Ong - G-code  [{ten_file}{dau_sao}]")

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
            time.sleep(2)  # cho ESP32 khoi dong lai sau khi mo cong Serial
            self.dang_ket_noi = True
            self.btn_ketnoi.config(text="Ngat ket noi")
            self._ghi_log(f"[He thong] Da ket noi toi {cong}", "he_thong")
            self._cap_nhat_trang_thai("SAN_SANG")

            self.dang_doc = True
            threading.Thread(target=self._doc_serial_lien_tuc, daemon=True).start()
        except Exception as loi:
            messagebox.showerror("Loi ket noi", f"Khong the ket noi toi {cong}:\n{loi}")

    def _ngat_ket_noi(self):
        self.dang_doc = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.dang_ket_noi = False
        self.btn_ketnoi.config(text="Ket noi")
        self._ghi_log("[He thong] Da ngat ket noi", "he_thong")
        self._cap_nhat_trang_thai("CHUA_KETNOI")

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
        the = "loi" if dong.startswith("Loi:") else ("ok" if any(
            dong.startswith(tien_to) for tien_to in
            ("OK", "RUNNING", "ZEROED", "RESUMED", "Hoan thanh", "PLASMA_ON", "PLASMA_OFF")
        ) else None)
        self._ghi_log(f"[ESP32] {dong}", the)
        self._thu_cap_nhat_vi_tri(dong)
        self._thu_cap_nhat_trang_thai_tu_log(dong)
        self._thu_to_mau_dong_loi(dong)

    def _thu_to_mau_dong_loi(self, dong):
        # Bat cac dong bao loi co dang: Loi: ... dong 'G1 X...' -> to mau dong tuong ung trong o soan thao
        if not dong.startswith("Loi:") or "dong '" not in dong:
            return
        try:
            noi_dung_loi = dong.split("dong '", 1)[1].rsplit("'", 1)[0]
        except IndexError:
            return
        if not noi_dung_loi.strip():
            return
        vi_tri = self.text_gcode.search(noi_dung_loi, "1.0", stopindex="end")
        if vi_tri:
            dong_ket_thuc = f"{vi_tri}+{len(noi_dung_loi)}c"
            self.text_gcode.tag_add("dong_loi", vi_tri, dong_ket_thuc)
            self.text_gcode.see(vi_tri)

    def _thu_cap_nhat_vi_tri(self, dong):
        # Bat cac dong dang "...Vi tri: X=90.00 Y=49.95"
        if "Vi tri: X=" in dong and "A=" in dong:
            try:
                phan = dong.split("Vi tri: X=")[1]
                x_str, y_str = phan.split("A=")
                x_val = float(x_str.strip())
                y_val = float(y_str.strip())
                self.lbl_vitri.config(text=f"X = {x_val:.2f} mm    A = {y_val:.2f} do")
            except (IndexError, ValueError):
                pass

    def _thu_cap_nhat_trang_thai_tu_log(self, dong):
        if dong.startswith("Loi: DUNG KHAN CAP"):
            self._cap_nhat_trang_thai("LOI")
        elif dong.startswith("RUNNING"):
            self._cap_nhat_trang_thai("DANG_CHAY")
        elif dong.startswith("PAUSED") or dong.startswith("M0_PAUSED"):
            self._cap_nhat_trang_thai("TAM_DUNG")
        elif dong.startswith("RESUMED"):
            self._cap_nhat_trang_thai("DANG_CHAY")
        elif dong.startswith("STOPPED") or dong.startswith("Da dung han"):
            self._cap_nhat_trang_thai("DA_DUNG")
        elif dong.startswith("Hoan thanh"):
            self._cap_nhat_trang_thai("SAN_SANG")
        elif dong.startswith("He thong: da het dieu kien loi"):
            self._cap_nhat_trang_thai("SAN_SANG")

    # ---------------------------------------------------------
    # TRANG THAI MAY
    # ---------------------------------------------------------
    def _cap_nhat_trang_thai(self, ma):
        self.trang_thai = ma
        nhan, mau = MAU_TRANG_THAI[ma]
        self.lbl_trangthai.config(text=nhan, bg=mau)
        self._cap_nhat_nut_theo_trang_thai()

    def _cap_nhat_nut_theo_trang_thai(self):
        dang_chay = self.trang_thai == "DANG_CHAY"
        tam_dung = self.trang_thai == "TAM_DUNG"
        loi = self.trang_thai == "LOI"
        san_sang_jog = self.dang_ket_noi and not dang_chay and not tam_dung and not loi

        self.btn_run.config(state="normal" if (self.dang_ket_noi and not dang_chay and not loi) else "disabled")
        self.btn_pause.config(state="normal" if dang_chay else "disabled")
        self.btn_resume.config(state="normal" if tam_dung else "disabled")
        self.btn_stop.config(state="normal" if self.dang_ket_noi else "disabled")
        self.btn_zero.config(state="normal" if san_sang_jog else "disabled")
        for b in self.nut_jog:
            b.config(state="normal" if san_sang_jog else "disabled")

    # ---------------------------------------------------------
    # GUI LENH
    # ---------------------------------------------------------
    def _gui_qua_serial(self, chuoi_lenh, ghi_log=True):
        if not self.dang_ket_noi or not self.ser or not self.ser.is_open:
            messagebox.showwarning("Chua ket noi", "Vui long ket noi Serial truoc.")
            return False
        try:
            self.ser.write((chuoi_lenh + "\n").encode())
            if ghi_log:
                self._ghi_log(f"[Gui] {chuoi_lenh}")
            return True
        except Exception as loi:
            messagebox.showerror("Loi gui lenh", str(loi))
            return False

    def _nap_va_chay(self):
        if not self.dang_ket_noi:
            messagebox.showwarning("Chua ket noi", "Vui long ket noi Serial truoc.")
            return

        noi_dung = self.text_gcode.get("1.0", "end")
        cac_dong = [d for d in noi_dung.splitlines() if d.strip() != ""]

        if not cac_dong:
            messagebox.showwarning("Trong", "Chua co dong G-code nao de chay.")
            return

        self.text_gcode.tag_remove("dong_loi", "1.0", "end")
        self._ghi_log(f"[He thong] Dang nap {len(cac_dong)} dong G-code...", "he_thong")
        if not self._gui_qua_serial("PROG;BEGIN"):
            return
        time.sleep(0.05)

        for dong in cac_dong:
            self.ser.write((dong + "\n").encode())
            time.sleep(0.005)  # nghi nho tranh tran buffer UART khi gui nhanh

        self._gui_qua_serial("PROG;END")
        time.sleep(0.3)  # cho ESP32 xu ly va bao OK_NAP / LOI_NAP truoc khi RUN

        self._gui_qua_serial("RUN")

    def _jog(self, truc, dau):
        """Gui lenh JOG. truc = 'X' (keo, mm) hoac 'A' (xoay, do)."""
        try:
            if truc == "X":
                buoc = float(self.entry_jog_x.get())
            else:
                buoc = float(self.entry_jog_y.get())
            rpm = float(self.entry_jog_rpm.get())
        except ValueError:
            messagebox.showwarning("Sai du lieu", "Buoc jog va toc do phai la so hop le.")
            return

        if buoc <= 0:
            messagebox.showwarning("Sai du lieu", "Buoc jog phai > 0 (dung nut +/- de chon chieu).")
            return
        if rpm <= 0:
            messagebox.showwarning("Sai du lieu", "Toc do phai > 0.")
            return

        self._gui_qua_serial(f"JOG;{truc};{buoc * dau};{rpm}")

    def _dat_goc(self):
        tra_loi = messagebox.askyesno(
            "Xac nhan dat goc",
            "Dat vi tri HIEN TAI lam diem goc (0, 0)?\n\n"
            "Chuong trinh G-code sau do se tinh toan tu diem nay."
        )
        if tra_loi:
            self._gui_qua_serial("ZERO")

    def _gui_stop(self):
        self._gui_qua_serial("STOP")

    def _gui_lenh_don(self):
        lenh = self.entry_lenh_don.get().strip()
        if not lenh:
            return
        if self._gui_qua_serial(lenh):
            self.entry_lenh_don.delete(0, "end")

    # ---------------------------------------------------------
    # LOG
    # ---------------------------------------------------------
    def _ghi_log(self, dong_chu, the=None):
        self.text_log.config(state="normal")
        if the:
            self.text_log.insert("end", dong_chu + "\n", the)
        else:
            self.text_log.insert("end", dong_chu + "\n")
        self.text_log.see("end")
        self.text_log.config(state="disabled")

    def dong_ung_dung(self):
        if self.da_thay_doi:
            tra_loi = messagebox.askyesnocancel("Chua luu", "Noi dung G-code chua duoc luu. Luu truoc khi thoat?")
            if tra_loi is None:
                return
            if tra_loi and not self._luu_file():
                return
        self.dang_doc = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = GCodeApp(root)
    root.protocol("WM_DELETE_WINDOW", app.dong_ung_dung)
    root.mainloop()
