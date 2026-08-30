"""PHAN MEM MAY CAT ONG PLASMA CNC

Mot cua so duy nhat lo het moi viec hang ngay:
  - THU VIEN MOI NOI: chon kieu ghep (yen ngua, cat vat, lo, ranh...), nhap so
    do, bam Them - lam bao nhieu mieng tren mot cay ong cung duoc
  - MO FILE .NC san co tu phan mem CAM
  - MO PHONG 3D ong nam trong mam kep, xem truoc duong cat truoc khi bam chay
  - Doi 3 CHE DO va nhap DUONG KINH ONG ngay tren man hinh chinh
  - Chay / tam dung / dung, nap dan nen bai dai bao nhieu cung chay duoc

Cai dat phan cung (chan GPIO, so xung moi vong, dao chieu truc) nam o file
rieng "cnc_settings.pyw" - mo tu menu Settings.

Yeu cau: pip install pyserial
"""

import os
import sys
import time
import math
import queue
import subprocess
import tkinter as tk
from tkinter import ttk, messagebox, filedialog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from loi import thu_vien_moi_noi as tv
from loi import phan_tich_gcode as pg
from loi import ket_noi as kn
from loi.ve_3d import MoPhong3D

TEN_PHAN_MEM = "May cat ong plasma CNC"

