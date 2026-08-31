"""Kiem chung phan XEP BAI va quy uoc DO KHOANG CACH cua the xep 2D.

Khong mo cua so: chi goi thang cac ham tinh toan.
"""
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from loi import thu_vien_moi_noi as tv
from loi.xep_2d import Xep2D


class CanvasGia:
    """Thay cho Canvas that: bao kich thuoc, con moi lenh ve thi nuot khong lam gi."""
    def winfo_width(self):
        return 800

    def winfo_height(self):
        return 300

    def __getattr__(self, ten):        # delete / create_line / create_text ...
        return lambda *a, **k: None


loi = 0


def ktra(ten, dat, chi_tiet=""):
    global loi
    print(f"  [{'DAT' if dat else 'SAI'}] {ten}" + (f"   {chi_tiet}" if chi_tiet else ""))
    if not dat:
        loi += 1


print("=== 1. THU VIEN chi con dung 3 kieu ghep ===")
ktra("co dung 3 kieu", len(tv.THU_VIEN) == 3,
     ", ".join(k.ma for k in tv.THU_VIEN))
ktra("goc 90 do -> moi dau vat 45 do",
     "45" in tv.THEO_MA["goc_90"].sinh(60.0, {"a": 0.0}, 100.0).ten)
ktra("goc 45 do -> moi dau vat 22,5 do",
     "22.5" in tv.THEO_MA["goc_45"].sinh(60.0, {"a": 0.0}, 100.0).ten)
ktra("nhanh chu T -> long yen ngua 90 do",
     "Yen ngua" in tv.THEO_MA["nhanh_t_90"].sinh(
         60.0, {"d_chinh": 60.0, "khe_ho": 0.0, "a": 0.0}, 100.0).ten)

print("\n=== 2. Goc ghep = 2 lan goc vat (up hai mat vat vao nhau) ===")
for ma, goc_ghep in (("goc_90", 90.0), ("goc_45", 45.0)):
    r = 30.0
    d = tv.THEO_MA[ma].sinh(2 * r, {"a": 0.0}, 100.0)
    x1, x2 = d.pham_vi_x()
    # Be ngang mieng vat theo truc X = D * tan(goc_vat)  =>  goc = atan(rong / D)
    goc_vat = math.degrees(math.atan((x2 - x1) / (2 * r)))
    ktra(f"{ma}: goc vat do duoc = {goc_ghep / 2:g} do",
         abs(goc_vat - goc_ghep / 2) < 1e-6, f"{goc_vat:.4f} do")

print("\n=== 3. Goc dat mieng cat xoay ca duong cat ===")
d0 = tv.THEO_MA["goc_90"].sinh(60.0, {"a": 0.0}, 100.0)
d90 = tv.THEO_MA["goc_90"].sinh(60.0, {"a": 90.0}, 100.0)
ktra("xoay 90 do thi moi diem lech dung 90 do",
     all(abs((b[1] - a[1]) - 90.0) < 1e-9 for a, b in zip(d0.diem, d90.diem)))
ktra("xoay khong lam doi vi tri truc X",
     all(abs(a[0] - b[0]) < 1e-12 for a, b in zip(d0.diem, d90.diem)))

print("\n=== 4. XEP BAI: khuc noi tiep nhau, cach dung khoang khe ===")
muc, dai_khuc, khe = [], 200.0, 8.0
for ma, g in (("goc_90", {"a": 0.0}), ("goc_45", {"a": 0.0}),
              ("nhanh_t_90", {"d_chinh": 60.0, "khe_ho": 0.0, "a": 0.0})):
    x = tv.vi_tri_ke_tiep(60.0, muc, ma, g, dai_khuc, khe, chua_dau=20.0)
    muc.append({"ma": ma, "gia_tri": g, "x": x})
kq = tv.xep_bai(60.0, muc)
ktra("nhat cat dau: chua dau 20 + khuc 200 = 220",
     abs(muc[0]["x"] - 220.0) < 1e-9, f"{muc[0]['x']:.3f} mm")
