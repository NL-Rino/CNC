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
             bo_tron=0.0, so_diem=360):
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
    diem = _bo_tron_day(diem, r, bo_tron)
    return DuongCat(f"Yen ngua {goc_do:g} do", diem, kin=True)


def _bo_tron_day(diem, r, ban_kinh):
    """Bo tron nhung cho duong cat GAP GOC qua gat, bang phep "lan bi".

    TAI SAO CAN: khi ong nhanh va ong chinh BANG duong kinh nhau, day long yen
    ngua la mot diem NHON that su (cong thuc rut gon thanh L = R*|cos phi|).
    Tai diem do truc X phai doi chieu NGAY LAP TUC o het toc do cat - do duoc
    +-0,4 mm moi buoc. Dong co buoc khong the dao chieu nhu vay: no se TRUOT
    BUOC va vi tri sau do sai het. Ma mo plasma co be rong mach cat huu han
    cung khong tao noi goc nhon do.

    CACH LAM: lan mot vien bi ban kinh R doc theo day rooc (phep "dong" hinh
    thai hoc - closing). Cho nao vien bi lot vao duoc thi giu nguyen; cho nao
    hep hon vien bi thi thay bang chinh mat vien bi.
      - Ket qua LUON >= duong cat goc, tuc chi de lai vat lieu chu khong cat
        lem them - dieu can thiet vi cat lem la hong phoi
      - Cho nao von da tron hon vien bi thi KHONG bi dong toi
      - Do o mat phang trai phang (s = r*phi) nen vien bi tron that trong
        khong gian, khong bi meo theo duong kinh ong

    Voi ong chinh chi to hon ong nhanh vai mm, do gap da con +-0,01 mm nen
    ham nay gan nhu khong lam gi.
    """
    if ban_kinh <= 0 or len(diem) < 5:
        return diem
    # Doi sang (s, x): s la chieu dai cung that tren mat ong
    s_x = [(math.radians(a) * r, x) for x, a in diem]
    n = len(s_x)
    chu_vi = 2 * math.pi * r

    def lay(i):
        """Lay diem theo chi so VONG TRON - duong cat khep kin nen day yen o
        goc 0 do cung phai duoc bo tron nhu moi cho khac."""
        j = i % (n - 1)
        vong = (i - j) // (n - 1)
        s, x = s_x[j]
        return s + vong * chu_vi, x

    # So diem nam trong ban kinh vien bi
    buoc_s = chu_vi / (n - 1)
    k = max(1, int(math.ceil(ban_kinh / max(buoc_s, 1e-9))))

    def vom(t):
        d = ban_kinh * ban_kinh - t * t
        return math.sqrt(d) if d > 0 else 0.0

    # Phinh ra roi co lai = phep DONG: lap day nhung day rooc hep hon vien bi
    phinh = []
    for i in range(n):
        s0, _ = lay(i)
        phinh.append(max(lay(i + t)[1] + vom(lay(i + t)[0] - s0)
                         for t in range(-k, k + 1)))

    def phinh_vong(i):
        return phinh[i % (n - 1)]

    ra = []
    for i in range(n):
        s0, x0 = lay(i)
        x_moi = min(phinh_vong(i + t) - vom(lay(i + t)[0] - s0)
                    for t in range(-k, k + 1))
        # Chan chan: khong bao gio cat sau hon duong cat goc
        ra.append((max(x_moi, x0), diem[i][1]))
    return ra


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
    """Mot kieu ghep trong thu vien. Giao dien chi doc bang nay, khong biet chi tiet."""

    def __init__(self, ma, ten, mo_ta, tham_so, ham, hinh):
        self.ma = ma
        self.ten = ten
        self.mo_ta = mo_ta
        self.tham_so = tham_so     # [ThamSo, ...] - KHONG gom vi tri X
        self._ham = ham
        self.hinh = hinh           # ma hinh minh hoa (ve_bieu_tuong doc)

    def sinh(self, duong_kinh_ong, gia_tri, x_goc=0.0):
        """Sinh DuongCat tai vi tri x_goc doc theo ong.

        Vi tri KHONG con la tham so nguoi dung nhap: no duoc tinh tu chieu dai
        cua tung khuc ong khi xep bai (xem xep_bai).
        """
        for ts in self.tham_so:
            loi = ts.kiem_tra(gia_tri.get(ts.ma, ts.mac_dinh))
            if loi:
                raise ValueError(loi)
        return self._ham(duong_kinh_ong / 2.0, gia_tri, x_goc)


def _xoay(duong, a_lech):
    """Xoay ca duong cat quanh truc ong mot goc (de dat mieng cat huong khac)."""
    if not a_lech:
        return duong
    return DuongCat(duong.ten, [(x, a + a_lech) for x, a in duong.diem], duong.kin)


