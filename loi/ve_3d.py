"""MO PHONG 3D - ve ong va duong cat tren mat ong.

Ve thang len Canvas cua Tkinter, KHONG can thu vien do hoa nao. Cach lam:
  - dung mo hinh bang cac mat tu giac nho
  - sap xep theo do sau roi ve tu xa den gan (thuat toan "tho son")
  - mat quay ra sau bi mat truoc de len tren => trong nhu khoi dac

DUNG NHU MAY THAT: dau cat DUNG YEN, ONG quay quanh truc va truot ra vao.
Truoc day mo phong cho dau cat chay doc theo ong - nhin thi hieu duoc nhung
sai voi may that, de gay nham lan khi canh chinh.
"""

import math


class CanhNhin:
    """Goc nhin va do phong cua nguoi xem."""

    def __init__(self):
        self.xoay_ngang = -32.0     # do
        self.xoay_doc = 20.0        # do
        self.phong = 1.0
        self.day_ngang = 0.0        # tinh tien khi keo chuot giua
        self.day_doc = 0.0

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

        # Cac duong cat bi AN di cho de nhin. Chua chi so cua tung doan.
        self.doan_bi_an = set()
        # Nhom cac doan lien tiep thanh "mieng cat" de an ca mieng chi bang 1 cu bam
        self.nhom = []              # [(chi_so_dau, chi_so_cuoi), ...]
        self.nhom_bi_an = set()

        self.mau_nen = "#1b1f24"
        self.mau_ong = (0x8a, 0x93, 0x9e)

        # Diem bam gan nhat (de giao dien biet nguoi dung vua bam trung mieng nao)
        self._diem_man_hinh = []    # [(x_man, y_man, chi_so_doan), ...]

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
        self._chia_nhom()

    def _chia_nhom(self):
        """Gom cac doan CAT lien tiep thanh mot mieng - bam 1 cai la an ca mieng."""
        self.nhom = []
        dau = None
        for i, d in enumerate(self.doan):
            if d[4]:
                if dau is None:
                    dau = i
            elif dau is not None:
                self.nhom.append((dau, i - 1))
                dau = None
        if dau is not None:
            self.nhom.append((dau, len(self.doan) - 1))
        self.nhom_bi_an = {n for n in self.nhom_bi_an if n < len(self.nhom)}

    def nhom_cua_doan(self, chi_so):
        for k, (a, b) in enumerate(self.nhom):
            if a <= chi_so <= b:
                return k
        return None

    def bat_tat_nhom(self, k):
        if k is None:
            return
        if k in self.nhom_bi_an:
            self.nhom_bi_an.discard(k)
        else:
            self.nhom_bi_an.add(k)

    def hien_lai_het(self):
        self.nhom_bi_an.clear()

    def _bi_an(self, chi_so):
        k = self.nhom_cua_doan(chi_so)
        return k is not None and k in self.nhom_bi_an

    # ------------------------------------------------------------------
    # VI TRI ONG (giong may that)
    # ------------------------------------------------------------------
    def _trang_thai_ong(self):
        """Tra ve (x_ong, goc_ong): ong truot ra bao nhieu va da quay bao nhieu do.

        May that giu dau cat DUNG YEN o dinh, con ong thi vua truot doc truc vua
        quay. Vi vay diem dang cat luon nam ngay duoi dau cat.

        Khi CHUA chay thi khong co diem cat nao dang o duoi mo, luc do dat ong
        sao cho phan co duong cat nam giua khung hinh - neu khong, ong dai 1200mm
        se cham dau X=0 vao giua man hinh roi chay tuot ra ngoai khung.
        """
        if self.vi_tri_chay is None or not self.doan:
            return self._giua_vung_cat(), 0.0
        i = max(0, min(self.vi_tri_chay, len(self.doan) - 1))
        return self.doan[i][2], self.doan[i][3]

    def _giua_vung_cat(self):
        if not self.doan:
            return self.chieu_dai_ong / 2.0
        cac_x = [d[0] for d in self.doan] + [d[2] for d in self.doan]
        return (min(cac_x) + max(cac_x)) / 2.0

    # ------------------------------------------------------------------
    # PHEP CHIEU
    # ------------------------------------------------------------------
    def _quay(self, x, y, z):
        cn = self.canh_nhin
        a = math.radians(cn.xoay_ngang)
        b = math.radians(cn.xoay_doc)
        x1 = x * math.cos(a) + z * math.sin(a)
        z1 = -x * math.sin(a) + z * math.cos(a)
        y2 = y * math.cos(b) - z1 * math.sin(b)
        z2 = y * math.sin(b) + z1 * math.cos(b)
        return x1, y2, z2

    def _chieu(self, x, y, z, tam_x, tam_y, ty_le):
        px, py, pz = self._quay(x, y, z)
        return (tam_x + px * ty_le + self.canh_nhin.day_ngang,
                tam_y - py * ty_le + self.canh_nhin.day_doc,
                pz)

    @staticmethod
    def _to_bong(mau_goc, sang):
        sang = max(0.18, min(1.0, sang))
        return "#%02x%02x%02x" % tuple(min(255, int(c * sang)) for c in mau_goc)

    # ------------------------------------------------------------------
    # VE
    # ------------------------------------------------------------------
    def ve(self):
        c = self.canvas
        c.delete("all")
        self._diem_man_hinh = []
        rong = c.winfo_width() or 640
        cao = c.winfo_height() or 400
        c.create_rectangle(0, 0, rong, cao, fill=self.mau_nen, outline="")

        r = self.duong_kinh / 2.0
        dai = self.chieu_dai_ong
        ty_le = min(rong / (dai * 1.35 + 1), cao / (self.duong_kinh * 3.0 + 1))
        ty_le *= self.canh_nhin.phong
        tam_x, tam_y = rong / 2, cao / 2

        # ----- ONG TRUOT RA VAO: mo cat dung yen o giua khung hinh -----
        # Toa do ve = toa do tren ong TRU di vi tri dang cat, nen diem dang cat
        # luon roi vao giua man hinh, con ca cay ong thi truot qua.
        x_ong, goc_ong = self._trang_thai_ong()
        goc_ong_rad = math.radians(goc_ong)
        goc_x = -x_ong                    # X=0 cua ong nam o day tren man hinh

        mat = []

        def them_mat(diem_3d, mau, vien=""):
            chieu = [self._chieu(x, y, z, tam_x, tam_y, ty_le) for x, y, z in diem_3d]
            do_sau = sum(p[2] for p in chieu) / len(chieu)
            phang = []
            for p in chieu:
                phang.extend((p[0], p[1]))
            mat.append((do_sau, phang, mau, vien))

        self._them_than_ong(them_mat, r, goc_x, dai, goc_ong_rad)

        for _, diem, mau, vien in sorted(mat, key=lambda m: m[0]):
            c.create_polygon(diem, fill=mau, outline=vien or mau, width=1)

        self._ve_duong_cat(r, goc_x, tam_x, tam_y, ty_le, goc_ong_rad)
        self._ve_dau_cat(r, tam_x, tam_y, ty_le)
        self._ve_chu_thich(rong, cao, x_ong, goc_ong)

    # ------------------------------------------------------------------
    def _them_than_ong(self, them_mat, r, goc_x, dai, goc_ong, so_mui=48):
        """Than ong. goc_ong = ong da QUAY bao nhieu radian quanh truc cua no."""
        for i in range(so_mui):
            g1 = 2 * math.pi * i / so_mui + goc_ong
            g2 = 2 * math.pi * (i + 1) / so_mui + goc_ong
            y1, z1 = r * math.cos(g1), r * math.sin(g1)
            y2, z2 = r * math.cos(g2), r * math.sin(g2)
            gm = (g1 + g2) / 2
            sang = 0.30 + 0.70 * max(0.0, math.cos(gm - 1.0))
            mau = self._to_bong(self.mau_ong, sang)
            them_mat([(goc_x, y1, z1), (goc_x + dai, y1, z1),
                      (goc_x + dai, y2, z2), (goc_x, y2, z2)], mau)
        # Hai mat cat o hai dau ong
        for x_dau, sang in ((goc_x, 0.30), (goc_x + dai, 0.38)):
            for i in range(so_mui):
                g1 = 2 * math.pi * i / so_mui + goc_ong
                g2 = 2 * math.pi * (i + 1) / so_mui + goc_ong
                them_mat([(x_dau, 0, 0),
                          (x_dau, r * math.cos(g1), r * math.sin(g1)),
                          (x_dau, r * math.cos(g2), r * math.sin(g2))],
                         self._to_bong(self.mau_ong, sang))

    # ------------------------------------------------------------------
    def _ve_duong_cat(self, r, goc_x, tam_x, tam_y, ty_le, goc_ong):
        c = self.canvas
        r_ve = r * 1.012
        for chi_so, (x1, a1, x2, a2, la_cat) in enumerate(self.doan):
            if self._bi_an(chi_so):
                continue
            g1 = math.radians(a1) + goc_ong
            g2 = math.radians(a2) + goc_ong
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
                mau, day = "#ff5c4d", 2
            c.create_line(p1[0], p1[1], p2[0], p2[1], fill=mau, width=day)
            if la_cat:
                self._diem_man_hinh.append((p1[0], p1[1], chi_so))
        if not self.doan:
            c.create_text(tam_x, tam_y, text="Chua co duong cat nao",
                          fill="#6c7581", font=("Segoe UI", 11))

    def _ve_dau_cat(self, r, tam_x, tam_y, ty_le):
        """Dau cat DUNG YEN o dinh khung hinh - dung nhu may that."""
        c = self.canvas
        cao_mo = r * 1.9
        chan = self._chieu(0.0, r * 1.02, 0.0, tam_x, tam_y, ty_le)
        dinh = self._chieu(0.0, cao_mo, 0.0, tam_x, tam_y, ty_le)
        than = self._chieu(0.0, cao_mo * 1.6, 0.0, tam_x, tam_y, ty_le)
        c.create_line(dinh[0], dinh[1], than[0], than[1], fill="#c8ced6", width=8)
        c.create_line(chan[0], chan[1], dinh[0], dinh[1], fill="#e8703a", width=3)
        c.create_oval(chan[0] - 5, chan[1] - 5, chan[0] + 5, chan[1] + 5,
                      fill="#ffd23f", outline="")

    def _ve_chu_thich(self, rong, cao, x_ong, goc_ong):
        c = self.canvas
        an = f"  |  dang an {len(self.nhom_bi_an)} mieng" if self.nhom_bi_an else ""
        c.create_text(10, 12, anchor="w", fill="#9aa4b0", font=("Consolas", 9),
                      text=f"Ong D{self.duong_kinh:g}  |  dai {self.chieu_dai_ong:.0f}mm"
                           f"  |  {len(self.doan)} doan{an}")
        if self.vi_tri_chay is not None:
            c.create_text(10, 28, anchor="w", fill="#ffd23f", font=("Consolas", 9),
                          text=f"Ong: truot X={x_ong:.1f}mm   quay A={goc_ong:.1f}do")
        y = cao - 14
        for chu, mau, lech in (("cat", "#ff5c4d", 0), ("chay nhanh", "#5a6472", 52),
                               ("dang cat", "#ffd23f", 150)):
            c.create_line(10 + lech, y, 26 + lech, y, fill=mau, width=3)
            c.create_text(30 + lech, y, anchor="w", text=chu, fill="#9aa4b0",
                          font=("Segoe UI", 8))
        c.create_text(rong - 10, cao - 14, anchor="e", fill="#5a6472",
                      font=("Segoe UI", 8),
                      text="keo trai = xoay  |  keo giua = day  |  lan = phong to")

    # ------------------------------------------------------------------
    def doan_gan_diem(self, x_man, y_man, ban_kinh=14):
        """Tim doan cat gan diem bam nhat - dung cho che do AN BOT duong cat."""
        gan_nhat, cach_nhat = None, ban_kinh * ban_kinh
        for px, py, chi_so in self._diem_man_hinh:
            d = (px - x_man) ** 2 + (py - y_man) ** 2
            if d < cach_nhat:
                gan_nhat, cach_nhat = chi_so, d
        return gan_nhat
