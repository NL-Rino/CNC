/* Kiem chung phan XEP BAI va quy uoc DO KHOANG CACH cua the xep 2D.
 *
 * Khong mo cua so: chi goi thang cac ham tinh toan.
 * Day la ban chuyen sang C cua kiem_tra/test_xep.py, giu nguyen tung phep thu.
 */
#include "../loi_c/thu_vien_moi_noi.h"
#include "../loi_c/xep_2d.h"
#include "../loi_c/loi_chung.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int so_loi = 0;

static void ktra(const char *ten, int dat, const char *chi_tiet)
{
    printf("  [%s] %s", dat ? "DAT" : "SAI", ten);
    if (chi_tiet && chi_tiet[0]) printf("   %s", chi_tiet);
    printf("\n");
    if (!dat) so_loi++;
}

static const KieuGhep *theo_ma(const char *ma)
{
    const KieuGhep *k = kieu_theo_ma(ma);
    if (!k) { printf("  [SAI] khong co kieu '%s'\n", ma); so_loi++; }
    return k;
}

/* Sinh mot duong cat, bao loi va thoat neu that bai */
static int sinh_thu(const char *ma, double duong_kinh, const double *tham_so,
                double x, DuongCat *ra)
{
    const KieuGhep *k = theo_ma(ma);
    GiaTriThamSo gt;
    char loi[CO_LOI] = "";
    int i;
    if (!k) return -1;
    memset(&gt, 0, sizeof(gt));
    for (i = 0; i < k->so_tham_so; i++) gt.gt[i] = tham_so[i];
    duong_cat_khoi_tao(ra);
    if (kieu_sinh(k, duong_kinh, &gt, x, ra, loi) != 0) {
        printf("  [SAI] sinh %s that bai: %s\n", ma, loi);
        so_loi++;
        return -1;
    }
    return 0;
}