# ----- BA KIEU GHEP -----
# Ghep hai ong thanh mot goc thi MOI DAU chi can vat NUA goc do, roi up hai mat
# vat vao nhau. Vi du goc 90 do -> moi dau vat 45 do; goc 45 do -> vat 22,5 do.
THU_VIEN = [
    KieuMoiNoi(
        "goc_90", "Ghep goc 90 do (dau ong)",
        "Noi hai ong thanh goc vuong o dau ong. Moi dau vat 45 do (nua cua 90), "
        "up hai mat vat vao nhau la thanh goc vuong.",
        [ThamSo("a", "Goc dat mieng vat", 0.0, "do")],
        lambda r, g, x: _xoay(cat_vat(r, 45.0, x), g.get("a", 0.0)),
        "goc_90"),

    KieuMoiNoi(
        "goc_45", "Ghep goc 45 do (dau ong)",
        "Noi hai ong thanh goc 45 do o dau ong. Moi dau vat 22,5 do "
        "(nua cua 45), up hai mat vat vao nhau.",
        [ThamSo("a", "Goc dat mieng vat", 0.0, "do")],
        lambda r, g, x: _xoay(cat_vat(r, 22.5, x), g.get("a", 0.0)),
        "goc_45"),

    KieuMoiNoi(
        "nhanh_t_90", "Ong nhanh chu T 90 do",
        "Ong nay la ONG NHANH, dau duoc cat long yen ngua de om vuong goc vao "
        "GIUA than mot ong chinh. Ong chinh khong phai cat gi.",
        [ThamSo("d_chinh", "Duong kinh ong chinh", 60.0, "mm", 1.0),
         ThamSo("khe_ho", "Khe ho han", 0.0, "mm", 0.0, 10.0),
         ThamSo("bo_tron", "Bo tron day yen", 2.0, "mm", 0.0, 20.0),
         ThamSo("a", "Goc dat mieng cat", 0.0, "do")],
        lambda r, g, x: _xoay(yen_ngua(r, g["d_chinh"] / 2.0, 90.0,
                                       khe_ho=g["khe_ho"], x_goc=x,
                                       bo_tron=g.get("bo_tron", 0.0)), g.get("a", 0.0)),
        "nhanh_t_90"),
]

THEO_MA = {k.ma: k for k in THU_VIEN}


# =============================================================================
# XEP BAI - cat mot cay ong thanh nhieu khuc noi tiep
# =============================================================================
class KetQuaXep:
    def __init__(self):
        self.cac_duong = []     # [DuongCat, ...] cung thu tu voi cac muc
        self.tong_dung = 0.0    # cho xa nhat cua bai tinh tu mam kep (mm)
        self.canh_bao = []


def xep_bai(duong_kinh_ong, cac_muc, dai_cay_ong=None):
    """Sinh duong cat cho tung muc trong bai.

    cac_muc: [{"ma": <ma kieu>, "gia_tri": {...}, "x": <vi tri tam nhat cat, mm>}, ...]

    "x" la vi tri TAM cua nhat cat, do tu MAM KEP. Voi mat cat vat thi tam la
    tam ong; voi long yen ngua thi la diem hai truc ong gap nhau. Do theo tam
    chu khong theo mep vi mep dai va mep ngan khac nhau, con tam thi khong doi.
    """
    kq = KetQuaXep()
    for i, muc in enumerate(cac_muc, 1):
        kieu = THEO_MA.get(muc["ma"])
        if kieu is None:
            raise ValueError(f"Nhat cat {i}: khong biet kieu ghep {muc['ma']!r}")
        try:
            duong = kieu.sinh(duong_kinh_ong, muc["gia_tri"], float(muc.get("x", 0.0)))
        except ValueError as loi:
            raise ValueError(f"Nhat cat {i} ({kieu.ten}): {loi}") from loi
        kq.cac_duong.append(duong)
        # Cay ong bi chiem toi cho XA NHAT cua nhat cat, khong phai toi tam no
        kq.tong_dung = max(kq.tong_dung, duong.pham_vi_x()[1])

    if dai_cay_ong and kq.tong_dung > dai_cay_ong:
        kq.canh_bao.append(
            f"Bai can {kq.tong_dung:.1f} mm nhung cay ong chi dai {dai_cay_ong:g} mm "
            f"- THIEU {kq.tong_dung - dai_cay_ong:.1f} mm.")
    return kq


def vi_tri_ke_tiep(duong_kinh_ong, cac_muc, ma_moi, gia_tri_moi, dai_khuc, khe,
                   chua_dau=20.0):
    """Tinh vi tri dat nhat cat MOI de no nam noi tiep sau cac nhat da co.

    Khuc ong nam giua nhat cat truoc va nhat cat moi phai dai dung dai_khuc, con
    khe la phan vat lieu mat di cho mach cat va cho kep.
    """
    if dai_khuc <= 0:
        raise ValueError("Chieu dai khuc phai lon hon 0")
    if khe < 0:
        raise ValueError("Khoang cach giua cac nhat cat khong the am")
    if not cac_muc:
        return chua_dau + dai_khuc
    truoc = xep_bai(duong_kinh_ong, cac_muc)
    return truoc.tong_dung + khe + dai_khuc


def khung_duong_cat(duong):
    """Khung chu nhat bao quanh mot duong cat theo truc ong: (x_dau, x_cuoi, dai).

    Trong the XEP 2D moi nhat cat duoc ve thanh mot hinh chu nhat dai bang be
    ngang cua no theo truc X - nhin phat la biet no an het bao nhieu ong.
    """
    x1, x2 = duong.pham_vi_x()
    return (x1, x2, x2 - x1)


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
