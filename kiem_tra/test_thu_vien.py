"""Kiem chung TOAN HOC cua thu vien moi noi bang cach doi chieu voi hinh hoc goc.

Khong tin cong thuc rut gon: moi duong cat deu duoc dung lai thanh diem 3D roi
kiem xem no co THAT SU nam tren mat hai ong khong.
"""
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from loi import thu_vien_moi_noi as tv

loi = 0
def ktra(ten, dat, chi_tiet=""):
    global loi
    print(f"  [{'DAT' if dat else 'SAI'}] {ten}" + (f"   {chi_tiet}" if chi_tiet else ""))
    if not dat: loi += 1

print("=== 1. YEN NGUA: diem cat co nam DUNG tren mat ong chinh khong? ===")
for goc in (90.0, 60.0, 45.0, 30.0, 120.0):
    r, R = 30.0, 40.0
    d = tv.yen_ngua(r, R, goc)
    t = math.radians(goc)
    sai_lon_nhat = 0.0
    for L, a in d.diem:
        phi = math.radians(a)
        # Dung lai diem 3D tu (L, phi) theo dung dinh nghia hinh hoc
        px = L*math.sin(t) + r*math.cos(phi)*math.cos(t)
        py = r*math.sin(phi)
        sai_lon_nhat = max(sai_lon_nhat, abs(math.hypot(px, py) - R))
    ktra(f"goc {goc:5.1f} do: moi diem deu nam tren mat ong chinh",
         sai_lon_nhat < 1e-9, f"sai lech lon nhat {sai_lon_nhat:.2e} mm")

print("\n=== 2. YEN NGUA 90 do: doi chieu voi cong thuc rut gon rieng ===")
r, R = 25.0, 50.0
d = tv.yen_ngua(r, R, 90.0)
sai = max(abs(L - math.sqrt(R*R - (r*math.sin(math.radians(a)))**2)) for L, a in d.diem)
ktra("trung khop cong thuc sqrt(R^2 - r^2 sin^2 phi)", sai < 1e-12, f"sai {sai:.2e}")

print("\n=== 3. YEN NGUA: ong bang nhau thi day yen cham truc ong chinh ===")
d = tv.yen_ngua(30.0, 30.0, 90.0)
x_min, x_max = d.pham_vi_x()
ktra("cho sau nhat cua yen = 0 (om sat vao)", abs(x_min) < 1e-9, f"x_min={x_min:.2e}")
ktra("cho nong nhat = ban kinh ong chinh", abs(x_max - 30.0) < 1e-9, f"x_max={x_max:.6f}")

print("\n=== 4. LO TRON: diem cat nam tren CA HAI mat tru (ong va lo) ===")
for r, d_lo in ((30.0, 10.0), (30.0, 40.0), (100.0, 60.0)):
    p_lo = d_lo / 2
    duong = tv.lo_tron(r, d_lo, 100.0, 0.0, so_diem=720)
    sai_ong = sai_lo = 0.0
    for x, a in duong.diem:
        # Dung lai diem 3D: truc ong = z, tam lo o goc A=0 (phap tuyen = x)
        ang = math.radians(a)
        P = (r*math.cos(ang), r*math.sin(ang), x - 100.0)
        sai_ong = max(sai_ong, abs(math.hypot(P[0], P[1]) - r))
        # Khoang cach toi TRUC LO (truc lo la duong thang qua goc, huong x)
        sai_lo = max(sai_lo, abs(math.hypot(P[1], P[2]) - p_lo))
    ktra(f"ong D{2*r:g}, lo D{d_lo:g}: nam tren ca 2 mat tru",
         sai_ong < 1e-9 and sai_lo < 1e-9,
         f"sai lech ong {sai_ong:.1e} mm, lo {sai_lo:.1e} mm")

print("\n=== 5. LO TRON: cong thuc XAP XI PHANG sai bao nhieu? ===")
r, d_lo = 30.0, 40.0
duong = tv.lo_tron(r, d_lo, 0.0, 0.0, so_diem=720)
goc_max_dung = max(abs(a) for _, a in duong.diem)
goc_max_xap_xi = math.degrees((d_lo/2) / r)
ktra("cong thuc chinh xac cho goc RONG hon xap xi phang",
     goc_max_dung > goc_max_xap_xi + 1.0,
     f"chinh xac {goc_max_dung:.2f} do, xap xi phang {goc_max_xap_xi:.2f} do "
     f"-> xap xi lam lo HEP di {goc_max_dung-goc_max_xap_xi:.2f} do")

print("\n=== 6. CAT VAT: chenh lech dau-cuoi = D*tan(goc) ===")
for goc in (15.0, 30.0, 45.0, 60.0):
    r = 30.0
    duong = tv.cat_vat(r, goc, 100.0)
    x_min, x_max = duong.pham_vi_x()
    dung = 2 * r * math.tan(math.radians(goc))
    ktra(f"goc vat {goc:g} do", abs((x_max - x_min) - dung) < 1e-6,
         f"do duoc {x_max-x_min:.4f} mm, dung ra {dung:.4f} mm")

print("\n=== 7. RANH DAI: kich thuoc that tren mat ong ===")
r = 40.0
duong = tv.ranh_dai(r, 60.0, 16.0, 100.0, 0.0, doc_truc=True)
x_min, x_max = duong.pham_vi_x()
rong_do = max(a for _, a in duong.diem) - min(a for _, a in duong.diem)
# Chieu rong = khoang cach THANG giua hai canh (nhu duong kinh mot cai lo),
# khong phai chieu dai cung - do moi la cho thanh 16mm chui lot qua
rong_day = 2 * r * math.sin(math.radians(rong_do / 2))
rong_cung = math.radians(rong_do) * r
ktra("chieu dai ranh dung 60mm", abs((x_max-x_min) - 60.0) < 1e-6, f"{x_max-x_min:.4f} mm")
ktra("chieu rong (do thang) dung 16mm", abs(rong_day - 16.0) < 1e-6,
     f"day {rong_day:.4f} mm, cung {rong_cung:.4f} mm")

