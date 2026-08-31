"""PHAN MEM MAY CAT ONG PLASMA CNC

Mot cua so lo het viec hang ngay:
  - THU VIEN MOI NOI: 3 kieu ghep, nhap chieu dai khuc roi bam Them - phan mem
    tu xep cac nhat cat noi tiep nhau tren cay ong
  - THE XEP 2D: nhin cay ong nam thang, keo tha tung nhat cat, co thuoc do
  - MO PHONG 3D: dung nhu may that - dau cat dung yen, ong quay va truot ra vao
  - MO FILE .NC san co tu phan mem CAM
  - Chay / tam dung / chay tiep (co hoi duc lo lai) / dung

Cai dat phan cung (chan GPIO, so xung moi vong, dao chieu truc) nam o file
rieng "cnc_settings.pyw" - mo tu menu Settings.

Yeu cau: pip install pyserial
"""

import copy
import os
import sys
import time
import queue
import subprocess
import tkinter as tk
from tkinter import ttk, messagebox, filedialog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from loi import thu_vien_moi_noi as tv
from loi import phan_tich_gcode as pg
from loi import ket_noi as kn
from loi.ve_3d import MoPhong3D
from loi.xep_2d import Xep2D

TEN_PHAN_MEM = "May cat ong plasma CNC"

# Duong COM luon chay 115200 - muc on dinh nhat voi moi chip USB-UART.
BAUD_CO_DINH = 115200

MAU = {
    "nen": "#eef1f5", "khung": "#ffffff", "vien": "#c3cad4",
    "chu": "#1d2530", "chu_mo": "#6b7686", "nhan": "#2f6fb8",
    "chay": "#2f9e44", "dung": "#c92a2a", "cho": "#e8890c",
    "term_nen": "#12161b", "term_chu": "#c8d3de",
}

TRANG_THAI = {
    "CHUA_KETNOI": ("CHUA KET NOI", "#868e96"),
    "SAN_SANG":    ("SAN SANG",     "#2f9e44"),
    "DANG_NAP":    ("DANG NAP...",  "#7048e8"),
    "DANG_CHAY":   ("DANG CHAY",    "#1971c2"),
    "TAM_DUNG":    ("TAM DUNG",     "#e8890c"),
    "LOI":         ("LOI / EMG",    "#c92a2a"),
    "DA_DUNG":     ("DA DUNG",      "#495057"),
}

TEN_CHE_DO = {
    1: "Mode 1 - X tinh bang mm, A tinh bang do",
    2: "Mode 2 - nhap duong kinh, giu toc do mo cat khong doi",
    3: "Mode 3 - nhap duong kinh, ca hai truc tinh bang mm",
}

DUOI_FILE = [("File G-code", "*.nc *.gcode *.tap *.txt"), ("Tat ca file", "*.*")]


# =============================================================================
# VE BIEU TUONG 3 KIEU GHEP (net ve ky thuat, khong can file anh)
# =============================================================================
def ve_bieu_tuong(canvas, ma, chon=False):
    canvas.delete("all")
    w, h = int(canvas["width"]), int(canvas["height"])
    canvas.create_rectangle(1, 1, w - 1, h - 1,
                            fill="#dbe7f5" if chon else "#ffffff",
                            outline=MAU["nhan"] if chon else MAU["vien"],
                            width=2 if chon else 1)
    m, n = w / 2, h / 2
    net, do = MAU["chu"], "#d9480f"

    if ma == "goc_90":                       # hai ong gap vuong goc
        canvas.create_polygon(9, n + 15, m + 5, n + 15, m + 5, 9,
                              m - 7, 9, m - 7, n + 3, 9, n + 3,
                              outline=net, fill="")
        canvas.create_line(m - 7, n + 3, m + 5, n + 15, fill=do, width=2)
    elif ma == "goc_45":                     # hai ong gap 45 do
        canvas.create_polygon(9, n + 14, m + 8, n + 14, w - 10, 14,
                              w - 16, 7, m - 2, n + 2, 9, n + 2,
                              outline=net, fill="")
        canvas.create_line(m - 2, n + 2, m + 8, n + 14, fill=do, width=2)
    elif ma == "nhanh_t_90":                 # nhanh dam vuong goc vao giua ong
        canvas.create_rectangle(8, n + 7, w - 8, n + 19, outline=net)
        canvas.create_oval(5, n + 7, 11, n + 19, outline=net)
        canvas.create_rectangle(m - 7, 8, m + 7, n + 7, outline=net)
        canvas.create_arc(m - 7, n + 2, m + 7, n + 14, start=0, extent=180,
                          style="arc", outline=do, width=2)