int main(void)
{
    DuongCat d, d0, d90;
    MucBai muc[3];
    KetQuaXep kq, kq2;
    Xep2D *xep;
    KhungNhatCat khung[3];
    char loi[CO_LOI] = "", chi_tiet[128];
    double dai_khuc = 200.0, khe = 8.0;
    int i;

    /* Tham so cua tung kieu, theo dung thu tu khai bao trong thu vien */
    static const double TS_GOC_90[1]   = { 0.0 };            /* a */
    static const double TS_GOC_45[1]   = { 0.0 };            /* a */
    static const double TS_NHANH_T[3]  = { 60.0, 0.0, 0.0 }; /* d_chinh, khe_ho, a */
    static const char *MA[3] = { "goc_90", "goc_45", "nhanh_t_90" };
    static const double *TS[3] = { TS_GOC_90, TS_GOC_45, TS_NHANH_T };

    printf("=== 1. THU VIEN chi con dung 3 kieu ghep ===\n");
    snprintf(chi_tiet, sizeof(chi_tiet), "%s, %s, %s",
             THU_VIEN[0].ma, THU_VIEN[1].ma, THU_VIEN[2].ma);
    ktra("co dung 3 kieu", SO_KIEU_GHEP == 3, chi_tiet);

    if (sinh_thu("goc_90", 60.0, TS_GOC_90, 100.0, &d) == 0) {
        ktra("goc 90 do -> moi dau vat 45 do", strstr(d.ten, "45") != NULL, d.ten);
        duong_cat_giai_phong(&d);
    }
    if (sinh_thu("goc_45", 60.0, TS_GOC_45, 100.0, &d) == 0) {
        ktra("goc 45 do -> moi dau vat 22,5 do", strstr(d.ten, "22.5") != NULL, d.ten);
        duong_cat_giai_phong(&d);
    }
    if (sinh_thu("nhanh_t_90", 60.0, TS_NHANH_T, 100.0, &d) == 0) {
        ktra("nhanh chu T -> long yen ngua 90 do",
             strstr(d.ten, "Yen ngua") != NULL, d.ten);
        duong_cat_giai_phong(&d);
    }

    printf("\n=== 2. Goc ghep = 2 lan goc vat (up hai mat vat vao nhau) ===\n");
    for (i = 0; i < 2; i++) {
        double r = 30.0, goc_ghep = i == 0 ? 90.0 : 45.0;
        double x1, x2, goc_vat;
        if (sinh_thu(MA[i], 2 * r, TS[i], 100.0, &d) != 0) continue;
        duong_cat_pham_vi_x(&d, &x1, &x2);
        /* Be ngang mieng vat theo truc X = D * tan(goc_vat)
         * => goc = atan(rong / D) */
        goc_vat = atan((x2 - x1) / (2 * r)) * 180.0 / PI;
        snprintf(chi_tiet, sizeof(chi_tiet), "%.4f do", goc_vat);
        {
            char ten[96];
            snprintf(ten, sizeof(ten), "%s: goc vat do duoc = %g do",
                     MA[i], goc_ghep / 2);
            ktra(ten, fabs(goc_vat - goc_ghep / 2) < 1e-6, chi_tiet);
        }
        duong_cat_giai_phong(&d);
    }

    printf("\n=== 3. Goc dat mieng cat xoay ca duong cat ===\n");
    {
        double ts0[1] = { 0.0 }, ts90[1] = { 90.0 };
        int lech_dung = 1, x_khong_doi = 1;
        if (sinh_thu("goc_90", 60.0, ts0, 100.0, &d0) == 0 &&
            sinh_thu("goc_90", 60.0, ts90, 100.0, &d90) == 0) {
            if (d0.so_diem != d90.so_diem) {
                lech_dung = x_khong_doi = 0;
            } else {
                for (i = 0; i < d0.so_diem; i++) {
                    if (fabs((d90.diem[i].a - d0.diem[i].a) - 90.0) > 1e-9)
                        lech_dung = 0;
                    if (fabs(d0.diem[i].x - d90.diem[i].x) > 1e-12)
                        x_khong_doi = 0;
                }
            }
            ktra("xoay 90 do thi moi diem lech dung 90 do", lech_dung, "");
            ktra("xoay khong lam doi vi tri truc X", x_khong_doi, "");
            duong_cat_giai_phong(&d0);
            duong_cat_giai_phong(&d90);
        }
    }

    printf("\n=== 4. XEP BAI: khuc noi tiep nhau, cach dung khoang khe ===\n");
    for (i = 0; i < 3; i++) {
        const KieuGhep *k = theo_ma(MA[i]);
        double x;
        int j;
        if (!k) return 1;
        if (vi_tri_ke_tiep(60.0, muc, i, dai_khuc, khe, 20.0, &x, loi) != 0) {
            printf("  [SAI] vi_tri_ke_tiep: %s\n", loi);
            return 1;
        }
        snprintf(muc[i].ma, sizeof(muc[i].ma), "%s", MA[i]);
        memset(&muc[i].gia_tri, 0, sizeof(muc[i].gia_tri));
        for (j = 0; j < k->so_tham_so; j++) muc[i].gia_tri.gt[j] = TS[i][j];
        muc[i].x = x;
    }
    ket_qua_xep_khoi_tao(&kq);
    if (xep_bai(60.0, muc, 3, 0.0, &kq, loi) != 0) {
        printf("  [SAI] xep_bai: %s\n", loi);
        return 1;
    }
    snprintf(chi_tiet, sizeof(chi_tiet), "%.3f mm", muc[0].x);
    ktra("nhat cat dau: chua dau 20 + khuc 200 = 220",
         fabs(muc[0].x - 220.0) < 1e-9, chi_tiet);
    for (i = 1; i < 3; i++) {
        double x1, x_cuoi_truoc, dai;
        char ten[128];
        khung_duong_cat(&kq.duong[i - 1], &x1, &x_cuoi_truoc, &dai);
        snprintf(ten, sizeof(ten),
                 "nhat %d cach cho sau nhat cua nhat %d dung %g+%g mm",
                 i + 1, i, khe, dai_khuc);
        snprintf(chi_tiet, sizeof(chi_tiet), "%.1f mm", muc[i].x - x_cuoi_truoc);
        ktra(ten, fabs(muc[i].x - (x_cuoi_truoc + khe + dai_khuc)) < 1e-9, chi_tiet);
    }

    printf("\n=== 5. KHUNG duong cat = be ngang theo truc X ===\n");
    for (i = 0; i < kq.so_duong; i++) {
        double x1, x2, dai, nho, lon, thuc;
        char ten[96];
        int j;
        khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
        nho = lon = kq.duong[i].diem[0].x;
        for (j = 0; j < kq.duong[i].so_diem; j++) {
            double v = kq.duong[i].diem[j].x;
            if (v < nho) nho = v;
            if (v > lon) lon = v;
        }
        thuc = lon - nho;
        snprintf(ten, sizeof(ten), "khung %d rong dung bang be ngang duong cat", i + 1);
        snprintf(chi_tiet, sizeof(chi_tiet), "%.3f mm", dai);
        ktra(ten, fabs(dai - thuc) < 1e-12, chi_tiet);
    }

    printf("\n=== 6. DO KHOANG CACH tu DIEM GOC (dau ong xa mam kep) ===\n");
    xep = xep2d_tao(NULL);
    for (i = 0; i < kq.so_duong; i++) {
        double x1, x2, dai;
        khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
        khung[i].x_dau = x1;
        khung[i].x_cuoi = x2;
        snprintf(khung[i].ten, sizeof(khung[i].ten), "%s", kq.duong[i].ten);
        khung[i].x_tam = muc[i].x;
    }
    xep2d_dat_du_lieu(xep, khung, kq.so_duong, 60.0, 1200.0);
    xep2d_dat_co_khung_hinh(xep, 800, 300);
    for (i = 0; i < kq.so_duong; i++) {
        double x1, x2, dai, kc;
        char ten[96];
        khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
        kc = xep2d_khoang_cach_tu_goc(xep, x2);
        snprintf(ten, sizeof(ten),
                 "nhat %d: do toi CANH GAN diem goc nhat (x_cuoi)", i + 1);
        snprintf(chi_tiet, sizeof(chi_tiet), "%.1f mm", kc);
        ktra(ten, fabs(kc - (1200.0 - x2)) < 1e-12, chi_tiet);
    }
    {
        int giam_dan = 1;
        for (i = 0; i + 1 < kq.so_duong; i++) {
            double a1, a2, b1, b2, dai;
            khung_duong_cat(&kq.duong[i], &a1, &a2, &dai);
            khung_duong_cat(&kq.duong[i + 1], &b1, &b2, &dai);
            if (!(xep2d_khoang_cach_tu_goc(xep, a2) >
                  xep2d_khoang_cach_tu_goc(xep, b2)))
                giam_dan = 0;
        }
        ktra("nhat cang xa mam kep thi khoang cach cang nho", giam_dan, "");
    }

    printf("\n=== 7. Go khoang cach vao o -> nhat cat nhay dung cho ===\n");
    {
        static const double MUON[3] = { 100.0, 350.0, 800.0 };
        int t;
        for (t = 0; t < 3; t++) {
            MucBai muc2[3];
            double x1, x2, dai, x_moi, kc;
            char ten[96];
            i = 1;
            khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
            x_moi = xep2d_tu_khoang_cach(xep, MUON[t], muc[i].x, x2);
            memcpy(muc2, muc, sizeof(muc2));
            muc2[i].x = x_moi;
            ket_qua_xep_khoi_tao(&kq2);
            if (xep_bai(60.0, muc2, 3, 0.0, &kq2, loi) != 0) {
                printf("  [SAI] xep_bai: %s\n", loi);
                so_loi++;
                continue;
            }
            khung_duong_cat(&kq2.duong[i], &x1, &x2, &dai);
            kc = xep2d_khoang_cach_tu_goc(xep, x2);
            snprintf(ten, sizeof(ten), "go %g mm -> do lai duoc dung %g mm",
                     MUON[t], MUON[t]);
            snprintf(chi_tiet, sizeof(chi_tiet), "%.4f mm", kc);
            ktra(ten, fabs(kc - MUON[t]) < 1e-9, chi_tiet);
            ket_qua_xep_giai_phong(&kq2);
        }
    }

    printf("\n=== 8. Doi pixel <-> mm cua the xep khop nhau ===\n");
    {
        static const double MM[3] = { 0.0, 250.0, 1200.0 };
        int t;
        for (t = 0; t < 3; t++) {
            char ten[64];
            snprintf(ten, sizeof(ten), "%g mm -> pixel -> mm", MM[t]);
            ktra(ten, fabs(xep2d_sang_mm(xep, xep2d_sang_pixel(xep, MM[t])) - MM[t])
                      < 1e-9, "");
        }
        xep2d_phong_to(xep, 3.0, 400);
        ktra("sau khi phong to van khop",
             fabs(xep2d_sang_mm(xep, xep2d_sang_pixel(xep, 600.0)) - 600.0) < 1e-9, "");
        ktra("phong to giu nguyen diem duoi con tro",
             fabs(xep2d_sang_pixel(xep, xep2d_sang_mm(xep, 400)) - 400) < 1e-9, "");
    }

    printf("\n=== 9. Bao loi khi cay ong khong du dai ===\n");
    ket_qua_xep_khoi_tao(&kq2);
    if (xep_bai(60.0, muc, 3, 300.0, &kq2, loi) == 0) {
        ktra("bao THIEU khi cay ong ngan", strstr(kq2.canh_bao, "THIEU") != NULL,
             kq2.canh_bao[0] ? kq2.canh_bao : "khong bao gi");
        ket_qua_xep_giai_phong(&kq2);
    } else {
        printf("  [SAI] xep_bai (cay ngan): %s\n", loi);
        so_loi++;
    }
    ket_qua_xep_khoi_tao(&kq2);
    if (xep_bai(60.0, muc, 3, 2000.0, &kq2, loi) == 0) {
        ktra("khong bao gi khi cay ong du dai", kq2.canh_bao[0] == '\0', "");
        ket_qua_xep_giai_phong(&kq2);
    }

    ket_qua_xep_giai_phong(&kq);
    xep2d_giai_phong(xep);

    if (so_loi == 0) printf("\n=== TAT CA DAT ===\n");
    else             printf("\n=== CO %d LOI ===\n", so_loi);
    return so_loi ? 1 : 0;
}
