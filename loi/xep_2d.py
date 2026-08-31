"""THE XEP 2D - nhin cay ong nam thang, keo tha cac nhat cat doc theo no.

Moi nhat cat duoc ve thanh MOT HINH CHU NHAT dai bang be ngang cua duong cat do
theo truc ong. Nhin phat la biet nhat cat an het bao nhieu ong.

DIEM GOC de do khoang cach la DAU ONG XA MAM KEP NHAT. Khoang cach cua mot nhat
cat = tu diem goc toi CANH GAN DIEM GOC NHAT cua khung chu nhat do.

    mam kep |=====================================| dau xa (DIEM GOC)
            0                                    dai_cay          (toa do may)
            <----------- khoang cach ------------>
                              [khung]
"""

import math


class KhungNhin2D:
    def __init__(self):
        self.phong = 1.0        # pixel tren mot mm
        self.day = 0.0          # tinh tien ngang (pixel)
        self.tu_dong = True     # True = tu canh cho vua khung hinh

    def dat_lai(self):
        self.phong = 1.0
        self.day = 0.0
        self.tu_dong = True


class Xep2D:
    """Ve va xu ly tuong tac cua the xep. KHONG tu sua du lieu bai.

    Giao dien truyen vao mot ham goi lai khi nguoi dung keo mot nhat cat sang
    cho khac, va tu quyet dinh co chap nhan hay khong.
    """

    LE_TRAI = 60        # chua cho nhan thuoc ben trai
    LE_PHAI = 20
    CAO_THUOC = 34

    def __init__(self, canvas, khi_chon=None, khi_keo=None):
        self.canvas = canvas
        self.khung = KhungNhin2D()
        self.khi_chon = khi_chon      # khi_chon(chi_so hoac None)
        self.khi_keo = khi_keo        # khi_keo(chi_so, x_tam_moi_mm)

        self.duong_kinh = 60.0
        self.dai_cay_ong = 1000.0
        self.cac_khung = []      # [(x_dau, x_cuoi, ten, x_tam), ...] toa do may
        self.dang_chon = None

        self._keo_khung = None   # chi so nhat cat dang bi keo
        self._keo_lech = 0.0     # chenh lech giua diem bam va tam nhat cat (mm)
        self._day_tu = None      # diem bat dau khi day khung nhin bang chuot giua

    # ------------------------------------------------------------------
    def dat_du_lieu(self, cac_khung, duong_kinh, dai_cay_ong):
        self.cac_khung = cac_khung
        self.duong_kinh = max(duong_kinh, 1.0)
        self.dai_cay_ong = max(dai_cay_ong, 1.0)
        if self.dang_chon is not None and self.dang_chon >= len(cac_khung):
            self.dang_chon = None

    # ------------------------------------------------------------------
    # DOI TOA DO
    # ------------------------------------------------------------------
    def _ty_le(self):
        rong = self.canvas.winfo_width() or 800
        if self.khung.tu_dong:
            return max(0.02, (rong - self.LE_TRAI - self.LE_PHAI) / self.dai_cay_ong)
        return self.khung.phong

    def sang_pixel(self, x_may):
        return self.LE_TRAI + x_may * self._ty_le() + self.khung.day

    def sang_mm(self, px):
        return (px - self.LE_TRAI - self.khung.day) / self._ty_le()

    def khoang_cach_tu_goc(self, x_dau, x_cuoi):
        """Tu DAU ONG XA MAM KEP toi canh gan diem goc nhat cua khung.

        Diem goc o toa do may = dai_cay_ong. Canh gan no nhat la canh co toa do
        may LON hon, tuc x_cuoi.
        """
        return self.dai_cay_ong - x_cuoi

    def tu_khoang_cach(self, khoang_cach, dai_khung, x_tam, x_cuoi):
        """Doi nguoc: nguoi dung go khoang cach -> tam nhat cat nam o dau."""
        x_cuoi_moi = self.dai_cay_ong - khoang_cach
        return x_tam + (x_cuoi_moi - x_cuoi)

    # ------------------------------------------------------------------
    # VE
    # ------------------------------------------------------------------
    def ve(self):
        c = self.canvas
        c.delete("all")
        rong = c.winfo_width() or 800
        cao = c.winfo_height() or 300
        c.create_rectangle(0, 0, rong, cao, fill="#161a1f", outline="")

        y_ong = self.CAO_THUOC + (cao - self.CAO_THUOC) / 2
        nua_cao = min(46, (cao - self.CAO_THUOC) * 0.22)

        self._ve_thuoc(rong, cao)
        self._ve_ong(y_ong, nua_cao)
        self._ve_cac_khung(y_ong, nua_cao, cao)
        self._ve_chu_thich(rong, cao)

    def _ve_ong(self, y_ong, nua_cao):
        c = self.canvas
        x1 = self.sang_pixel(0.0)
        x2 = self.sang_pixel(self.dai_cay_ong)
        c.create_rectangle(x1, y_ong - nua_cao, x2, y_ong + nua_cao,
                           fill="#2b3138", outline="#4a525c")
        c.create_line(x1, y_ong, x2, y_ong, fill="#3a424c", dash=(6, 4))
        # Mam kep o dau X=0
        c.create_rectangle(x1 - 14, y_ong - nua_cao * 1.7, x1,
                           y_ong + nua_cao * 1.7, fill="#2f6fb8", outline="#4a86c8")
        c.create_text(x1 - 7, y_ong - nua_cao * 1.7 - 9, text="mam kep",
                      fill="#6f9fd8", font=("Segoe UI", 7))
        # DIEM GOC o dau xa
        c.create_line(x2, y_ong - nua_cao * 2.0, x2, y_ong + nua_cao * 2.0,
                      fill="#ffd23f", width=2)
        c.create_text(x2, y_ong - nua_cao * 2.0 - 9, text="DIEM GOC (dau xa)",
                      fill="#ffd23f", font=("Segoe UI", 7), anchor="e")

    def _ve_thuoc(self, rong, cao):
        """Thuoc do tu DIEM GOC (dau xa) tro ve, don vi mm."""
        c = self.canvas
        c.create_rectangle(0, 0, rong, self.CAO_THUOC, fill="#1e242b", outline="")
        c.create_line(0, self.CAO_THUOC, rong, self.CAO_THUOC, fill="#3a424c")

        ty_le = self._ty_le()
        # Chon buoc chia sao cho vach cach nhau it nhat ~55 pixel
        for buoc in (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000):
            if buoc * ty_le >= 55:
                break
        buoc_nho = buoc / 5.0

        k = 0
        while True:
            d = k * buoc_nho                  # khoang cach tu DIEM GOC
            if d > self.dai_cay_ong + buoc:
                break
            px = self.sang_pixel(self.dai_cay_ong - d)
            k += 1
            if px < -50 or px > rong + 50:
                continue
            lon = abs(d / buoc - round(d / buoc)) < 1e-9
            c.create_line(px, self.CAO_THUOC - (13 if lon else 6),
                          px, self.CAO_THUOC, fill="#6b7686" if lon else "#454d57")
            if lon:
                c.create_text(px, 9, text=f"{d:g}", fill="#8b96a5",
                              font=("Consolas", 8))

    def _ve_cac_khung(self, y_ong, nua_cao, cao):
        c = self.canvas
        for i, (x_dau, x_cuoi, ten, x_tam) in enumerate(self.cac_khung):
            p1 = self.sang_pixel(x_dau)
            p2 = self.sang_pixel(x_cuoi)
            if p2 - p1 < 3:            # nhat cat rat mong: van ve thay duoc
                giua = (p1 + p2) / 2
                p1, p2 = giua - 1.5, giua + 1.5
            chon = (i == self.dang_chon)
            c.create_rectangle(p1, y_ong - nua_cao * 1.25, p2, y_ong + nua_cao * 1.25,
                               fill="#8c2f2f" if chon else "#5c2626",
                               outline="#ffd23f" if chon else "#b84848",
                               width=2 if chon else 1, tags=("khung", f"k{i}"))
            c.create_text((p1 + p2) / 2, y_ong, text=str(i + 1),
                          fill="#ffe9c9", font=("Segoe UI", 9, "bold"))

            if chon:
                # Duong goc + so do khoang cach tu DIEM GOC toi canh gan nhat
                x_goc_px = self.sang_pixel(self.dai_cay_ong)
                y_do = y_ong + nua_cao * 1.9
                c.create_line(p2, y_do, x_goc_px, y_do, fill="#ffd23f", width=1,
                              arrow="both")
                kc = self.khoang_cach_tu_goc(x_dau, x_cuoi)
                c.create_text((p2 + x_goc_px) / 2, y_do - 9,
                              text=f"{kc:.1f} mm", fill="#ffd23f",
                              font=("Consolas", 9, "bold"))
                c.create_text((p1 + p2) / 2, y_ong - nua_cao * 1.25 - 10,
                              text=f"{ten}  (rong {x_cuoi - x_dau:.1f} mm)",
                              fill="#ffd23f", font=("Segoe UI", 8))

    def _ve_chu_thich(self, rong, cao):
        c = self.canvas
        if not self.cac_khung:
            c.create_text(rong / 2, cao / 2 + 30,
                          text="Chua co nhat cat nao - them tu thu vien moi noi",
                          fill="#6c7581", font=("Segoe UI", 10))
        c.create_text(rong - 8, cao - 10, anchor="e", fill="#5a6472",
                      font=("Segoe UI", 8),
                      text="keo khung = doi cho  |  keo chuot giua = day  |  "
                           "lan chuot = phong to")

    # ------------------------------------------------------------------
    # TUONG TAC
    # ------------------------------------------------------------------
    def bam(self, x_man, y_man):
        """Bam chuot trai: chon nhat cat duoi con tro (hoac bo chon)."""
        chon = None
        for i, (x_dau, x_cuoi, _, _) in enumerate(self.cac_khung):
            p1, p2 = self.sang_pixel(x_dau), self.sang_pixel(x_cuoi)
            if p2 - p1 < 8:
                giua = (p1 + p2) / 2
                p1, p2 = giua - 4, giua + 4
            if p1 - 2 <= x_man <= p2 + 2:
                chon = i
                break
        self.dang_chon = chon
        if chon is not None:
            self._keo_khung = chon
            self._keo_lech = self.cac_khung[chon][3] - self.sang_mm(x_man)
        else:
            self._keo_khung = None
        if self.khi_chon:
            self.khi_chon(chon)
        self.ve()

    def keo(self, x_man, y_man):
        if self._keo_khung is None:
            return
        x_tam_moi = self.sang_mm(x_man) + self._keo_lech
        if self.khi_keo:
            self.khi_keo(self._keo_khung, x_tam_moi)

    def nha(self):
        self._keo_khung = None

    def bat_dau_day(self, x_man):
        self._day_tu = x_man

    def day_khung_nhin(self, x_man):
        if self._day_tu is None:
            return
        self.khung.tu_dong = False
        self.khung.phong = self._ty_le()
        self.khung.day += x_man - self._day_tu
        self._day_tu = x_man
        self.ve()

    def het_day(self):
        self._day_tu = None

    def phong_to(self, he_so, x_man=None):
        """Phong to quanh diem dat con tro, de cho dang xem khong bi troi di."""
        ty_le_cu = self._ty_le()
        if x_man is None:
            x_man = (self.canvas.winfo_width() or 800) / 2
        mm_duoi_con_tro = self.sang_mm(x_man)

        self.khung.tu_dong = False
        self.khung.phong = max(0.02, min(80.0, ty_le_cu * he_so))
        # Giu nguyen diem mm dang nam duoi con tro
        self.khung.day = x_man - self.LE_TRAI - mm_duoi_con_tro * self.khung.phong
        self.ve()

    def vua_khung_hinh(self):
        self.khung.dat_lai()
        self.ve()
