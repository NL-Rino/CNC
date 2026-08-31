#include "xep_2d.h"
#include "loi_chung.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define MAU_NEN_XEP 0x161a1fu

struct Xep2D {
    HamXep2D ham;
    KhungNhin2D khung;

    double duong_kinh;
    double dai_cay_ong;
    KhungNhatCat *cac_khung;
    int so_khung;
    int dang_chon;              /* -1 = khong chon gi */

    int rong, cao;              /* co khung hinh hien tai, pixel */

    int    keo_khung;           /* chi so nhat cat dang bi keo, -1 = khong */
    double keo_lech;            /* chenh lech giua diem bam va tam nhat cat (mm) */
    int    dang_day;            /* dang day khung nhin bang chuot giua */
    double day_tu;
};

void khung_nhin_2d_dat_lai(KhungNhin2D *k)
{
    k->phong = 1.0;
    k->day = 0.0;
    k->tu_dong = 1;
}

Xep2D *xep2d_tao(const HamXep2D *ham)
{
    Xep2D *x = (Xep2D *)calloc(1, sizeof(*x));
    if (!x) return NULL;
    if (ham) x->ham = *ham;
    khung_nhin_2d_dat_lai(&x->khung);
    x->duong_kinh = 60.0;
    x->dai_cay_ong = 1000.0;
    x->dang_chon = -1;
    x->keo_khung = -1;
    x->rong = 800;
    x->cao = 300;
    return x;
}

void xep2d_giai_phong(Xep2D *x)
{
    if (!x) return;
    free(x->cac_khung);
    free(x);
}

void xep2d_dat_du_lieu(Xep2D *x, const KhungNhatCat *cac_khung, int so_khung,
                       double duong_kinh, double dai_cay_ong)
{
    free(x->cac_khung);
    x->cac_khung = NULL;
    x->so_khung = 0;
    if (so_khung > 0 && cac_khung) {
        x->cac_khung = (KhungNhatCat *)malloc(sizeof(KhungNhatCat) * (size_t)so_khung);
        if (x->cac_khung) {
            memcpy(x->cac_khung, cac_khung, sizeof(KhungNhatCat) * (size_t)so_khung);
            x->so_khung = so_khung;
        }
    }
    x->duong_kinh  = duong_kinh  > 1.0 ? duong_kinh  : 1.0;
    x->dai_cay_ong = dai_cay_ong > 1.0 ? dai_cay_ong : 1.0;
    if (x->dang_chon >= x->so_khung) x->dang_chon = -1;
}

void xep2d_dat_co_khung_hinh(Xep2D *x, int rong, int cao)
{
    if (rong > 0) x->rong = rong;
    if (cao  > 0) x->cao  = cao;
}

int  xep2d_dang_chon(const Xep2D *x) { return x->dang_chon; }
int  xep2d_so_khung(const Xep2D *x) { return x->so_khung; }

void xep2d_dat_dang_chon(Xep2D *x, int chi_so)
{
    x->dang_chon = (chi_so >= 0 && chi_so < x->so_khung) ? chi_so : -1;
}

/* ============================================================= DOI TOA DO */
double xep2d_ty_le(const Xep2D *x)
{
    if (x->khung.tu_dong) {
        double t = (x->rong - LE_TRAI_XEP - LE_PHAI_XEP) / x->dai_cay_ong;
        return t > 0.02 ? t : 0.02;
    }
    return x->khung.phong;
}

double xep2d_sang_pixel(const Xep2D *x, double x_may)
{
    return LE_TRAI_XEP + x_may * xep2d_ty_le(x) + x->khung.day;
}

double xep2d_sang_mm(const Xep2D *x, double px)
{
    return (px - LE_TRAI_XEP - x->khung.day) / xep2d_ty_le(x);
}

/* Tu DAU ONG XA MAM KEP toi canh gan diem goc nhat cua khung.
 * Diem goc o toa do may = dai_cay_ong. Canh gan no nhat la canh co toa do
 * may LON hon, tuc x_cuoi. */
double xep2d_khoang_cach_tu_goc(const Xep2D *x, double x_cuoi)
{
    return x->dai_cay_ong - x_cuoi;
}

double xep2d_tu_khoang_cach(const Xep2D *x, double khoang_cach,
                            double x_tam, double x_cuoi)
{
    double x_cuoi_moi = x->dai_cay_ong - khoang_cach;
    return x_tam + (x_cuoi_moi - x_cuoi);
}

