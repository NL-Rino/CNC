"""THU VIEN MOI NOI - sinh duong cat cho tung kieu ghep ong.

May giu ong trong mam kep va XOAY no (truc A, don vi do), dong thoi keo mo cat
doc theo ong (truc X, don vi mm). Vi vay MOI duong cat deu quy ve mot ham:

        A (do)  ->  X (mm)

Toan bo cong thuc o day deu la hinh hoc CHINH XAC (khong xap xi), duoc dan
ngay tren tung ham de sau nay con kiem lai duoc.

QUY UOC CHUNG
  - r  = ban kinh ong DANG CAT (ong nam trong mam kep)
  - A  = goc xoay ong, 0 do la duong sinh nam o dinh ong
  - X  = vi tri doc truc, tang dan khi di xa mam kep
  - Moi phep cat tra ve mot "DuongCat": danh sach diem (X, A) noi tiep nhau
"""

import math


# =============================================================================
# KIEU DU LIEU
# =============================================================================
class DuongCat:
    """Mot duong cat kin hoac ho tren mat ong."""

    def __init__(self, ten, diem, kin=True):
        self.ten = ten
        self.diem = diem      # [(x_mm, a_do), ...]
        self.kin = kin        # True = duong khep kin (quay het 1 vong)

    def pham_vi_x(self):
        if not self.diem:
            return (0.0, 0.0)
        cac_x = [d[0] for d in self.diem]
        return (min(cac_x), max(cac_x))


class ThamSo:
    """Mo ta mot o nhap cua kieu moi noi (de giao dien tu dung form)."""

    def __init__(self, ma, nhan, mac_dinh, don_vi="mm", nho_nhat=None, lon_nhat=None):
        self.ma = ma
        self.nhan = nhan
        self.mac_dinh = mac_dinh
        self.don_vi = don_vi
        self.nho_nhat = nho_nhat
        self.lon_nhat = lon_nhat

    def kiem_tra(self, gia_tri):
        """Tra ve chuoi loi, hoac None neu hop le."""
        if self.nho_nhat is not None and gia_tri < self.nho_nhat:
            return f"{self.nhan} phai >= {self.nho_nhat}"
        if self.lon_nhat is not None and gia_tri > self.lon_nhat:
            return f"{self.nhan} phai <= {self.lon_nhat}"
        return None


