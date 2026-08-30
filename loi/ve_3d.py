"""MO PHONG 3D - ve ong trong mam kep, duong cat tren mat ong, va dau cat.

Ve thang len Canvas cua Tkinter, KHONG can thu vien do hoa nao. Cach lam:
  - dung mo hinh bang cac mat tu giac nho
  - sap xep theo do sau roi ve tu xa den gan (thuat toan "tho son")
  - mat quay ra sau bi mat truoc de len tren => trong nhu khoi dac
"""

import math


class CanhNhin:
    """Goc nhin va do phong cua nguoi xem."""

    def __init__(self):
        self.xoay_ngang = -32.0     # do
        self.xoay_doc = 20.0        # do
        self.phong = 1.0
        self.day = 0.0              # tinh tien ngang khi keo chuot phai

    def dat_lai(self):
        self.__init__()


class MoPhong3D:
    def __init__(self, canvas):
        self.canvas = canvas
        self.canh_nhin = CanhNhin()

        self.duong_kinh = 60.0
        self.chieu_dai_ong = 300.0
        self.doan = []              # (x1, a1, x2, a2, la_cat) - tu bo phan tich
        self.vi_tri_chay = None     # chi so doan dang chay (mo phong)

        self.mau_nen = "#1b1f24"
        self.mau_ong = (0x8a, 0x93, 0x9e)
        self.mau_mam = (0x4a, 0x86, 0xc8)

    # ------------------------------------------------------------------
    def dat_du_lieu(self, doan, duong_kinh, chieu_dai=None):
        self.doan = doan or []
        self.duong_kinh = max(duong_kinh, 1.0)
        if chieu_dai:
            self.chieu_dai_ong = chieu_dai
        else:
            x_max = 100.0
            for x1, _, x2, _, _ in self.doan:
                x_max = max(x_max, x1, x2)
            self.chieu_dai_ong = x_max * 1.15 + 40.0

    # ------------------------------------------------------------------
    # PHEP CHIEU
    # ------------------------------------------------------------------
    def _quay(self, x, y, z):
        """Quay diem theo goc nhin roi chieu ra man hinh (phep chieu truc giao)."""
        cn = self.canh_nhin
        a = math.radians(cn.xoay_ngang)
        b = math.radians(cn.xoay_doc)
        # quay quanh truc dung (Y)
        x1 = x * math.cos(a) + z * math.sin(a)
        z1 = -x * math.sin(a) + z * math.cos(a)
        # quay quanh truc ngang (X)
        y2 = y * math.cos(b) - z1 * math.sin(b)
        z2 = y * math.sin(b) + z1 * math.cos(b)
        return x1, y2, z2

    def _chieu(self, x, y, z, tam_x, tam_y, ty_le):
        px, py, pz = self._quay(x, y, z)
        return (tam_x + px * ty_le + self.canh_nhin.day,
                tam_y - py * ty_le,
                pz)

    @staticmethod
    def _to_bong(mau_goc, sang):
        """Do bong mot mat theo huong phap tuyen so voi nguon sang."""
        sang = max(0.18, min(1.0, sang))
        return "#%02x%02x%02x" % tuple(min(255, int(c * sang)) for c in mau_goc)

    # ------------------------------------------------------------------
    # VE
    # ------------------------------------------------------------------
    def ve(self):
        c = self.canvas
        c.delete("all")
        rong = c.winfo_width() or 640
        cao = c.winfo_height() or 400
        c.create_rectangle(0, 0, rong, cao, fill=self.mau_nen, outline="")

        r = self.duong_kinh / 2.0
        dai = self.chieu_dai_ong
        # Ty le sao cho ca ong lan mam kep vua khung hinh
        ty_le = min(rong / (dai * 1.5 + 1), cao / (self.duong_kinh * 3.4 + 1))
        ty_le *= self.canh_nhin.phong
        tam_x, tam_y = rong / 2, cao / 2
        goc_x = -dai / 2.0          # goc toa do X=0 nam o day (sat mam kep)

        mat = []                    # (do_sau, cac_diem, mau, vien)

        def them_mat(diem_3d, mau, vien=""):
            chieu = [self._chieu(x, y, z, tam_x, tam_y, ty_le) for x, y, z in diem_3d]
            do_sau = sum(p[2] for p in chieu) / len(chieu)
            phang = []
            for p in chieu:
                phang.extend((p[0], p[1]))
            mat.append((do_sau, phang, mau, vien))

        self._them_mam_kep(them_mat, r, goc_x)
        self._them_than_ong(them_mat, r, goc_x, dai)

        # Ve tu XA den GAN: mat gan che mat xa => khoi trong nhu dac
        for _, diem, mau, vien in sorted(mat, key=lambda m: m[0]):
            c.create_polygon(diem, fill=mau, outline=vien or mau, width=1)

        self._ve_duong_cat(r, goc_x, tam_x, tam_y, ty_le)
        self._ve_dau_cat(r, goc_x, tam_x, tam_y, ty_le)
        self._ve_chu_thich(rong, cao)

    # ------------------------------------------------------------------
    def _them_than_ong(self, them_mat, r, goc_x, dai, so_mui=48):
        """Than ong: mot vanh cac mat tu giac chay doc theo ong."""
        for i in range(so_mui):
            g1 = 2 * math.pi * i / so_mui
            g2 = 2 * math.pi * (i + 1) / so_mui
            y1, z1 = r * math.cos(g1), r * math.sin(g1)
            y2, z2 = r * math.cos(g2), r * math.sin(g2)
            # Phap tuyen giua mat, chieu len huong sang (tren - truoc)
            gm = (g1 + g2) / 2
            sang = 0.30 + 0.70 * max(0.0, math.cos(gm - 1.0))
            mau = self._to_bong(self.mau_ong, sang)
            them_mat([(goc_x, y1, z1), (goc_x + dai, y1, z1),
                      (goc_x + dai, y2, z2), (goc_x, y2, z2)], mau)
        # Mat cat o dau ong xa (de nhin ra ong rong)
        for i in range(so_mui):
            g1 = 2 * math.pi * i / so_mui
            g2 = 2 * math.pi * (i + 1) / so_mui
            them_mat([(goc_x + dai, 0, 0),
                      (goc_x + dai, r * math.cos(g1), r * math.sin(g1)),
                      (goc_x + dai, r * math.cos(g2), r * math.sin(g2))],
                     self._to_bong(self.mau_ong, 0.35))

    def _them_mam_kep(self, them_mat, r, goc_x, so_mui=40):
        """MAM KEP 3 chau - de nhin ra dau nao la goc toa do cua ong."""
        r_mam = r * 2.3
        day = r * 0.55
        x0 = goc_x - day
        for i in range(so_mui):
            g1 = 2 * math.pi * i / so_mui
            g2 = 2 * math.pi * (i + 1) / so_mui
            y1, z1 = r_mam * math.cos(g1), r_mam * math.sin(g1)
            y2, z2 = r_mam * math.cos(g2), r_mam * math.sin(g2)
            gm = (g1 + g2) / 2
            sang = 0.32 + 0.68 * max(0.0, math.cos(gm - 1.0))
            them_mat([(x0, y1, z1), (goc_x, y1, z1),
                      (goc_x, y2, z2), (x0, y2, z2)],
                     self._to_bong(self.mau_mam, sang))
        # Mat truoc cua mam (hinh vanh khan)
        for i in range(so_mui):
            g1 = 2 * math.pi * i / so_mui
            g2 = 2 * math.pi * (i + 1) / so_mui
            them_mat([(goc_x, r * math.cos(g1), r * math.sin(g1)),
                      (goc_x, r_mam * math.cos(g1), r_mam * math.sin(g1)),
                      (goc_x, r_mam * math.cos(g2), r_mam * math.sin(g2)),
                      (goc_x, r * math.cos(g2), r * math.sin(g2))],
                     self._to_bong(self.mau_mam, 0.95))
        # Ba chau kep
        for k in range(3):
            g = 2 * math.pi * k / 3 + 0.4
            for lech in (-0.16, 0.16):
                gy, gz = math.cos(g + lech), math.sin(g + lech)
                them_mat([(x0 - day * 0.3, r * gy, r * gz),
                          (goc_x + r * 0.35, r * gy, r * gz),
                          (goc_x + r * 0.35, r_mam * 0.8 * gy, r_mam * 0.8 * gz),
                          (x0 - day * 0.3, r_mam * 0.8 * gy, r_mam * 0.8 * gz)],
                         self._to_bong((0xd0, 0xd6, 0xdd), 0.75 + 0.25 * math.cos(g - 1.0)))

    # ------------------------------------------------------------------
    def _ve_duong_cat(self, r, goc_x, tam_x, tam_y, ty_le):
        """Duong cat nam TREN mat ong. Chi ve doan dang quay ve phia nguoi xem."""
        c = self.canvas
        r_ve = r * 1.012        # nhac len chut cho khong bi than ong che mat
        tong = len(self.doan)
        for chi_so, (x1, a1, x2, a2, la_cat) in enumerate(self.doan):
            g1, g2 = math.radians(a1), math.radians(a2)
            p1 = self._chieu(goc_x + x1, r_ve * math.cos(g1), r_ve * math.sin(g1),
                             tam_x, tam_y, ty_le)
            p2 = self._chieu(goc_x + x2, r_ve * math.cos(g2), r_ve * math.sin(g2),
                             tam_x, tam_y, ty_le)
            if p1[2] < -r * 0.15 and p2[2] < -r * 0.15:
                continue        # nam khuat sau ong
            if not la_cat:
                mau, day = "#5a6472", 1
            elif self.vi_tri_chay is None:
                mau, day = "#ff5c4d", 2
            elif chi_so < self.vi_tri_chay:
                mau, day = "#3fa34d", 2        # da cat xong
            elif chi_so == self.vi_tri_chay:
                mau, day = "#ffd23f", 3        # dang cat
            else:
                mau, day = "#ff5c4d", 2        # con lai
            c.create_line(p1[0], p1[1], p2[0], p2[1], fill=mau, width=day)
        if tong == 0:
            c.create_text(tam_x, tam_y, text="Chua co duong cat nao",
                          fill="#6c7581", font=("Segoe UI", 11))

    def _ve_dau_cat(self, r, goc_x, tam_x, tam_y, ty_le):
        """Dau cat plasma - dung o vi tri dang chay, luon o phia truoc ong."""
        if not self.doan:
            return
        chi_so = self.vi_tri_chay if self.vi_tri_chay is not None else 0
        chi_so = max(0, min(chi_so, len(self.doan) - 1))
        x, a = self.doan[chi_so][2], self.doan[chi_so][3]
        g = math.radians(a)

        c = self.canvas
        # Mo cat luon nam o dinh ong (12 gio) - ong xoay chu dau cat khong xoay
        cao_mo = r * 1.9
        chan = self._chieu(goc_x + x, r * 1.02, 0.0, tam_x, tam_y, ty_le)
        dinh = self._chieu(goc_x + x, cao_mo, 0.0, tam_x, tam_y, ty_le)
        than = self._chieu(goc_x + x, cao_mo * 1.55, 0.0, tam_x, tam_y, ty_le)
        c.create_line(dinh[0], dinh[1], than[0], than[1], fill="#c8ced6", width=7)
        c.create_line(chan[0], chan[1], dinh[0], dinh[1], fill="#e8703a", width=3)
        c.create_oval(chan[0] - 4, chan[1] - 4, chan[0] + 4, chan[1] + 4,
                      fill="#ffd23f", outline="")
        # Diem dang cat tren mat ong
        diem = self._chieu(goc_x + x, r * 1.02 * math.cos(g), r * 1.02 * math.sin(g),
                           tam_x, tam_y, ty_le)
        if diem[2] > -r * 0.2:
            c.create_oval(diem[0] - 3, diem[1] - 3, diem[0] + 3, diem[1] + 3,
                          fill="#ffffff", outline="")

    def _ve_chu_thich(self, rong, cao):
        c = self.canvas
        c.create_text(10, 12, anchor="w", fill="#9aa4b0", font=("Consolas", 9),
                      text=f"Ong D{self.duong_kinh:g}  |  dai {self.chieu_dai_ong:.0f}mm"
                           f"  |  {len(self.doan)} doan")
        y = cao - 14
        for chu, mau, lech in (("cat", "#ff5c4d", 0), ("chay nhanh", "#5a6472", 52),
                               ("dang cat", "#ffd23f", 150)):
            c.create_line(10 + lech, y, 26 + lech, y, fill=mau, width=3)
            c.create_text(30 + lech, y, anchor="w", text=chu, fill="#9aa4b0",
                          font=("Segoe UI", 8))
        c.create_text(rong - 10, cao - 14, anchor="e", fill="#5a6472",
                      font=("Segoe UI", 8),
                      text="keo chuot trai = xoay  |  lan chuot = phong to")