/* =================================================================== VE */
/* Thuoc do tu DIEM GOC (dau xa) tro ve, don vi mm. */
static void ve_thuoc(const Xep2D *x, KhungVe *k, int rong)
{
    static const double cac_buoc[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500,
                                       1000, 2000, 5000 };
    double ty_le = xep2d_ty_le(x);
    double buoc = cac_buoc[sizeof(cac_buoc) / sizeof(cac_buoc[0]) - 1];
    double buoc_nho;
    int i, dem = 0;

    khung_them_hcn(k, 0, 0, rong, CAO_THUOC, 0x1e242bu, 1, 0, 0, 0);
    khung_them_duong(k, 0, CAO_THUOC, rong, CAO_THUOC, 0x3a424cu, 1);

    /* Chon buoc chia sao cho vach cach nhau it nhat ~55 pixel */
    for (i = 0; i < (int)(sizeof(cac_buoc) / sizeof(cac_buoc[0])); i++)
        if (cac_buoc[i] * ty_le >= 55) { buoc = cac_buoc[i]; break; }
    buoc_nho = buoc / 5.0;

    for (;;) {
        double d = dem * buoc_nho;      /* khoang cach tu DIEM GOC */
        double px, ti;
        int lon;
        if (d > x->dai_cay_ong + buoc) break;
        px = xep2d_sang_pixel(x, x->dai_cay_ong - d);
        dem++;
        if (px < -50 || px > rong + 50) continue;
        ti = d / buoc;
        lon = fabs(ti - floor(ti + 0.5)) < 1e-9;
        khung_them_duong(k, px, CAO_THUOC - (lon ? 13 : 6), px, CAO_THUOC,
                         lon ? 0x6b7686u : 0x454d57u, 1);
        if (lon) {
            char so[24];
            so_gon(so, sizeof(so), d);
            khung_them_chu(k, px, 9, so, 0x8b96a5u, 8, 1, 0, NEO_GIUA);
        }
    }
}

static void ve_ong(const Xep2D *x, KhungVe *k, double y_ong, double nua_cao)
{
    double x1 = xep2d_sang_pixel(x, 0.0);
    double x2 = xep2d_sang_pixel(x, x->dai_cay_ong);
    khung_them_hcn(k, x1, y_ong - nua_cao, x2, y_ong + nua_cao,
                   0x2b3138u, 1, 0x4a525cu, 1, 1);
    khung_them_duong_dac_biet(k, x1, y_ong, x2, y_ong, 0x3a424cu, 1, 1, 0);
    /* Mam kep o dau X=0 */
    khung_them_hcn(k, x1 - 14, y_ong - nua_cao * 1.7, x1, y_ong + nua_cao * 1.7,
                   0x2f6fb8u, 1, 0x4a86c8u, 1, 1);
    khung_them_chu(k, x1 - 7, y_ong - nua_cao * 1.7 - 9, "mam kep",
                   0x6f9fd8u, 7, 0, 0, NEO_GIUA);
    /* DIEM GOC o dau xa */
    khung_them_duong(k, x2, y_ong - nua_cao * 2.0, x2, y_ong + nua_cao * 2.0,
                     0xffd23fu, 2);
    khung_them_chu(k, x2, y_ong - nua_cao * 2.0 - 9, "DIEM GOC (dau xa)",
                   0xffd23fu, 7, 0, 0, NEO_PHAI);
}

static void ve_cac_khung(const Xep2D *x, KhungVe *k, double y_ong, double nua_cao)
{
    int i;
    for (i = 0; i < x->so_khung; i++) {
        const KhungNhatCat *n = &x->cac_khung[i];
        double p1 = xep2d_sang_pixel(x, n->x_dau);
        double p2 = xep2d_sang_pixel(x, n->x_cuoi);
        int chon = (i == x->dang_chon);
        char chu[CO_CHU_VE];

        if (p2 - p1 < 3) {          /* nhat cat rat mong: van ve thay duoc */
            double giua = (p1 + p2) / 2.0;
            p1 = giua - 1.5; p2 = giua + 1.5;
        }
        khung_them_hcn(k, p1, y_ong - nua_cao * 1.25, p2, y_ong + nua_cao * 1.25,
                       chon ? 0x8c2f2fu : 0x5c2626u, 1,
                       chon ? 0xffd23fu : 0xb84848u, 1, chon ? 2 : 1);
        snprintf(chu, sizeof(chu), "%d", i + 1);
        khung_them_chu(k, (p1 + p2) / 2.0, y_ong, chu, 0xffe9c9u, 9, 0, 1, NEO_GIUA);

        if (chon) {
            /* Duong goc + so do khoang cach tu DIEM GOC toi canh gan nhat */
            double x_goc_px = xep2d_sang_pixel(x, x->dai_cay_ong);
            double y_do = y_ong + nua_cao * 1.9;
            double kc = xep2d_khoang_cach_tu_goc(x, n->x_cuoi);
            khung_them_duong_dac_biet(k, p2, y_do, x_goc_px, y_do,
                                      0xffd23fu, 1, 0, 1);
            snprintf(chu, sizeof(chu), "%.1f mm", kc);
            khung_them_chu(k, (p2 + x_goc_px) / 2.0, y_do - 9, chu,
                           0xffd23fu, 9, 1, 1, NEO_GIUA);
            snprintf(chu, sizeof(chu), "%s  (rong %.1f mm)",
                     n->ten, n->x_cuoi - n->x_dau);
            khung_them_chu(k, (p1 + p2) / 2.0, y_ong - nua_cao * 1.25 - 10, chu,
                           0xffd23fu, 8, 0, 0, NEO_GIUA);
        }
    }
}