# =============================================================================
# UNG DUNG
# =============================================================================
class UngDung:
    def __init__(self, root):
        self.root = root
        root.title(TEN_PHAN_MEM)
        root.geometry("1180x730")
        root.minsize(1024, 640)
        root.configure(bg=MAU["nen"])

        # ---- Ket noi ----
        self.hang_doi = queue.Queue()
        self.may = kn.KetNoiESP32(lambda loai, nd: self.hang_doi.put((loai, nd)))
        self.trang_thai = "CHUA_KETNOI"
        cac_cong = kn.danh_sach_cong()
        self.cong_com = tk.StringVar(value=cac_cong[0] if cac_cong else "COM3")

        # ---- Tham so bai ----
        self.che_do = tk.IntVar(value=1)
        self.duong_kinh = tk.DoubleVar(value=60.0)
        self.dai_cay_ong = tk.DoubleVar(value=1000.0)
        self.toc_do_cat = tk.DoubleVar(value=15.0)
        self.toc_do_nhanh = tk.DoubleVar(value=60.0)
        self.toc_do_tay = tk.DoubleVar(value=30.0)
        self.buoc_nhich = tk.DoubleVar(value=1.0)
        self.thoi_gian_duc_lo = tk.DoubleVar(value=0.8)
        self.dai_khuc = tk.DoubleVar(value=200.0)
        self.khe_cat = tk.DoubleVar(value=8.0)
        self.chua_dau = tk.DoubleVar(value=20.0)
        self.chay_thu = tk.BooleanVar(value=False)

        # ---- Bai ----
        self.cac_muc = []            # [{"ma","gia_tri","x"}, ...]
        self.file_ngoai = None       # {"ten": ..., "dong": [...]} hoac None
        self.lich_su = []            # cho Ctrl+Z
        self.lich_su_lam_lai = []
        self.bang_nho = []           # cho Ctrl+C / Ctrl+V
        self._dang_keo = False

        self.kieu_dang_chon = tv.THU_VIEN[0]
        self.o_tham_so = {}
        self.ket_qua = None
        self.mo_dang_bat = False
        self.moc_bat_dau = None
        self.tong_doan = 0
        self.doan_da_chay = 0
        self.che_do_an_cat = False

        self._xay_dung()
        self._chon_kieu(tv.THU_VIEN[0])
        self._cap_nhat_nhan_cong()
        self._cap_nhat_nut()
        self.root.after(50, self._doc_hang_doi)
        self.root.after(2000, self._hoi_vi_tri_dinh_ky)
        self.root.protocol("WM_DELETE_WINDOW", self._thoat)

    # ==================================================================
    # GIAO DIEN
    # ==================================================================
    def _xay_dung(self):
        self._menu()
        self._thanh_tren()

        than = tk.Frame(self.root, bg=MAU["nen"])
        than.pack(fill="both", expand=True, padx=6)
        self._cot_trai(than)
        self._giua(than)
        self._cot_phai(than)

        self._hang_nut()
        self._the_duoi()
        self._thanh_trang_thai()
        self._phim_tat()

    # ------------------------------------------------------------------
    def _menu(self):
        thanh = tk.Menu(self.root)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Mo file .NC...", command=self._mo_file, accelerator="Ctrl+O")
        m.add_command(label="Luu G-code...", command=self._luu_file, accelerator="Ctrl+S")
        m.add_separator()
        m.add_command(label="Thoat", command=self._thoat)
        thanh.add_cascade(label="File", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Cong COM va che do may...", command=self._hop_tham_so)
        m.add_separator()
        m.add_command(label="Toc do cat va toc do khong tai...", command=self._hop_toc_do)
        m.add_command(label="Thoi gian duc lo...", command=self._hop_duc_lo)
        thanh.add_cascade(label="Parameters", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Xep lai ca bai cho noi tiep", command=self._xep_lai)
        m.add_command(label="Kiem tra do dai cay ong", command=self._kiem_cay_ong)
        thanh.add_cascade(label="Nesting", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Hoi vi tri hien tai", command=lambda: self._gui("POS"))
        m.add_command(label="Hoi bo dem con trong", command=lambda: self._gui("BUF"))
        m.add_command(label="Doc cau hinh ESP32", command=lambda: self._gui("CFG;GET"))
        m.add_separator()
        m.add_command(label="Khoi dong lai ESP32", command=self._khoi_dong_lai)
        thanh.add_cascade(label="Diagnostics", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Buoc nhich va toc do tay...", command=self._hop_tay)
        m.add_separator()
        m.add_command(label="Cai dat phan cung (chan GPIO, hieu chuan)...",
                      command=self._mo_cai_dat_nang_cao)
        thanh.add_cascade(label="Settings", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Xem danh sach loi", command=lambda: self.the_duoi.select(2))
        m.add_command(label="Xoa danh sach loi", command=self._xoa_loi)
        thanh.add_cascade(label="Alarm", menu=m)

        self.root.config(menu=thanh)

    # ------------------------------------------------------------------
    def _thanh_tren(self):
        k = tk.Frame(self.root, bg=MAU["khung"], highlightbackground=MAU["vien"],
                     highlightthickness=1)
        k.pack(fill="x", padx=6, pady=(6, 4))

        self.nut_ket_noi = ttk.Button(k, text="Ket noi", width=11,
                                      command=self._toggle_ket_noi)
        self.nut_ket_noi.pack(side="left", padx=(8, 6), pady=6)
        self.nhan_cong = tk.Label(k, text="", bg=MAU["khung"], fg=MAU["chu_mo"],
                                  font=("Segoe UI", 8), width=32, anchor="w")
        self.nhan_cong.pack(side="left")

        ttk.Separator(k, orient="vertical").pack(side="left", fill="y", pady=4)
        ttk.Button(k, text="Tham so...", width=13,
                   command=self._hop_tham_so).pack(side="left", padx=10)

        self.nhan_che_do = tk.Label(k, text=TEN_CHE_DO[1], bg=MAU["khung"],
                                    fg=MAU["nhan"], font=("Segoe UI", 8, "bold"))
        self.nhan_che_do.pack(side="left", padx=6)

    # ------------------------------------------------------------------
    def _cot_trai(self, cha):
        cot = tk.Frame(cha, bg=MAU["nen"], width=250)
        cot.pack(side="left", fill="y")
        cot.pack_propagate(False)

        # Dieu khien tay duoc cap cho TRUOC, thu vien lay phan con lai
        khung_tay = self._khung(cot, "Dieu khien tay")
        khung_tay.pack(side="bottom", fill="x")

        k = self._khung(cot, "Thu vien moi noi")
        k.pack(side="top", fill="both", expand=True, pady=(0, 4))

        luoi = tk.Frame(k, bg=MAU["khung"])
        luoi.pack(fill="x", padx=6, pady=4)
        self.bieu_tuong = {}
        for i, kieu in enumerate(tv.THU_VIEN):
            c = tk.Canvas(luoi, width=72, height=54, highlightthickness=0,
                          bg=MAU["khung"], cursor="hand2")
            c.grid(row=0, column=i, padx=2)
            c.bind("<Button-1>", lambda e, kk=kieu: self._chon_kieu(kk))
            self.bieu_tuong[kieu.ma] = c

        self.nhan_kieu = tk.Label(k, text="", bg=MAU["khung"], fg=MAU["nhan"],
                                  font=("Segoe UI", 9, "bold"), anchor="w",
                                  wraplength=230, justify="left")
        self.nhan_kieu.pack(fill="x", padx=8)

        tk.Button(k, text="THEM VAO BAI", bg=MAU["nhan"], fg="white", relief="flat",
                  font=("Segoe UI", 9, "bold"), command=self._them_nhat_cat
                  ).pack(side="bottom", fill="x", padx=8, pady=(6, 8))
        self.khung_tham_so = tk.Frame(k, bg=MAU["khung"])
        self.khung_tham_so.pack(side="bottom", fill="x", padx=8)
        self.mo_ta_kieu = tk.Label(k, text="", bg=MAU["khung"], fg=MAU["chu_mo"],
                                   font=("Segoe UI", 8), anchor="nw",
                                   wraplength=230, justify="left")
        self.mo_ta_kieu.pack(side="top", fill="both", expand=True, padx=8, pady=(0, 2))

        # ----- Dieu khien tay: co ca TOC DO TAY ngay o day cho tien -----
        g = tk.Frame(khung_tay, bg=MAU["khung"])
        g.pack(padx=8, pady=(6, 2))
        for chu, ham, r, c in (("X-", lambda: self._jog("X", -1), 0, 0),
                               ("X+", lambda: self._jog("X", +1), 0, 1),
                               ("A nguoc", lambda: self._jog("A", -1), 1, 0),
                               ("A thuan", lambda: self._jog("A", +1), 1, 1)):
            ttk.Button(g, text=chu, width=11, command=ham).grid(row=r, column=c,
                                                                padx=2, pady=2)
        h = tk.Frame(khung_tay, bg=MAU["khung"])
        h.pack(fill="x", padx=8, pady=1)
        tk.Label(h, text="Toc do tay:", bg=MAU["khung"],
                 font=("Segoe UI", 8)).pack(side="left")
        ttk.Entry(h, width=6, textvariable=self.toc_do_tay).pack(side="left", padx=3)
        tk.Label(h, text="RPM", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8)).pack(side="left")
        h = tk.Frame(khung_tay, bg=MAU["khung"])
        h.pack(fill="x", padx=8, pady=1)
        tk.Label(h, text="Buoc nhich:", bg=MAU["khung"],
                 font=("Segoe UI", 8)).pack(side="left")
        ttk.Entry(h, width=5, textvariable=self.buoc_nhich).pack(side="left", padx=3)
        tk.Label(h, text="mm/do", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8)).pack(side="left")
        ttk.Button(khung_tay, text="DAT GOC 0 TAI DAY", command=self._dat_goc
                   ).pack(fill="x", padx=8, pady=(2, 8))

    # ------------------------------------------------------------------
    def _giua(self, cha):
        khung = tk.Frame(cha, bg=MAU["nen"])
        khung.pack(side="left", fill="both", expand=True, padx=6)
        self.the_giua = ttk.Notebook(khung)
        self.the_giua.pack(fill="both", expand=True)
        self.the_giua.bind("<<NotebookTabChanged>>", self._doi_the_giua)

        # ---------- 3D ----------
        t3 = tk.Frame(self.the_giua, bg=MAU["khung"])
        self.the_giua.add(t3, text="  Mo phong 3D  ")
        self.canvas3d = tk.Canvas(t3, bg="#1b1f24", highlightthickness=0)
        self.canvas3d.pack(fill="both", expand=True, padx=2, pady=2)
        self.mp = MoPhong3D(self.canvas3d)
        self.canvas3d.bind("<Configure>", lambda e: self._ve_3d())
        self.canvas3d.bind("<Button-1>", self._chuot_nhan_3d)
        self.canvas3d.bind("<B1-Motion>", self._chuot_keo_3d)
        self.canvas3d.bind("<Button-2>", self._day_3d_bat_dau)
        self.canvas3d.bind("<B2-Motion>", self._day_3d)
        self.canvas3d.bind("<MouseWheel>", lambda e: self._phong_3d(
            1.15 if e.delta > 0 else 1 / 1.15))
        self.canvas3d.bind("<Button-4>", lambda e: self._phong_3d(1.15))
        self.canvas3d.bind("<Button-5>", lambda e: self._phong_3d(1 / 1.15))

        d = tk.Frame(t3, bg=MAU["khung"])
        d.pack(fill="x", padx=4, pady=(0, 4))
        ttk.Button(d, text="Goc nhin mac dinh", command=self._goc_nhin_goc
                   ).pack(side="left", padx=2)
        self.nut_an_cat = tk.Button(d, text="An bot duong cat", relief="flat",
                                    bg="#dfe4ea", font=("Segoe UI", 8), padx=6,
                                    command=self._bat_tat_an_cat)
        self.nut_an_cat.pack(side="left", padx=2)
        ttk.Button(d, text="Hien lai het", command=self._hien_lai_het
                   ).pack(side="left", padx=2)
        self.thanh_truot = ttk.Scale(d, from_=0, to=100, command=self._truot_mo_phong)
        self.thanh_truot.pack(side="left", fill="x", expand=True, padx=8)

        # ---------- XEP 2D ----------
        t2 = tk.Frame(self.the_giua, bg=MAU["khung"])
        self.the_giua.add(t2, text="  Xep tren cay ong  ")
        self.canvas2d = tk.Canvas(t2, bg="#161a1f", highlightthickness=0)
        self.canvas2d.pack(fill="both", expand=True, padx=2, pady=2)
        self.xep = Xep2D(self.canvas2d, khi_chon=self._chon_khung_2d,
                         khi_keo=self._keo_khung_2d)
        self.canvas2d.bind("<Configure>", lambda e: self._ve_2d())
        self.canvas2d.bind("<Button-1>", lambda e: self.xep.bam(e.x, e.y))
        self.canvas2d.bind("<B1-Motion>", lambda e: self.xep.keo(e.x, e.y))
        self.canvas2d.bind("<ButtonRelease-1>", self._het_keo_2d)
        self.canvas2d.bind("<Button-2>", lambda e: self.xep.bat_dau_day(e.x))
        self.canvas2d.bind("<B2-Motion>", lambda e: self.xep.day_khung_nhin(e.x))
        self.canvas2d.bind("<ButtonRelease-2>", lambda e: self.xep.het_day())
        self.canvas2d.bind("<MouseWheel>", lambda e: self.xep.phong_to(
            1.2 if e.delta > 0 else 1 / 1.2, e.x))
        self.canvas2d.bind("<Button-4>", lambda e: self.xep.phong_to(1.2, e.x))
        self.canvas2d.bind("<Button-5>", lambda e: self.xep.phong_to(1 / 1.2, e.x))

        d = tk.Frame(t2, bg=MAU["khung"])
        d.pack(fill="x", padx=4, pady=(0, 4))
        ttk.Button(d, text="Vua khung hinh", command=self.xep.vua_khung_hinh
                   ).pack(side="left", padx=2)
        ttk.Button(d, text="Xep lai noi tiep", command=self._xep_lai
                   ).pack(side="left", padx=2)
        tk.Label(d, text="Dai cay ong:", bg=MAU["khung"],
                 font=("Segoe UI", 8)).pack(side="left", padx=(12, 2))
        o = ttk.Entry(d, width=8, textvariable=self.dai_cay_ong)
        o.pack(side="left")
        o.bind("<Return>", lambda e: self._ve_lai())
        tk.Label(d, text="mm", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8)).pack(side="left", padx=(2, 10))
        tk.Label(d, text="Cach nhau:", bg=MAU["khung"],
                 font=("Segoe UI", 8)).pack(side="left", padx=(4, 2))
        ttk.Entry(d, width=5, textvariable=self.khe_cat).pack(side="left")
        tk.Label(d, text="mm", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8)).pack(side="left", padx=2)

    # ------------------------------------------------------------------
    def _cot_phai(self, cha):
        cot = tk.Frame(cha, bg=MAU["nen"], width=214)
        cot.pack(side="left", fill="y")
        cot.pack_propagate(False)
        self.cot_phai = cot

        # Khung nay doi vai tro theo the dang mo: vi tri MAY, hoac vi tri NHAT CAT
        self.khung_vi_tri = self._khung(cot, "Vi tri may")
        self.nhan_x = self._o_so(self.khung_vi_tri, "X", "mm")
        self.nhan_a = self._o_so(self.khung_vi_tri, "A", "do")

        self.khung_nhat_cat = self._khung(cot, "Vi tri nhat cat")
        tk.Label(self.khung_nhat_cat, text="Cach dau ong xa nhat:", bg=MAU["khung"],
                 fg=MAU["chu_mo"], font=("Segoe UI", 8), anchor="w"
                 ).pack(fill="x", padx=8, pady=(4, 1))
        h = tk.Frame(self.khung_nhat_cat, bg=MAU["khung"])
        h.pack(fill="x", padx=8)
        self.o_khoang_cach = tk.StringVar(value="")
        self.entry_khoang_cach = ttk.Entry(h, textvariable=self.o_khoang_cach,
                                           font=("Consolas", 13), width=8)
        self.entry_khoang_cach.pack(side="left", fill="x", expand=True)
        self.entry_khoang_cach.bind("<Return>", lambda e: self._ap_khoang_cach())
        tk.Label(h, text="mm", bg=MAU["khung"], fg=MAU["chu_mo"]).pack(side="left", padx=3)
        ttk.Button(self.khung_nhat_cat, text="Ap dung", command=self._ap_khoang_cach
                   ).pack(fill="x", padx=8, pady=(3, 1))
        self.nhan_nhat_cat = tk.Label(self.khung_nhat_cat, text="Chua chon nhat cat",
                                      bg=MAU["khung"], fg=MAU["chu_mo"],
                                      font=("Segoe UI", 7), wraplength=192,
                                      justify="left", anchor="w")
        self.nhan_nhat_cat.pack(fill="x", padx=8, pady=(0, 4))

        self.khung_vi_tri.pack(fill="x", pady=(0, 4))

        k = self._khung(cot, "Tien do")
        k.pack(fill="x", pady=(0, 4))
        self.thanh_tien_do = ttk.Progressbar(k, maximum=100)
        self.thanh_tien_do.pack(fill="x", padx=8, pady=(8, 2))
        self.nhan_phan_tram = tk.Label(k, text="0%", bg=MAU["khung"],
                                       fg=MAU["chu_mo"], font=("Segoe UI", 8))
        self.nhan_phan_tram.pack(anchor="e", padx=8)
        self.nhan_toc_do = self._dong(k, "Toc do cat", "-")
        self.nhan_thoi_gian = self._dong(k, "Thoi gian chay", "00:00:00")
        tk.Frame(k, bg=MAU["khung"], height=6).pack()

        k = self._khung(cot, "Kich thuoc bai")
        k.pack(fill="x")
        h = tk.Frame(k, bg=MAU["khung"])
        h.pack(fill="x", padx=8, pady=(6, 2))
        tk.Label(h, text="Duong kinh ong:", bg=MAU["khung"],
                 font=("Segoe UI", 8)).pack(side="left")
        o = ttk.Entry(h, width=6, textvariable=self.duong_kinh)
        o.pack(side="left", padx=3)
        o.bind("<Return>", lambda e: self._ap_duong_kinh())
        tk.Label(h, text="mm", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8)).pack(side="left")
        ttk.Button(k, text="Ap dung duong kinh", command=self._ap_duong_kinh
                   ).pack(fill="x", padx=8, pady=(0, 4))
        self.nhan_pham_vi = self._dong(k, "Bai chiem doc ong", "-")
        self.nhan_so_dong = self._dong(k, "So dong G-code", "0")
        self.nhan_bo_dem = self._dong(k, "Buoc / bo dem", "0")
        tk.Frame(k, bg=MAU["khung"], height=6).pack()

    def _o_so(self, cha, ten, don_vi):
        h = tk.Frame(cha, bg=MAU["khung"])
        h.pack(fill="x", padx=8, pady=4)
        tk.Label(h, text=ten + ":", bg=MAU["khung"], font=("Segoe UI", 10, "bold"),
                 width=2, anchor="w").pack(side="left")
        nhan = tk.Label(h, text="0.00", bg="#f6f8fa", fg=MAU["chu"], anchor="e",
                        font=("Consolas", 13), relief="sunken", bd=1, width=9)
        nhan.pack(side="left", fill="x", expand=True, padx=4)
        tk.Label(h, text=don_vi, bg=MAU["khung"], fg=MAU["chu_mo"],
                 width=3, anchor="w").pack(side="left")
        return nhan

    def _dong(self, cha, ten, gia_tri):
        h = tk.Frame(cha, bg=MAU["khung"])
        h.pack(fill="x", padx=8, pady=1)
        tk.Label(h, text=ten, bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8), anchor="w").pack(side="left")
        nhan = tk.Label(h, text=gia_tri, bg=MAU["khung"], fg=MAU["chu"],
                        font=("Segoe UI", 8, "bold"), anchor="e")
        nhan.pack(side="right")
        return nhan

    # ------------------------------------------------------------------
    def _hang_nut(self):
        k = tk.Frame(self.root, bg=MAU["nen"])
        k.pack(fill="x", padx=6, pady=6)
        c = {"side": "left", "expand": True, "fill": "x", "padx": 3}
        self.nut_mo = tk.Button(k, text="Mo .NC", command=self._mo_file, **self._kn())
        self.nut_mo.pack(**c)
        self.nut_goc = tk.Button(k, text="Ve goc 0", command=self._ve_goc, **self._kn())
        self.nut_goc.pack(**c)
        self.nut_thu = tk.Button(k, text="Chay thu", command=self._bat_tat_chay_thu,
                                 **self._kn())
        self.nut_thu.pack(**c)
        self.nut_mo_cat = tk.Button(k, text="Bat mo", command=self._bat_tat_mo, **self._kn())
        self.nut_mo_cat.pack(**c)
        self.nut_chay = tk.Button(k, text="CHAY", command=self._chay,
                                  **self._kn(MAU["chay"], "white"))
        self.nut_chay.pack(**c)
        self.nut_dung_tam = tk.Button(k, text="TAM DUNG",
                                      command=lambda: self._gui("PAUSE"),
                                      **self._kn(MAU["cho"], "white"))
        self.nut_dung_tam.pack(**c)
        self.nut_tiep = tk.Button(k, text="CHAY TIEP", command=self._chay_tiep,
                                  **self._kn("#1971c2", "white"))
        self.nut_tiep.pack(**c)
        self.nut_dung = tk.Button(k, text="DUNG (Esc)", command=self._dung,
                                  **self._kn(MAU["dung"], "white"))
        self.nut_dung.pack(**c)

    @staticmethod
    def _kn(nen=None, chu=None):
        return {"bg": nen or "#dfe4ea", "fg": chu or MAU["chu"], "relief": "flat",
                "font": ("Segoe UI", 9, "bold"), "pady": 7,
                "activebackground": nen or "#cdd4dd", "disabledforeground": "#a9b1bb"}

    # ------------------------------------------------------------------
    def _the_duoi(self):
        self.the_duoi = ttk.Notebook(self.root, height=150)
        self.the_duoi.pack(fill="both", padx=6, pady=(0, 4))

        # ----- EDIT -----
        t = tk.Frame(self.the_duoi, bg=MAU["khung"])
        self.the_duoi.add(t, text="  Edit (ve bai)  ")

        trai = tk.Frame(t, bg=MAU["khung"])
        trai.pack(side="left", fill="both", expand=True, padx=4, pady=4)
        self.bang = ttk.Treeview(trai, columns=("kieu", "vi_tri", "so_do"),
                                 show="headings", height=6, selectmode="extended")
        for cot, chu, rong in (("kieu", "Noi dung", 150), ("vi_tri", "Cach dau xa", 82),
                               ("so_do", "So do", 230)):
            self.bang.heading(cot, text=chu)
            self.bang.column(cot, width=rong, anchor="w")
        self.bang.pack(side="left", fill="both", expand=True)
        self.bang.bind("<<TreeviewSelect>>", self._chon_dong_bang)
        cot_nut = tk.Frame(trai, bg=MAU["khung"])
        cot_nut.pack(side="left", fill="y", padx=4)
        for chu, ham in (("Len", lambda: self._doi_cho(-1)),
                         ("Xuong", lambda: self._doi_cho(+1)),
                         ("Xoa", self._xoa_muc), ("Xoa het", self._xoa_het)):
            ttk.Button(cot_nut, text=chu, width=8, command=ham).pack(pady=1)

        phai = tk.Frame(t, bg=MAU["khung"])
        phai.pack(side="left", fill="both", expand=True, padx=4, pady=4)
        h = tk.Frame(phai, bg=MAU["khung"])
        h.pack(fill="x")
        tk.Label(h, text="G-code", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8)).pack(side="left")
        tk.Button(h, text="NAP LAI G-CODE (F5)", bg=MAU["nhan"], fg="white",
                  relief="flat", font=("Segoe UI", 8, "bold"), padx=8,
                  command=self._ve_lai).pack(side="right")
        khung_g = tk.Frame(phai)
        khung_g.pack(fill="both", expand=True, pady=(2, 0))
        self.o_gcode = tk.Text(khung_g, font=("Consolas", 9), wrap="none", undo=True)
        th = ttk.Scrollbar(khung_g, command=self.o_gcode.yview)
        self.o_gcode.config(yscrollcommand=th.set)
        th.pack(side="right", fill="y")
        self.o_gcode.pack(side="left", fill="both", expand=True)

        # ----- SYSTEM -----
        t = tk.Frame(self.the_duoi, bg=MAU["term_nen"])
        self.the_duoi.add(t, text="  System (terminal)  ")
        khung = tk.Frame(t, bg=MAU["term_nen"])
        khung.pack(fill="both", expand=True, padx=4, pady=(4, 0))
        self.term = tk.Text(khung, bg=MAU["term_nen"], fg=MAU["term_chu"],
                            font=("Consolas", 9), wrap="word", state="disabled")
        th = ttk.Scrollbar(khung, command=self.term.yview)
        self.term.config(yscrollcommand=th.set)
        th.pack(side="right", fill="y")
        self.term.pack(side="left", fill="both", expand=True)
        for the, mau in (("gui", "#74c0fc"), ("loi", "#ff8787"),
                         ("ok", "#8ce99a"), ("he_thong", "#ffd43b")):
            self.term.tag_config(the, foreground=mau)
        h = tk.Frame(t, bg=MAU["term_nen"])
        h.pack(fill="x", padx=4, pady=4)
        tk.Label(h, text=">", bg=MAU["term_nen"], fg="#74c0fc",
                 font=("Consolas", 10)).pack(side="left")
        self.o_lenh = tk.Entry(h, bg="#1c232b", fg=MAU["term_chu"], relief="flat",
                               font=("Consolas", 9), insertbackground="white")
        self.o_lenh.pack(side="left", fill="x", expand=True, padx=4)
        self.o_lenh.bind("<Return>", lambda e: self._gui_lenh_go())
        ttk.Button(h, text="Gui", width=6, command=self._gui_lenh_go).pack(side="left")
        ttk.Button(h, text="Xoa man hinh", width=13,
                   command=self._xoa_terminal).pack(side="left", padx=3)

        # ----- ALARM -----
        t = tk.Frame(self.the_duoi, bg=MAU["khung"])
        self.the_duoi.add(t, text="  Alarm (loi)  ")
        self.bang_loi = ttk.Treeview(t, columns=("gio", "noi_dung"),
                                     show="headings", height=7)
        self.bang_loi.heading("gio", text="Thoi diem")
        self.bang_loi.heading("noi_dung", text="Noi dung")
        self.bang_loi.column("gio", width=90, anchor="center")
        self.bang_loi.column("noi_dung", width=880, anchor="w")
        self.bang_loi.pack(fill="both", expand=True, padx=4, pady=4)
        self.bang_loi.tag_configure("nang", foreground=MAU["dung"])

    # ------------------------------------------------------------------
    def _thanh_trang_thai(self):
        k = tk.Frame(self.root, bg="#dfe4ea")
        k.pack(fill="x", side="bottom")
        self.nhan_trang_thai = tk.Label(k, text="CHUA KET NOI", bg="#868e96",
                                        fg="white", font=("Segoe UI", 9, "bold"),
                                        width=16)
        self.nhan_trang_thai.pack(side="left", padx=(0, 8), ipady=3)
        self.nhan_goi_y = tk.Label(k, text="", bg="#dfe4ea", fg=MAU["chu_mo"],
                                   font=("Segoe UI", 8))
        self.nhan_goi_y.pack(side="left")

    @staticmethod
    def _khung(cha, tieu_de):
        return tk.LabelFrame(cha, text=" " + tieu_de + " ", bg=MAU["khung"],
                             fg=MAU["chu"], font=("Segoe UI", 9, "bold"),
                             bd=1, relief="solid", labelanchor="nw")

    # ------------------------------------------------------------------
    def _phim_tat(self):
        r = self.root
        r.bind("<Control-o>", lambda e: self._mo_file())
        r.bind("<Control-s>", lambda e: self._luu_file())
        r.bind("<F5>", lambda e: self._ve_lai())
        r.bind("<Escape>", lambda e: self._dung())
        # Phim tat lam viec voi NHAT CAT dang chon (o bang Edit hoac the xep 2D)
        for phim, ham in (("<Control-z>", self._hoan_tac), ("<Control-Z>", self._hoan_tac),
                          ("<Control-y>", self._lam_lai), ("<Control-Y>", self._lam_lai),
                          ("<Control-c>", self._sao_chep), ("<Control-C>", self._sao_chep),
                          ("<Control-x>", self._cat_di), ("<Control-X>", self._cat_di),
                          ("<Control-v>", self._dan_vao), ("<Control-V>", self._dan_vao),
                          ("<Delete>", self._xoa_muc_phim)):
            r.bind_all(phim, ham)

    @staticmethod
    def _dang_go_chu(su_kien):
        """True neu con tro dang o trong mot o nhap - de khong cuop phim tat cua no."""
        return isinstance(getattr(su_kien, "widget", None),
                          (tk.Entry, ttk.Entry, tk.Text, ttk.Combobox))

    # ==================================================================
    # THU VIEN
    # ==================================================================
    def _chon_kieu(self, kieu):
        self.kieu_dang_chon = kieu
        for ma, c in self.bieu_tuong.items():
            ve_bieu_tuong(c, ma, chon=(ma == kieu.ma))
        self.nhan_kieu.config(text=kieu.ten)
        self.mo_ta_kieu.config(text=kieu.mo_ta)
        self.nhan_goi_y.config(text=f"{kieu.ten}: {kieu.mo_ta}")

        for w in self.khung_tham_so.winfo_children():
            w.destroy()
        self.o_tham_so = {}
        # Chieu dai khuc la cua RIENG tung nhat cat chu khong phai cua kieu ghep,
        # nen no luon co mat du chon kieu nao
        tk.Label(self.khung_tham_so, text="Chieu dai khuc ong", bg=MAU["khung"],
                 font=("Segoe UI", 8), anchor="w").grid(row=0, column=0, sticky="w", pady=1)
        ttk.Entry(self.khung_tham_so, width=8, textvariable=self.dai_khuc).grid(
            row=0, column=1, padx=3, pady=1)
        tk.Label(self.khung_tham_so, text="mm", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8), width=3, anchor="w").grid(row=0, column=2, sticky="w")
        for hang, ts in enumerate(kieu.tham_so, start=1):
            tk.Label(self.khung_tham_so, text=ts.nhan, bg=MAU["khung"],
                     font=("Segoe UI", 8), anchor="w").grid(row=hang, column=0,
                                                            sticky="w", pady=1)
            o = ttk.Entry(self.khung_tham_so, width=8)
            o.insert(0, f"{ts.mac_dinh:g}")
            o.grid(row=hang, column=1, padx=3, pady=1)
            tk.Label(self.khung_tham_so, text=ts.don_vi, bg=MAU["khung"],
                     fg=MAU["chu_mo"], font=("Segoe UI", 8), width=3,
                     anchor="w").grid(row=hang, column=2, sticky="w")
            self.o_tham_so[ts.ma] = o
        self.khung_tham_so.columnconfigure(0, weight=1)

    def _them_nhat_cat(self):
        kieu = self.kieu_dang_chon
        gia_tri = {}
        for ts in kieu.tham_so:
            chu = self.o_tham_so[ts.ma].get().strip().replace(",", ".")
            try:
                gia_tri[ts.ma] = float(chu)
            except ValueError:
                messagebox.showwarning("Sai so lieu",
                                       f"'{ts.nhan}' phai la so. Dang nhap: {chu!r}")
                return
        try:
            x = tv.vi_tri_ke_tiep(self.duong_kinh.get(), self.cac_muc, kieu.ma,
                                  gia_tri, float(self.dai_khuc.get()),
                                  float(self.khe_cat.get()), float(self.chua_dau.get()))
            kieu.sinh(self.duong_kinh.get(), gia_tri, x)      # thu sinh de bat loi ngay
        except (ValueError, tk.TclError) as loi:
            messagebox.showwarning("Khong lam duoc nhat cat nay", str(loi))
            self._them_loi(f"{kieu.ten}: {loi}")
            return
        self._ghi_lich_su()
        self.cac_muc.append({"ma": kieu.ma, "gia_tri": gia_tri, "x": x})
        self._bai_da_doi()

    # ==================================================================
    # BAI: BANG, LICH SU, SAO CHEP
    # ==================================================================
    def _ghi_lich_su(self):
        self.lich_su.append((copy.deepcopy(self.cac_muc), copy.deepcopy(self.file_ngoai)))
        if len(self.lich_su) > 60:
            self.lich_su.pop(0)
        self.lich_su_lam_lai.clear()

    def _hoan_tac(self, su_kien=None):
        if self._dang_go_chu(su_kien) or not self.lich_su:
            return
        self.lich_su_lam_lai.append((copy.deepcopy(self.cac_muc),
                                     copy.deepcopy(self.file_ngoai)))
        self.cac_muc, self.file_ngoai = self.lich_su.pop()
        self._bai_da_doi()
        self._ghi("Da hoan tac (Ctrl+Z).", "he_thong")

    def _lam_lai(self, su_kien=None):
        if self._dang_go_chu(su_kien) or not self.lich_su_lam_lai:
            return
        self.lich_su.append((copy.deepcopy(self.cac_muc), copy.deepcopy(self.file_ngoai)))
        self.cac_muc, self.file_ngoai = self.lich_su_lam_lai.pop()
        self._bai_da_doi()
        self._ghi("Da lam lai (Ctrl+Y).", "he_thong")

    def _sao_chep(self, su_kien=None):
        if self._dang_go_chu(su_kien):
            return
        chon = self._cac_nhat_cat_dang_chon()
        if not chon:
            return
        self.bang_nho = [copy.deepcopy(self.cac_muc[i]) for i in chon]
        self._ghi(f"Da sao chep {len(self.bang_nho)} nhat cat (Ctrl+C).", "he_thong")

    def _cat_di(self, su_kien=None):
        if self._dang_go_chu(su_kien):
            return
        chon = self._cac_nhat_cat_dang_chon()
        if not chon:
            return
        self.bang_nho = [copy.deepcopy(self.cac_muc[i]) for i in chon]
        self._ghi_lich_su()
        for i in sorted(chon, reverse=True):
            del self.cac_muc[i]
        self.xep.dang_chon = None
        self._bai_da_doi()
        self._ghi(f"Da cat {len(self.bang_nho)} nhat cat (Ctrl+X).", "he_thong")

    def _dan_vao(self, su_kien=None):
        if self._dang_go_chu(su_kien) or not self.bang_nho:
            return
        self._ghi_lich_su()
        for muc in self.bang_nho:
            moi = copy.deepcopy(muc)
            try:                     # dat noi tiep sau bai hien co
                moi["x"] = tv.vi_tri_ke_tiep(
                    self.duong_kinh.get(), self.cac_muc, moi["ma"], moi["gia_tri"],
                    float(self.dai_khuc.get()), float(self.khe_cat.get()),
                    float(self.chua_dau.get()))
            except (ValueError, tk.TclError):
                pass
            self.cac_muc.append(moi)
        self._bai_da_doi()
        self._ghi(f"Da dan {len(self.bang_nho)} nhat cat (Ctrl+V).", "he_thong")

    def _cac_nhat_cat_dang_chon(self):
        """Chi so cac NHAT CAT dang duoc chon (bo qua dong file .NC o dau bang)."""
        ra = []
        lech = 1 if self.file_ngoai else 0
        for iid in self.bang.selection():
            i = self.bang.index(iid) - lech
            if 0 <= i < len(self.cac_muc):
                ra.append(i)
        if not ra and self.xep.dang_chon is not None:
            if 0 <= self.xep.dang_chon < len(self.cac_muc):
                ra.append(self.xep.dang_chon)
        return sorted(set(ra))

    def _bai_da_doi(self):
        self._nap_lai_bang()
        self._sinh_gcode()

    def _nap_lai_bang(self):
        self.bang.delete(*self.bang.get_children())
        if self.file_ngoai:
            self.bang.insert("", "end", values=(
                f"[FILE] {self.file_ngoai['ten']}", "-",
                f"{len(self.file_ngoai['dong'])} dong doc tu file"))
        try:
            kq = tv.xep_bai(self.duong_kinh.get(), self.cac_muc)
            khung = [tv.khung_duong_cat(d) for d in kq.cac_duong]
        except (ValueError, tk.TclError):
            khung = [(0.0, 0.0, 0.0)] * len(self.cac_muc)
        dai_cay = self._so(self.dai_cay_ong, 1000.0)
        for i, muc in enumerate(self.cac_muc):
            kieu = tv.THEO_MA[muc["ma"]]
            so_do = "  ".join(
                f"{ts.nhan}={muc['gia_tri'].get(ts.ma, ts.mac_dinh):g}{ts.don_vi}"
                for ts in kieu.tham_so)
            self.bang.insert("", "end",
                             values=(f"{i + 1}. {kieu.ten}",
                                     f"{dai_cay - khung[i][1]:.1f} mm", so_do))

    def _chon_dong_bang(self, _=None):
        chon = self._cac_nhat_cat_dang_chon()
        self.xep.dang_chon = chon[0] if chon else None
        self._cap_nhat_o_nhat_cat()
        if self.the_giua.index("current") == 1:
            self.xep.ve()

    def _doi_cho(self, huong):
        chon = self._cac_nhat_cat_dang_chon()
        if len(chon) != 1:
            return
        i, j = chon[0], chon[0] + huong
        if not 0 <= j < len(self.cac_muc):
            return
        self._ghi_lich_su()
        self.cac_muc[i], self.cac_muc[j] = self.cac_muc[j], self.cac_muc[i]
        self.cac_muc[i]["x"], self.cac_muc[j]["x"] = \
            self.cac_muc[j]["x"], self.cac_muc[i]["x"]
        self._bai_da_doi()

    def _xoa_muc_phim(self, su_kien=None):
        if not self._dang_go_chu(su_kien):
            self._xoa_muc()

    def _xoa_muc(self):
        # Dong dau bang la file .NC (neu co) - xoa dong do la bo file ra khoi bai
        xoa_file = bool(self.file_ngoai) and any(
            self.bang.index(i) == 0 for i in self.bang.selection())
        chon = self._cac_nhat_cat_dang_chon()
        if not chon and not xoa_file:
            return
        self._ghi_lich_su()
        if xoa_file:
            self._ghi(f"Da bo file {self.file_ngoai['ten']} khoi bai.", "he_thong")
            self.file_ngoai = None
            self.root.title(TEN_PHAN_MEM)
        for i in sorted(chon, reverse=True):
            del self.cac_muc[i]
        self.xep.dang_chon = None
        self._bai_da_doi()

    def _xoa_het(self):
        if not self.cac_muc and not self.file_ngoai:
            return
        if not messagebox.askyesno("Xoa het", "Xoa toan bo bai?"):
            return
        self._ghi_lich_su()
        self.cac_muc, self.file_ngoai = [], None
        self.xep.dang_chon = None
        self.root.title(TEN_PHAN_MEM)
        self._bai_da_doi()

    def _xep_lai(self):
        """Xep lai toan bo nhat cat cho noi tiep nhau, giu nguyen thu tu va kieu."""
        if not self.cac_muc:
            return
        tam = []
        try:
            for muc in self.cac_muc:
                moi = copy.deepcopy(muc)
                moi["x"] = tv.vi_tri_ke_tiep(
                    self.duong_kinh.get(), tam, muc["ma"], muc["gia_tri"],
                    float(self.dai_khuc.get()), float(self.khe_cat.get()),
                    float(self.chua_dau.get()))
                tam.append(moi)
        except (ValueError, tk.TclError) as loi:
            messagebox.showwarning("Khong xep duoc", str(loi))
            return
        self._ghi_lich_su()
        self.cac_muc = tam
        self._bai_da_doi()
        self._ghi(f"Da xep lai {len(tam)} nhat cat, moi khuc {self.dai_khuc.get():g}mm, "
                  f"cach nhau {self.khe_cat.get():g}mm.", "he_thong")

    def _kiem_cay_ong(self):
        try:
            kq = tv.xep_bai(self.duong_kinh.get(), self.cac_muc,
                            dai_cay_ong=self._so(self.dai_cay_ong, 1000.0))
        except (ValueError, tk.TclError) as loi:
            messagebox.showwarning("Bai co van de", str(loi))
            return
        dai = self._so(self.dai_cay_ong, 1000.0)
        du = dai - kq.tong_dung
        messagebox.showinfo(
            "Do dai cay ong",
            f"Bai gom {len(self.cac_muc)} nhat cat.\n"
            f"Chiem toi {kq.tong_dung:.1f} mm tinh tu mam kep.\n\n"
            + (f"Cay ong {dai:g} mm con thua {du:.1f} mm."
               if du >= 0 else f"THIEU {abs(du):.1f} mm - cay ong khong du dai!"))

    # ==================================================================
    # SINH G-CODE + PHAN TICH
    # ==================================================================
    def _sinh_gcode(self):
        dong = []
        if self.file_ngoai:
            dong += self.file_ngoai["dong"]
        if self.cac_muc:
            try:
                kq = tv.xep_bai(self.duong_kinh.get(), self.cac_muc,
                                dai_cay_ong=self._so(self.dai_cay_ong, 1000.0))
            except (ValueError, tk.TclError) as loi:
                self._them_loi(str(loi))
                messagebox.showwarning("Bai co van de", str(loi))
                return
            for cb in kq.canh_bao:
                self._them_loi(cb)
            if dong:
                dong.append("(--- cac nhat cat ve tu thu vien ---)")
            dong += tv.sinh_gcode(
                kq.cac_duong, self.toc_do_cat.get(), self.toc_do_nhanh.get(),
                self.thoi_gian_duc_lo.get(), x_ve_cho=0.0,
                tieu_de=[f"{TEN_PHAN_MEM} - ong D{self.duong_kinh.get():g}",
                         f"{len(kq.cac_duong)} nhat cat"])
        self.o_gcode.delete("1.0", "end")
        self.o_gcode.insert("1.0", "\n".join(dong))
        self._ve_lai()

    def _ve_lai(self):
        cac_dong = self.o_gcode.get("1.0", "end").split("\n")
        self.ket_qua = pg.phan_tich_chuong_trinh(
            cac_dong, chuan_hoa=True, ghi_de_toc_do=False,
            toc_do_cat=self.toc_do_cat.get(), toc_do_nhanh=self.toc_do_nhanh.get(),
            che_do=self.che_do.get(), duong_kinh=self.duong_kinh.get())
        self.mp.dat_du_lieu(self.ket_qua.doan, self.duong_kinh.get(),
                            self._so(self.dai_cay_ong, 1000.0))
        self.mp.vi_tri_chay = None
        self._ve_3d()
        self._ve_2d()
        self._cap_nhat_thong_ke()
        for cb in self.ket_qua.canh_bao:
            if self.ket_qua.co_loi_nang:
                self._them_loi(cb)
            else:
                self._ghi("Ghi chu: " + cb)

    def _cap_nhat_thong_ke(self):
        kq = self.ket_qua
        if not kq:
            return
        self.tong_doan = len(kq.doan)
        self.nhan_so_dong.config(text=str(len(kq.dong_chuan_hoa)))
        self.nhan_bo_dem.config(text=f"{kq.so_buoc_firmware} / {pg.SUC_CHUA_BO_DEM}")
        if kq.doan:
            xs = [d[0] for d in kq.doan] + [d[2] for d in kq.doan]
            self.nhan_pham_vi.config(text=f"{min(xs):.0f} .. {max(xs):.0f} mm")
        else:
            self.nhan_pham_vi.config(text="-")
        self.thanh_truot.config(to=max(1, self.tong_doan))
        self.nhan_toc_do.config(
            text=f"{self.toc_do_cat.get():g} / {self.toc_do_nhanh.get():g} RPM")

    # ==================================================================
    # VE 3D
    # ==================================================================
    def _ve_3d(self):
        try:
            self.mp.ve()
        except Exception as loi:
            self._them_loi(f"Loi ve mo phong 3D: {loi}")

    def _chuot_nhan_3d(self, e):
        if self.che_do_an_cat:
            chi_so = self.mp.doan_gan_diem(e.x, e.y)
            if chi_so is not None:
                self.mp.bat_tat_nhom(self.mp.nhom_cua_doan(chi_so))
                self._ve_3d()
            return
        self._chuot = (e.x, e.y)

    def _chuot_keo_3d(self, e):
        if self.che_do_an_cat or not hasattr(self, "_chuot"):
            return
        dx, dy = e.x - self._chuot[0], e.y - self._chuot[1]
        self._chuot = (e.x, e.y)
        self.mp.canh_nhin.xoay_ngang += dx * 0.5
        self.mp.canh_nhin.xoay_doc -= dy * 0.5
        self.mp.canh_nhin.xoay_doc = max(-88, min(88, self.mp.canh_nhin.xoay_doc))
        self._ve_3d()

    def _day_3d_bat_dau(self, e):
        self._day_tu = (e.x, e.y)

    def _day_3d(self, e):
        if not hasattr(self, "_day_tu"):
            return
        self.mp.canh_nhin.day_ngang += e.x - self._day_tu[0]
        self.mp.canh_nhin.day_doc += e.y - self._day_tu[1]
        self._day_tu = (e.x, e.y)
        self._ve_3d()

    def _phong_3d(self, he_so):
        self.mp.canh_nhin.phong = max(0.25, min(8.0, self.mp.canh_nhin.phong * he_so))
        self._ve_3d()

    def _goc_nhin_goc(self):
        self.mp.canh_nhin.dat_lai()
        self._ve_3d()

    def _bat_tat_an_cat(self):
        self.che_do_an_cat = not self.che_do_an_cat
        self.nut_an_cat.config(
            text="Dang an: bam duong cat" if self.che_do_an_cat else "An bot duong cat",
            bg="#ffd43b" if self.che_do_an_cat else "#dfe4ea")
        self.canvas3d.config(cursor="tcross" if self.che_do_an_cat else "")
        self.nhan_goi_y.config(
            text="Bam vao mot duong cat trong mo phong de an no di cho de nhin."
            if self.che_do_an_cat else "")

    def _hien_lai_het(self):
        self.mp.hien_lai_het()
        self._ve_3d()

    def _truot_mo_phong(self, gia_tri):
        if not self.ket_qua or not self.ket_qua.doan:
            return
        self.mp.vi_tri_chay = int(float(gia_tri))
        self._ve_3d()

    # ==================================================================
    # THE XEP 2D
    # ==================================================================
    def _ve_2d(self):
        try:
            khung = []
            if self.cac_muc:
                kq = tv.xep_bai(self.duong_kinh.get(), self.cac_muc)
                for muc, duong in zip(self.cac_muc, kq.cac_duong):
                    x1, x2, _ = tv.khung_duong_cat(duong)
                    khung.append((x1, x2, duong.ten, muc["x"]))
            self.xep.dat_du_lieu(khung, self.duong_kinh.get(),
                                 self._so(self.dai_cay_ong, 1000.0))
            self.xep.ve()
        except (ValueError, tk.TclError) as loi:
            self._them_loi(str(loi))

    def _doi_the_giua(self, _=None):
        """Sang the XEP thi o ben phai doi thanh o nhap vi tri nhat cat."""
        if not hasattr(self, "khung_vi_tri"):
            return
        if self.the_giua.index("current") == 1:
            self.khung_vi_tri.pack_forget()
            self.khung_nhat_cat.pack(fill="x", pady=(0, 4))
            self.khung_nhat_cat.lift()
            self._ve_2d()
            self._cap_nhat_o_nhat_cat()
        else:
            self.khung_nhat_cat.pack_forget()
            self.khung_vi_tri.pack(fill="x", pady=(0, 4))
            self.khung_vi_tri.lift()
            self._ve_3d()
        # Hai khung nay nam tren cung nen phai day cac khung con lai xuong duoi
        for w in self.cot_phai.winfo_children():
            if w not in (self.khung_vi_tri, self.khung_nhat_cat):
                w.pack_forget()
                w.pack(fill="x", pady=(0, 4))

    def _chon_khung_2d(self, chi_so):
        self._cap_nhat_o_nhat_cat()
        cac = self.bang.get_children()
        lech = 1 if self.file_ngoai else 0
        if chi_so is not None and 0 <= chi_so + lech < len(cac):
            self.bang.selection_set(cac[chi_so + lech])

    def _keo_khung_2d(self, chi_so, x_tam_moi):
        if not 0 <= chi_so < len(self.cac_muc):
            return
        if not self._dang_keo:
            self._ghi_lich_su()          # chi ghi MOT lan cho ca lan keo
            self._dang_keo = True
        self.cac_muc[chi_so]["x"] = max(0.0, x_tam_moi)
        self._ve_2d()
        self._cap_nhat_o_nhat_cat()

    def _het_keo_2d(self, _=None):
        self.xep.nha()
        if self._dang_keo:
            self._dang_keo = False
            self._bai_da_doi()

    def _cap_nhat_o_nhat_cat(self):
        i = self.xep.dang_chon
        if i is None or not 0 <= i < len(self.cac_muc):
            self.o_khoang_cach.set("")
            self.nhan_nhat_cat.config(text="Chua chon nhat cat")
            return
        try:
            kq = tv.xep_bai(self.duong_kinh.get(), self.cac_muc)
            x1, x2, dai = tv.khung_duong_cat(kq.cac_duong[i])
        except (ValueError, tk.TclError, IndexError):
            return
        self.o_khoang_cach.set(f"{self.xep.khoang_cach_tu_goc(x1, x2):.1f}")
        self.nhan_nhat_cat.config(
            text=f"Nhat {i + 1}: {tv.THEO_MA[self.cac_muc[i]['ma']].ten}\n"
                 f"Khung rong {dai:.1f} mm\n"
                 f"Cach mam kep {x1:.1f} .. {x2:.1f} mm")

    def _ap_khoang_cach(self):
        i = self.xep.dang_chon
        if i is None or not 0 <= i < len(self.cac_muc):
            return
        try:
            kc = float(self.o_khoang_cach.get().strip().replace(",", "."))
        except ValueError:
            messagebox.showwarning("Sai so lieu", "Khoang cach phai la so.")
            return
        try:
            kq = tv.xep_bai(self.duong_kinh.get(), self.cac_muc)
            x1, x2, _ = tv.khung_duong_cat(kq.cac_duong[i])
        except (ValueError, tk.TclError, IndexError):
            return
        self._ghi_lich_su()
        self.cac_muc[i]["x"] = self.xep.tu_khoang_cach(kc, x2 - x1,
                                                       self.cac_muc[i]["x"], x2)
        self._bai_da_doi()

    # ==================================================================
    # THAM SO
    # ==================================================================
    def _hop_tham_so(self):
        hop = tk.Toplevel(self.root)
        hop.title("Tham so may")
        hop.transient(self.root)
        hop.resizable(False, False)
        hop.configure(bg=MAU["khung"])

        tk.Label(hop, text="Cong COM:", bg=MAU["khung"], anchor="w").grid(
            row=0, column=0, sticky="w", padx=12, pady=(12, 4))
        o_cong = ttk.Combobox(hop, width=16, textvariable=self.cong_com,
                              values=kn.danh_sach_cong())
        o_cong.grid(row=0, column=1, sticky="w", padx=4, pady=(12, 4))

        def lam_moi():
            cac = kn.danh_sach_cong()
            o_cong.config(values=cac)
            if cac and self.cong_com.get() not in cac:
                self.cong_com.set(cac[0])

        ttk.Button(hop, text="Lam moi", width=9, command=lam_moi).grid(
            row=0, column=2, padx=(4, 12), pady=(12, 4))

        tk.Label(hop, text="Toc do truyen:", bg=MAU["khung"], anchor="w").grid(
            row=1, column=0, sticky="w", padx=12, pady=4)
        tk.Label(hop, text=f"{BAUD_CO_DINH} baud  (co dinh - on dinh nhat)",
                 bg=MAU["khung"], fg=MAU["chu_mo"], anchor="w").grid(
            row=1, column=1, columnspan=2, sticky="w", padx=4, pady=4)

        ttk.Separator(hop, orient="horizontal").grid(
            row=2, column=0, columnspan=3, sticky="ew", padx=12, pady=8)
        tk.Label(hop, text="Che do lam viec:", bg=MAU["khung"],
                 font=("Segoe UI", 9, "bold"), anchor="w").grid(
            row=3, column=0, columnspan=3, sticky="w", padx=12)

        bien = tk.IntVar(value=self.che_do.get())
        for i, so in enumerate((1, 2, 3)):
            tk.Radiobutton(hop, text=TEN_CHE_DO[so], variable=bien, value=so,
                           bg=MAU["khung"], anchor="w", font=("Segoe UI", 9)).grid(
                row=4 + i, column=0, columnspan=3, sticky="w", padx=22, pady=1)

        tk.Label(hop, bg=MAU["khung"], fg=MAU["chu_mo"], justify="left", anchor="w",
                 wraplength=400, font=("Segoe UI", 8),
                 text="Mode 2 va 3 can duong kinh ong - nhap o khung \"Kich thuoc bai\" "
                      "ben phai man hinh chinh.\n"
                      "Doi cong COM chi co hieu luc o lan ket noi sau."
                 ).grid(row=7, column=0, columnspan=3, sticky="w", padx=12, pady=(8, 4))

        def luu():
            self.che_do.set(bien.get())
            self.nhan_che_do.config(text=TEN_CHE_DO[bien.get()])
            if self._dang_ket_noi():
                self._gui(f"CFG;MODE;{bien.get()}")
                if bien.get() in (2, 3):
                    self._gui(f"CFG;DUONGKINH;{self.duong_kinh.get():g}")
            hop.destroy()
            self._cap_nhat_nhan_cong()
            self._ve_lai()

        h = tk.Frame(hop, bg=MAU["khung"])
        h.grid(row=8, column=0, columnspan=3, pady=(4, 12))
        ttk.Button(h, text="Xong", width=10, command=luu).pack(side="left", padx=4)
        ttk.Button(h, text="Bo qua", width=10, command=hop.destroy).pack(side="left", padx=4)
        hop.bind("<Return>", lambda e: luu())
        hop.grab_set()

    def _hop_toc_do(self):
        self._hop_nhap("Toc do",
                       [("Toc do CAT", self.toc_do_cat, "RPM"),
                        ("Toc do CHAY KHONG TAI (G0)", self.toc_do_nhanh, "RPM")],
                       sau=self._sinh_gcode)

    def _hop_duc_lo(self):
        self._hop_nhap("Duc lo", [("Thoi gian duc lo truoc khi cat",
                                   self.thoi_gian_duc_lo, "giay")], sau=self._sinh_gcode)

    def _hop_tay(self):
        self._hop_nhap("Dieu khien tay",
                       [("Toc do di chuyen tay", self.toc_do_tay, "RPM"),
                        ("Buoc nhich moi lan bam", self.buoc_nhich, "mm hoac do")])

    def _hop_nhap(self, tieu_de, cac_muc, sau=None):
        hop = tk.Toplevel(self.root)
        hop.title(tieu_de)
        hop.transient(self.root)
        hop.resizable(False, False)
        hop.configure(bg=MAU["khung"])
        o = {}
        for i, (nhan, bien, don_vi) in enumerate(cac_muc):
            tk.Label(hop, text=nhan, bg=MAU["khung"], anchor="w").grid(
                row=i, column=0, sticky="w", padx=10, pady=6)
            e = ttk.Entry(hop, width=10)
            e.insert(0, f"{bien.get():g}")
            e.grid(row=i, column=1, padx=4)
            tk.Label(hop, text=don_vi, bg=MAU["khung"], fg=MAU["chu_mo"]).grid(
                row=i, column=2, sticky="w", padx=(0, 10))
            o[nhan] = (e, bien)

        def luu():
            for nhan, (e, bien) in o.items():
                try:
                    gt = float(e.get().strip().replace(",", "."))
                except ValueError:
                    messagebox.showwarning("Sai so lieu", f"'{nhan}' phai la so.",
                                           parent=hop)
                    return
                if gt <= 0:
                    messagebox.showwarning("Sai so lieu", f"'{nhan}' phai lon hon 0.",
                                           parent=hop)
                    return
                bien.set(gt)
            hop.destroy()
            if sau:
                sau()

        h = tk.Frame(hop, bg=MAU["khung"])
        h.grid(row=len(cac_muc), column=0, columnspan=3, pady=8)
        ttk.Button(h, text="Luu", width=10, command=luu).pack(side="left", padx=4)
        ttk.Button(h, text="Bo qua", width=10, command=hop.destroy).pack(side="left", padx=4)
        hop.bind("<Return>", lambda e: luu())
        hop.grab_set()

    def _ap_duong_kinh(self):
        try:
            d = float(self.duong_kinh.get())
        except (ValueError, tk.TclError):
            messagebox.showwarning("Sai so lieu", "Duong kinh phai la so.")
            return
        if d <= 0:
            messagebox.showwarning("Sai so lieu", "Duong kinh phai lon hon 0.")
            return
        if self._dang_ket_noi():
            self._gui(f"CFG;DUONGKINH;{d:g}")
        self._bai_da_doi()

    @staticmethod
    def _so(bien, mac_dinh):
        try:
            gt = float(bien.get())
            return gt if gt > 0 else mac_dinh
        except (ValueError, tk.TclError):
            return mac_dinh

    def _mo_cai_dat_nang_cao(self):
        duong = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cnc_settings.pyw")
        if not os.path.exists(duong):
            messagebox.showwarning("Khong tim thay",
                                   "Khong thay file cnc_settings.pyw canh phan mem nay.")
            return
        if self._dang_ket_noi():
            if not messagebox.askyesno(
                    "Dang ket noi",
                    "Chi mot phan mem duoc giu cong COM cung luc.\n\n"
                    "Ngat ket noi de mo phan cai dat nang cao?"):
                return
            self._ngat_ket_noi()
        try:
            subprocess.Popen([sys.executable, duong])
        except Exception as loi:
            messagebox.showerror("Khong mo duoc", str(loi))

    # ==================================================================
    # FILE
    # ==================================================================
    def _mo_file(self):
        duong = filedialog.askopenfilename(title="Mo file G-code", filetypes=DUOI_FILE)
        if not duong:
            return
        try:
            with open(duong, "r", encoding="utf-8", errors="replace") as f:
                noi_dung = f.read()
        except Exception as loi:
            messagebox.showerror("Khong doc duoc file", str(loi))
            return
        self._ghi_lich_su()
        # File hien thanh MOT DONG trong bang Edit - chon roi bam Xoa la bo no ra
        self.file_ngoai = {"ten": os.path.basename(duong), "dong": noi_dung.split("\n")}
        self._bai_da_doi()
        self.the_duoi.select(0)
        self._ghi(f"Da mo {os.path.basename(duong)} ({len(self.file_ngoai['dong'])} dong). "
                  f"Chon dong [FILE] trong bang roi bam Xoa de bo ra.", "he_thong")
        self.root.title(f"{TEN_PHAN_MEM} - {os.path.basename(duong)}")

    def _luu_file(self):
        duong = filedialog.asksaveasfilename(title="Luu G-code", defaultextension=".nc",
                                             filetypes=DUOI_FILE)
        if not duong:
            return
        try:
            with open(duong, "w", encoding="utf-8") as f:
                f.write(self.o_gcode.get("1.0", "end").rstrip() + "\n")
            self._ghi(f"Da luu {os.path.basename(duong)}", "he_thong")
        except Exception as loi:
            messagebox.showerror("Khong luu duoc", str(loi))

    # ==================================================================
    # KET NOI
    # ==================================================================
    def _dang_ket_noi(self):
        return self.may.dang_mo

    def _toggle_ket_noi(self):
        self._ngat_ket_noi() if self._dang_ket_noi() else self._ket_noi()

    def _ket_noi(self):
        cong = self.cong_com.get().strip()
        if not cong:
            messagebox.showwarning("Chua chon cong",
                                   "Chua chon cong COM.\n\nBam 'Tham so...' de chon.")
            return
        try:
            self.may.mo(cong, BAUD_CO_DINH)
        except Exception as loi:
            messagebox.showerror("Loi ket noi", str(loi))
            self._them_loi(f"Khong ket noi duoc: {loi}")
            return
        self.nut_ket_noi.config(text="Ngat ket noi")
        self._dat_trang_thai("SAN_SANG")
        self._cap_nhat_nhan_cong()
        self._ghi(f"Da ket noi {cong} o {BAUD_CO_DINH} baud", "he_thong")

    def _ngat_ket_noi(self):
        self.may.dong()
        self.nut_ket_noi.config(text="Ket noi")
        self._dat_trang_thai("CHUA_KETNOI")
        self._cap_nhat_nhan_cong()
        self._ghi("Da ngat ket noi", "he_thong")

    def _cap_nhat_nhan_cong(self):
        if self._dang_ket_noi():
            self.nhan_cong.config(text=f"{self.cong_com.get()} - {BAUD_CO_DINH} baud",
                                  fg=MAU["chay"])
        else:
            self.nhan_cong.config(text=f"Chua ket noi ({self.cong_com.get()})",
                                  fg=MAU["chu_mo"])

    def _gui(self, lenh, ghi=True):
        if not self._dang_ket_noi():
            messagebox.showwarning("Chua ket noi", "Hay ket noi cong COM truoc.")
            return False
        if self.may.gui(lenh) and ghi:
            self._ghi("> " + lenh, "gui")
        return True

    def _gui_lenh_go(self):
        lenh = self.o_lenh.get().strip()
        if lenh and self._gui(lenh):
            self.o_lenh.delete(0, "end")

    def _khoi_dong_lai(self):
        if messagebox.askyesno("Khoi dong lai", "Khoi dong lai ESP32?"):
            self._gui("CFG;REBOOT")

    def _hoi_vi_tri_dinh_ky(self):
        """Hoi POS moi 2 giay khi may DANG RANH de o vi tri luon dung.

        KHONG hoi luc dang chay: moi lenh gui xuong deu lam ESP32 in ra UART, ma
        in giua chuoi cat se chan vong xuat xung -> tao vet dung tren duong cat.
        """
        if self._dang_ket_noi() and self.trang_thai in ("SAN_SANG", "DA_DUNG", "TAM_DUNG"):
            self.may.gui("POS")
        self.root.after(2000, self._hoi_vi_tri_dinh_ky)

    # ==================================================================
    # DIEU KHIEN MAY
    # ==================================================================
    def _jog(self, truc, dau):
        try:
            buoc = float(self.buoc_nhich.get()) * dau
            toc_do = float(self.toc_do_tay.get())
        except (ValueError, tk.TclError):
            messagebox.showwarning("Sai so lieu", "Buoc nhich va toc do tay phai la so.")
            return
        if buoc == 0 or toc_do <= 0:
            messagebox.showwarning("Sai so lieu",
                                   "Buoc nhich phai khac 0 va toc do tay phai lon hon 0.")
            return
        self._gui(f"JOG;{truc};{buoc:g};{toc_do:g}")

    def _dat_goc(self):
        if messagebox.askyesno("Dat goc 0",
                               "Lay vi tri hien tai lam goc 0 cua ca hai truc?"):
            self._gui("ZERO")

    def _ve_goc(self):
        """Dua ca hai truc ve diem goc 0 - hai truc chay dong thoi."""
        if not self._dang_ket_noi():
            messagebox.showwarning("Chua ket noi", "Hay ket noi cong COM truoc.")
            return
        self._gui("G90")        # bat toa do tuyet doi cho chac
        self._gui(f"G0 X0 A0 F{self.toc_do_nhanh.get():g}")

    def _bat_tat_chay_thu(self):
        self.chay_thu.set(not self.chay_thu.get())
        bat = self.chay_thu.get()
        self.nut_thu.config(text="Chay thu: BAT" if bat else "Chay thu",
                            bg="#ffd43b" if bat else "#dfe4ea")
        self._ghi("Che do chay thu " + ("BAT - se KHONG bat mo cat" if bat else "TAT"),
                  "he_thong")

    def _bat_tat_mo(self):
        if not self._dang_ket_noi():
            messagebox.showwarning("Chua ket noi", "Hay ket noi cong COM truoc.")
            return
        if not self.mo_dang_bat and not messagebox.askyesno(
                "Bat mo cat", "BAT MO CAT PLASMA ngay bay gio?\n\n"
                              "Chi bat khi da chac chan khong co ai dung gan may."):
            return
        self.mo_dang_bat = not self.mo_dang_bat
        self._gui("M3" if self.mo_dang_bat else "M5")
        self.nut_mo_cat.config(text="TAT mo" if self.mo_dang_bat else "Bat mo",
                               bg="#ff8787" if self.mo_dang_bat else "#dfe4ea")

    def _chay(self):
        if not self._dang_ket_noi():
            messagebox.showwarning("Chua ket noi", "Hay ket noi cong COM truoc.")
            return
        self._ve_lai()
        kq = self.ket_qua
        cac_dong = [d for d in kq.dong_chuan_hoa if d.strip()]
        if not cac_dong:
            messagebox.showwarning("Bai trong", "Chua co dong G-code nao de chay.")
            return
        if kq.co_loi_nang and not messagebox.askyesno(
                "Bai co van de", "Phan kiem tra truoc phat hien van de nghiem trong "
                                 "(xem tab Alarm).\n\nVan chay?"):
            return
        if self.chay_thu.get():
            cac_dong = [d for d in cac_dong if d.strip().upper() not in ("M3", "M4")]
            self._ghi("CHAY THU: da bo cac lenh bat mo cat.", "he_thong")

        self.doan_da_chay = 0
        self.moc_bat_dau = time.time()
        self.mp.vi_tri_chay = 0
        self._dat_trang_thai("DANG_NAP")
        self.may.nap_va_chay(pg.nen_ca_bai(cac_dong))

    def _chay_tiep(self):
        """Hoi truoc: co can duc lo lai truoc khi cat tiep khong."""
        hop = tk.Toplevel(self.root)
        hop.title("Chay tiep")
        hop.transient(self.root)
        hop.resizable(False, False)
        hop.configure(bg=MAU["khung"])

        tk.Label(hop, bg=MAU["khung"], justify="left", anchor="w", wraplength=420,
                 font=("Segoe UI", 9),
                 text="Luc tam dung, may da TAT mo cat de khong thung phoi.\n\n"
                      "Neu dang dung giua duong cat thi phai bat mo va cho duc xuyen "
                      "qua thanh ong roi moi chay tiep, neu khong mach cat se bi "
                      "dut doan."
                 ).grid(row=0, column=0, columnspan=3, padx=14, pady=(12, 10), sticky="w")

        co_duc = tk.BooleanVar(value=True)
        thoi_gian = tk.StringVar(value=f"{self.thoi_gian_duc_lo.get():g}")
        o_tg = ttk.Entry(hop, width=8, textvariable=thoi_gian, font=("Segoe UI", 10))

        def doi():
            o_tg.config(state="normal" if co_duc.get() else "disabled")

        tk.Checkbutton(hop, text="Bat mo va cho duc lo truoc khi chay tiep",
                       variable=co_duc, bg=MAU["khung"], command=doi, anchor="w",
                       font=("Segoe UI", 9)).grid(row=1, column=0, columnspan=3,
                                                  sticky="w", padx=14)
        tk.Label(hop, text="Thoi gian duc lo:", bg=MAU["khung"]).grid(
            row=2, column=0, sticky="e", padx=(34, 4), pady=(2, 12))
        o_tg.grid(row=2, column=1, sticky="w", pady=(2, 12))
        tk.Label(hop, text="giay", bg=MAU["khung"], fg=MAU["chu_mo"]).grid(
            row=2, column=2, sticky="w", padx=(4, 14), pady=(2, 12))

        def chay_tiep():
            if co_duc.get():
                try:
                    giay = float(thoi_gian.get().strip().replace(",", "."))
                except ValueError:
                    messagebox.showwarning("Sai so lieu",
                                           "Thoi gian duc lo phai la so.", parent=hop)
                    return
                if not 0 < giay <= 30:
                    messagebox.showwarning("Sai so lieu",
                                           "Thoi gian duc lo phai trong khoang 0..30 giay.",
                                           parent=hop)
                    return
                hop.destroy()
                self._gui(f"RESUME;{int(giay * 1000)}")
                self._ghi(f"Chay tiep, duc lo {giay:g} giay truoc.", "he_thong")
            else:
                hop.destroy()
                self._gui("RESUME")
                self._ghi("Chay tiep, KHONG duc lo lai.", "he_thong")

        h = tk.Frame(hop, bg=MAU["khung"])
        h.grid(row=3, column=0, columnspan=3, pady=(0, 14))
        tk.Button(h, text="CHAY TIEP", width=16, command=chay_tiep,
                  **self._kn("#1971c2", "white")).pack(side="left", padx=8)
        tk.Button(h, text="Khong chay nua", width=16, command=hop.destroy,
                  **self._kn()).pack(side="left", padx=8)
        hop.bind("<Return>", lambda e: chay_tiep())
        hop.bind("<Escape>", lambda e: hop.destroy())
        hop.update_idletasks()
        hop.minsize(hop.winfo_reqwidth(), hop.winfo_reqheight())
        hop.grab_set()
        o_tg.focus_set()

    def _dung(self):
        self.may.huy_nap()
        if self._dang_ket_noi():
            self.may.gui("STOP")
            self._ghi("> STOP", "gui")
        self.mo_dang_bat = False
        self.nut_mo_cat.config(text="Bat mo", bg="#dfe4ea")

    # ==================================================================
    # HANG DOI SU KIEN
    # ==================================================================
    def _doc_hang_doi(self):
        try:
            while True:
                loai, nd = self.hang_doi.get_nowait()
                if loai == "esp32":
                    self._tu_esp32(nd)
                elif loai == "nhat_ky":
                    self._ghi(nd, "he_thong")
                elif loai == "loi_nap":
                    self._nap_that_bai(nd)
                elif loai == "vi_tri":
                    self.nhan_x.config(text=f"{nd[0]:.2f}")
                    self.nhan_a.config(text=f"{nd[1]:.2f}")
        except queue.Empty:
            pass
        self._dong_ho()
        self.root.after(50, self._doc_hang_doi)

    def _tu_esp32(self, dong):
        the = None
        if dong.startswith("Loi:") or dong.startswith("LOI_"):
            the = "loi"
            self._them_loi(dong)
        elif dong.startswith(("OK", "RUNNING", "ZEROED", "RESUMED", "Hoan thanh",
                              "PLASMA_", "XONG_", "DUC_LO")):
            the = "ok"
        self._ghi(dong, the)

        if dong.startswith("Loi: DUNG KHAN CAP"):
            self._dat_trang_thai("LOI")
        elif dong.startswith(("RUNNING", "RESUMED")):
            self._dat_trang_thai("DANG_CHAY")
        elif dong.startswith(("PAUSED", "M0_PAUSED")):
            self._dat_trang_thai("TAM_DUNG")
        elif dong.startswith(("STOPPED", "Da dung han")):
            self._dat_trang_thai("DA_DUNG")
        elif dong.startswith("XONG_CHUONG_TRINH"):
            self._dat_trang_thai("SAN_SANG")
            self._xong_bai()
        elif dong.startswith("He thong: da het dieu kien loi"):
            self._dat_trang_thai("SAN_SANG")
        elif dong.startswith("PLASMA_ON"):
            self.mo_dang_bat = True
            self.nut_mo_cat.config(text="TAT mo", bg="#ff8787")
        elif dong.startswith("PLASMA_OFF"):
            self.mo_dang_bat = False
            self.nut_mo_cat.config(text="Bat mo", bg="#dfe4ea")

        if dong.startswith("Hoan thanh"):
            self.doan_da_chay += 1
            self._tien_do()
        if dong.startswith("CFG:"):
            self._doc_cau_hinh(dong)

    def _doc_cau_hinh(self, dong):
        for cap in dong[4:].strip().split():
            if "=" not in cap:
                continue
            ten, gt = cap.split("=", 1)
            try:
                if ten == "che_do":
                    self.che_do.set(int(gt))
                    self.nhan_che_do.config(text=TEN_CHE_DO.get(int(gt), ""))
                elif ten == "duong_kinh_ong":
                    self.duong_kinh.set(round(float(gt), 3))
            except ValueError:
                pass

    def _xong_bai(self):
        self.doan_da_chay = self.tong_doan
        self._tien_do()
        self.moc_bat_dau = None
        self.mo_dang_bat = False
        self.nut_mo_cat.config(text="Bat mo", bg="#dfe4ea")

    def _tien_do(self):
        if self.tong_doan <= 0:
            return
        pt = min(100, 100 * self.doan_da_chay / self.tong_doan)
        self.thanh_tien_do["value"] = pt
        self.nhan_phan_tram.config(text=f"{pt:.0f}%")
        self.mp.vi_tri_chay = min(self.doan_da_chay, self.tong_doan - 1)
        if self.the_giua.index("current") == 0:
            self._ve_3d()

    def _dong_ho(self):
        if self.moc_bat_dau is None:
            return
        g = int(time.time() - self.moc_bat_dau)
        self.nhan_thoi_gian.config(text=f"{g // 3600:02d}:{(g // 60) % 60:02d}:{g % 60:02d}")

    def _nap_that_bai(self, chu):
        self._dat_trang_thai("SAN_SANG")
        self._them_loi("NAP THAT BAI: " + chu)
        messagebox.showerror("Nap that bai", chu + "\n\nMay CHUA chay.")

    # ==================================================================
    # TRANG THAI / NHAT KY
    # ==================================================================
    def _dat_trang_thai(self, ma):
        self.trang_thai = ma
        chu, mau = TRANG_THAI[ma]
        self.nhan_trang_thai.config(text=chu, bg=mau)
        self._cap_nhat_nut()

    def _cap_nhat_nut(self):
        noi = self._dang_ket_noi()
        chay = self.trang_thai in ("DANG_CHAY", "DANG_NAP")
        tam_dung = self.trang_thai == "TAM_DUNG"

        def dat(nut, bat):
            nut.config(state="normal" if bat else "disabled")

        dat(self.nut_chay, noi and not chay and self.trang_thai != "LOI")
        dat(self.nut_dung_tam, self.trang_thai == "DANG_CHAY")
        dat(self.nut_tiep, tam_dung)
        dat(self.nut_dung, noi)
        dat(self.nut_goc, noi and not chay and not tam_dung)
        dat(self.nut_mo_cat, noi and not chay)

    def _ghi(self, dong, the=None):
        self.term.config(state="normal")
        self.term.insert("end", time.strftime("[%H:%M:%S] ") + dong + "\n", the or ())
        if int(self.term.index("end-1c").split(".")[0]) > 2000:
            self.term.delete("1.0", "500.0")
        self.term.see("end")
        self.term.config(state="disabled")

    def _xoa_terminal(self):
        self.term.config(state="normal")
        self.term.delete("1.0", "end")
        self.term.config(state="disabled")

    def _them_loi(self, noi_dung):
        cac = self.bang_loi.get_children()
        if cac and self.bang_loi.item(cac[-1])["values"][1] == noi_dung:
            return
        self.bang_loi.insert("", "end", values=(time.strftime("%H:%M:%S"), noi_dung),
                             tags=("nang",))
        self.bang_loi.see(self.bang_loi.get_children()[-1])

    def _xoa_loi(self):
        self.bang_loi.delete(*self.bang_loi.get_children())

    # ==================================================================
    def _thoat(self):
        if self.trang_thai in ("DANG_CHAY", "DANG_NAP") and not messagebox.askyesno(
                "Dang chay", "May DANG CHAY. Van thoat phan mem?"):
            return
        self.may.dong()
        self.root.destroy()


def main():
    root = tk.Tk()
    try:
        ttk.Style().theme_use("clam")
    except tk.TclError:
        pass
    UngDung(root)
    root.mainloop()


if __name__ == "__main__":
    main()