MAU = {
    "nen":        "#eef1f5",
    "khung":      "#ffffff",
    "vien":       "#c3cad4",
    "chu":        "#1d2530",
    "chu_mo":     "#6b7686",
    "nhan":       "#2f6fb8",
    "chay":       "#2f9e44",
    "dung":       "#c92a2a",
    "cho":        "#e8890c",
    "term_nen":   "#12161b",
    "term_chu":   "#c8d3de",
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

# Ten NGAN de vua thanh tren; giai thich day du nam o phan chu goi y ben duoi
TEN_CHE_DO = {
    1: "1 - X mm, A do",
    2: "2 - giu toc do mo",
    3: "3 - tat ca bang mm",
}
GIAI_THICH_CHE_DO = {
    1: "Truc X nhan mm, truc A nhan do. Khong can duong kinh ong.",
    2: "Nhap duong kinh ong; may tu chinh toc do xoay sao cho mo cat luot qua "
       "be mat ong voi toc do KHONG DOI. Toa do van la mm va do.",
    3: "Nhap duong kinh ong; ca hai truc deu nhan MM (truc A la mm cung do "
       "tren mat ong). May tu doi ra do.",
}

DUOI_FILE = [("File G-code", "*.nc *.gcode *.tap *.txt"), ("Tat ca file", "*.*")]


# =============================================================================
# VE BIEU TUONG KIEU MOI NOI (net ve ky thuat, khong can file anh)
# =============================================================================
def ve_bieu_tuong(canvas, ma, chon=False):
    canvas.delete("all")
    w = int(canvas["width"])
    h = int(canvas["height"])
    nen = "#dbe7f5" if chon else "#ffffff"
    canvas.create_rectangle(0, 0, w, h, fill=nen,
                            outline=MAU["nhan"] if chon else MAU["vien"],
                            width=2 if chon else 1)
    m, n = w / 2, h / 2
    net, do = MAU["chu"], "#d9480f"

    def ong_ngang(y, nua_cao, x1, x2, mau=net):
        canvas.create_rectangle(x1, y - nua_cao, x2, y + nua_cao, outline=mau)
        canvas.create_oval(x1 - 3, y - nua_cao, x1 + 3, y + nua_cao, outline=mau)

    if ma in ("yen_ngua_t", "yen_ngua_goc"):
        ong_ngang(n + 8, 9, 8, w - 8)
        if ma == "yen_ngua_t":
            canvas.create_rectangle(m - 7, 6, m + 7, n + 8, outline=net)
            canvas.create_arc(m - 7, n + 1, m + 7, n + 15, start=0, extent=180,
                              style="arc", outline=do, width=2)
        else:
            canvas.create_line(m - 12, 6, m - 1, n + 8, fill=net)
            canvas.create_line(m + 2, 4, m + 13, n + 8, fill=net)
            canvas.create_arc(m - 6, n + 1, m + 12, n + 15, start=0, extent=180,
                              style="arc", outline=do, width=2)
    elif ma == "cat_vat":
        canvas.create_polygon(8, n - 9, w - 14, n - 9, w - 26, n + 9, 8, n + 9,
                              outline=net, fill="")
        canvas.create_line(w - 14, n - 9, w - 26, n + 9, fill=do, width=2)
        canvas.create_oval(5, n - 9, 11, n + 9, outline=net)
    elif ma == "cat_thang":
        ong_ngang(n, 9, 8, w - 18)
        canvas.create_line(w - 18, n - 14, w - 18, n + 14, fill=do, width=2)
    elif ma == "lo_tron":
        ong_ngang(n, 11, 8, w - 8)
        canvas.create_oval(m - 7, n - 6, m + 7, n + 6, outline=do, width=2)
    elif ma in ("ranh_doc", "ranh_vong"):
        ong_ngang(n, 11, 8, w - 8)
        if ma == "ranh_doc":
            canvas.create_oval(m - 15, n - 4, m + 15, n + 4, outline=do, width=2)
        else:
            canvas.create_oval(m - 4, n - 8, m + 4, n + 8, outline=do, width=2)
    elif ma == "khia_v":
        ong_ngang(n, 10, 8, w - 8)
        canvas.create_polygon(m - 10, n - 10, m + 10, n - 10, m, n + 4,
                              outline=do, fill="", width=2)
    canvas.create_rectangle(0, 0, w, h, fill="", outline="")


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

        # ---- Trang thai ----
        self.hang_doi = queue.Queue()
        self.may = kn.KetNoiESP32(lambda loai, nd: self.hang_doi.put((loai, nd)))
        self.trang_thai = "CHUA_KETNOI"
        self.duong_kinh = tk.DoubleVar(value=60.0)
        self.che_do = tk.IntVar(value=1)
        self.toc_do_cat = tk.DoubleVar(value=15.0)
        self.toc_do_nhanh = tk.DoubleVar(value=60.0)
        self.toc_do_tay = tk.DoubleVar(value=30.0)
        self.buoc_nhich = tk.DoubleVar(value=1.0)
        self.thoi_gian_duc_lo = tk.DoubleVar(value=0.8)
        self.chay_thu = tk.BooleanVar(value=False)

        self.cac_phep_cat = []       # [{"ma":..., "gia_tri":{...}}]
        self.kieu_dang_chon = tv.THU_VIEN[0]
        self.o_tham_so = {}
        self.ket_qua = None
        self.mo_dang_bat = False
        self.moc_bat_dau = None
        self.tong_doan = 0
        self.doan_da_chay = 0

        self._xay_dung()
        self._chon_kieu(tv.THU_VIEN[0])
        self._cap_nhat_nut()
        self.root.after(50, self._doc_hang_doi)
        self.root.after(2000, self._hoi_vi_tri_dinh_ky)
        self.root.protocol("WM_DELETE_WINDOW", self._thoat)
        self.root.bind("<Escape>", lambda e: self._dung())

    # ==================================================================
    # DUNG GIAO DIEN
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
        m.add_command(label="Toc do cat / khong tai / tay...", command=self._hop_toc_do)
        m.add_command(label="Thoi gian duc lo...", command=self._hop_duc_lo)
        thanh.add_cascade(label="Parameters", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Sap xep cac mieng tren cay ong...", command=self._hop_xep_ong)
        thanh.add_cascade(label="Nesting", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Hoi vi tri hien tai", command=lambda: self._gui("POS"))
        m.add_command(label="Hoi bo dem con trong", command=lambda: self._gui("BUF"))
        m.add_command(label="Doc cau hinh ESP32", command=lambda: self._gui("CFG;GET"))
        m.add_separator()
        m.add_command(label="Khoi dong lai ESP32", command=self._khoi_dong_lai)
        thanh.add_cascade(label="Diagnostics", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Toc do di chuyen tay va buoc nhich...",
                      command=self._hop_cai_dat_co_ban)
        m.add_separator()
        m.add_command(label="Cai dat phan cung nang cao (chan GPIO, hieu chuan)...",
                      command=self._mo_cai_dat_nang_cao)
        thanh.add_cascade(label="Settings", menu=m)

        m = tk.Menu(thanh, tearoff=0)
        m.add_command(label="Xem danh sach loi", command=lambda: self.the_duoi.select(2))
        m.add_command(label="Xoa danh sach loi", command=self._xoa_loi)
        thanh.add_cascade(label="Alarm", menu=m)

        self.root.config(menu=thanh)
        self.root.bind("<Control-o>", lambda e: self._mo_file())
        self.root.bind("<Control-s>", lambda e: self._luu_file())

    # ------------------------------------------------------------------
    def _thanh_tren(self):
        k = tk.Frame(self.root, bg=MAU["khung"], highlightbackground=MAU["vien"],
                     highlightthickness=1)
        k.pack(fill="x", padx=6, pady=(6, 4))

        tk.Label(k, text="COM:", bg=MAU["khung"]).pack(side="left", padx=(8, 2), pady=6)
        self.o_com = ttk.Combobox(k, width=9, values=kn.danh_sach_cong())
        cong = kn.danh_sach_cong()
        self.o_com.set(cong[0] if cong else "COM3")
        self.o_com.pack(side="left", padx=2)
        ttk.Button(k, text="Lam moi", width=8,
                   command=lambda: self.o_com.config(values=kn.danh_sach_cong())
                   ).pack(side="left", padx=3)

        tk.Label(k, text="Toc do:", bg=MAU["khung"]).pack(side="left", padx=(10, 2))
        self.o_baud = ttk.Combobox(k, width=15, state="readonly",
                                   values=["Tu dong (nhanh nhat)"] +
                                          [str(b) for b in kn.BAUD_THU_DAN])
        self.o_baud.set("Tu dong (nhanh nhat)")
        self.o_baud.pack(side="left", padx=2)

        self.nut_ket_noi = ttk.Button(k, text="Ket noi", width=11, command=self._toggle_ket_noi)
        self.nut_ket_noi.pack(side="left", padx=(6, 14))

        ttk.Separator(k, orient="vertical").pack(side="left", fill="y", pady=4)

        tk.Label(k, text="Che do:", bg=MAU["khung"]).pack(side="left", padx=(10, 2))
        self.o_che_do = ttk.Combobox(k, width=17, state="readonly",
                                     values=[TEN_CHE_DO[i] for i in (1, 2, 3)])
        self.o_che_do.set(TEN_CHE_DO[1])
        self.o_che_do.bind("<<ComboboxSelected>>", self._doi_che_do)
        self.o_che_do.pack(side="left", padx=2)

        tk.Label(k, text="Duong kinh:", bg=MAU["khung"]).pack(side="left", padx=(10, 2))
        o = ttk.Entry(k, width=7, textvariable=self.duong_kinh)
        o.pack(side="left")
        o.bind("<Return>", lambda e: self._ap_dung_duong_kinh())
        tk.Label(k, text="mm", bg=MAU["khung"], fg=MAU["chu_mo"]).pack(side="left", padx=(2, 4))
        ttk.Button(k, text="Ap dung", width=8,
                   command=self._ap_dung_duong_kinh).pack(side="left", padx=(4, 8), pady=6)

    # ------------------------------------------------------------------
    def _cot_trai(self, cha):
        cot = tk.Frame(cha, bg=MAU["nen"], width=252)
        cot.pack(side="left", fill="y")
        cot.pack_propagate(False)

        # LUU Y THU TU PACK: khung "Dieu khien tay" duoc gan xuong DAY truoc,
        # roi thu vien moi lay phan con lai. Lam nguoc lai thi thu vien gian ra
        # nuot het cho va cac nut jog bi cat mat khi cua so thap.
        khung_tay = self._khung(cot, "Dieu khien tay")
        khung_tay.pack(side="bottom", fill="x")

        # ----- THU VIEN MOI NOI -----
        k = self._khung(cot, "Thu vien moi noi")
        k.pack(side="top", fill="both", expand=True, pady=(0, 4))

        luoi = tk.Frame(k, bg=MAU["khung"])
        luoi.pack(fill="x", padx=6, pady=4)
        self.bieu_tuong = {}
        for i, kieu in enumerate(tv.THU_VIEN):
            c = tk.Canvas(luoi, width=54, height=38, highlightthickness=0,
                          bg=MAU["khung"], cursor="hand2")
            c.grid(row=i // 4, column=i % 4, padx=2, pady=2)
            c.bind("<Button-1>", lambda e, kk=kieu: self._chon_kieu(kk))
            self.bieu_tuong[kieu.ma] = c

        self.nhan_kieu = tk.Label(k, text="", bg=MAU["khung"], fg=MAU["nhan"],
                                  font=("Segoe UI", 9, "bold"), anchor="w",
                                  wraplength=232, justify="left")
        self.nhan_kieu.pack(fill="x", padx=8)
        # Nut THEM va o nhap so do duoc gan xuong DAY truoc, phan mo ta dai
        # ngan the nao thi lay cho con lai - de dai may cung khong day mat nut
        tk.Button(k, text="THEM VAO BAI", bg=MAU["nhan"], fg="white", relief="flat",
                  font=("Segoe UI", 9, "bold"), command=self._them_phep_cat
                  ).pack(side="bottom", fill="x", padx=8, pady=(6, 8))
        self.khung_tham_so = tk.Frame(k, bg=MAU["khung"])
        self.khung_tham_so.pack(side="bottom", fill="x", padx=8)

        # Chi 2 dong o day cho gon; cau day du hien o thanh trang thai duoi cung
        self.mo_ta_kieu = tk.Label(k, text="", bg=MAU["khung"], fg=MAU["chu_mo"],
                                   font=("Segoe UI", 8), anchor="nw", height=2,
                                   wraplength=232, justify="left")
        self.mo_ta_kieu.pack(side="top", fill="both", expand=True, padx=8, pady=(0, 2))

        # ----- DIEU KHIEN TAY (khung da tao va gan o tren) -----
        k = khung_tay
        g = tk.Frame(k, bg=MAU["khung"])
        g.pack(padx=8, pady=6)
        cac_nut = [("X-", lambda: self._jog("X", -1), 0, 0),
                   ("X+", lambda: self._jog("X", +1), 0, 1),
                   ("A nguoc", lambda: self._jog("A", -1), 1, 0),
                   ("A thuan", lambda: self._jog("A", +1), 1, 1)]
        for chu, ham, r, c in cac_nut:
            ttk.Button(g, text=chu, width=11, command=ham).grid(row=r, column=c, padx=2, pady=2)
        h = tk.Frame(k, bg=MAU["khung"])
        h.pack(fill="x", padx=8, pady=(0, 6))
        tk.Label(h, text="Buoc:", bg=MAU["khung"], font=("Segoe UI", 8)).pack(side="left")
        ttk.Entry(h, width=5, textvariable=self.buoc_nhich).pack(side="left", padx=2)
        tk.Label(h, text="mm/do", bg=MAU["khung"], fg=MAU["chu_mo"],
                 font=("Segoe UI", 8)).pack(side="left", padx=(0, 6))
        ttk.Button(h, text="DAT GOC 0", command=self._dat_goc
                   ).pack(side="left", fill="x", expand=True)

    # ------------------------------------------------------------------
    def _giua(self, cha):
        k = self._khung(cha, "Mo phong 3D duong cat")
        k.pack(side="left", fill="both", expand=True, padx=6)
        self.canvas3d = tk.Canvas(k, bg="#1b1f24", highlightthickness=0)
        self.canvas3d.pack(fill="both", expand=True, padx=2, pady=2)
        self.mp = MoPhong3D(self.canvas3d)

        self.canvas3d.bind("<Configure>", lambda e: self._ve_3d())
        self.canvas3d.bind("<Button-1>", self._chuot_nhan)
        self.canvas3d.bind("<B1-Motion>", self._chuot_keo)
        self.canvas3d.bind("<MouseWheel>", self._lan_chuot)
        self.canvas3d.bind("<Button-4>", lambda e: self._phong(1.15))
        self.canvas3d.bind("<Button-5>", lambda e: self._phong(1 / 1.15))

        d = tk.Frame(k, bg=MAU["khung"])
        d.pack(fill="x", padx=4, pady=(0, 4))
        ttk.Button(d, text="Ve lai (F5)", command=self._ve_lai).pack(side="left", padx=2)
        ttk.Button(d, text="Goc nhin mac dinh", command=self._goc_nhin_goc).pack(side="left", padx=2)
        self.thanh_truot = ttk.Scale(d, from_=0, to=100, command=self._truot_mo_phong)
        self.thanh_truot.pack(side="left", fill="x", expand=True, padx=8)
        self.root.bind("<F5>", lambda e: self._ve_lai())

    # ------------------------------------------------------------------
    def _cot_phai(self, cha):
        cot = tk.Frame(cha, bg=MAU["nen"], width=210)
        cot.pack(side="left", fill="y")
        cot.pack_propagate(False)

        k = self._khung(cot, "Vi tri may")
        k.pack(fill="x", pady=(0, 4))
        self.nhan_x = self._o_so(k, "X", "mm")
        self.nhan_a = self._o_so(k, "A", "do")

        k = self._khung(cot, "Tien do")
        k.pack(fill="x", pady=(0, 4))
        self.thanh_tien_do = ttk.Progressbar(k, maximum=100)
        self.thanh_tien_do.pack(fill="x", padx=8, pady=(8, 2))
        self.nhan_phan_tram = tk.Label(k, text="0%", bg=MAU["khung"],
                                       fg=MAU["chu_mo"], font=("Segoe UI", 8))
        self.nhan_phan_tram.pack(anchor="e", padx=8)
        self.nhan_toc_do = self._dong_thong_tin(k, "Toc do cat", "-")
        self.nhan_thoi_gian = self._dong_thong_tin(k, "Thoi gian chay", "00:00:00")
        self.nhan_so_doan = self._dong_thong_tin(k, "So doan cat", "0")
        tk.Frame(k, bg=MAU["khung"], height=6).pack()

        k = self._khung(cot, "Kich thuoc bai")
        k.pack(fill="x")
        self.nhan_pham_vi = self._dong_thong_tin(k, "Doc ong", "-")
        self.nhan_so_dong = self._dong_thong_tin(k, "So dong G-code", "0")
        self.nhan_bo_dem = self._dong_thong_tin(k, "Buoc / bo dem", "0")
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

    def _dong_thong_tin(self, cha, ten, gia_tri):
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
        cach = {"side": "left", "expand": True, "fill": "x", "padx": 3}

        self.nut_mo = tk.Button(k, text="Mo .NC", command=self._mo_file, **self._kieu_nut())
        self.nut_mo.pack(**cach)
        self.nut_goc = tk.Button(k, text="Ve goc 0", command=self._ve_goc, **self._kieu_nut())
        self.nut_goc.pack(**cach)
        self.nut_thu = tk.Button(k, text="Chay thu", command=self._bat_tat_chay_thu,
                                 **self._kieu_nut())
        self.nut_thu.pack(**cach)
        self.nut_mo_cat = tk.Button(k, text="Bat mo", command=self._bat_tat_mo,
                                    **self._kieu_nut())
        self.nut_mo_cat.pack(**cach)
        self.nut_chay = tk.Button(k, text="CHAY", command=self._chay,
                                  **self._kieu_nut(MAU["chay"], "white"))
        self.nut_chay.pack(**cach)
        self.nut_dung_tam = tk.Button(k, text="TAM DUNG", command=self._tam_dung,
                                      **self._kieu_nut(MAU["cho"], "white"))
        self.nut_dung_tam.pack(**cach)
        self.nut_tiep = tk.Button(k, text="CHAY TIEP", command=self._chay_tiep,
                                  **self._kieu_nut("#1971c2", "white"))
        self.nut_tiep.pack(**cach)
        self.nut_dung = tk.Button(k, text="DUNG (Esc)", command=self._dung,
                                  **self._kieu_nut(MAU["dung"], "white"))
        self.nut_dung.pack(**cach)

    @staticmethod
    def _kieu_nut(nen=None, chu=None):
        return {"bg": nen or "#dfe4ea", "fg": chu or MAU["chu"], "relief": "flat",
                "font": ("Segoe UI", 9, "bold"), "pady": 7,
                "activebackground": nen or "#cdd4dd",
                "disabledforeground": "#a9b1bb"}

    # ------------------------------------------------------------------
    def _the_duoi(self):
        self.the_duoi = ttk.Notebook(self.root, height=150)
        self.the_duoi.pack(fill="both", padx=6, pady=(0, 4))

        # ----- EDIT: danh sach phep cat + G-code -----
        t = tk.Frame(self.the_duoi, bg=MAU["khung"])
        self.the_duoi.add(t, text="  Edit (ve bai)  ")

        trai = tk.Frame(t, bg=MAU["khung"])
        trai.pack(side="left", fill="both", expand=True, padx=4, pady=4)
        self.bang = ttk.Treeview(trai, columns=("kieu", "so_do"), show="headings", height=6)
        self.bang.heading("kieu", text="Kieu moi noi")
        self.bang.heading("so_do", text="So do")
        self.bang.column("kieu", width=170, anchor="w")
        self.bang.column("so_do", width=300, anchor="w")
        self.bang.pack(side="left", fill="both", expand=True)
        cot_nut = tk.Frame(trai, bg=MAU["khung"])
        cot_nut.pack(side="left", fill="y", padx=4)
        for chu, ham in (("Len", lambda: self._doi_cho(-1)),
                         ("Xuong", lambda: self._doi_cho(+1)),
                         ("Xoa", self._xoa_phep_cat),
                         ("Xoa het", self._xoa_het)):
            ttk.Button(cot_nut, text=chu, width=8, command=ham).pack(pady=1)

        phai = tk.Frame(t, bg=MAU["khung"])
        phai.pack(side="left", fill="both", expand=True, padx=4, pady=4)
        tk.Label(phai, text="G-code (sua tay duoc, bam Ve lai de cap nhat)",
                 bg=MAU["khung"], fg=MAU["chu_mo"], font=("Segoe UI", 8)).pack(anchor="w")
        khung_g = tk.Frame(phai)
        khung_g.pack(fill="both", expand=True)
        self.o_gcode = tk.Text(khung_g, font=("Consolas", 9), wrap="none", undo=True)
        thanh = ttk.Scrollbar(khung_g, command=self.o_gcode.yview)
        self.o_gcode.config(yscrollcommand=thanh.set)
        thanh.pack(side="right", fill="y")
        self.o_gcode.pack(side="left", fill="both", expand=True)
        self.o_gcode.tag_config("loi", background="#ffe3e3")

        # ----- SYSTEM: terminal -----
        t = tk.Frame(self.the_duoi, bg=MAU["term_nen"])
        self.the_duoi.add(t, text="  System (terminal)  ")
        khung = tk.Frame(t, bg=MAU["term_nen"])
        khung.pack(fill="both", expand=True, padx=4, pady=(4, 0))
        self.term = tk.Text(khung, bg=MAU["term_nen"], fg=MAU["term_chu"],
                            font=("Consolas", 9), wrap="word", state="disabled",
                            insertbackground="white")
        th = ttk.Scrollbar(khung, command=self.term.yview)
        self.term.config(yscrollcommand=th.set)
        th.pack(side="right", fill="y")
        self.term.pack(side="left", fill="both", expand=True)
        self.term.tag_config("gui", foreground="#74c0fc")
        self.term.tag_config("loi", foreground="#ff8787")
        self.term.tag_config("ok", foreground="#8ce99a")
        self.term.tag_config("he_thong", foreground="#ffd43b")

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
        self.nhan_trang_thai = tk.Label(k, text="CHUA KET NOI", bg="#868e96", fg="white",
                                        font=("Segoe UI", 9, "bold"), width=16)
        self.nhan_trang_thai.pack(side="left", padx=(0, 8), ipady=3)
        self.nhan_ket_noi = tk.Label(k, text="Chua ket noi", bg="#dfe4ea",
                                     fg=MAU["chu_mo"], font=("Segoe UI", 8))
        self.nhan_ket_noi.pack(side="left")
        self.nhan_mach = tk.Label(k, text=GIAI_THICH_CHE_DO[1], bg="#dfe4ea",
                                  fg=MAU["chu_mo"], font=("Segoe UI", 8))
        self.nhan_mach.pack(side="right", padx=8)

    @staticmethod
    def _khung(cha, tieu_de):
        k = tk.LabelFrame(cha, text=" " + tieu_de + " ", bg=MAU["khung"],
                          fg=MAU["chu"], font=("Segoe UI", 9, "bold"),
                          bd=1, relief="solid", labelanchor="nw")
        return k

    # ==================================================================
    # THU VIEN MOI NOI
    # ==================================================================
    def _chon_kieu(self, kieu):
        self.kieu_dang_chon = kieu
        for ma, c in self.bieu_tuong.items():
            ve_bieu_tuong(c, ma, chon=(ma == kieu.ma))
        self.nhan_kieu.config(text=kieu.ten)
        self.mo_ta_kieu.config(text=kieu.mo_ta)
        if hasattr(self, "nhan_mach"):
            self.nhan_mach.config(text=f"{kieu.ten}: {kieu.mo_ta}")

        for w in self.khung_tham_so.winfo_children():
            w.destroy()
        self.o_tham_so = {}
        for i, ts in enumerate(kieu.tham_so):
            tk.Label(self.khung_tham_so, text=ts.nhan, bg=MAU["khung"],
                     font=("Segoe UI", 8), anchor="w").grid(row=i, column=0, sticky="w", pady=1)
            o = ttk.Entry(self.khung_tham_so, width=8)
            o.insert(0, f"{ts.mac_dinh:g}")
            o.grid(row=i, column=1, padx=3, pady=1)
            tk.Label(self.khung_tham_so, text=ts.don_vi, bg=MAU["khung"],
                     fg=MAU["chu_mo"], font=("Segoe UI", 8), width=3,
                     anchor="w").grid(row=i, column=2, sticky="w")
            self.o_tham_so[ts.ma] = o
        self.khung_tham_so.columnconfigure(0, weight=1)

    def _them_phep_cat(self):
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
            kieu.sinh(self.duong_kinh.get(), gia_tri)     # thu sinh de bat loi ngay
        except ValueError as loi:
            messagebox.showwarning("Khong lam duoc mieng nay", str(loi))
            self._them_loi(f"{kieu.ten}: {loi}", nang=False)
            return
        self.cac_phep_cat.append({"ma": kieu.ma, "gia_tri": gia_tri})
        self._nap_lai_bang()
        self._sinh_gcode_tu_bai()

    def _nap_lai_bang(self):
        self.bang.delete(*self.bang.get_children())
        for i, p in enumerate(self.cac_phep_cat, 1):
            kieu = tv.THEO_MA[p["ma"]]
            so_do = "  ".join(
                f"{ts.nhan}={p['gia_tri'].get(ts.ma, ts.mac_dinh):g}{ts.don_vi}"
                for ts in kieu.tham_so)
            self.bang.insert("", "end", values=(f"{i}. {kieu.ten}", so_do))

    def _chon_dong_bang(self):
        chon = self.bang.selection()
        if not chon:
            return None
        return self.bang.index(chon[0])

    def _xoa_phep_cat(self):
        i = self._chon_dong_bang()
        if i is None:
            return
        del self.cac_phep_cat[i]
        self._nap_lai_bang()
        self._sinh_gcode_tu_bai()

    def _xoa_het(self):
        if not self.cac_phep_cat:
            return
        if messagebox.askyesno("Xoa het", "Xoa toan bo cac mieng trong bai?"):
            self.cac_phep_cat = []
            self._nap_lai_bang()
            self._sinh_gcode_tu_bai()

    def _doi_cho(self, huong):
        i = self._chon_dong_bang()
        if i is None:
            return
        j = i + huong
        if not 0 <= j < len(self.cac_phep_cat):
            return
        self.cac_phep_cat[i], self.cac_phep_cat[j] = \
            self.cac_phep_cat[j], self.cac_phep_cat[i]
        self._nap_lai_bang()
        self._sinh_gcode_tu_bai()
        self.bang.selection_set(self.bang.get_children()[j])

    def _sinh_gcode_tu_bai(self):
        if not self.cac_phep_cat:
            self.o_gcode.delete("1.0", "end")
            self._ve_lai()
            return
        cac_duong = []
        for p in self.cac_phep_cat:
            kieu = tv.THEO_MA[p["ma"]]
            try:
                cac_duong.append(kieu.sinh(self.duong_kinh.get(), p["gia_tri"]))
            except ValueError as loi:
                self._them_loi(f"{kieu.ten}: {loi}", nang=True)
        dong = tv.sinh_gcode(cac_duong, self.toc_do_cat.get(), self.toc_do_nhanh.get(),
                             self.thoi_gian_duc_lo.get(), x_ve_cho=0.0,
                             tieu_de=[f"{TEN_PHAN_MEM} - ong D{self.duong_kinh.get():g}",
                                      f"{len(cac_duong)} mieng"])
        self.o_gcode.delete("1.0", "end")
        self.o_gcode.insert("1.0", "\n".join(dong))
        self._ve_lai()

    # ==================================================================
    # PHAN TICH + VE
    # ==================================================================
    def _ve_lai(self):
        cac_dong = self.o_gcode.get("1.0", "end").split("\n")
        self.o_gcode.tag_remove("loi", "1.0", "end")
        self.ket_qua = pg.phan_tich_chuong_trinh(
            cac_dong, chuan_hoa=True, ghi_de_toc_do=False,
            toc_do_cat=self.toc_do_cat.get(), toc_do_nhanh=self.toc_do_nhanh.get(),
            che_do=self.che_do.get(), duong_kinh=self.duong_kinh.get())
        self.mp.dat_du_lieu(self.ket_qua.doan, self.duong_kinh.get())
        self.mp.vi_tri_chay = None
        self._ve_3d()
        self._cap_nhat_thong_ke()
        # Tab Alarm chi danh cho LOI THAT SU. Nhung ghi chu kieu "dong nay dung
        # che do modal" van huu ich nhung khong phai loi - cho xuong terminal de
        # nguoi van hanh khong tuong may dang hong.
        for cb in self.ket_qua.canh_bao:
            if self.ket_qua.co_loi_nang:
                self._them_loi(cb, nang=True)
            else:
                self._ghi("Ghi chu: " + cb)

    def _ve_3d(self):
        try:
            self.mp.ve()
        except Exception as loi:
            self._them_loi(f"Loi ve mo phong: {loi}", nang=False)

    def _cap_nhat_thong_ke(self):
        kq = self.ket_qua
        if not kq:
            return
        so_cat = sum(1 for d in kq.doan if d[4])
        self.tong_doan = len(kq.doan)
        self.nhan_so_doan.config(text=str(so_cat))
        self.nhan_so_dong.config(text=str(len(kq.dong_chuan_hoa)))
        self.nhan_bo_dem.config(text=f"{kq.so_buoc_firmware} / {pg.SUC_CHUA_BO_DEM}")
        if kq.doan:
            xs = [d[0] for d in kq.doan] + [d[2] for d in kq.doan]
            self.nhan_pham_vi.config(text=f"{min(xs):.1f} .. {max(xs):.1f} mm")
        else:
            self.nhan_pham_vi.config(text="-")
        self.thanh_truot.config(to=max(1, self.tong_doan))
        self.nhan_toc_do.config(
            text=f"{self.toc_do_cat.get():g} / {self.toc_do_nhanh.get():g} RPM")

    # ---- chuot 3D ----
    def _chuot_nhan(self, e):
        self._chuot = (e.x, e.y)

    def _chuot_keo(self, e):
        if not hasattr(self, "_chuot"):
            return
        dx, dy = e.x - self._chuot[0], e.y - self._chuot[1]
        self._chuot = (e.x, e.y)
        self.mp.canh_nhin.xoay_ngang += dx * 0.5
        self.mp.canh_nhin.xoay_doc -= dy * 0.5      # keo len = nhin tu tren xuong
        self.mp.canh_nhin.xoay_doc = max(-88, min(88, self.mp.canh_nhin.xoay_doc))
        self._ve_3d()

    def _lan_chuot(self, e):
        self._phong(1.15 if e.delta > 0 else 1 / 1.15)

    def _phong(self, he_so):
        self.mp.canh_nhin.phong = max(0.25, min(6.0, self.mp.canh_nhin.phong * he_so))
        self._ve_3d()

    def _goc_nhin_goc(self):
        self.mp.canh_nhin.dat_lai()
        self._ve_3d()

    def _truot_mo_phong(self, gia_tri):
        if not self.ket_qua or not self.ket_qua.doan:
            return
        self.mp.vi_tri_chay = int(float(gia_tri))
        self._ve_3d()

    # ==================================================================
    # CHE DO / THAM SO
    # ==================================================================
    def _doi_che_do(self, _=None):
        so = [k for k, v in TEN_CHE_DO.items() if v == self.o_che_do.get()]
        if not so:
            return
        self.che_do.set(so[0])
        self.nhan_mach.config(text=GIAI_THICH_CHE_DO[so[0]])
        if self._dang_ket_noi():
            self._gui(f"CFG;MODE;{so[0]}")
            if so[0] in (2, 3):
                self._gui(f"CFG;DUONGKINH;{self.duong_kinh.get():g}")
        self._ve_lai()

    def _ap_dung_duong_kinh(self):
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
        self._sinh_gcode_tu_bai() if self.cac_phep_cat else self._ve_lai()

    def _hop_toc_do(self):
        self._hop_nhap("Toc do", [
            ("Toc do CAT", self.toc_do_cat, "RPM dong co"),
            ("Toc do CHAY KHONG TAI (G0)", self.toc_do_nhanh, "RPM dong co"),
            ("Toc do DI CHUYEN TAY", self.toc_do_tay, "RPM dong co"),
        ], sau=self._sinh_lai_neu_co_bai)

    def _hop_duc_lo(self):
        self._hop_nhap("Duc lo", [
            ("Thoi gian duc lo truoc khi cat", self.thoi_gian_duc_lo, "giay"),
        ], sau=self._sinh_lai_neu_co_bai)

    def _hop_cai_dat_co_ban(self):
        self._hop_nhap("Cai dat co ban", [
            ("Toc do di chuyen tay", self.toc_do_tay, "RPM dong co"),
            ("Buoc nhich moi lan bam", self.buoc_nhich, "mm hoac do"),
        ])

    def _sinh_lai_neu_co_bai(self):
        if self.cac_phep_cat:
            self._sinh_gcode_tu_bai()
        else:
            self._ve_lai()

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
                    gia_tri = float(e.get().strip().replace(",", "."))
                except ValueError:
                    messagebox.showwarning("Sai so lieu", f"'{nhan}' phai la so.",
                                           parent=hop)
                    return
                if gia_tri <= 0:
                    messagebox.showwarning("Sai so lieu", f"'{nhan}' phai lon hon 0.",
                                           parent=hop)
                    return
                bien.set(gia_tri)
            hop.destroy()
            if sau:
                sau()

        h = tk.Frame(hop, bg=MAU["khung"])
        h.grid(row=len(cac_muc), column=0, columnspan=3, pady=8)
        ttk.Button(h, text="Luu", width=10, command=luu).pack(side="left", padx=4)
        ttk.Button(h, text="Bo qua", width=10, command=hop.destroy).pack(side="left", padx=4)
        hop.bind("<Return>", lambda e: luu())
        hop.grab_set()

    # ---- Nesting ----
    def _hop_xep_ong(self):
        if not self.cac_phep_cat:
            messagebox.showinfo("Chua co bai", "Hay them it nhat mot mieng vao bai truoc.")
            return
        hop = tk.Toplevel(self.root)
        hop.title("Sap xep cac mieng tren cay ong")
        hop.transient(self.root)
        hop.configure(bg=MAU["khung"])
        tk.Label(hop, bg=MAU["khung"], justify="left", anchor="w", wraplength=430,
                 text="Doi vi tri X cua tung mieng de chung nam noi tiep nhau tren cay "
                      "ong, cach nhau mot khoang cho mach cat va cho kep.").grid(
            row=0, column=0, columnspan=3, padx=10, pady=(10, 4), sticky="w")

        o = {}
        for i, (nhan, mac_dinh, don_vi) in enumerate(
                [("Chua tu vi tri X", 20.0, "mm"),
                 ("Khoang cach giua cac mieng", 8.0, "mm"),
                 ("Chieu dai cay ong", 3000.0, "mm")], start=1):
            tk.Label(hop, text=nhan, bg=MAU["khung"], anchor="w").grid(
                row=i, column=0, sticky="w", padx=10, pady=3)
            e = ttk.Entry(hop, width=10)
            e.insert(0, f"{mac_dinh:g}")
            e.grid(row=i, column=1, padx=4)
            tk.Label(hop, text=don_vi, bg=MAU["khung"], fg=MAU["chu_mo"]).grid(
                row=i, column=2, sticky="w", padx=(0, 10))
            o[nhan] = e

        ket_qua = tk.Label(hop, text="", bg=MAU["khung"], fg=MAU["chu_mo"],
                           justify="left", anchor="w", wraplength=430)
        ket_qua.grid(row=5, column=0, columnspan=3, padx=10, pady=4, sticky="w")

        def xep():
            try:
                bat_dau = float(o["Chua tu vi tri X"].get())
                khe = float(o["Khoang cach giua cac mieng"].get())
                cay = float(o["Chieu dai cay ong"].get())
            except ValueError:
                messagebox.showwarning("Sai so lieu", "Cac o phai la so.", parent=hop)
                return
            x = bat_dau
            for p in self.cac_phep_cat:
                kieu = tv.THEO_MA[p["ma"]]
                if "x" not in p["gia_tri"]:
                    continue
                cu = p["gia_tri"]["x"]
                p["gia_tri"]["x"] = x
                try:
                    duong = kieu.sinh(self.duong_kinh.get(), p["gia_tri"])
                    x_min, x_max = duong.pham_vi_x()
                    x += (x_max - x_min) + khe
                except ValueError:
                    p["gia_tri"]["x"] = cu
            self._nap_lai_bang()
            self._sinh_gcode_tu_bai()
            du = cay - x
            ket_qua.config(
                text=f"Da xep {len(self.cac_phep_cat)} mieng, chiem {x - bat_dau:.1f} mm.\n"
                     + (f"Cay ong {cay:g} mm con thua {du:.1f} mm."
                        if du >= 0 else
                        f"THIEU {abs(du):.1f} mm - cay ong {cay:g} mm khong du dai!"),
                fg=MAU["chu_mo"] if du >= 0 else MAU["dung"])

        h = tk.Frame(hop, bg=MAU["khung"])
        h.grid(row=6, column=0, columnspan=3, pady=8)
        ttk.Button(h, text="Sap xep", width=12, command=xep).pack(side="left", padx=4)
        ttk.Button(h, text="Dong", width=10, command=hop.destroy).pack(side="left", padx=4)
        hop.grab_set()

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
        self.o_gcode.delete("1.0", "end")
        self.o_gcode.insert("1.0", noi_dung)
        self.cac_phep_cat = []          # file ngoai khong con la "bai ve" nua
        self._nap_lai_bang()
        self.the_duoi.select(0)
        self._ve_lai()
        self._ghi(f"Da mo {os.path.basename(duong)} "
                  f"({len(noi_dung.splitlines())} dong)", "he_thong")
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
        if self._dang_ket_noi():
            self._ngat_ket_noi()
        else:
            self._ket_noi()

    def _ket_noi(self):
        chon = self.o_baud.get()
        baud = None if chon.startswith("Tu dong") else int(chon)
        try:
            self.may.mo(self.o_com.get().strip(), baud)
        except Exception as loi:
            messagebox.showerror("Loi ket noi", str(loi))
            self._them_loi(f"Khong ket noi duoc: {loi}", nang=True)
            return
        self.nut_ket_noi.config(text="Ngat ket noi")
        self.nhan_ket_noi.config(text=f"{self.o_com.get()} - dang do toc do...")
        self._dat_trang_thai("SAN_SANG")
        self._ghi(f"Da ket noi {self.o_com.get()}", "he_thong")

    def _ngat_ket_noi(self):
        self.may.dong()
        self.nut_ket_noi.config(text="Ket noi")
        self.nhan_ket_noi.config(text="Chua ket noi")
        self._dat_trang_thai("CHUA_KETNOI")
        self._ghi("Da ngat ket noi", "he_thong")

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
        if messagebox.askyesno("Khoi dong lai",
                               "Khoi dong lai ESP32? May se ve trang thai ban dau."):
            self._gui("CFG;REBOOT")

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
        self._gui(f"JOG;{truc};{buoc:g};{toc_do:g}")

    def _dat_goc(self):
        if messagebox.askyesno("Dat goc 0",
                               "Lay vi tri hien tai lam goc 0 cua ca hai truc?"):
            self._gui("ZERO")

    def _ve_goc(self):
        """Dua ca hai truc ve diem goc 0 bang mot lenh G0 - hai truc chay dong thoi."""
        if not self._dang_ket_noi():
            messagebox.showwarning("Chua ket noi", "Hay ket noi cong COM truoc.")
            return
        self._gui("G90")        # bat toa do tuyet doi truoc cho chac
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
        if not self.mo_dang_bat:
            if not messagebox.askyesno(
                    "Bat mo cat",
                    "BAT MO CAT PLASMA ngay bay gio?\n\n"
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
                "Bai co van de",
                "Phan kiem tra truoc phat hien van de nghiem trong "
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

    def _tam_dung(self):
        self._gui("PAUSE")

    def _chay_tiep(self):
        """RESUME co hoi truoc: co can duc lo lai truoc khi cat tiep khong.

        Khi tam dung giua duong cat, firmware da TAT mo cat de khong thung phoi.
        Chay tiep ma khong duc lo lai thi mo cat chua xuyen qua thanh ong da phai
        di chuyen -> ranh cat bi dut doan.
        """
        hop = tk.Toplevel(self.root)
        hop.title("Chay tiep")
        hop.transient(self.root)
        hop.resizable(False, False)
        hop.configure(bg=MAU["khung"])

        tk.Label(hop, bg=MAU["khung"], justify="left", anchor="w", wraplength=400,
                 font=("Segoe UI", 9),
                 text="Luc tam dung, may da TAT mo cat de khong thung phoi.\n\n"
                      "Neu dang dung giua duong cat thi phai bat mo va cho duc "
                      "xuyen qua thanh ong roi moi chay tiep, neu khong mach cat "
                      "se bi dut doan.").pack(padx=14, pady=(12, 8))

        co_duc = tk.BooleanVar(value=True)
        thoi_gian = tk.StringVar(value=f"{self.thoi_gian_duc_lo.get():g}")

        h = tk.Frame(hop, bg=MAU["khung"])
        h.pack(fill="x", padx=14)
        o_tg = ttk.Entry(h, width=7, textvariable=thoi_gian)

        def doi():
            o_tg.config(state="normal" if co_duc.get() else "disabled")

        tk.Checkbutton(h, text="Bat mo va cho duc lo truoc khi chay tiep",
                       variable=co_duc, bg=MAU["khung"], command=doi,
                       anchor="w").pack(anchor="w")
        h2 = tk.Frame(hop, bg=MAU["khung"])
        h2.pack(fill="x", padx=34, pady=(2, 10))
        tk.Label(h2, text="Thoi gian duc lo:", bg=MAU["khung"]).pack(side="left")
        o_tg.pack(in_=h2, side="left", padx=6)
        tk.Label(h2, text="giay", bg=MAU["khung"], fg=MAU["chu_mo"]).pack(side="left")

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

        h3 = tk.Frame(hop, bg=MAU["khung"])
        h3.pack(pady=(0, 12))
        tk.Button(h3, text="CHAY TIEP", width=14, command=chay_tiep,
                  **self._kieu_nut("#1971c2", "white")).pack(side="left", padx=6)
        ttk.Button(h3, text="Khong chay nua", width=14,
                   command=hop.destroy).pack(side="left", padx=6)
        hop.grab_set()
        hop.bind("<Return>", lambda e: chay_tiep())

    def _dung(self):
        self.may.huy_nap()
        if self._dang_ket_noi():
            self.may.gui("STOP")
            self._ghi("> STOP", "gui")
        self.mo_dang_bat = False
        self.nut_mo_cat.config(text="Bat mo", bg="#dfe4ea")

    # ==================================================================
    # HANG DOI SU KIEN (luong nen -> luong giao dien)
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
                elif loai == "baud":
                    self.nhan_ket_noi.config(text=f"{self.o_com.get()} - {nd} baud")
        except queue.Empty:
            pass
        self._cap_nhat_dong_ho()
        self.root.after(50, self._doc_hang_doi)

    def _hoi_vi_tri_dinh_ky(self):
        """Hoi POS moi 2 giay khi may DANG RANH de o vi tri luon dung.

        KHONG hoi luc dang chay: moi lenh gui xuong deu lam ESP32 in ra UART, ma
        in giua chuoi cat se chan vong xuat xung -> tao vet dung tren duong cat.
        """
        if self._dang_ket_noi() and self.trang_thai in ("SAN_SANG", "DA_DUNG", "TAM_DUNG"):
            self.may.gui("POS")
        self.root.after(2000, self._hoi_vi_tri_dinh_ky)

    def _tu_esp32(self, dong):
        the = None
        if dong.startswith("Loi:") or dong.startswith("LOI_"):
            the = "loi"
            self._them_loi(dong, nang=True)
        elif dong.startswith(("OK", "RUNNING", "ZEROED", "RESUMED", "Hoan thanh",
                              "PLASMA_", "XONG_")):
            the = "ok"
        self._ghi(dong, the)

        if dong.startswith("Loi: DUNG KHAN CAP"):
            self._dat_trang_thai("LOI")
        elif dong.startswith("RUNNING") or dong.startswith("RESUMED"):
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
            self._cap_nhat_tien_do()
        if dong.startswith("CFG:"):
            self._doc_cau_hinh(dong)

    def _doc_cau_hinh(self, dong):
        for cap in dong[4:].strip().split():
            if "=" not in cap:
                continue
            ten, gia_tri = cap.split("=", 1)
            try:
                if ten == "che_do":
                    self.che_do.set(int(gia_tri))
                    self.o_che_do.set(TEN_CHE_DO.get(int(gia_tri), TEN_CHE_DO[1]))
                    self.nhan_mach.config(
                        text=GIAI_THICH_CHE_DO.get(int(gia_tri), ""))
                elif ten == "duong_kinh_ong":
                    self.duong_kinh.set(round(float(gia_tri), 3))
            except ValueError:
                pass

    def _xong_bai(self):
        self.doan_da_chay = self.tong_doan
        self._cap_nhat_tien_do()
        self.moc_bat_dau = None
        self.mo_dang_bat = False
        self.nut_mo_cat.config(text="Bat mo", bg="#dfe4ea")

    def _cap_nhat_tien_do(self):
        if self.tong_doan <= 0:
            return
        phan_tram = min(100, 100 * self.doan_da_chay / self.tong_doan)
        self.thanh_tien_do["value"] = phan_tram
        self.nhan_phan_tram.config(text=f"{phan_tram:.0f}%")
        self.mp.vi_tri_chay = min(self.doan_da_chay, self.tong_doan - 1)
        self._ve_3d()

    def _cap_nhat_dong_ho(self):
        if self.moc_bat_dau is None:
            return
        giay = int(time.time() - self.moc_bat_dau)
        self.nhan_thoi_gian.config(
            text=f"{giay // 3600:02d}:{(giay // 60) % 60:02d}:{giay % 60:02d}")

    def _nap_that_bai(self, chu):
        self._dat_trang_thai("SAN_SANG")
        self._them_loi("NAP THAT BAI: " + chu, nang=True)
        self._ghi("NAP THAT BAI: " + chu, "loi")
        messagebox.showerror("Nap that bai", chu + "\n\nMay CHUA chay.")

    # ==================================================================
    # TRANG THAI / NHAT KY / LOI
    # ==================================================================
    def _dat_trang_thai(self, ma):
        self.trang_thai = ma
        chu, mau = TRANG_THAI[ma]
        self.nhan_trang_thai.config(text=chu, bg=mau)
        self._cap_nhat_nut()

    def _cap_nhat_nut(self):
        noi = self._dang_ket_noi()
        chay = self.trang_thai in ("DANG_CHAY", "DANG_NAP")
        dung_tam = self.trang_thai == "TAM_DUNG"

        def dat(nut, bat):
            nut.config(state="normal" if bat else "disabled")

        dat(self.nut_chay, noi and not chay and self.trang_thai != "LOI")
        dat(self.nut_dung_tam, self.trang_thai == "DANG_CHAY")
        dat(self.nut_tiep, dung_tam)
        dat(self.nut_dung, noi)
        dat(self.nut_goc, noi and not chay and not dung_tam)
        dat(self.nut_mo_cat, noi and not chay)

    def _ghi(self, dong, the=None):
        self.term.config(state="normal")
        self.term.insert("end", time.strftime("[%H:%M:%S] ") + dong + "\n",
                         the or ())
        # Giu terminal khong phinh vo han khi chay bai dai
        if int(self.term.index("end-1c").split(".")[0]) > 2000:
            self.term.delete("1.0", "500.0")
        self.term.see("end")
        self.term.config(state="disabled")

    def _xoa_terminal(self):
        self.term.config(state="normal")
        self.term.delete("1.0", "end")
        self.term.config(state="disabled")

    def _them_loi(self, noi_dung, nang=True):
        cac = self.bang_loi.get_children()
        if cac and self.bang_loi.item(cac[-1])["values"][1] == noi_dung:
            return                     # khong lap lai cung mot loi lien tiep
        self.bang_loi.insert("", "end", values=(time.strftime("%H:%M:%S"), noi_dung),
                             tags=("nang",) if nang else ())
        self.bang_loi.see(self.bang_loi.get_children()[-1])

    def _xoa_loi(self):
        self.bang_loi.delete(*self.bang_loi.get_children())

    # ==================================================================
    def _thoat(self):
        if self.trang_thai in ("DANG_CHAY", "DANG_NAP"):
            if not messagebox.askyesno("Dang chay",
                                       "May DANG CHAY. Van thoat phan mem?"):
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