static void ve_chu_thich(const Xep2D *x, KhungVe *k, int rong, int cao)
{
    if (x->so_khung == 0)
        khung_them_chu(k, rong / 2.0, cao / 2.0 + 30,
                       "Chua co nhat cat nao - them tu thu vien moi noi",
                       0x6c7581u, 10, 0, 0, NEO_GIUA);
    khung_them_chu(k, rong - 8, cao - 10,
                   "keo khung = doi cho  |  keo chuot giua = day  |  "
                   "lan chuot = phong to",
                   0x5a6472u, 8, 0, 0, NEO_PHAI);
}

int xep2d_dung_hinh(Xep2D *x, int rong, int cao, KhungVe *ra)
{
    double y_ong, nua_cao;

    xep2d_dat_co_khung_hinh(x, rong, cao);
    rong = x->rong;
    cao = x->cao;

    khung_ve_khoi_tao(ra, MAU_NEN_XEP);
    ra->rong = rong;
    ra->cao = cao;
    khung_them_hcn(ra, 0, 0, rong, cao, MAU_NEN_XEP, 1, 0, 0, 0);

    y_ong = CAO_THUOC + (cao - CAO_THUOC) / 2.0;
    nua_cao = (cao - CAO_THUOC) * 0.22;
    if (nua_cao > 46) nua_cao = 46;
    if (nua_cao < 4)  nua_cao = 4;

    ve_thuoc(x, ra, rong);
    ve_ong(x, ra, y_ong, nua_cao);
    ve_cac_khung(x, ra, y_ong, nua_cao);
    ve_chu_thich(x, ra, rong, cao);
    return 0;
}

/* ============================================================== TUONG TAC */
/* Bam chuot trai: chon nhat cat duoi con tro (hoac bo chon). */
void xep2d_bam(Xep2D *x, double x_man, double y_man)
{
    int i, chon = -1;
    (void)y_man;
    for (i = 0; i < x->so_khung; i++) {
        double p1 = xep2d_sang_pixel(x, x->cac_khung[i].x_dau);
        double p2 = xep2d_sang_pixel(x, x->cac_khung[i].x_cuoi);
        if (p2 - p1 < 8) {
            double giua = (p1 + p2) / 2.0;
            p1 = giua - 4; p2 = giua + 4;
        }
        if (p1 - 2 <= x_man && x_man <= p2 + 2) { chon = i; break; }
    }
    x->dang_chon = chon;
    if (chon >= 0) {
        x->keo_khung = chon;
        x->keo_lech = x->cac_khung[chon].x_tam - xep2d_sang_mm(x, x_man);
    } else {
        x->keo_khung = -1;
    }
    if (x->ham.khi_chon) x->ham.khi_chon(x->ham.ctx, chon);
    if (x->ham.can_ve_lai) x->ham.can_ve_lai(x->ham.ctx);
}

void xep2d_keo(Xep2D *x, double x_man, double y_man)
{
    double x_tam_moi;
    (void)y_man;
    if (x->keo_khung < 0) return;
    x_tam_moi = xep2d_sang_mm(x, x_man) + x->keo_lech;
    if (x->ham.khi_keo) x->ham.khi_keo(x->ham.ctx, x->keo_khung, x_tam_moi);
}

void xep2d_nha(Xep2D *x) { x->keo_khung = -1; }

void xep2d_bat_dau_day(Xep2D *x, double x_man)
{
    x->dang_day = 1;
    x->day_tu = x_man;
}

void xep2d_day_khung_nhin(Xep2D *x, double x_man)
{
    if (!x->dang_day) return;
    x->khung.phong = xep2d_ty_le(x);
    x->khung.tu_dong = 0;
    x->khung.day += x_man - x->day_tu;
    x->day_tu = x_man;
    if (x->ham.can_ve_lai) x->ham.can_ve_lai(x->ham.ctx);
}

void xep2d_het_day(Xep2D *x) { x->dang_day = 0; }

/* Phong to quanh diem dat con tro, de cho dang xem khong bi troi di. */
void xep2d_phong_to(Xep2D *x, double he_so, double x_man)
{
    double ty_le_cu = xep2d_ty_le(x);
    double mm_duoi_con_tro;
    if (x_man < 0) x_man = x->rong / 2.0;
    mm_duoi_con_tro = xep2d_sang_mm(x, x_man);

    x->khung.tu_dong = 0;
    x->khung.phong = ty_le_cu * he_so;
    if (x->khung.phong < 0.02) x->khung.phong = 0.02;
    if (x->khung.phong > 80.0) x->khung.phong = 80.0;
    /* Giu nguyen diem mm dang nam duoi con tro */
    x->khung.day = x_man - LE_TRAI_XEP - mm_duoi_con_tro * x->khung.phong;
    if (x->ham.can_ve_lai) x->ham.can_ve_lai(x->ham.ctx);
}

void xep2d_vua_khung_hinh(Xep2D *x)
{
    khung_nhin_2d_dat_lai(&x->khung);
    if (x->ham.can_ve_lai) x->ham.can_ve_lai(x->ham.ctx);
}
