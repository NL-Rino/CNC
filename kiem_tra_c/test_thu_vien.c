/* Kiem chung TOAN HOC cua thu vien moi noi (ban C).
 *
 * Chuyen nguyen tung phep kiem tu ban Python (kiem_tra/test_thu_vien.py) sang,
 * de biet chac hai ban cho ket qua nhu nhau. Khong tin cong thuc rut gon: moi
 * duong cat deu duoc DUNG LAI thanh diem 3D roi kiem xem no co THAT SU nam
 * tren mat hai ong khong.
 */
#include "../loi_c/thu_vien_moi_noi.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int loi_tong = 0;

static void ktra(const char *ten, int dat, const char *chi_tiet)
{
    printf("  [%s] %s", dat ? "DAT" : "SAI", ten);
    if (chi_tiet && chi_tiet[0]) printf("   %s", chi_tiet);
    printf("\n");
    if (!dat) loi_tong++;
}

static double rad(double d) { return d * M_PI / 180.0; }

/* Buoc doi X lon nhat khi doi chieu - do gat cua cho gap goc */
static double do_gap(const DuongCat *d)
{
    double lon_nhat = 0.0;
    int i;
    for (i = 1; i < d->so_diem - 1; i++) {
        double truoc = d->diem[i].x - d->diem[i - 1].x;
        double sau = d->diem[i + 1].x - d->diem[i].x;
        if (truoc * sau < 0.0) {
            double a = fabs(truoc), b = fabs(sau);
            if (a > lon_nhat) lon_nhat = a;
            if (b > lon_nhat) lon_nhat = b;
        }
    }
    return lon_nhat;
}

