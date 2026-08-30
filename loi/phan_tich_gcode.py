"""PHAN TICH G-CODE - chay tren may tinh, TRUOC khi gui xuong ESP32.

Doc truoc ca chuong trinh de:
  - dung lai duong di (ve hinh xem truoc + mo phong 3D)
  - bao truoc nhung dong firmware se tu choi, KHONG de may dung giua duong cat
  - chuan hoa ve dang firmware chac chan hieu
  - nen lai truoc khi day xuong day COM
"""

import math
import re

TOC_DO_CAT_MAC_DINH = 15.0      # F khi dang cat (RPM dong co)
TOC_DO_NHANH_MAC_DINH = 60.0    # F khi chay khong tai (G0, ve goc)

# Do sau vong dem cua firmware (#define SUC_CHUA_BUOC). Chi de bao cho nguoi
# dung biet, KHONG con la gioi han do dai chuong trinh - may tinh nap dan.
SUC_CHUA_BO_DEM = 1200

# Cac ma G firmware hieu (xem xu_ly_1_dong_gcode trong main.c)
MA_G_FIRMWARE_HIEU = {0, 1, 2, 3, 4, 17, 18, 19, 20, 21, 28, 30,
                      40, 41, 42, 43, 49, 54, 55, 56, 57, 58, 59,
                      61, 64, 80, 90, 91, 92, 93, 94, 98, 99}
MA_M_FIRMWARE_HIEU = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 30}
MA_G_DI_CHUYEN = {0, 1, 2, 3}

_RE_TOKEN = re.compile(r"([A-Za-z])\s*([+-]?(?:\d+\.?\d*|\.\d+))")


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
def nen_dong_gui(dong):
    """Nen mot dong G-code truoc khi day xuong day COM (KHONG doi y nghia).

    Duong COM la tai nguyen hiem nhat cua he thong - moi byte tiet kiem duoc la
    bo dem ESP32 day len nhanh hon bay nhieu. Ba viec, deu khong mat mat gi:
      - bo comment ';...' va '(...)' - may khong doc, gui xuong chi phi bang thong
      - bo khoang trang: "G1 X10 A45" -> "G1X10A45" (bo tach token cua firmware
        doc dung y het, xem tach_token_gcode trong main.c)
      - bo so 0 thua o duoi: "X10.500" -> "X10.5", "X10.000" -> "X10"

    Dong hien tren man hinh van giu nguyen dinh dang de nguoi doc - chi ban khi
    GUI moi nen.
    """
    cham = dong.find(";")
    if cham >= 0:
        dong = dong[:cham]
    while True:
        mo = dong.find("(")
        if mo < 0:
            break
        dong_ngoac = dong.find(")", mo)
        dong = dong[:mo] + (dong[dong_ngoac + 1:] if dong_ngoac >= 0 else "")
    dong = _BO_SO_0_THUA.sub(lambda m: m.group(0).rstrip("0").rstrip("."), dong)
    return "".join(dong.split())


# So thap phan co phan le: chi cat 0 thua o dang "12.3400", khong dong den "1200"
_BO_SO_0_THUA = re.compile(r"\d+\.\d+")


def nen_ca_bai(cac_dong):
    """Nen ca chuong trinh truoc khi gui, va BO HAN cac dong tro thanh rong.

    Dong chi co chu thich - "(Yen ngua 90 do)" - sau khi nen se khong con gi.
    Gui mot dong rong xuong ESP32 thi no bo qua ma KHONG bao nhan, lam may tinh
    dem thieu va bao nham "ESP32 nhan thieu dong". Loc thang o day, vua dung so
    dem vua do ton bang thong.
    """
    ra = []
    for dong in cac_dong:
        nen = nen_dong_gui(dong)
        if nen:
            ra.append(nen)
    return ra