# =============================================================================
# HAM PHU
# =============================================================================
def _goc_deu(so_diem):
    """so_diem goc chia deu tu 0 den 360 do (co ca diem 360 de khep kin).

    So diem LUON duoc lam tron len boi cua 4 de cac goc 0 / 90 / 180 / 270 roi
    dung vao mau. Day chinh la cho SAU NHAT va NONG NHAT cua moi duong cat
    (day yen ngua, dinh cat vat...). Neu truot qua chung thi may bo lai mot chut
    vat lieu ngay tai cho quan trong nhat - vi du yen ngua hai ong bang nhau,
    day yen dang le cham truc ong chinh lai con du 0,1 mm.
    """
    so_diem = ((so_diem + 3) // 4) * 4
    return [i * 360.0 / so_diem for i in range(so_diem + 1)]


def _do_min(so_diem_moi_vong, r):
    """So diem can thiet de duong cat du muot tren ong ban kinh r.

    Lay theo CHIEU DAI CUNG chu khong lay so diem co dinh: ong D200 co chu vi
    gap 5 lan ong D40 nen phai nhieu diem hon moi cho ra duong cong nhu nhau.
    Muc tieu: moi doan thang khong dai qua ~0,4 mm tren mat ong.
    """
    chu_vi = 2 * math.pi * max(r, 1.0)
    can = int(chu_vi / 0.4)
    return max(so_diem_moi_vong, min(can, 1440))


# =============================================================================
# CAC KIEU MOI NOI
# =============================================================================
def yen_ngua(r, r_chinh, goc_do=90.0, lech_tam=0.0, khe_ho=0.0, x_goc=0.0,
             so_diem=360):
    """YEN NGUA (fishmouth): dau ong nhanh om vao than ong chinh.

    Hinh hoc:
      Ong chinh: hinh tru ban kinh R, truc trung voi truc z.
      Ong nhanh: hinh tru ban kinh r, truc nghieng goc theta so voi truc z,
                 cat qua truc ong chinh.

      Diem tren mat ong nhanh, cach diem giao L doc theo truc nhanh, o goc phi:
          P = L*w + r*cos(phi)*u + r*sin(phi)*v
      voi   w = (sin t, 0, cos t)      truc ong nhanh
            u = (cos t, 0, -sin t)     vuong goc w, nam trong mat phang xz
            v = (0, 1, 0)

      Bat P nam tren mat ong chinh  (Px^2 + Py^2 = R^2):
          (L sin t + r cos phi cos t)^2 + (r sin phi)^2 = R^2
      =>  L(phi) = [ sqrt(R^2 - r^2 sin^2 phi) - r cos phi cos t ] / sin t

      Goc 90 do rut gon thanh  L = sqrt(R^2 - r^2 sin^2 phi).

    lech_tam: truc ong nhanh khong cat truc ong chinh ma lech di e (mm).
      Luc do dieu kien thanh (L sin t + r cos phi cos t)^2 + (e + r sin phi)^2 = R^2
    khe_ho: noi rong duong cat de con cho han (tru bot L).
    """
    if r <= 0:
        raise ValueError("Ban kinh ong nhanh phai > 0")
    if r_chinh <= 0:
        raise ValueError("Ban kinh ong chinh phai > 0")
    if r > r_chinh:
        raise ValueError(f"Ong nhanh (D{2*r:.0f}) khong the lon hon ong chinh "
                         f"(D{2*r_chinh:.0f}) - khong om vao duoc")
    if not 5.0 <= goc_do <= 175.0:
        raise ValueError("Goc yen ngua phai trong khoang 5..175 do")

    t = math.radians(goc_do)
    sin_t, cos_t = math.sin(t), math.cos(t)
    diem = []
    for a in _goc_deu(_do_min(so_diem, r)):
        phi = math.radians(a)
        # Nua truc y cua diem tren ong nhanh, cong them do lech tam
        y = lech_tam + r * math.sin(phi)
        duoi_can = r_chinh * r_chinh - y * y
        if duoi_can < 0:
            raise ValueError(
                f"Lech tam {lech_tam:.1f}mm qua lon: ong nhanh tut ra ngoai ong chinh")
        L = (math.sqrt(duoi_can) - r * math.cos(phi) * cos_t) / sin_t
        diem.append((x_goc + L - khe_ho, a))
    return DuongCat(f"Yen ngua {goc_do:g} do", diem, kin=True)


def cat_thang(x_goc=0.0, so_diem=180, r=30.0):
    """CAT THANG: cat vuong goc voi truc ong (dau ong phang)."""
    diem = [(x_goc, a) for a in _goc_deu(_do_min(so_diem, r))]
    return DuongCat("Cat thang", diem, kin=True)


def cat_vat(r, goc_do=45.0, x_goc=0.0, so_diem=360):
    """CAT VAT (miter): mat phang cat nghieng goc so voi truc ong.

    Hinh hoc: mat phang di qua diem (x_goc, 0, 0), phap tuyen nghieng goc beta.
    Diem tren mat ong o goc phi co toa do (r cos phi, r sin phi) trong mat cat
    ngang, nen vi tri truc:
        X(phi) = x_goc + r * tan(beta) * cos(phi)

    Dung de ghep CO (elbow): hai ong cung vat beta = goc_co/2 roi up vao nhau.
    Vi du co 90 do -> hai dau cung vat 45 do.
    """
    if not 0.0 <= goc_do < 89.0:
        raise ValueError("Goc vat phai trong khoang 0..89 do "
                         "(89 do tro len thi duong cat dai vo han)")
    he_so = r * math.tan(math.radians(goc_do))
    diem = []
    for a in _goc_deu(_do_min(so_diem, r)):
        diem.append((x_goc + he_so * math.cos(math.radians(a)), a))
    return DuongCat(f"Cat vat {goc_do:g} do", diem, kin=True)


def lo_tron(r, duong_kinh_lo, x_tam=0.0, a_tam=0.0, so_diem=180):
    """LO TRON khoan XUYEN TAM qua thanh ong.

    Hinh hoc: lo la hinh tru ban kinh p, truc huong theo phuong ban kinh cua ong.
      Dat truc ong = z, tam lo o goc A=0 nen phap tuyen n = x.
      Diem tren mat tru lo:  Q = ( t, p sin psi, p cos psi )
      Bat Q nam tren mat ong  (Qx^2 + Qy^2 = r^2):
          t = sqrt(r^2 - p^2 sin^2 psi)
      Suy ra tren mat ong:
          X = x_tam + p cos psi
          A = a_tam + asin( p sin psi / r )

    Day la cong thuc CHINH XAC. Neu chi lay X = p cos psi, A = p sin psi / r
    (xap xi phang) thi lo se bi HEP lai theo chieu vong, cang ro khi lo to.
    """
    p = duong_kinh_lo / 2.0
    if p <= 0:
        raise ValueError("Duong kinh lo phai > 0")
    if p >= r:
        raise ValueError(f"Lo D{duong_kinh_lo:g} khong the >= duong kinh ong D{2*r:g}")

    diem = []
    for i in range(so_diem + 1):
        psi = 2 * math.pi * i / so_diem
        x = x_tam + p * math.cos(psi)
        a = a_tam + math.degrees(math.asin(max(-1.0, min(1.0, p * math.sin(psi) / r))))
        diem.append((x, a))
    return DuongCat(f"Lo tron D{duong_kinh_lo:g}", diem, kin=True)


def ranh_dai(r, chieu_dai, chieu_rong, x_tam=0.0, a_tam=0.0, doc_truc=True,
             so_diem_cung=48):
    """RANH DAI (slot): hai canh thang + hai dau bo tron.

    doc_truc = True  : ranh nam doc theo truc ong
    doc_truc = False : ranh nam theo chieu vong quanh ong

    Phan bo tron o hai dau dung dung cong thuc cua lo_tron nen ranh khong bi
    meo khi ong nho.
    """
    p = chieu_rong / 2.0
    if p <= 0:
        raise ValueError("Chieu rong ranh phai > 0")
    if p >= r:
        raise ValueError("Chieu rong ranh khong the >= duong kinh ong")
    if chieu_dai <= chieu_rong:
        raise ValueError("Chieu dai ranh phai lon hon chieu rong "
                         "(neu bang nhau thi dung LO TRON)")

    nua = (chieu_dai - chieu_rong) / 2.0

    def sang_goc(cung_mm):
        """Doi chieu dai cung tren mat ong sang goc quay (do) - chinh xac."""
        return math.degrees(math.asin(max(-1.0, min(1.0, cung_mm / r))))

    diem = []
    if doc_truc:
        # Hai nua duong tron o hai dau, noi bang hai doan thang doc truc
        for i in range(so_diem_cung + 1):        # dau phai
            psi = -math.pi / 2 + math.pi * i / so_diem_cung
            diem.append((x_tam + nua + p * math.cos(psi),
                         a_tam + sang_goc(p * math.sin(psi))))
        for i in range(so_diem_cung + 1):        # dau trai
            psi = math.pi / 2 + math.pi * i / so_diem_cung
            diem.append((x_tam - nua + p * math.cos(psi),
                         a_tam + sang_goc(p * math.sin(psi))))
        diem.append(diem[0])
    else:
        goc_nua = sang_goc(nua) if nua < r else 90.0
        for i in range(so_diem_cung + 1):
            psi = -math.pi / 2 + math.pi * i / so_diem_cung
            diem.append((x_tam + p * math.sin(psi),
                         a_tam + goc_nua + sang_goc(p * math.cos(psi))))
        for i in range(so_diem_cung + 1):
            psi = math.pi / 2 + math.pi * i / so_diem_cung
            diem.append((x_tam + p * math.sin(psi),
                         a_tam - goc_nua + sang_goc(p * math.cos(psi))))
        diem.append(diem[0])
    return DuongCat(f"Ranh {chieu_dai:g}x{chieu_rong:g}", diem, kin=True)


def khia_chu_v(r, goc_uon_do, x_tam=0.0, a_tam=0.0, phan_chu_vi=0.5, so_diem=180):
    """KHIA CHU V de UON ong.

    Khia mot ranh chu V roi bop lai thi ong gap duoc mot goc. Chieu rong khia
    tai dinh (cho sau nhat) phai bang:
        w = 2 * r * tan(goc_uon / 2)
    Do rong nay giam dan ve hai ben theo hinh dang thuc te cua vet khia:
        w(A) = w_dinh * cos( A ) trong pham vi khia
    (A tinh tu tam khia; ngoai pham vi thi khong cat de con "ban le" giu ong)
    """
    if not 5.0 <= goc_uon_do <= 150.0:
        raise ValueError("Goc uon phai trong khoang 5..150 do")
    if not 0.1 <= phan_chu_vi <= 0.9:
        raise ValueError("Phan chu vi bi khia phai trong khoang 0,1..0,9")

    nua_rong = r * math.tan(math.radians(goc_uon_do) / 2.0)
    nua_goc = 180.0 * phan_chu_vi

    diem = []
    for i in range(so_diem + 1):           # canh tren cua chu V
        a = -nua_goc + 2 * nua_goc * i / so_diem
        rong = nua_rong * math.cos(math.radians(a * 90.0 / nua_goc))
        diem.append((x_tam + rong, a_tam + a))
    for i in range(so_diem + 1):           # canh duoi, chay nguoc lai
        a = nua_goc - 2 * nua_goc * i / so_diem
        rong = nua_rong * math.cos(math.radians(a * 90.0 / nua_goc))
        diem.append((x_tam - rong, a_tam + a))
    diem.append(diem[0])
    return DuongCat(f"Khia V uon {goc_uon_do:g} do", diem, kin=True)


# =============================================================================
# BANG DANG KY - giao dien doc bang nay de dung thu vien, khong biet chi tiet
# =============================================================================
class KieuMoiNoi:
    def __init__(self, ma, ten, mo_ta, tham_so, ham, hinh):
        self.ma = ma
        self.ten = ten
        self.mo_ta = mo_ta
        self.tham_so = tham_so     # [ThamSo, ...]
        self._ham = ham
        self.hinh = hinh           # ma hinh minh hoa (ve_bieu_tuong doc)

    def sinh(self, duong_kinh_ong, gia_tri):
        """Sinh DuongCat. gia_tri la dict {ma_tham_so: so}."""
        for ts in self.tham_so:
            loi = ts.kiem_tra(gia_tri.get(ts.ma, ts.mac_dinh))
            if loi:
                raise ValueError(loi)
        return self._ham(duong_kinh_ong / 2.0, gia_tri)


THU_VIEN = [
    KieuMoiNoi(
        "yen_ngua_t", "Yen ngua chu T (90 do)",
        "Dau ong om vao than ong chinh, vuong goc. Kieu ghep pho bien nhat.",
        [ThamSo("d_chinh", "Duong kinh ong chinh", 60.0, "mm", 1.0),
         ThamSo("x", "Vi tri dau ong", 0.0, "mm"),
         ThamSo("khe_ho", "Khe ho han", 0.0, "mm", 0.0, 10.0)],
        lambda r, g: yen_ngua(r, g["d_chinh"] / 2.0, 90.0,
                              khe_ho=g["khe_ho"], x_goc=g["x"]),
        "yen_ngua_t"),

    KieuMoiNoi(
        "yen_ngua_goc", "Yen ngua goc nghieng",
        "Nhu chu T nhung ong nhanh dam vao ong chinh o goc bat ky.",
        [ThamSo("d_chinh", "Duong kinh ong chinh", 60.0, "mm", 1.0),
         ThamSo("goc", "Goc giua 2 ong", 45.0, "do", 5.0, 175.0),
         ThamSo("lech_tam", "Do lech tam", 0.0, "mm"),
         ThamSo("x", "Vi tri dau ong", 0.0, "mm"),
         ThamSo("khe_ho", "Khe ho han", 0.0, "mm", 0.0, 10.0)],
        lambda r, g: yen_ngua(r, g["d_chinh"] / 2.0, g["goc"],
                              lech_tam=g["lech_tam"], khe_ho=g["khe_ho"],
                              x_goc=g["x"]),
        "yen_ngua_goc"),

    KieuMoiNoi(
        "cat_vat", "Cat vat (ghep co)",
        "Cat nghieng mot goc. Hai ong cung vat nua goc co roi up vao nhau "
        "thi thanh khuyu - vi du co 90 do thi moi dau vat 45 do.",
        [ThamSo("goc", "Goc vat", 45.0, "do", 0.0, 88.0),
         ThamSo("x", "Vi tri cat", 0.0, "mm")],
        lambda r, g: cat_vat(r, g["goc"], g["x"]),
        "cat_vat"),

    KieuMoiNoi(
        "cat_thang", "Cat thang (cat dut)",
        "Cat vuong goc voi truc ong - cat ong thanh doan, hoac lam phang dau ong.",
        [ThamSo("x", "Vi tri cat", 100.0, "mm")],
        lambda r, g: cat_thang(g["x"], r=r),
        "cat_thang"),

    KieuMoiNoi(
        "lo_tron", "Lo tron",
        "Khoan mot lo tron xuyen tam qua thanh ong.",
        [ThamSo("d_lo", "Duong kinh lo", 20.0, "mm", 0.5),
         ThamSo("x", "Vi tri tam lo", 100.0, "mm"),
         ThamSo("a", "Goc dat lo", 0.0, "do")],
        lambda r, g: lo_tron(r, g["d_lo"], g["x"], g["a"]),
        "lo_tron"),

    KieuMoiNoi(
        "ranh_doc", "Ranh dai doc truc",
        "Ranh dai bo tron hai dau, nam doc theo chieu dai ong.",
        [ThamSo("dai", "Chieu dai ranh", 40.0, "mm", 1.0),
         ThamSo("rong", "Chieu rong ranh", 12.0, "mm", 0.5),
         ThamSo("x", "Vi tri tam ranh", 100.0, "mm"),
         ThamSo("a", "Goc dat ranh", 0.0, "do")],
        lambda r, g: ranh_dai(r, g["dai"], g["rong"], g["x"], g["a"], doc_truc=True),
        "ranh_doc"),

    KieuMoiNoi(
        "ranh_vong", "Ranh dai theo chieu vong",
        "Nhu tren nhung ranh nam vat ngang quanh than ong.",
        [ThamSo("dai", "Chieu dai ranh (cung)", 40.0, "mm", 1.0),
         ThamSo("rong", "Chieu rong ranh", 12.0, "mm", 0.5),
         ThamSo("x", "Vi tri tam ranh", 100.0, "mm"),
         ThamSo("a", "Goc dat ranh", 0.0, "do")],
        lambda r, g: ranh_dai(r, g["dai"], g["rong"], g["x"], g["a"], doc_truc=False),
        "ranh_vong"),

    KieuMoiNoi(
        "khia_v", "Khia chu V de uon",
        "Khia mot ranh chu V roi bop lai de uon ong mot goc dinh truoc.",
        [ThamSo("goc_uon", "Goc muon uon", 90.0, "do", 5.0, 150.0),
         ThamSo("x", "Vi tri khia", 100.0, "mm"),
         ThamSo("a", "Goc dat khia", 0.0, "do"),
         ThamSo("phan", "Phan chu vi bi khia", 0.5, "", 0.1, 0.9)],
        lambda r, g: khia_chu_v(r, g["goc_uon"], g["x"], g["a"], g["phan"]),
        "khia_v"),
]

THEO_MA = {k.ma: k for k in THU_VIEN}


# =============================================================================
# SINH G-CODE TU DANH SACH PHEP CAT
# =============================================================================
def sinh_gcode(cac_duong, toc_do_cat, toc_do_nhanh, thoi_gian_duc_lo=0.8,
               x_ve_cho=None, tieu_de=None):
    """Doi danh sach DuongCat thanh chuong trinh G-code hoan chinh.

    Moi duong cat -> chay nhanh toi diem dau, bat mo, cho duc lo, cat het duong,
    tat mo. Cac duong noi tiep nhau nen may khong bao gio dung giua duong cat.
    """
    ra = []
    if tieu_de:
        for dong in tieu_de:
            ra.append(f"({dong})")
    ra += ["G21", "G90"]
    for duong in cac_duong:
        if not duong.diem:
            continue
        x0, a0 = duong.diem[0]
        ra.append(f"({duong.ten})")
        ra.append(f"G0 X{_gon(x0)} A{_gon(a0)} F{toc_do_nhanh:g}")
        ra.append("M3")
        if thoi_gian_duc_lo > 0:
            ra.append(f"G4 P{thoi_gian_duc_lo:g}")
        # Dat F ngay tren dong G1 DAU TIEN chu khong de mot dong "F..." rieng:
        # dong F dung mot minh la G-code hop le nhung de rieng thi ton them mot
        # dong tren duong COM, va mot so bo dieu khien khac khong nhan.
        for i, (x, a) in enumerate(duong.diem[1:]):
            f = f" F{toc_do_cat:g}" if i == 0 else ""
            ra.append(f"G1 X{_gon(x)} A{_gon(a)}{f}")
        ra.append("M5")
    if x_ve_cho is not None:
        ra.append(f"G0 X{_gon(x_ve_cho)} A0 F{toc_do_nhanh:g}")
    ra.append("M30")
    return ra


def _gon(gia_tri):
    chuoi = f"{gia_tri:.3f}".rstrip("0").rstrip(".")
    return chuoi if chuoi not in ("", "-") else "0"