int main(void)
{
    char loi[CO_LOI], chu[256];
    DuongCat d, d2;
    int i;

    printf("=== 1. YEN NGUA: diem cat co nam DUNG tren mat ong chinh khong? ===\n");
    {
        double cac_goc[] = {90.0, 60.0, 45.0, 30.0, 120.0};
        for (i = 0; i < 5; i++) {
            double goc = cac_goc[i], r = 30.0, R = 40.0, sai = 0.0;
            double t = rad(goc);
            int j;
            if (yen_ngua(r, R, goc, 0.0, 0.0, 0.0, 0.0, &d, loi) != 0) {
                ktra("sinh duong cat", 0, loi);
                continue;
            }
            for (j = 0; j < d.so_diem; j++) {
                /* Dung lai diem 3D tu (L, phi) theo dung dinh nghia hinh hoc */
                double L = d.diem[j].x, phi = rad(d.diem[j].a);
                double px = L * sin(t) + r * cos(phi) * cos(t);
                double py = r * sin(phi);
                double lech = fabs(sqrt(px * px + py * py) - R);
                if (lech > sai) sai = lech;
            }
            snprintf(chu, sizeof(chu), "sai lech lon nhat %.2e mm", sai);
            snprintf(loi, sizeof(loi), "goc %5.1f do: moi diem deu nam tren mat ong chinh", goc);
            ktra(loi, sai < 1e-9, chu);
            duong_cat_giai_phong(&d);
        }
    }

    printf("\n=== 2. YEN NGUA 90 do: doi chieu voi cong thuc rut gon rieng ===\n");
    {
        double r = 25.0, R = 50.0, sai = 0.0;
        yen_ngua(r, R, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi);
        for (i = 0; i < d.so_diem; i++) {
            double s = r * sin(rad(d.diem[i].a));
            double lech = fabs(d.diem[i].x - sqrt(R * R - s * s));
            if (lech > sai) sai = lech;
        }
        snprintf(chu, sizeof(chu), "sai %.2e", sai);
        ktra("trung khop cong thuc sqrt(R^2 - r^2 sin^2 phi)", sai < 1e-12, chu);
        duong_cat_giai_phong(&d);
    }

    printf("\n=== 3. YEN NGUA: ong bang nhau thi day yen cham truc ong chinh ===\n");
    {
        double x_min, x_max;
        yen_ngua(30.0, 30.0, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi);
        duong_cat_pham_vi_x(&d, &x_min, &x_max);
        snprintf(chu, sizeof(chu), "x_min=%.2e", x_min);
        ktra("cho sau nhat cua yen = 0 (om sat vao)", fabs(x_min) < 1e-9, chu);
        snprintf(chu, sizeof(chu), "x_max=%.6f", x_max);
        ktra("cho nong nhat = ban kinh ong chinh", fabs(x_max - 30.0) < 1e-9, chu);
        duong_cat_giai_phong(&d);
    }

    printf("\n=== 4. LO TRON: da bo khoi thu vien (chi con 3 kieu ghep) ===\n");
    ktra("thu vien co dung 3 kieu", SO_KIEU_GHEP == 3, NULL);
    ktra("goc_90 co trong thu vien", kieu_theo_ma("goc_90") != NULL, NULL);
    ktra("goc_45 co trong thu vien", kieu_theo_ma("goc_45") != NULL, NULL);
    ktra("nhanh_t_90 co trong thu vien", kieu_theo_ma("nhanh_t_90") != NULL, NULL);
    ktra("khong con kieu la", kieu_theo_ma("lo_tron") == NULL, NULL);

    printf("\n=== 5. CAT VAT: chenh lech dau-cuoi = D*tan(goc) ===\n");
    {
        double cac_goc[] = {15.0, 30.0, 45.0, 60.0};
        for (i = 0; i < 4; i++) {
            double goc = cac_goc[i], r = 30.0, x_min, x_max, dung;
            cat_vat(r, goc, 100.0, &d, loi);
            duong_cat_pham_vi_x(&d, &x_min, &x_max);
            dung = 2 * r * tan(rad(goc));
            snprintf(chu, sizeof(chu), "do duoc %.4f mm, dung ra %.4f mm", x_max - x_min, dung);
            snprintf(loi, sizeof(loi), "goc vat %g do", goc);
            ktra(loi, fabs((x_max - x_min) - dung) < 1e-6, chu);
            duong_cat_giai_phong(&d);
        }
    }

    printf("\n=== 6. CAT VAT trai phang ra dung la MOT DUONG SIN ===\n");
    {
        double r = 30.0, sai = 0.0;
        cat_vat(r, 45.0, 100.0, &d, loi);
        for (i = 0; i < d.so_diem; i++) {
            double lech = fabs((d.diem[i].x - 100.0) - r * cos(rad(d.diem[i].a)));
            if (lech > sai) sai = lech;
        }
        snprintf(chu, sizeof(chu), "sai lech %.2e mm so voi r*cos(A)", sai);
        ktra("cat vat 45 do = mot duong sin tron ven", sai < 1e-9, chu);
        duong_cat_giai_phong(&d);
    }

    printf("\n=== 7. ONG NHANH CHU T = SIN CHINH LUU (sin bi gap nguoc) ===\n");
    {
        const KieuGhep *k = kieu_theo_ma("nhanh_t_90");
        GiaTriThamSo g;
        double r = 30.0, sai = 0.0;
        gia_tri_mac_dinh(k, &g);
        g.gt[2] = 0.0;                       /* tat bo tron de so voi cong thuc goc */
        kieu_sinh(k, 60.0, &g, 100.0, &d, loi);
        for (i = 0; i < d.so_diem; i++) {
            double lech = fabs((d.diem[i].x - 100.0) - r * fabs(cos(rad(d.diem[i].a))));
            if (lech > sai) sai = lech;
        }
        snprintf(chu, sizeof(chu), "sai lech %.2e mm so voi r*|cos(A)|", sai);
        ktra("ong nhanh chu T = r*|cos(A)|", sai < 1e-9, chu);
        duong_cat_giai_phong(&d);
    }

    printf("\n=== 8. Bao loi khi tham so vo ly ===\n");
    {
        ktra("ong nhanh to hon ong chinh",
             yen_ngua(40.0, 30.0, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi) != 0, loi);
        ktra("goc vat 90 do", cat_vat(30.0, 90.0, 0.0, &d, loi) != 0, loi);
        ktra("goc yen ngua 200 do",
             yen_ngua(30.0, 40.0, 200.0, 0.0, 0.0, 0.0, 0.0, &d, loi) != 0, loi);
        ktra("lech tam qua lon",
             yen_ngua(20.0, 30.0, 90.0, 25.0, 0.0, 0.0, 0.0, &d, loi) != 0, loi);
    }

    printf("\n=== 9. So diem tu tang theo chu vi ong ===\n");
    {
        int n_nho, n_to;
        yen_ngua(10.0, 20.0, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi);
        n_nho = d.so_diem;
        duong_cat_giai_phong(&d);
        yen_ngua(100.0, 150.0, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi);
        n_to = d.so_diem;
        duong_cat_giai_phong(&d);
        snprintf(chu, sizeof(chu), "ong D20: %d diem, ong D200: %d diem", n_nho, n_to);
        ktra("ong to duoc chia nhieu diem hon ong nho", n_to > n_nho * 3, chu);
    }

    printf("\n=== 10. Sinh G-code chay duoc ===\n");
    {
        static char dong[4000][CO_DONG_GCODE];
        const char *tieu_de[1] = {"Test"};
        int n, co_m3 = 0, co_m5 = 0, co_g4 = 0, co_dong_rong = 0;
        yen_ngua(30.0, 30.0, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi);
        n = sinh_gcode(&d, 1, 15.0, 60.0, 0.8, 1, 0.0, tieu_de, 1, dong, 4000);
        for (i = 0; i < n; i++) {
            if (strcmp(dong[i], "M3") == 0) co_m3 = 1;
            if (strcmp(dong[i], "M5") == 0) co_m5 = 1;
            if (strncmp(dong[i], "G4 P", 4) == 0) co_g4 = 1;
            if (dong[i][0] == '\0') co_dong_rong = 1;
        }
        ktra("co bat/tat mo cat", co_m3 && co_m5, NULL);
        ktra("co cho duc lo", co_g4, NULL);
        ktra("ket thuc bang M30", n > 0 && strcmp(dong[n - 1], "M30") == 0, NULL);
        ktra("khong co dong rong", !co_dong_rong, NULL);
        snprintf(chu, sizeof(chu), "(%d dong G-code)", n);
        printf("     %s\n", chu);
        duong_cat_giai_phong(&d);
    }

    printf("\n=== 11. BO TRON DAY YEN - chong dao chieu truc X qua gat ===\n");
    {
        double r = 30.0, gap_goc, gap_tron, thua = 0.0;
        int khong_cat_lem = 1, khong_dung_goc = 1;

        yen_ngua(r, r, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi);      /* chua bo tron */
        yen_ngua(r, r, 90.0, 0.0, 0.0, 0.0, 2.0, &d2, loi);     /* bo tron 2mm */
        gap_goc = do_gap(&d);
        gap_tron = do_gap(&d2);

        snprintf(chu, sizeof(chu), "%.4f mm moi buoc", gap_goc);
        ktra("chua bo tron: truc X dao chieu het toc do", gap_goc > 0.3, chu);
        snprintf(chu, sizeof(chu), "%.4f -> %.4f mm moi buoc", gap_goc, gap_tron);
        ktra("bo tron 2mm: dao chieu em hon it nhat 5 lan", gap_tron < gap_goc / 5, chu);

        for (i = 0; i < d.so_diem; i++) {
            double du = d2.diem[i].x - d.diem[i].x;
            if (du < -1e-9) khong_cat_lem = 0;
            if (du > thua) thua = du;
            if (fabs(d.diem[i].a - d2.diem[i].a) > 1e-12) khong_dung_goc = 0;
        }
        ktra("KHONG BAO GIO cat sau hon duong cat goc", khong_cat_lem, NULL);
        snprintf(chu, sizeof(chu), "%.3f mm", thua);
        ktra("vat lieu de lai nho hon mach cat plasma (~1,2mm)", thua < 1.2, chu);
        ktra("khong dung toi goc quay", khong_dung_goc, NULL);
        duong_cat_giai_phong(&d);
        duong_cat_giai_phong(&d2);

        printf("\n  -- Cho da tron san thi bo tron KHONG duoc dung toi --\n");
        {
            double cac_d[] = {70.0, 90.0, 120.0};
            for (i = 0; i < 3; i++) {
                double lech = 0.0;
                int j;
                yen_ngua(r, cac_d[i] / 2, 90.0, 0.0, 0.0, 0.0, 0.0, &d, loi);
                yen_ngua(r, cac_d[i] / 2, 90.0, 0.0, 0.0, 0.0, 2.0, &d2, loi);
                for (j = 0; j < d.so_diem; j++) {
                    double t = fabs(d.diem[j].x - d2.diem[j].x);
                    if (t > lech) lech = t;
                }
                snprintf(chu, sizeof(chu), "lech %.4f mm", lech);
                snprintf(loi, sizeof(loi), "ong chinh D%g: gan nhu khong doi", cac_d[i]);
                ktra(loi, lech < 0.05, chu);
                duong_cat_giai_phong(&d);
                duong_cat_giai_phong(&d2);
            }
        }
    }

    printf("\n%s (%d loi)\n", loi_tong ? "=== CO LOI ===" : "=== TAT CA DAT ===", loi_tong);
    return loi_tong ? 1 : 0;
}
