"""
Giao dien SU DUNG HANG NGAY - dieu khien may cat ong bang G-code qua USB COM.
Yeu cau: pip install pyserial   (KHONG can thu vien ve do hoa nao khac)

Chuc nang:
  - Go G-code tu do hoac MO FILE .nc/.gcode co san
  - DOC TRUOC file va VE HINH duong cat (dang "trai phang": ngang = X mm doc
    theo ong, doc = A do goc xoay). Doan CAT ve mau do, doan chay nhanh mau xam
  - KIEM TRA TRUOC khi chay: bao cac dong firmware se tu choi, cac lenh chua ho tro...
  - O nhap TOC DO CAT va TOC DO CHAY KHONG TAI rieng, tu ghi de vao file
  - Nap chuong trinh (PROG;BEGIN...PROG;END...RUN), jog, dat goc, pause/stop

File nay CHI danh cho van hanh hang ngay. Cai dat phan cung (chan GPIO,
so xung/vong, dao chieu truc) nam o file rieng "cnc_settings.pyw".
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import threading
import queue
import time
import os
import re
import math

COM_PORT_MAC_DINH = "COM3"
BAUD_RATE = 115200

TOC_DO_CAT_MAC_DINH = 15.0      # F khi dang cat (RPM dong co)
TOC_DO_NHANH_MAC_DINH = 60.0    # F khi chay khong tai (G0, ve goc)

# Do sau VONG DEM cua firmware: #define SUC_CHUA_BUOC 1200.
# Day KHONG con la gioi han do dai chuong trinh: may tinh nap dan (streaming),
# vua chay vua nap tiep, nen file dai bao nhieu cung chay duoc. Con so nay chi
# de bao cho nguoi dung biet bo dem sau bao nhieu buoc.
SUC_CHUA_BO_DEM = 1200

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

MAU_TRANG_THAI = {
    "CHUA_KETNOI": ("Chua ket noi", "#888888"),
    "SAN_SANG":    ("SAN SANG", "#28a745"),
    "DANG_NAP":    ("DANG NAP CHUONG TRINH...", "#6f42c1"),
    "DANG_CHAY":   ("DANG CHAY...", "#007bff"),
    "TAM_DUNG":    ("TAM DUNG", "#f0ad4e"),
    "LOI":         ("LOI / DUNG KHAN CAP", "#d9534f"),
    "DA_DUNG":     ("DA DUNG", "#6c757d"),
}

# Cac ma G firmware hieu (xem xu_ly_1_dong_gcode trong main.c)
MA_G_FIRMWARE_HIEU = {0, 1, 2, 3, 4, 17, 18, 19, 20, 21, 28, 30,
                      40, 41, 42, 43, 49, 54, 55, 56, 57, 58, 59,
                      61, 64, 80, 90, 91, 92, 93, 94, 98, 99}
MA_M_FIRMWARE_HIEU = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 30}

MA_G_DI_CHUYEN = {0, 1, 2, 3}

_RE_TOKEN = re.compile(r"([A-Za-z])\s*([+-]?(?:\d+\.?\d*|\.\d+))")


# =============================================================================
# PHAN TICH G-CODE (chay tren PC, truoc khi gui xuong ESP32)
# =============================================================================
def bo_chu_thich(dong):
    """Bo phan chu thich '(...)' va ';...' ra khoi 1 dong G-code."""
    ket_qua = []
    do_sau_ngoac = 0
    for ky_tu in dong:
        if ky_tu == "(":
            do_sau_ngoac += 1
        elif ky_tu == ")":
            if do_sau_ngoac > 0:
                do_sau_ngoac -= 1
            else:
                ket_qua.append(ky_tu)
        elif ky_tu == ";" and do_sau_ngoac == 0:
            break
        elif do_sau_ngoac == 0:
            ket_qua.append(ky_tu)
    return "".join(ket_qua)


def tach_token(dong_sach):
    """Tra ve danh sach (CHU_CAI_HOA, gia_tri_so) trong 1 dong da bo chu thich."""
    ra = []
    for chu, so in _RE_TOKEN.findall(dong_sach):
        try:
            ra.append((chu.upper(), float(so)))
        except ValueError:
            pass
    return ra


def _so_gon(gia_tri):
    """In so gon: 10.0 -> '10', 10.500 -> '10.5'"""
    chuoi = f"{gia_tri:.4f}".rstrip("0").rstrip(".")
    return chuoi if chuoi not in ("", "-") else "0"


class KetQuaPhanTich:
    def __init__(self):
        self.doan = []          # (x1, a1, x2, a2, la_cat)
        self.diem_moi = []      # (x, a) - vi tri bat mo cat (M3)
        self.canh_bao = []      # chuoi mo ta
        self.dong_chuan_hoa = []
        self.so_buoc_firmware = 0
        self.co_loi_nang = False


def phan_tich_chuong_trinh(cac_dong, chuan_hoa=True, ghi_de_toc_do=False,
                           toc_do_cat=TOC_DO_CAT_MAC_DINH,
                           toc_do_nhanh=TOC_DO_NHANH_MAC_DINH,
                           che_do=1, duong_kinh=60.0):
    """Doc truoc toan bo chuong trinh G-code.

    Tra ve KetQuaPhanTich gom: cac doan duong di (de ve hinh), diem moi,
    canh bao, va ban G-code da chuan hoa de gui xuong firmware.

    Chuan hoa lam gi (deu la nhung cho firmware hien CHUA xu ly duoc):
      - Dong chi co toa do (che do modal, vi du "X10 A20") -> them lai ma G
      - Dong chi co F -> nho lam toc do modal, bo dong do di
      - Nhieu ma G/M tren 1 dong -> tach thanh nhieu dong
      - File dung inch (G20) -> tu nhan toa do X voi 25.4 va doi sang G21
      - Luon ghi F vao moi dong di chuyen -> khong bao gio bi loi "chua khai bao F"
    """
    kq = KetQuaPhanTich()

    # CHE DO 3: toa do truc A trong file la MM CUNG tren mat ong. Doi sang DO de
    # ca hinh xem truoc lan mo phong 3D deu ve dung.
    chu_vi = math.pi * duong_kinh if duong_kinh > 0 else 0.0
    doi_a_sang_do = (che_do == 3 and chu_vi > 0)

    x = 0.0
    a = 0.0
    tuyet_doi = True
    plasma = False
    he_so_dai = 1.0        # 25.4 neu dang o che do inch (G20)
    da_bao_inch = False
    g_di_chuyen_modal = None
    f_modal = None

    def them_canh_bao(so_dong, chu):
        if len(kq.canh_bao) < 60:
            kq.canh_bao.append(f"Dong {so_dong}: {chu}")

    for chi_so, dong_goc in enumerate(cac_dong, start=1):
        dong_sach = bo_chu_thich(dong_goc).strip()
        if not dong_sach:
            if chuan_hoa and dong_goc.strip():
                kq.dong_chuan_hoa.append(dong_goc.rstrip())
            continue

        token = tach_token(dong_sach)
        if not token:
            continue

        ma_g = [int(round(v)) for c, v in token if c == "G"]
        ma_m = [int(round(v)) for c, v in token if c == "M"]
        tu_khac = {}
        for c, v in token:
            if c in ("X", "A", "Y", "F", "P"):
                tu_khac[c] = v

        # ----- Kiem tra ma khong duoc ho tro -----
        for g in ma_g:
            if g not in MA_G_FIRMWARE_HIEU:
                them_canh_bao(chi_so, f"G{g} firmware KHONG ho tro - chuong trinh se bi tu choi khi nap")
                kq.co_loi_nang = True
        for m in ma_m:
            if m not in MA_M_FIRMWARE_HIEU:
                them_canh_bao(chi_so, f"M{m} firmware KHONG ho tro - chuong trinh se bi tu choi khi nap")
                kq.co_loi_nang = True

        if len(ma_g) + len(ma_m) > 1:
            them_canh_bao(chi_so, "co nhieu ma G/M tren 1 dong (firmware moi da chay duoc het)")

        # ----- Cap nhat trang thai modal -----
        for g in ma_g:
            if g == 20:
                he_so_dai = 25.4
                if not da_bao_inch:
                    them_canh_bao(chi_so, "file dung don vi INCH (G20) - toa do X duoc doi sang mm (x25.4)")
                    da_bao_inch = True
            elif g == 21:
                he_so_dai = 1.0
            elif g == 90:
                tuyet_doi = True
            elif g == 91:
                tuyet_doi = False

        if "F" in tu_khac:
            f_modal = tu_khac["F"]

        chi_co_f = (not ma_g and not ma_m and "F" in tu_khac
                    and "X" not in tu_khac and "A" not in tu_khac and "Y" not in tu_khac)
        if chi_co_f:
            them_canh_bao(chi_so, "dong chi co F - dat toc do modal (firmware moi da chap nhan)")
            continue

        # ----- Xac dinh lenh di chuyen (co ke ca che do modal) -----
        co_toa_do = ("X" in tu_khac) or ("A" in tu_khac) or ("Y" in tu_khac)
        ma_dc = None
        for g in ma_g:
            if g in MA_G_DI_CHUYEN:
                ma_dc = g
                g_di_chuyen_modal = g

        ve_goc = (28 in ma_g) or (30 in ma_g)
        dat_goc = 92 in ma_g

        if ma_dc is None and co_toa_do and not ve_goc and not dat_goc:
            if g_di_chuyen_modal is None:
                them_canh_bao(chi_so, "co toa do nhung chua tung khai bao G0/G1 truoc do - bo qua dong nay")
                kq.co_loi_nang = True
                continue
            ma_dc = g_di_chuyen_modal
            them_canh_bao(chi_so, f"dong chi co toa do (che do modal) - dung lai G{ma_dc} cua dong truoc")

        # ----- Cac dong khong di chuyen: chi cap nhat trang thai -----
        for m in ma_m:
            if m in (3, 4):
                plasma = True
                kq.diem_moi.append((x, a))
            elif m in (5, 2, 30):
                plasma = False

        # ----- Tinh toan quang duong -----
        x_moi, a_moi = x, a
        co_di_chuyen = False
        la_cat = False

        if dat_goc:
            if "X" in tu_khac:
                x = tu_khac["X"] * he_so_dai
            if "A" in tu_khac or "Y" in tu_khac:
                gia_tri_a = tu_khac.get("A", tu_khac.get("Y"))
                a = gia_tri_a / chu_vi * 360.0 if doi_a_sang_do else gia_tri_a
        elif ve_goc:
            x_moi, a_moi = 0.0, 0.0
            co_di_chuyen = (abs(x_moi - x) > 1e-9) or (abs(a_moi - a) > 1e-9)
            la_cat = False
        elif ma_dc is not None and co_toa_do:
            if "X" in tu_khac:
                gia_tri = tu_khac["X"] * he_so_dai
                x_moi = gia_tri if tuyet_doi else x + gia_tri
            if "A" in tu_khac or "Y" in tu_khac:
                # A la GOC (do) - khong nhan he so inch.
                # Che do 3: gia tri trong file la mm cung -> doi sang do
                gia_tri = tu_khac.get("A", tu_khac.get("Y"))
                if doi_a_sang_do:
                    gia_tri = gia_tri / chu_vi * 360.0
                a_moi = gia_tri if tuyet_doi else a + gia_tri
            co_di_chuyen = (abs(x_moi - x) > 1e-9) or (abs(a_moi - a) > 1e-9)
            la_cat = plasma and ma_dc != 0

        if co_di_chuyen:
            kq.doan.append((x, a, x_moi, a_moi, la_cat))
            x, a = x_moi, a_moi

        # ----- Dem so buoc ma firmware se sinh ra -----
        if co_di_chuyen:
            kq.so_buoc_firmware += 1
        if dat_goc:
            kq.so_buoc_firmware += 1
        if 4 in ma_g:
            kq.so_buoc_firmware += 1
        for m in ma_m:
            if m in (0, 1, 2, 3, 4, 5, 30):
                kq.so_buoc_firmware += 1

        # ----- Sinh ban G-code chuan hoa -----
        if not chuan_hoa:
            kq.dong_chuan_hoa.append(dong_goc.rstrip())
            continue

        ra = []
        for g in ma_g:
            if g in MA_G_DI_CHUYEN or g in (4, 20, 28, 30, 92):
                continue  # xu ly rieng ben duoi (G4 can kem P, G92 can kem toa do...)
            ra.append(f"G{g}")
        if he_so_dai != 1.0 and 20 in ma_g:
            ra.append("G21")  # da doi sang mm roi

        for m in ma_m:
            if m in (3, 4):
                ra.append(f"M{m}")   # bat mo cat TRUOC khi di chuyen

        if dat_goc:
            phan = "G92"
            if "X" in tu_khac:
                phan += f" X{_so_gon(x)}"
            if "A" in tu_khac or "Y" in tu_khac:
                # Gui xuong theo DUNG don vi trong file goc - firmware tu doi
                phan += f" A{_so_gon(tu_khac.get('A', tu_khac.get('Y')))}"
            ra.append(phan)

        if 4 in ma_g:
            ra.append(f"G4 P{_so_gon(tu_khac.get('P', 0.0))}")

        if co_di_chuyen:
            if ghi_de_toc_do:
                f_dung = toc_do_cat if la_cat else toc_do_nhanh
            else:
                f_dung = f_modal if (f_modal and f_modal > 0) else (
                    toc_do_cat if la_cat else toc_do_nhanh)
            if ve_goc:
                # G28/G30 -> doi thanh lenh tuong duong ro rang, tranh phu thuoc
                # vao che do G90/G91 dang hieu luc va toc do modal khong xac dinh
                if not tuyet_doi:
                    ra.append("G90")
                ra.append(f"G0 X0 A0 F{_so_gon(toc_do_nhanh)}")
                if not tuyet_doi:
                    ra.append("G91")
            else:
                # Toa do A gui xuong phai theo DUNG don vi ma firmware dang cho
                # doi (che do 3 = mm cung), nen doi nguoc lai neu da doi o tren
                a_gui = a / 360.0 * chu_vi if doi_a_sang_do else a
                phan = f"G{ma_dc} X{_so_gon(x)} A{_so_gon(a_gui)} F{_so_gon(f_dung)}"
                ra.append(phan)

        for m in ma_m:
            if m in (5, 2, 30, 0, 1, 6, 7, 8, 9):
                ra.append(f"M{m}")   # tat mo cat / ket thuc SAU khi di chuyen

        kq.dong_chuan_hoa.extend(ra)

    # Khong con canh bao "chuong trinh qua dai" nua - may tinh nap dan nen do dai
    # khong bi chan. Chi nhac khi bai dai hon bo dem de nguoi dung biet luc chay
    # may tinh phai giu ket noi COM suot ca duong cat.
    if kq.so_buoc_firmware > SUC_CHUA_BO_DEM:
        kq.canh_bao.insert(0, f"Bai dai {kq.so_buoc_firmware} buoc, lon hon bo dem "
                              f"{SUC_CHUA_BO_DEM} buoc cua ESP32 - may se vua chay vua "
                              f"nap dan. GIU NGUYEN ket noi COM, dung tat phan mem hay "
                              f"rut day trong luc dang cat.")

    return kq


# =============================================================================
# GIAO DIEN
# =============================================================================
class GCodeApp:
    # ----- Tham so NAP DAN (streaming) -----
    # Gui truoc ngan nay dong roi bam RUN ngay. Firmware giu 150 buoc trong bo
    # dem truoc khi chay (BUOC_DAY_TRUOC_KHI_CHAY) nen con so nay khop voi no.
    SO_DONG_NAP_TRUOC = 150
    # Chi gui tiep khi ESP32 con it nhat ngan nay o trong - de lai bien an toan
    # phong khi mot dong sinh ra nhieu buoc
    NGUONG_GUI_TIEP = 20
    # So dong toi da gui lien mot mach (khong nghi) trong mot lo
    LO_GUI_TOI_DA = 60
    # Neu ESP32 khong voi ra o trong nao trong ngan nay giay thi coi nhu may da
    # dung han (EMG, mat dien...) - huy nap thay vi treo giao dien mai mai
    CHO_TOI_DA_S = 60.0

    def __init__(self, root):
        self.root = root
        self.root.geometry("980x720")
        self.root.minsize(900, 640)

        self.ser = None
        self.dang_ket_noi = False
        self.dang_doc = False
        self.trang_thai = "CHUA_KETNOI"
        self.file_hien_tai = None
        self.da_thay_doi = False
        self.ket_qua_phan_tich = None
        self.gcode_da_ve = None      # noi dung G-code cua lan ve hinh gan nhat
        self.che_do_may = 1          # 1/2/3 - doc tu ESP32 bang CFG;GET
        self.duong_kinh_may = 60.0   # mm - doc tu ESP32
        self.ket_qua_nap = None      # "OK" / "LOI" - dat khi ESP32 tra loi

        # ----- NAP DAN (streaming) -----
        # Bai dai khong con phai nap het vao RAM ESP32 truoc khi chay: gui truoc
        # mot dem nho roi bam chay, vua chay vua nap tiep. Cac bien nay do luong
        # doc serial cap nhat va luong nap doc de dieu tiet luu luong.
        self.cho_trong_esp32 = 0     # so o trong con lai trong vong dem ESP32
        self.so_dong_da_nhan = 0     # so dong ESP32 da bao nhan (OK;...)
        self.dang_nap_dan = False    # dat False de huy nap giua chung (STOP)
        # Cac luong nen (doc serial, nap chuong trinh) KHONG duoc dung cham vao
        # giao dien Tkinter. Chung chi bo du lieu vao hang doi nay, con luong
        # giao dien tu lay ra bang root.after - day la cach an toan duy nhat.
        self.hang_doi_su_kien = queue.Queue()

        self._xay_dung_giao_dien()
        self._cap_nhat_tieu_de()
        self._cap_nhat_trang_thai("CHUA_KETNOI")
        self.root.after(200, self._ve_lai_xem_truoc)
        self.root.after(50, self._lay_su_kien_tu_hang_doi)

    # ---------------------------------------------------------
    def _xay_dung_giao_dien(self):
        pad = {"padx": 8, "pady": 3}

        self._dung_menu()

        # ----- Hang 1: ket noi + trang thai may -----
        khung_tren = ttk.Frame(self.root)
        khung_tren.pack(fill="x", **pad)

        khung_ketnoi = ttk.LabelFrame(khung_tren, text="Ket noi Serial")
        khung_ketnoi.pack(side="left", fill="x", expand=True, padx=(0, 4))

        ttk.Label(khung_ketnoi, text="COM:").grid(row=0, column=0, padx=(6, 2), pady=5)
        self.combo_port = ttk.Combobox(khung_ketnoi, width=9, values=self._danh_sach_cong())
        self.combo_port.set(COM_PORT_MAC_DINH)
        self.combo_port.grid(row=0, column=1, padx=2, pady=5)
        ttk.Button(khung_ketnoi, text="Lam moi", width=8,
                   command=self._lam_moi_cong).grid(row=0, column=2, padx=3, pady=5)
        self.btn_ketnoi = ttk.Button(khung_ketnoi, text="Ket noi", width=11,
                                     command=self._toggle_ket_noi)
        self.btn_ketnoi.grid(row=0, column=3, padx=3, pady=5)

        khung_tt = ttk.LabelFrame(khung_tren, text="Trang thai may")
        khung_tt.pack(side="left", fill="x", expand=True, padx=(4, 0))
        self.lbl_trangthai = tk.Label(khung_tt, text="Chua ket noi",
                                      font=("Segoe UI", 12, "bold"), fg="white", bg="#888888")
        self.lbl_trangthai.pack(fill="x", padx=6, pady=5)

        # ----- Hang 2: vi tri + JOG -----
        khung_jog = ttk.LabelFrame(self.root, text="Vi tri hien tai & dieu khien thu cong")
        khung_jog.pack(fill="x", **pad)

        self.lbl_vitri = ttk.Label(khung_jog, text="X = 0.00 mm     A = 0.00 do",
                                   font=("Segoe UI", 12, "bold"))
        self.lbl_vitri.grid(row=0, column=0, columnspan=4, padx=8, pady=(6, 2), sticky="w")

        # So xung tho do firmware dem - de doi chieu khi nghi ngo may bi truot buoc
        self.lbl_xung = ttk.Label(khung_jog, text="xung: X=0  A=0",
                                  font=("Consolas", 9), foreground="#666666")
        self.lbl_xung.grid(row=0, column=4, padx=4, pady=(6, 2), sticky="w")

        # Nut DAT GOC de rieng o hang tren, khong chen chung hang voi cac nut jog
        # (neu de chung hang se bi bop lai con moi chu "GOC 0")
        self.btn_zero = tk.Button(khung_jog, text="🎯  DAT GOC 0 TAI DAY (ZERO)",
                                  font=("Segoe UI", 9, "bold"), bg="#6f42c1", fg="white",
                                  command=self._dat_goc)
        self.btn_zero.grid(row=0, column=5, columnspan=2, padx=8, pady=(6, 2), sticky="e")

        ttk.Label(khung_jog, text="Buoc X (mm):").grid(row=1, column=0, padx=(8, 2), pady=4, sticky="w")
        self.entry_jog_x = ttk.Entry(khung_jog, width=6)
        self.entry_jog_x.insert(0, "10")
        self.entry_jog_x.grid(row=1, column=1, padx=2, pady=4)

        ttk.Label(khung_jog, text="Buoc A (do):").grid(row=1, column=2, padx=(10, 2), pady=4, sticky="w")
        self.entry_jog_y = ttk.Entry(khung_jog, width=6)
        self.entry_jog_y.insert(0, "15")
        self.entry_jog_y.grid(row=1, column=3, padx=2, pady=4)

        ttk.Label(khung_jog, text="Toc do JOG:").grid(row=1, column=4, padx=(10, 2), pady=4, sticky="w")
        self.entry_jog_rpm = ttk.Entry(khung_jog, width=6)
        self.entry_jog_rpm.insert(0, "30")
        self.entry_jog_rpm.grid(row=1, column=5, padx=2, pady=4)

        khung_nut_jog = ttk.Frame(khung_jog)
        khung_nut_jog.grid(row=1, column=6, padx=(14, 8), pady=4, sticky="we")
        khung_jog.columnconfigure(6, weight=1)

        self.nut_jog = []
        for chu, truc, dau in [("◀ X-", "X", -1), ("X+ ▶", "X", 1),
                               ("↺ A-", "A", -1), ("A+ ↻", "A", 1)]:
            b = tk.Button(khung_nut_jog, text=chu, font=("Segoe UI", 9, "bold"), width=8,
                          command=lambda t=truc, d=dau: self._jog(t, d))
            b.pack(side="left", padx=3)
            self.nut_jog.append(b)

        # ----- Hang 3: notebook G-code / xem truoc -----
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill="both", expand=True, **pad)
        # Tu ve lai khi chuyen sang tab xem truoc / mo phong neu G-code da sua,
        # de khong bao gio nhin phai hinh cu
        self.notebook.bind("<<NotebookTabChanged>>", self._doi_tab)
        self._dung_tab_gcode()
        self._dung_tab_xem_truoc()
        self._dung_tab_3d()

        # ----- Hang 4: toc do -----
        self._dung_khung_toc_do(pad)

        # ----- Hang 5: nut chay -----
        khung_nut = ttk.Frame(self.root)
        khung_nut.pack(fill="x", **pad)

        self.btn_run = tk.Button(khung_nut, text="▶  NAP & CHAY", font=("Segoe UI", 11, "bold"),
                                 bg="#5cb85c", fg="white", command=self._nap_va_chay)
        self.btn_run.pack(side="left", fill="x", expand=True, padx=(0, 4))

        self.btn_pause = tk.Button(khung_nut, text="⏸  PAUSE", font=("Segoe UI", 11, "bold"),
                                   bg="#f0ad4e", fg="white",
                                   command=lambda: self._gui_qua_serial("PAUSE"))
        self.btn_pause.pack(side="left", fill="x", expand=True, padx=4)

        self.btn_resume = tk.Button(khung_nut, text="⏵  RESUME", font=("Segoe UI", 11, "bold"),
                                    bg="#5bc0de", fg="white",
                                    command=lambda: self._gui_qua_serial("RESUME"))
        self.btn_resume.pack(side="left", fill="x", expand=True, padx=4)

        self.btn_stop = tk.Button(khung_nut, text="⛔  STOP (Esc)", font=("Segoe UI", 11, "bold"),
                                  bg="#d9534f", fg="white", command=self._gui_stop)
        self.btn_stop.pack(side="left", fill="x", expand=True, padx=(4, 0))

        # ----- Hang 6: lenh don + log -----
        khung_don = ttk.Frame(self.root)
        khung_don.pack(fill="x", **pad)
        ttk.Label(khung_don, text="Lenh nhanh:").pack(side="left", padx=(0, 4))
        self.entry_lenh_don = ttk.Entry(khung_don, font=("Consolas", 9))
        self.entry_lenh_don.pack(side="left", fill="x", expand=True, padx=(0, 4))
        self.entry_lenh_don.bind("<Return>", lambda e: self._gui_lenh_don())
        ttk.Button(khung_don, text="Gui", width=7, command=self._gui_lenh_don).pack(side="left")

        # Log KHONG expand: chi notebook o tren duoc gian ra, nho vay o soan thao
        # va hinh xem truoc chiem het cho trong, log giu chieu cao co dinh
        khung_log = ttk.LabelFrame(self.root, text="Nhat ky (log) tu ESP32")
        khung_log.pack(fill="x", **pad)
        self.text_log = tk.Text(khung_log, height=4, state="disabled",
                                bg="#111111", fg="#00ff00", font=("Consolas", 9))
        self.text_log.tag_configure("loi", foreground="#ff5555")
        self.text_log.tag_configure("ok", foreground="#55ff7f")
        self.text_log.tag_configure("he_thong", foreground="#aaaaff")
        scroll_log = ttk.Scrollbar(khung_log, orient="vertical", command=self.text_log.yview)
        self.text_log.configure(yscrollcommand=scroll_log.set)
        self.text_log.pack(side="left", fill="both", expand=True, padx=(5, 0), pady=4)
        scroll_log.pack(side="right", fill="y", padx=(0, 5), pady=4)

        self._cap_nhat_nut_theo_trang_thai()

    def _dung_menu(self):
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
        self.root.bind_all("<F5>", lambda e: self._ve_lai_xem_truoc())

    def _dung_tab_gcode(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Chuong trinh G-code  ")

        thanh = ttk.Frame(tab)
        thanh.pack(fill="x", padx=5, pady=(5, 0))
        tk.Button(thanh, text="🔄  CAP NHAT MO PHONG (F5)", font=("Segoe UI", 9, "bold"),
                  bg="#0d6efd", fg="white", command=self._ve_lai_xem_truoc
                  ).pack(side="left")
        ttk.Label(thanh, text="   Sua G-code xong bam nut nay (hoac chuyen sang tab khac) "
                              "de ve lai hinh va kiem tra lai",
                  foreground="#555555").pack(side="left", padx=8)

        khung_text = ttk.Frame(tab)
        khung_text.pack(fill="both", expand=True, padx=5, pady=5)

        self.text_gcode = tk.Text(khung_text, height=8, font=("Consolas", 10), wrap="none", undo=True)
        self.text_gcode.insert("1.0", VI_DU_GCODE)
        self.text_gcode.edit_modified(False)
        self.text_gcode.bind("<<Modified>>", self._doi_gcode)
        self.text_gcode.tag_configure("dong_loi", background="#5c1a1a")

        scroll_y = ttk.Scrollbar(khung_text, orient="vertical", command=self.text_gcode.yview)
        self.text_gcode.configure(yscrollcommand=scroll_y.set)
        self.text_gcode.pack(side="left", fill="both", expand=True)
        scroll_y.pack(side="right", fill="y")

    def _dung_tab_xem_truoc(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Xem truoc duong cat  ")

        thanh = ttk.Frame(tab)
        thanh.pack(fill="x", padx=5, pady=(5, 0))
        tk.Button(thanh, text="🔄  CAP NHAT (F5)", font=("Segoe UI", 9, "bold"),
                  bg="#0d6efd", fg="white", command=self._ve_lai_xem_truoc).pack(side="left")
        ttk.Label(thanh, text="   Do = duong CAT     Xam dut net = chay nhanh khong tai"
                              "     Cham vang = diem moi (M3)",
                  foreground="#555555").pack(side="left", padx=8)

        # height nho de chieu cao "tu nhien" cua tab khong day cac khung ben duoi
        # ra khoi man hinh; canvas van tu gian ra nho fill/expand
        self.canvas_xem = tk.Canvas(tab, bg="white", height=120, highlightthickness=1,
                                    highlightbackground="#cccccc")
        self.canvas_xem.pack(fill="both", expand=True, padx=5, pady=5)
        self.canvas_xem.bind("<Configure>", lambda e: self._ve_hinh())

        self.lbl_thong_ke = ttk.Label(tab, text="", justify="left", font=("Consolas", 9))
        self.lbl_thong_ke.pack(anchor="w", padx=8, pady=(0, 2))

        khung_cb = ttk.LabelFrame(tab, text="Kiem tra truoc khi chay")
        khung_cb.pack(fill="x", padx=5, pady=(0, 5))
        self.text_canh_bao = tk.Text(khung_cb, height=3, state="disabled",
                                     font=("Consolas", 9), wrap="word")
        self.text_canh_bao.tag_configure("nang", foreground="#c00000")
        scroll_cb = ttk.Scrollbar(khung_cb, orient="vertical", command=self.text_canh_bao.yview)
        self.text_canh_bao.configure(yscrollcommand=scroll_cb.set)
        self.text_canh_bao.pack(side="left", fill="both", expand=True, padx=(4, 0), pady=4)
        scroll_cb.pack(side="right", fill="y", padx=(0, 4), pady=4)

    # ---------------------------------------------------------
    # TAB MO PHONG 3D
    # ---------------------------------------------------------
    def _dung_tab_3d(self):
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="  Mo phong 3D  ")

        thanh = ttk.Frame(tab)
        thanh.pack(fill="x", padx=5, pady=(5, 0))
        ttk.Label(thanh, text="Duong kinh ong (mm):").pack(side="left")
        self.entry_duong_kinh = ttk.Entry(thanh, width=6)
        self.entry_duong_kinh.insert(0, "60")
        self.entry_duong_kinh.pack(side="left", padx=(4, 8))
        self.entry_duong_kinh.bind("<Return>", lambda e: self._ve_3d())

        tk.Button(thanh, text="🔄 Cap nhat", font=("Segoe UI", 9, "bold"),
                  bg="#0d6efd", fg="white", command=self._ve_lai_xem_truoc
                  ).pack(side="left", padx=(0, 6))

        self.btn_chay_mo_phong = ttk.Button(thanh, text="▶ Chay mo phong",
                                            command=self._bat_tat_mo_phong)
        self.btn_chay_mo_phong.pack(side="left", padx=(0, 6))
        ttk.Button(thanh, text="Goc nhin mac dinh",
                   command=self._dat_lai_goc_nhin_3d).pack(side="left", padx=(0, 6))

        self.bien_tien_do = tk.DoubleVar(value=100.0)
        ttk.Scale(thanh, from_=0, to=100, variable=self.bien_tien_do,
                  command=lambda v: self._ve_3d(), length=170).pack(side="left", padx=4)
        self.lbl_tien_do = ttk.Label(thanh, text="100%", width=5, font=("Consolas", 9))
        self.lbl_tien_do.pack(side="left")

        ttk.Label(thanh, text="  Keo chuot de xoay, lan chuot de phong to",
                  foreground="#555555").pack(side="left", padx=6)

        self.canvas_3d = tk.Canvas(tab, bg="#f7f9fb", height=120, highlightthickness=1,
                                   highlightbackground="#cccccc")
        self.canvas_3d.pack(fill="both", expand=True, padx=5, pady=5)

        # Goc nhin: xoay quanh truc doc (yaw) va truc ngang (pitch), theo radian
        self.goc_yaw = math.radians(32)
        self.goc_pitch = math.radians(22)
        self.ty_le_3d = 1.0
        self._chuot_truoc = None
        self.dang_chay_mo_phong = False

        self.canvas_3d.bind("<Configure>", lambda e: self._ve_3d())
        self.canvas_3d.bind("<ButtonPress-1>", self._chuot_nhan_3d)
        self.canvas_3d.bind("<B1-Motion>", self._chuot_keo_3d)
        self.canvas_3d.bind("<ButtonRelease-1>", lambda e: setattr(self, "_chuot_truoc", None))
        self.canvas_3d.bind("<MouseWheel>", self._lan_chuot_3d)           # Windows
        self.canvas_3d.bind("<Button-4>", lambda e: self._phong_3d(1.1))  # Linux
        self.canvas_3d.bind("<Button-5>", lambda e: self._phong_3d(1 / 1.1))

    # ----- Chay mo phong (dien lai duong cat theo thu tu that) -----
    def _bat_tat_mo_phong(self):
        self.dang_chay_mo_phong = not self.dang_chay_mo_phong
        if self.dang_chay_mo_phong:
            self.btn_chay_mo_phong.config(text="⏸ Dung mo phong")
            if self.bien_tien_do.get() >= 99.9:
                self.bien_tien_do.set(0.0)
            self._buoc_mo_phong()
        else:
            self.btn_chay_mo_phong.config(text="▶ Chay mo phong")

    def _buoc_mo_phong(self):
        if not self.dang_chay_mo_phong:
            return
        tien = self.bien_tien_do.get() + 1.5
        if tien >= 100.0:
            tien = 100.0
            self.dang_chay_mo_phong = False
            self.btn_chay_mo_phong.config(text="▶ Chay mo phong")
        self.bien_tien_do.set(tien)
        self._ve_3d()
        if self.dang_chay_mo_phong:
            self.root.after(60, self._buoc_mo_phong)

    def _dat_lai_goc_nhin_3d(self):
        self.goc_yaw = math.radians(32)
        self.goc_pitch = math.radians(22)
        self.ty_le_3d = 1.0
        self._ve_3d()

    def _chuot_nhan_3d(self, su_kien):
        self._chuot_truoc = (su_kien.x, su_kien.y)

    def _chuot_keo_3d(self, su_kien):
        if self._chuot_truoc is None:
            return
        dx = su_kien.x - self._chuot_truoc[0]
        dy = su_kien.y - self._chuot_truoc[1]
        self._chuot_truoc = (su_kien.x, su_kien.y)
        self.goc_yaw += dx * 0.01
        # Keo chuot LEN thi ong nghieng LEN theo (dau tru) - keo xuong thi nghieng
        # xuong. Neu de dau cong se thay nguoc voi chieu tay keo.
        self.goc_pitch = max(-1.4, min(1.4, self.goc_pitch - dy * 0.01))
        self._ve_3d()

    def _lan_chuot_3d(self, su_kien):
        self._phong_3d(1.1 if su_kien.delta > 0 else 1 / 1.1)

    def _phong_3d(self, he_so):
        self.ty_le_3d = max(0.2, min(6.0, self.ty_le_3d * he_so))
        self._ve_3d()

    # ----- Phep chieu 3D -----
    def _quay_3d(self, x, y, z):
        """Quay diem (x doc ong, y/z tren mat cat) theo goc nhin. Tra ve (x', y', do_sau)."""
        cy, sy = math.cos(self.goc_yaw), math.sin(self.goc_yaw)
        cp, sp = math.cos(self.goc_pitch), math.sin(self.goc_pitch)
        x1 = x * cy - z * sy
        z1 = x * sy + z * cy
        y2 = y * cp - z1 * sp
        z2 = y * sp + z1 * cp     # do sau: cang lon cang GAN mat nguoi xem
        return x1, y2, z2

    def _mau_to_bong(self, sang):
        """sang 0..1 -> ma mau xam xanh kim loai."""
        sang = max(0.0, min(1.0, sang))
        do = int(96 + 120 * sang)
        xanh_la = int(108 + 122 * sang)
        xanh_duong = int(122 + 128 * sang)
        return f"#{do:02x}{xanh_la:02x}{xanh_duong:02x}"

    def _ve_3d(self):
        canvas = self.canvas_3d
        canvas.delete("all")
        kq = self.ket_qua_phan_tich
        rong, cao = canvas.winfo_width(), canvas.winfo_height()
        if rong < 60 or cao < 60:
            return
        if kq is None or not kq.doan:
            canvas.create_text(rong // 2, cao // 2, text="Chua co duong cat de ve",
                               fill="#999999")
            return

        try:
            ban_kinh = float(self.entry_duong_kinh.get()) / 2.0
            if ban_kinh <= 0:
                raise ValueError
        except ValueError:
            canvas.create_text(rong // 2, cao // 2,
                               text="Duong kinh ong phai la so > 0", fill="#d9534f")
            return

        cac_x = [d[0] for d in kq.doan] + [d[2] for d in kq.doan]
        x_min, x_max = min(cac_x), max(cac_x)
        dem = max((x_max - x_min) * 0.18, ban_kinh * 0.6)
        ong_dau, ong_cuoi = x_min - dem, x_max + dem
        x_giua = (ong_dau + ong_cuoi) / 2.0

        do_lon = max(ong_cuoi - ong_dau, ban_kinh * 2) or 1.0
        ty_le = (min(rong, cao * 1.9) * 0.40 / do_lon) * self.ty_le_3d
        tam_x, tam_y = rong / 2, cao / 2

        def chieu(x, goc_do):
            """(x mm doc ong, A do) -> (toa do man hinh, do sau, do sang)"""
            g = math.radians(goc_do)
            phap_y, phap_z = math.cos(g), math.sin(g)   # phap tuyen mat ong
            x1, y2, z2 = self._quay_3d(x - x_giua, ban_kinh * phap_y, ban_kinh * phap_z)
            _, ny, nz = self._quay_3d(0.0, phap_y, phap_z)
            # Nguon sang cheo tu tren-truoc
            sang = 0.30 * ny + 0.85 * nz
            return (tam_x + x1 * ty_le, tam_y - y2 * ty_le), z2, sang

        # ================= VE THAN ONG TO BONG (thuat toan hoa si) =================
        so_vanh = 26          # so mieng quanh chu vi
        so_doi = 14           # so mieng doc than ong
        buoc_goc = 360.0 / so_vanh
        mat = []
        for i in range(so_doi):
            xa = ong_dau + (ong_cuoi - ong_dau) * i / so_doi
            xb = ong_dau + (ong_cuoi - ong_dau) * (i + 1) / so_doi
            for j in range(so_vanh):
                ga, gb = j * buoc_goc, (j + 1) * buoc_goc
                p1, z1, s1 = chieu(xa, ga)
                p2, z2, _ = chieu(xa, gb)
                p3, z3, _ = chieu(xb, gb)
                p4, z4, _ = chieu(xb, ga)
                mat.append(((z1 + z2 + z3 + z4) / 4.0, [p1, p2, p3, p4], s1))
        mat.sort(key=lambda m: m[0])          # ve mat XA truoc, mat GAN sau
        for _, diem, sang in mat:
            mau = self._mau_to_bong(sang)
            canvas.create_polygon([toa for p in diem for toa in p],
                                  fill=mau, outline=mau)

        # ----- Vanh mieng ong 2 dau cho ro hinh khoi -----
        for x_mieng in (ong_dau, ong_cuoi):
            vanh = [chieu(x_mieng, g)[0] for g in range(0, 361, 8)]
            canvas.create_line(vanh, fill="#5a6b7d", width=1)

        # ================= VE DUONG CAT BAM TREN MAT ONG =================
        # Chia nho tung doan de duong bam theo mat cong, va tach ro phan NAM
        # PHIA TRUOC (nhin thay) voi phan VONG RA SAU ong (bi che khuat)
        tien_do = self.bien_tien_do.get() / 100.0
        self.lbl_tien_do.config(text=f"{int(tien_do * 100)}%")

        cac_diem = []      # (toa do man hinh, nhin thay?, la doan cat?)
        for x1, a1, x2, a2, la_cat in kq.doan:
            buoc = max(2, int(max(abs(a2 - a1) / 5.0, abs(x2 - x1) / 2.0)) + 1)
            for t in range(buoc + 1):
                ti_le = t / buoc
                p, _, sang = chieu(x1 + (x2 - x1) * ti_le, a1 + (a2 - a1) * ti_le)
                cac_diem.append((p, sang > 0.12, la_cat))

        so_ve = max(2, int(len(cac_diem) * tien_do)) if cac_diem else 0
        for k in range(1, so_ve):
            (pa, hien_a, cat_a) = cac_diem[k - 1]
            (pb, hien_b, _) = cac_diem[k]
            nhin_thay = hien_a and hien_b
            if cat_a:
                mau, day, net = ("#d62828", 3, None) if nhin_thay else ("#f0a0a0", 2, (3, 3))
            else:
                mau, day, net = ("#7c8894", 1, (4, 3)) if nhin_thay else ("#c3cad1", 1, (2, 4))
            canvas.create_line(pa[0], pa[1], pb[0], pb[1], fill=mau, width=day,
                               dash=net) if net else \
                canvas.create_line(pa[0], pa[1], pb[0], pb[1], fill=mau, width=day)

        # ----- Dau cat: vi tri hien tai trong mo phong -----
        if 0 < so_ve <= len(cac_diem):
            px, py = cac_diem[so_ve - 1][0]
            canvas.create_oval(px - 7, py - 7, px + 7, py + 7,
                               outline="#0b5cad", width=2)
            canvas.create_oval(px - 3, py - 3, px + 3, py + 3,
                               fill="#0b5cad", outline="")

        # ----- Diem moi (M3) -----
        for gx, ga in kq.diem_moi:
            p, _, sang = chieu(gx, ga)
            if sang > 0.12:
                canvas.create_oval(p[0] - 4, p[1] - 4, p[0] + 4, p[1] + 4,
                                   fill="#f0ad4e", outline="#a06800")

        canvas.create_text(8, 6, anchor="nw", fill="#44515e", font=("Segoe UI", 8),
                           text=f"Ong D{ban_kinh * 2:g}mm    duong cat X: "
                                f"{x_min:.1f} -> {x_max:.1f} mm")
        canvas.create_text(8, cao - 6, anchor="sw", fill="#7a8792", font=("Segoe UI", 8),
                           text="Do dam = duong cat mat truoc    Do nhat = vong ra mat sau ong")

    def _dung_khung_toc_do(self, pad):
        khung = ttk.LabelFrame(self.root, text="Toc do (F - vong/phut dong co)  [Che do 1]")
        khung.pack(fill="x", **pad)
        self.khung_toc_do = khung

        ttk.Label(khung, text="Toc do CAT:").grid(row=0, column=0, padx=(8, 2), pady=6, sticky="w")
        self.entry_toc_do_cat = ttk.Entry(khung, width=8)
        self.entry_toc_do_cat.insert(0, _so_gon(TOC_DO_CAT_MAC_DINH))
        self.entry_toc_do_cat.grid(row=0, column=1, padx=2, pady=6)
        self.lbl_dv_cat = ttk.Label(khung, text="RPM", width=6, foreground="#666")
        self.lbl_dv_cat.grid(row=0, column=2, padx=(2, 8), pady=6, sticky="w")

        ttk.Label(khung, text="Toc do CHAY KHONG TAI (G0):").grid(
            row=0, column=3, padx=(10, 2), pady=6, sticky="w")
        self.entry_toc_do_nhanh = ttk.Entry(khung, width=8)
        self.entry_toc_do_nhanh.insert(0, _so_gon(TOC_DO_NHANH_MAC_DINH))
        self.entry_toc_do_nhanh.grid(row=0, column=4, padx=2, pady=6)
        self.lbl_dv_nhanh = ttk.Label(khung, text="RPM", width=6, foreground="#666")
        self.lbl_dv_nhanh.grid(row=0, column=5, padx=(2, 8), pady=6, sticky="w")

        self.bien_ghi_de = tk.BooleanVar(value=True)
        ttk.Checkbutton(khung, text="Ghi de F trong file",
                        variable=self.bien_ghi_de,
                        command=self._ve_lai_xem_truoc).grid(row=0, column=6, padx=(12, 6), pady=6)

        self.bien_chuan_hoa = tk.BooleanVar(value=True)
        ttk.Checkbutton(khung, text="Chuan hoa G-code (nen bat)",
                        variable=self.bien_chuan_hoa,
                        command=self._doi_chuan_hoa).grid(row=0, column=7, padx=6, pady=6)

    def _doi_chuan_hoa(self):
        if not self.bien_chuan_hoa.get():
            self.bien_ghi_de.set(False)
        self._ve_lai_xem_truoc()

    # ---------------------------------------------------------
    # DOC TRUOC + VE HINH
    # ---------------------------------------------------------
    def _lay_toc_do(self):
        """Doc 2 o toc do. Tra ve (cat, nhanh) hoac None neu sai."""
        try:
            cat = float(self.entry_toc_do_cat.get())
            nhanh = float(self.entry_toc_do_nhanh.get())
        except ValueError:
            return None
        if cat <= 0 or nhanh <= 0:
            return None
        return cat, nhanh

    def _doi_tab(self, su_kien=None):
        """Chuyen sang tab xem truoc / mo phong 3D thi tu ve lai neu G-code da doi."""
        try:
            chi_so = self.notebook.index(self.notebook.select())
        except tk.TclError:
            return
        if chi_so in (1, 2) and self._gcode_da_doi_tu_lan_ve():
            self._ve_lai_xem_truoc()

    def _gcode_da_doi_tu_lan_ve(self):
        return self.text_gcode.get("1.0", "end") != self.gcode_da_ve

    def _doc_duong_kinh(self):
        try:
            d = float(self.entry_duong_kinh.get())
            return d if d > 0 else self.duong_kinh_may
        except (ValueError, AttributeError):
            return self.duong_kinh_may

    def _ve_lai_xem_truoc(self):
        cac_dong = self.text_gcode.get("1.0", "end").splitlines()
        self.gcode_da_ve = self.text_gcode.get("1.0", "end")
        toc_do = self._lay_toc_do()
        if toc_do is None:
            toc_do = (TOC_DO_CAT_MAC_DINH, TOC_DO_NHANH_MAC_DINH)

        self.ket_qua_phan_tich = phan_tich_chuong_trinh(
            cac_dong,
            chuan_hoa=self.bien_chuan_hoa.get(),
            ghi_de_toc_do=self.bien_ghi_de.get(),
            toc_do_cat=toc_do[0],
            toc_do_nhanh=toc_do[1],
            che_do=self.che_do_may,
            duong_kinh=self._doc_duong_kinh(),
        )
        self._ve_hinh()
        self._ve_3d()
        self._cap_nhat_thong_ke()

    def _cap_nhat_thong_ke(self):
        kq = self.ket_qua_phan_tich
        if kq is None:
            return

        so_cat = sum(1 for d in kq.doan if d[4])
        so_nhanh = len(kq.doan) - so_cat
        if kq.doan:
            cac_x = [d[0] for d in kq.doan] + [d[2] for d in kq.doan]
            cac_a = [d[1] for d in kq.doan] + [d[3] for d in kq.doan]
            pham_vi = (f"X: {min(cac_x):.1f} -> {max(cac_x):.1f} mm     "
                       f"A: {min(cac_a):.1f} -> {max(cac_a):.1f} do")
        else:
            pham_vi = "khong co doan di chuyen nao"

        self.lbl_thong_ke.config(
            text=f"{so_cat} doan cat  |  {so_nhanh} doan chay nhanh  |  "
                 f"{len(kq.diem_moi)} diem moi  |  {kq.so_buoc_firmware} buoc firmware "
                 f"(bo dem {SUC_CHUA_BO_DEM}, nap dan)\n{pham_vi}")

        self.text_canh_bao.config(state="normal")
        self.text_canh_bao.delete("1.0", "end")
        if not kq.canh_bao:
            self.text_canh_bao.insert("end", "OK - khong phat hien van de nao.\n")
        else:
            for chu in kq.canh_bao:
                the = "nang" if ("QUA DAI" in chu or "KHONG ho tro" in chu) else None
                self.text_canh_bao.insert("end", chu + "\n", the)
        self.text_canh_bao.config(state="disabled")

    def _ve_hinh(self):
        canvas = self.canvas_xem
        canvas.delete("all")
        kq = self.ket_qua_phan_tich
        if kq is None or not kq.doan:
            canvas.create_text(canvas.winfo_width() // 2 or 200,
                               canvas.winfo_height() // 2 or 100,
                               text="Chua co duong cat de ve", fill="#999999")
            return

        rong = canvas.winfo_width()
        cao = canvas.winfo_height()
        if rong < 60 or cao < 60:
            return

        le_trai, le_phai, le_tren, le_duoi = 54, 16, 22, 34
        vung_rong = rong - le_trai - le_phai
        vung_cao = cao - le_tren - le_duoi

        cac_x = [d[0] for d in kq.doan] + [d[2] for d in kq.doan]
        cac_a = [d[1] for d in kq.doan] + [d[3] for d in kq.doan]
        x_min, x_max = min(cac_x), max(cac_x)
        a_min, a_max = min(cac_a), max(cac_a)
        if x_max - x_min < 1e-6:
            x_min -= 1
            x_max += 1
        if a_max - a_min < 1e-6:
            a_min -= 1
            a_max += 1

        def toa_do_man_hinh(gx, ga):
            px = le_trai + (gx - x_min) / (x_max - x_min) * vung_rong
            py = le_tren + (1 - (ga - a_min) / (a_max - a_min)) * vung_cao
            return px, py

        # ----- Khung + luoi -----
        canvas.create_rectangle(le_trai, le_tren, le_trai + vung_rong, le_tren + vung_cao,
                                outline="#cccccc")

        buoc_a = 90 if (a_max - a_min) > 180 else 45
        goc = int(a_min // buoc_a) * buoc_a
        while goc <= a_max:
            if goc >= a_min:
                _, py = toa_do_man_hinh(x_min, goc)
                canvas.create_line(le_trai, py, le_trai + vung_rong, py,
                                   fill="#eeeeee")
                canvas.create_text(le_trai - 6, py, text=f"{goc:g}°",
                                   anchor="e", fill="#666666", font=("Segoe UI", 8))
            goc += buoc_a

        for phan in range(5):
            gx = x_min + (x_max - x_min) * phan / 4.0
            px, _ = toa_do_man_hinh(gx, a_min)
            canvas.create_line(px, le_tren, px, le_tren + vung_cao, fill="#eeeeee")
            canvas.create_text(px, le_tren + vung_cao + 8, text=f"{gx:.0f}",
                               anchor="n", fill="#666666", font=("Segoe UI", 8))

        canvas.create_text(le_trai + vung_rong / 2, cao - 6,
                           text="X - doc theo ong (mm)", anchor="s",
                           fill="#444444", font=("Segoe UI", 8))
        # Nhan truc doc dat o goc tren-trai, khong dat giua truc de tranh de len so do.
        # Neo "sw" tu mep trai de khong bi cat mat chu.
        canvas.create_text(4, le_tren - 4, text="A (do)", anchor="sw",
                           fill="#444444", font=("Segoe UI", 8))

        # ----- Duong di: ve doan chay nhanh truoc, doan cat sau (de noi len tren) -----
        for la_cat_can_ve in (False, True):
            for x1, a1, x2, a2, la_cat in kq.doan:
                if la_cat != la_cat_can_ve:
                    continue
                px1, py1 = toa_do_man_hinh(x1, a1)
                px2, py2 = toa_do_man_hinh(x2, a2)
                if la_cat:
                    canvas.create_line(px1, py1, px2, py2, fill="#d62828", width=2)
                else:
                    canvas.create_line(px1, py1, px2, py2, fill="#9aa0a6", width=1, dash=(4, 3))

        # ----- Diem moi (M3) -----
        for gx, ga in kq.diem_moi:
            px, py = toa_do_man_hinh(gx, ga)
            canvas.create_oval(px - 4, py - 4, px + 4, py + 4,
                               fill="#f0ad4e", outline="#a06800")

        # ----- Diem bat dau / ket thuc -----
        px, py = toa_do_man_hinh(kq.doan[0][0], kq.doan[0][1])
        canvas.create_oval(px - 5, py - 5, px + 5, py + 5, outline="#1a7f37", width=2)
        px, py = toa_do_man_hinh(kq.doan[-1][2], kq.doan[-1][3])
        canvas.create_rectangle(px - 4, py - 4, px + 4, py + 4, outline="#0b5cad", width=2)

    # ---------------------------------------------------------
    # FILE G-CODE
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

        # DOC TRUOC: ve hinh + kiem tra ngay khi vua mo file
        self._ve_lai_xem_truoc()
        self.notebook.select(1)
        if self.ket_qua_phan_tich and self.ket_qua_phan_tich.co_loi_nang:
            messagebox.showwarning(
                "File co van de",
                "File nay co van de co the lam chuong trinh khong chay dung.\n"
                "Xem muc 'Kiem tra truoc khi chay' o tab Xem truoc.")

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
            self.root.after(300, lambda: self._gui_qua_serial("CFG;GET", ghi_log=False))
            self.root.after(800, self._hoi_vi_tri_dinh_ky)
        except Exception as loi:
            messagebox.showerror("Loi ket noi", f"Khong the ket noi toi {cong}:\n{loi}")

    def _hoi_vi_tri_dinh_ky(self):
        """Hoi POS moi 2 giay khi may DANG RANH, de vi tri va so xung luon dung.

        Khong hoi luc dang chay: moi lenh gui xuong deu lam ESP32 in ra UART,
        ma printf o giua chuoi cat se chan vong xuat xung gay vet chay.
        """
        if not self.dang_ket_noi:
            return
        if self.trang_thai in ("SAN_SANG", "DA_DUNG", "TAM_DUNG"):
            self._gui_qua_serial("POS", ghi_log=False)
        self.root.after(2000, self._hoi_vi_tri_dinh_ky)

    def _ngat_ket_noi(self):
        self.dang_doc = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.dang_ket_noi = False
        self.btn_ketnoi.config(text="Ket noi")
        self._ghi_log("[He thong] Da ngat ket noi", "he_thong")
        self._cap_nhat_trang_thai("CHUA_KETNOI")

    def _doc_serial_lien_tuc(self):
        """Chay o LUONG NEN - chi duoc bo du lieu vao hang doi, khong dung giao dien."""
        while self.dang_doc and self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting:
                    dong = self.ser.readline().decode(errors="ignore").strip()
                    if dong:
                        # Dat ket qua nap ngay tai day de luong nap thay duoc lien,
                        # khong phai cho luong giao dien xu ly xong
                        if dong.startswith("OK_NAP"):
                            self.ket_qua_nap = "OK"
                            try:   # "OK_NAP;<so_dong>;<so_buoc>: ..."
                                self.so_dong_da_nhan = int(dong.split(";")[1])
                            except (IndexError, ValueError):
                                pass
                        elif dong.startswith("LOI_NAP"):
                            self.ket_qua_nap = "LOI"
                        # --- Bao nhan tung dong khi NAP DAN ---
                        # "OK;<cho_trong>;<da_nhan>" tra ve sau MOI dong G-code.
                        # Day la co so de dieu tiet luu luong: may tinh chi gui
                        # tiep khi ESP32 con cho, nho vay bo dem khong bao gio
                        # tran ma cung khong bao gio can.
                        # "OK;<cho_trong>;<so_dong_da_nhan>" - ESP32 bao nhan theo
                        # LO (moi 8 dong) chu khong tung dong, de tiet kiem bang
                        # thong duong COM. So dong lay THANG tu ban tin nen bao
                        # theo lo hay tung dong deu cho ket qua nhu nhau.
                        if dong.startswith("OK;") or dong.startswith("BUF;"):
                            phan = dong.split(";")
                            try:
                                self.cho_trong_esp32 = int(phan[1])
                                self.so_dong_da_nhan = int(phan[2])
                            except (IndexError, ValueError):
                                pass
                            continue      # khong lam ngap khung log
                        if dong.startswith("OK_BEGIN;"):
                            try:
                                self.cho_trong_esp32 = int(dong.split(";")[1])
                            except (IndexError, ValueError):
                                pass
                            continue
                        self.hang_doi_su_kien.put(("esp32", dong))
            except Exception:
                break
            time.sleep(0.02)

    def _lay_su_kien_tu_hang_doi(self):
        """Chay o LUONG GIAO DIEN - lay du lieu cac luong nen gui len va hien thi."""
        try:
            while True:
                loai, noi_dung = self.hang_doi_su_kien.get_nowait()
                if loai == "esp32":
                    self._xu_ly_dong_tu_esp32(noi_dung)
                elif loai == "log":
                    self._ghi_log(noi_dung, "he_thong")
                elif loai == "nap_that_bai":
                    self._nap_that_bai(noi_dung)
        except queue.Empty:
            pass
        self.root.after(50, self._lay_su_kien_tu_hang_doi)

    def _xu_ly_dong_tu_esp32(self, dong):
        the = "loi" if dong.startswith("Loi:") else ("ok" if any(
            dong.startswith(tien_to) for tien_to in
            ("OK", "RUNNING", "ZEROED", "RESUMED", "Hoan thanh", "PLASMA_ON", "PLASMA_OFF")
        ) else None)
        self._ghi_log(f"[ESP32] {dong}", the)
        self._thu_cap_nhat_vi_tri(dong)
        self._thu_cap_nhat_so_xung(dong)
        self._thu_doc_che_do(dong)
        self._thu_cap_nhat_trang_thai_tu_log(dong)
        self._thu_to_mau_dong_loi(dong)

    def _thu_doc_che_do(self, dong):
        """Doc "CFG: che_do=2 duong_kinh_ong=60.0000" do lenh CFG;GET tra ve."""
        if not dong.startswith("CFG:"):
            return
        for cap in dong[len("CFG:"):].strip().split():
            if "=" not in cap:
                continue
            ten, gia_tri = cap.split("=", 1)
            try:
                if ten == "che_do":
                    self.che_do_may = int(gia_tri)
                    self._cap_nhat_nhan_toc_do()
                elif ten == "duong_kinh_ong":
                    self.duong_kinh_may = float(gia_tri)
                    self.entry_duong_kinh.delete(0, "end")
                    self.entry_duong_kinh.insert(0, _so_gon(self.duong_kinh_may))
                    self._ve_lai_xem_truoc()
            except ValueError:
                pass

    def _cap_nhat_nhan_toc_do(self):
        """Doi nhan o toc do cho dung don vi cua che do dang dung."""
        if self.che_do_may == 1:
            nhan = "Toc do (F - vong/phut dong co)  [Che do 1]"
            don_vi = "RPM"
        else:
            nhan = ("Toc do (F - mm/phut MO CAT LUOT TREN MAT ONG)  "
                    f"[Che do {self.che_do_may}]")
            don_vi = "mm/ph"
        self.khung_toc_do.config(text=nhan)
        self.lbl_dv_cat.config(text=don_vi)
        self.lbl_dv_nhanh.config(text=don_vi)

    def _thu_cap_nhat_so_xung(self, dong):
        """Doc dong "XUNG: X=12345 A=678" do lenh POS tra ve."""
        if not dong.startswith("XUNG: X="):
            return
        try:
            phan = dong[len("XUNG: X="):]
            x_str, con_lai = phan.split("A=", 1)
            a_str = con_lai.split()[0]
            self.lbl_xung.config(text=f"xung: X={int(x_str.strip())}  A={int(a_str)}")
        except (IndexError, ValueError):
            pass

    def _thu_cap_nhat_vi_tri(self, dong):
        if "Vi tri: X=" in dong and "A=" in dong:
            try:
                phan = dong.split("Vi tri: X=")[1]
                x_str, y_str = phan.split("A=")
                x_val = float(x_str.strip())
                y_val = float(y_str.strip())
                self.lbl_vitri.config(text=f"X = {x_val:.2f} mm     A = {y_val:.2f} do")
            except (IndexError, ValueError):
                pass

    def _thu_cap_nhat_trang_thai_tu_log(self, dong):
        # LUU Y: "Hoan thanh" la bao xong 1 BUOC, KHONG phai xong ca chuong trinh.
        # Truoc day dong nay bi hieu nham la da chay xong -> chuyen ve SAN SANG ->
        # nut PAUSE bi khoa ngay sau buoc dau tien, khong bam duoc luc dang chay.
        # Firmware nay in "XONG_CHUONG_TRINH" khi that su het chuong trinh.
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
        elif dong.startswith("XONG_CHUONG_TRINH"):
            self._cap_nhat_trang_thai("SAN_SANG")
        elif dong.startswith("LOI_CAN_BO_DEM"):
            # ESP32 chay het buoc trong bo dem ma may tinh chua nap kip. Firmware
            # da tu tat mo cat va dung lai - bao cho nguoi dung biet ngay.
            self._cap_nhat_trang_thai("TAM_DUNG")
            self._ghi_log("[He thong] May tinh nap khong kip - may da tu tat mo cat "
                          "va tam dung. Bam TIEP TUC khi san sang.", "loi")
        elif dong.startswith("He thong: da het dieu kien loi"):
            self._cap_nhat_trang_thai("SAN_SANG")

    def _thu_to_mau_dong_loi(self, dong):
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
            self.text_gcode.tag_add("dong_loi", vi_tri, f"{vi_tri}+{len(noi_dung_loi)}c")
            self.text_gcode.see(vi_tri)

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
        dang_nap = self.trang_thai == "DANG_NAP"
        tam_dung = self.trang_thai == "TAM_DUNG"
        loi = self.trang_thai == "LOI"
        san_sang_jog = (self.dang_ket_noi and not dang_chay and not dang_nap
                        and not tam_dung and not loi)

        self.btn_run.config(state="normal" if (self.dang_ket_noi and not dang_chay
                                               and not dang_nap and not loi) else "disabled")
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
        """NAP truoc, CHO ESP32 xac nhan OK_NAP, roi moi CHAY.

        Chay trong luong nen de giao dien khong bi dong bang trong luc nap -
        neu dong bang thi bam STOP cung khong an, rat nguy hiem khi may dang chay.
        """
        if not self.dang_ket_noi:
            messagebox.showwarning("Chua ket noi", "Vui long ket noi Serial truoc.")
            return

        if self._lay_toc_do() is None:
            messagebox.showwarning("Sai toc do", "Toc do cat va toc do chay khong tai phai la so > 0.")
            return

        self._ve_lai_xem_truoc()
        kq = self.ket_qua_phan_tich
        cac_dong = [d for d in kq.dong_chuan_hoa if d.strip()]

        if not cac_dong:
            messagebox.showwarning("Trong", "Chua co dong G-code nao de chay.")
            return

        if kq.co_loi_nang:
            if not messagebox.askyesno(
                    "File co van de",
                    "Phan kiem tra truoc phat hien van de nghiem trong "
                    "(xem tab Xem truoc).\n\nVan tiep tuc nap va chay?"):
                return

        self.text_gcode.tag_remove("dong_loi", "1.0", "end")
        self.ket_qua_nap = None          # se duoc luong doc serial dat khi co tra loi
        self._cap_nhat_trang_thai("DANG_NAP")
        threading.Thread(target=self._nap_roi_chay_nen, args=(cac_dong,), daemon=True).start()

    def _nap_roi_chay_nen(self, cac_dong):
        """NAP DAN (streaming) - chay o LUONG NEN.

        Truoc day phai nap TOAN BO bai vao RAM ESP32 roi moi duoc bam chay, nen
        bai dai vua ton RAM vua bat nguoi dung ngoi cho (300 buoc mat gan 2 giay,
        bai vai nghin dong thi khong nap noi).

        Bay gio: gui truoc BUOC_DAY_TRUOC_KHI_CHAY dong dau, bam RUN NGAY, roi
        vua chay vua nap dan phan con lai cho toi het bai.

        Dieu tiet luu luong: ESP32 tra "OK;<cho_trong>" sau moi dong. May tinh
        chi gui tiep khi <cho_trong> con du - nho vay bo dem KHONG BAO GIO TRAN
        (mat lenh) ma cung khong bao gio CAN (may dung giua duong cat).
        """
        bao = self.hang_doi_su_kien.put
        tong = len(cac_dong)
        try:
            self.cho_trong_esp32 = 0
            self.so_dong_da_nhan = 0
            self.dang_nap_dan = True

            bao(("log", f"[He thong] Nap dan: gui truoc {self.SO_DONG_NAP_TRUOC} dong "
                        f"roi vua chay vua nap (tong {tong} dong)."))
            self.ser.write(b"PROG;BEGIN\n")

            # Cho ESP32 xac nhan da san sang (OK_BEGIN) - cung la luc biet suc chua
            han = time.time() + 3.0
            while time.time() < han and self.cho_trong_esp32 <= 0:
                time.sleep(0.01)
            if self.cho_trong_esp32 <= 0:
                bao(("nap_that_bai", "ESP32 khong tra loi PROG;BEGIN. Kiem tra lai ket noi."))
                return

            da_gui = 0
            da_bam_chay = False
            moc_bao_cao = 0        # so dong cua lan bao tien do gan nhat
            moc_hoi_buf = time.time()   # lan cuoi ESP32 con cho de nhan them

            while da_gui < tong:
                if self.ket_qua_nap == "LOI":
                    bao(("nap_that_bai",
                         "ESP32 tu choi chuong trinh (LOI_NAP). Xem log de biet dong nao sai."))
                    return
                if not self.dang_nap_dan:      # nguoi dung bam STOP giua chung
                    bao(("log", "[He thong] Da huy nap dan theo yeu cau."))
                    return

                # Con bao nhieu dong dang "bay tren duong" chua duoc bao nhan.
                # Moi dong co the sinh toi 2 buoc (vd M3 + G1) nen tru gap doi
                # cho chac an.
                chua_bao_nhan = da_gui - self.so_dong_da_nhan
                cho_thuc = self.cho_trong_esp32 - chua_bao_nhan * 2

                if cho_thuc < self.NGUONG_GUI_TIEP:
                    # ESP32 dang day - doi dong co chay bot roi gui tiep.
                    # QUAN TRONG: ESP32 chi bao cho trong khi tra loi mot dong.
                    # Neu may tinh ngung gui va cu ngoi doi thi khong bao gio
                    # biet bo dem da voi ra -> ket cung ca hai ben. Vi vay phai
                    # CHU DONG hoi bang lenh BUF trong luc cho.
                    self.ser.write(b"BUF\n")
                    time.sleep(0.02)
                    if time.time() - moc_hoi_buf > self.CHO_TOI_DA_S:
                        bao(("nap_that_bai",
                             "ESP32 khong voi bo dem sau %.0f giay - may co the da dung. "
                             "Da huy nap." % self.CHO_TOI_DA_S))
                        return
                    continue
                moc_hoi_buf = time.time()

                # Gui mot lo lien tiep, khong nghi giua tung dong: doan nay dam
                # bao khong lam tran nen khong can chen sleep - day chinh la cho
                # truoc kia tu lam cham minh 4ms/dong.
                lo = min(cho_thuc, self.LO_GUI_TOI_DA, tong - da_gui)
                goi = "".join(d + "\n" for d in cac_dong[da_gui:da_gui + lo])
                self.ser.write(goi.encode())
                da_gui += lo

                # ----- Da du buoc dem dau tien -> CHAY NGAY, khong cho nap het -----
                if not da_bam_chay and da_gui >= min(self.SO_DONG_NAP_TRUOC, tong):
                    # cho toi khi ESP32 that su da nhan du so dong dau, roi moi chay
                    han = time.time() + 5.0
                    while (time.time() < han and
                           self.so_dong_da_nhan < min(self.SO_DONG_NAP_TRUOC, tong) and
                           self.ket_qua_nap != "LOI"):
                        time.sleep(0.005)
                    if self.ket_qua_nap == "LOI":
                        bao(("nap_that_bai",
                             "ESP32 tu choi chuong trinh (LOI_NAP). Xem log de biet dong nao sai."))
                        return
                    self.ser.write(b"RUN\n")
                    da_bam_chay = True
                    bao(("log", f"[He thong] Da dem san {self.so_dong_da_nhan} dong - "
                                f"BAT DAU CHAY, phan con lai nap tiep trong luc chay."))

                if da_gui - moc_bao_cao >= 200:
                    moc_bao_cao = da_gui
                    bao(("log", f"[He thong] ... da nap {da_gui}/{tong} dong"))

            # PROG;END di SAU cac dong G-code tren cung mot duong truyen nen
            # ESP32 chac chan xu ly no cuoi cung - khong can cho bao nhan tung
            # dong nua, cu gui roi doi OK_NAP la du.
            self.ser.write(b"PROG;END\n")

            han = time.time() + 15.0
            while time.time() < han and self.ket_qua_nap is None:
                time.sleep(0.02)
            if self.ket_qua_nap == "LOI":
                bao(("nap_that_bai",
                     "ESP32 tu choi chuong trinh (LOI_NAP). Xem log de biet dong nao sai."))
                return
            if self.ket_qua_nap is None:
                bao(("nap_that_bai", "ESP32 khong xac nhan nap xong sau 15 giay."))
                return
            if self.so_dong_da_nhan != tong:
                bao(("log", f"[He thong] Canh bao: da gui {tong} dong nhung ESP32 "
                            f"bao nhan {self.so_dong_da_nhan} dong."))

            if not da_bam_chay:
                # Bai qua ngan (it hon so dong nap truoc): nap xong het roi moi chay
                self.ser.write(b"RUN\n")
                bao(("log", "[He thong] Da nap xong ca bai, bat dau CHAY."))
            else:
                bao(("log", f"[He thong] Da nap xong toan bo {tong} dong, may dang chay tiep."))
        except Exception as loi:
            bao(("nap_that_bai", f"Loi khi gui du lieu: {loi}"))
        finally:
            self.dang_nap_dan = False

    def _nap_that_bai(self, chu):
        self._cap_nhat_trang_thai("SAN_SANG")
        self._ghi_log(f"[He thong] NAP THAT BAI: {chu}", "loi")
        messagebox.showerror("Nap that bai", chu + "\n\nMay CHUA chay.")

    def _jog(self, truc, dau):
        try:
            buoc = float(self.entry_jog_x.get() if truc == "X" else self.entry_jog_y.get())
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
        if messagebox.askyesno(
                "Xac nhan dat goc",
                "Dat vi tri HIEN TAI lam diem goc (0, 0)?\n\n"
                "Chuong trinh G-code sau do se tinh toan tu diem nay."):
            self._gui_qua_serial("ZERO")

    def _gui_stop(self):
        self.dang_nap_dan = False    # huy luon phien nap dan neu dang nap do
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