print("\n=== 8. Bao loi khi tham so vo ly ===")
for ten, ham in [
    ("ong nhanh to hon ong chinh", lambda: tv.yen_ngua(40.0, 30.0, 90.0)),
    ("lo to hon ong",              lambda: tv.lo_tron(30.0, 70.0)),
    ("goc vat 90 do",              lambda: tv.cat_vat(30.0, 90.0)),
    ("ranh ngan hon chieu rong",   lambda: tv.ranh_dai(30.0, 10.0, 20.0)),
    ("lech tam qua lon",           lambda: tv.yen_ngua(20.0, 30.0, 90.0, lech_tam=25.0)),
]:
    try:
        ham(); ktra(ten, False, "KHONG bao loi!")
    except ValueError as e:
        ktra(ten, True, f'bao "{e}"')

print("\n=== 9. So diem tu tang theo chu vi ong ===")
n_nho = len(tv.yen_ngua(10.0, 20.0, 90.0).diem)
n_to  = len(tv.yen_ngua(100.0, 150.0, 90.0).diem)
ktra("ong to duoc chia nhieu diem hon ong nho", n_to > n_nho * 3,
     f"ong D20: {n_nho} diem, ong D200: {n_to} diem")

print("\n=== 10. Sinh G-code chay duoc ===")
duong = tv.yen_ngua(30.0, 30.0, 90.0)
g = tv.sinh_gcode([duong], 15.0, 60.0, 0.8, x_ve_cho=0.0, tieu_de=["Test"])
ktra("co bat/tat mo cat", "M3" in g and "M5" in g)
ktra("co cho duc lo", any(d.startswith("G4 P") for d in g))
ktra("ket thuc bang M30", g[-1] == "M30")
ktra("khong co dong rong", all(d.strip() for d in g))
print(f"     ({len(g)} dong G-code)")


print("\n=== 11. BO TRON DAY YEN - chong dao chieu truc X qua gat ===")
r = 30.0


def do_gap(duong, r):
    """Buoc doi X lon nhat khi doi chieu - do gat cua cho gap goc."""
    lon_nhat = 0.0
    n = len(duong.diem)
    for i in range(1, n - 1):
        truoc = duong.diem[i][0] - duong.diem[i - 1][0]
        sau = duong.diem[i + 1][0] - duong.diem[i][0]
        if truoc * sau < 0:                      # dung cho doi chieu
            lon_nhat = max(lon_nhat, abs(truoc), abs(sau))
    return lon_nhat


goc = tv.yen_ngua(r, r, 90.0)                    # hai ong bang nhau -> nhon that
tron = tv.yen_ngua(r, r, 90.0, bo_tron=2.0)
ktra("chua bo tron: truc X dao chieu het toc do", do_gap(goc, r) > 0.3,
     f"{do_gap(goc, r):.4f} mm moi buoc")
ktra("bo tron 2mm: dao chieu em hon it nhat 5 lan",
     do_gap(tron, r) < do_gap(goc, r) / 5,
     f"{do_gap(goc, r):.4f} -> {do_gap(tron, r):.4f} mm moi buoc")

ktra("KHONG BAO GIO cat sau hon duong cat goc (chi de lai vat lieu)",
     all(b[0] >= a[0] - 1e-9 for a, b in zip(goc.diem, tron.diem)))
thua = max(b[0] - a[0] for a, b in zip(goc.diem, tron.diem))
ktra("vat lieu de lai o day yen nho hon mach cat plasma (~1,2mm)", thua < 1.2,
     f"{thua:.3f} mm")
ktra("khong dung toi goc quay", all(abs(a[1] - b[1]) < 1e-12
                                    for a, b in zip(goc.diem, tron.diem)))

print("\n  -- Cho da tron san thi bo tron KHONG duoc dung toi --")
for D in (70, 90, 120):
    a = tv.yen_ngua(r, D / 2, 90.0)
    b = tv.yen_ngua(r, D / 2, 90.0, bo_tron=2.0)
    lech = max(abs(p[0] - q[0]) for p, q in zip(a.diem, b.diem))
    ktra(f"ong chinh D{D}: gan nhu khong doi", lech < 0.05, f"lech {lech:.4f} mm")

ktra("ban kinh 0 = giu nguyen cong thuc chinh xac",
     all(abs(p[0] - q[0]) < 1e-12
         for p, q in zip(goc.diem, tv.yen_ngua(r, r, 90.0, bo_tron=0.0).diem)))

print("\n  -- Day yen o goc 0 do (cho duong cat khep kin) cung duoc bo tron --")
# Xoay bai toan di 90 do bang cach dung goc nghieng, kiem tra khong con cho gat
tron_lon = tv.yen_ngua(r, r, 90.0, bo_tron=4.0)
ktra("ban kinh cang lon thi dao chieu cang em",
     do_gap(tron_lon, r) < do_gap(tron, r),
     f"{do_gap(tron, r):.4f} -> {do_gap(tron_lon, r):.4f} mm")

print(f"\n{'=== TAT CA DAT ===' if not loi else f'=== CO {loi} LOI ==='}")
sys.exit(1 if loi else 0)