for i in range(1, len(muc)):
    x_cuoi_truoc = tv.khung_duong_cat(kq.cac_duong[i - 1])[1]
    ktra(f"nhat {i + 1} cach cho sau nhat cua nhat {i} dung {khe:g}+{dai_khuc:g} mm",
         abs(muc[i]["x"] - (x_cuoi_truoc + khe + dai_khuc)) < 1e-9,
         f"{muc[i]['x'] - x_cuoi_truoc:.1f} mm")

print("\n=== 5. KHUNG duong cat = be ngang theo truc X ===")
for i, d in enumerate(kq.cac_duong):
    x1, x2, dai = tv.khung_duong_cat(d)
    thuc = max(p[0] for p in d.diem) - min(p[0] for p in d.diem)
    ktra(f"khung {i + 1} rong dung bang be ngang duong cat", abs(dai - thuc) < 1e-12,
         f"{dai:.3f} mm")

print("\n=== 6. DO KHOANG CACH tu DIEM GOC (dau ong xa mam kep) ===")
xep = Xep2D(CanvasGia())
xep.dat_du_lieu([(tv.khung_duong_cat(d)[0], tv.khung_duong_cat(d)[1], d.ten, m["x"])
                 for d, m in zip(kq.cac_duong, muc)], 60.0, 1200.0)
for i, d in enumerate(kq.cac_duong):
    x1, x2, _ = tv.khung_duong_cat(d)
    kc = xep.khoang_cach_tu_goc(x1, x2)
    ktra(f"nhat {i + 1}: do toi CANH GAN diem goc nhat (x_cuoi)",
         abs(kc - (1200.0 - x2)) < 1e-12, f"{kc:.1f} mm")
ktra("nhat cang xa mam kep thi khoang cach cang nho",
     all(xep.khoang_cach_tu_goc(*tv.khung_duong_cat(kq.cac_duong[i])[:2]) >
         xep.khoang_cach_tu_goc(*tv.khung_duong_cat(kq.cac_duong[i + 1])[:2])
         for i in range(len(kq.cac_duong) - 1)))

print("\n=== 7. Go khoang cach vao o -> nhat cat nhay dung cho ===")
for muon in (100.0, 350.0, 800.0):
    i = 1
    x1, x2, _ = tv.khung_duong_cat(kq.cac_duong[i])
    x_moi = xep.tu_khoang_cach(muon, x2 - x1, muc[i]["x"], x2)
    muc2 = [dict(m) for m in muc]
    muc2[i]["x"] = x_moi
    d_moi = tv.xep_bai(60.0, muc2).cac_duong[i]
    kc = xep.khoang_cach_tu_goc(*tv.khung_duong_cat(d_moi)[:2])
    ktra(f"go {muon:g} mm -> do lai duoc dung {muon:g} mm",
         abs(kc - muon) < 1e-9, f"{kc:.4f} mm")

print("\n=== 8. Doi pixel <-> mm cua the xep khop nhau ===")
for mm in (0.0, 250.0, 1200.0):
    ktra(f"{mm:g} mm -> pixel -> mm", abs(xep.sang_mm(xep.sang_pixel(mm)) - mm) < 1e-9)
xep.phong_to(3.0, 400)
ktra("sau khi phong to van khop", abs(xep.sang_mm(xep.sang_pixel(600.0)) - 600.0) < 1e-9)
ktra("phong to giu nguyen diem duoi con tro",
     abs(xep.sang_pixel(xep.sang_mm(400)) - 400) < 1e-9)

print("\n=== 9. Bao loi khi cay ong khong du dai ===")
kq2 = tv.xep_bai(60.0, muc, dai_cay_ong=300.0)
ktra("bao THIEU khi cay ong ngan", any("THIEU" in c for c in kq2.canh_bao),
     kq2.canh_bao[0] if kq2.canh_bao else "khong bao gi")
ktra("khong bao gi khi cay ong du dai",
     not tv.xep_bai(60.0, muc, dai_cay_ong=2000.0).canh_bao)

print(f"\n{'=== TAT CA DAT ===' if not loi else f'=== CO {loi} LOI ==='}")
sys.exit(1 if loi else 0)
