#include "phan_tich_gcode.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Cac ma G firmware hieu (xem xu_ly_1_dong_gcode trong main.c) */
static const int MA_G_HIEU[] = {0, 1, 2, 3, 4, 17, 18, 19, 20, 21, 28, 30,
                                40, 41, 42, 43, 49, 54, 55, 56, 57, 58, 59,
                                61, 64, 80, 90, 91, 92, 93, 94, 98, 99};
static const int MA_M_HIEU[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 30};

static int co_trong(const int *bang, int n, int gt)
{
    int i;
    for (i = 0; i < n; i++) if (bang[i] == gt) return 1;
    return 0;
}

static int la_ma_di_chuyen(int g) { return g >= 0 && g <= 3; }

/* =========================================================================
 * TACH TOKEN
 * ========================================================================= */

/* Bo phan chu thich '(...)' va ';...' ra khoi mot dong G-code */
static void bo_chu_thich(const char *dong, char *ra, size_t co_ra)
{
    size_t n = 0;
    int do_sau = 0;
    const char *p;
    for (p = dong; *p && n + 1 < co_ra; p++) {
        if (*p == '(') { do_sau++; continue; }
        if (*p == ')') { if (do_sau > 0) { do_sau--; continue; } }
        if (*p == ';' && do_sau == 0) break;
        if (do_sau == 0) ra[n++] = *p;
    }
    ra[n] = '\0';
}

typedef struct { char chu; double so; } Token;
#define SO_TOKEN_TOI_DA 32

/* Doc mot so G-code: chi chap nhan dang [+-]?so[.so] - KHONG hex, KHONG mu.
 * Giong het bo doc trong firmware: "G0X100" phai ra G=0 va X=100, chu khong
 * duoc de strtod nuot "0X100" thanh so hex 256. */
static double doc_so(const char *p, const char **ket_thuc)
{
    const char *dau = p, *dau_so;
    char tam[48];
    size_t n;
    if (*p == '+' || *p == '-') p++;
    dau_so = p;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
    if (p == dau_so) { *ket_thuc = dau; return 0.0; }
    n = (size_t)(p - dau);
    if (n >= sizeof(tam)) n = sizeof(tam) - 1;
    memcpy(tam, dau, n);
    tam[n] = '\0';
    *ket_thuc = p;
    return atof(tam);
}

static int tach_token(const char *dong_sach, Token *ra, int toi_da)
{
    int n = 0;
    const char *p = dong_sach;
    while (*p && n < toi_da) {
        if (isalpha((unsigned char)*p)) {
            char chu = (char)toupper((unsigned char)*p);
            const char *ket, *q;
            double gt;
            p++;
            /* Cho phep co dau cach giua chu va so: "A 135" van la A135,
             * giong het bieu thuc chinh quy ben Python: [A-Za-z]\s*[+-]?so */
            q = p;
            while (*q && isspace((unsigned char)*q)) q++;
            gt = doc_so(q, &ket);
            if (ket == q) { continue; }
            ra[n].chu = chu;
            ra[n].so = gt;
            n++;
            p = ket;
        } else {
            p++;
        }
    }
    return n;
}

/* =========================================================================
 * KET QUA
 * ========================================================================= */
void kq_phan_tich_khoi_tao(KetQuaPhanTich *kq)
{
    memset(kq, 0, sizeof(*kq));
}

void kq_phan_tich_giai_phong(KetQuaPhanTich *kq)
{
    free(kq->doan);
    free(kq->dong_chuan_hoa);
    memset(kq, 0, sizeof(*kq));
}

static int them_doan(KetQuaPhanTich *kq, double x1, double a1,
                     double x2, double a2, int la_cat)
{
    if (kq->so_doan >= kq->suc_chua_doan) {
        int moi = kq->suc_chua_doan ? kq->suc_chua_doan * 2 : 256;
        DoanDi *t = (DoanDi *)realloc(kq->doan, (size_t)moi * sizeof(DoanDi));
        if (!t) return -1;
        kq->doan = t;
        kq->suc_chua_doan = moi;
    }
    kq->doan[kq->so_doan].x1 = x1;
    kq->doan[kq->so_doan].a1 = a1;
    kq->doan[kq->so_doan].x2 = x2;
    kq->doan[kq->so_doan].a2 = a2;
    kq->doan[kq->so_doan].la_cat = la_cat;
    kq->so_doan++;
    return 0;
}

static int them_dong(KetQuaPhanTich *kq, const char *chu)
{
    if (kq->so_dong >= kq->suc_chua_dong) {
        int moi = kq->suc_chua_dong ? kq->suc_chua_dong * 2 : 512;
        char (*t)[CO_DONG_G] = (char (*)[CO_DONG_G])realloc(
            kq->dong_chuan_hoa, (size_t)moi * CO_DONG_G);
        if (!t) return -1;
        kq->dong_chuan_hoa = t;
        kq->suc_chua_dong = moi;
    }
    snprintf(kq->dong_chuan_hoa[kq->so_dong], CO_DONG_G, "%s", chu);
    kq->so_dong++;
    return 0;
}

static void them_canh_bao(KetQuaPhanTich *kq, int so_dong, const char *chu)
{
    if (kq->so_canh_bao >= SO_CANH_BAO_TOI_DA) return;
    snprintf(kq->canh_bao[kq->so_canh_bao], CO_LOI, "Dong %d: %s", so_dong, chu);
    kq->so_canh_bao++;
}

/* =========================================================================
 * PHAN TICH
 * ========================================================================= */
int phan_tich_chuong_trinh(const char *const *cac_dong, int so_dong_vao,
                           double toc_do_cat, double toc_do_nhanh,
                           int che_do, double duong_kinh,
                           KetQuaPhanTich *kq)
{
    double x = 0.0, a = 0.0;
    int tuyet_doi = 1, plasma = 0, da_bao_inch = 0;
    double he_so_dai = 1.0;
    int g_di_chuyen_modal = -1;
    double f_modal = -1.0;
    double chu_vi = duong_kinh > 0.0 ? M_PI * duong_kinh : 0.0;
    int doi_a_sang_do = (che_do == 3 && chu_vi > 0.0);
    int chi_so;

    kq_phan_tich_khoi_tao(kq);

    for (chi_so = 1; chi_so <= so_dong_vao; chi_so++) {
        const char *dong_goc = cac_dong[chi_so - 1];
        char sach[512], tam[CO_DONG_G], sx[20], sa[20], sf[20];
        Token tk[SO_TOKEN_TOI_DA];
        int so_tk, i;
        int ma_g[SO_TOKEN_TOI_DA], so_g = 0;
        int ma_m[SO_TOKEN_TOI_DA], so_m = 0;
        int co_x = 0, co_a = 0, co_f = 0, co_p = 0;
        double gt_x = 0, gt_a = 0, gt_f = 0, gt_p = 0;
        int co_toa_do, ma_dc = -1, ve_goc = 0, dat_goc = 0;
        double x_moi, a_moi;
        int co_di_chuyen = 0, la_cat = 0, chi_co_f;

        bo_chu_thich(dong_goc, sach, sizeof(sach));
        /* cat khoang trang hai dau */
        {
            char *dau = sach, *cuoi;
            while (*dau && isspace((unsigned char)*dau)) dau++;
            cuoi = dau + strlen(dau);
            while (cuoi > dau && isspace((unsigned char)cuoi[-1])) cuoi--;
            *cuoi = '\0';
            if (dau != sach) memmove(sach, dau, strlen(dau) + 1);
        }
        if (sach[0] == '\0') {
            /* Dong chi co chu thich: giu nguyen de nguoi doc con thay */
            const char *p = dong_goc;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) {
                snprintf(tam, sizeof(tam), "%s", dong_goc);
                {
                    char *c = tam + strlen(tam);
                    while (c > tam && isspace((unsigned char)c[-1])) *--c = '\0';
                }
                if (them_dong(kq, tam) != 0) return -1;
            }
            continue;
        }

        so_tk = tach_token(sach, tk, SO_TOKEN_TOI_DA);
        if (so_tk == 0) continue;

        for (i = 0; i < so_tk; i++) {
            switch (tk[i].chu) {
            case 'G': ma_g[so_g++] = (int)floor(tk[i].so + 0.5); break;
            case 'M': ma_m[so_m++] = (int)floor(tk[i].so + 0.5); break;
            case 'X': co_x = 1; gt_x = tk[i].so; break;
            case 'A': co_a = 1; gt_a = tk[i].so; break;
            case 'Y': if (!co_a) { co_a = 1; gt_a = tk[i].so; } break;
            case 'F': co_f = 1; gt_f = tk[i].so; break;
            case 'P': co_p = 1; gt_p = tk[i].so; break;
            default: break;
            }
        }

        /* ----- Kiem tra ma khong duoc ho tro ----- */
        for (i = 0; i < so_g; i++) {
            if (!co_trong(MA_G_HIEU, (int)(sizeof(MA_G_HIEU) / sizeof(int)), ma_g[i])) {
                snprintf(tam, sizeof(tam), "G%d firmware KHONG ho tro - chuong "
                         "trinh se bi tu choi khi nap", ma_g[i]);
                them_canh_bao(kq, chi_so, tam);
                kq->co_loi_nang = 1;
            }
        }
        for (i = 0; i < so_m; i++) {
            if (!co_trong(MA_M_HIEU, (int)(sizeof(MA_M_HIEU) / sizeof(int)), ma_m[i])) {
                snprintf(tam, sizeof(tam), "M%d firmware KHONG ho tro - chuong "
                         "trinh se bi tu choi khi nap", ma_m[i]);
                them_canh_bao(kq, chi_so, tam);
                kq->co_loi_nang = 1;
            }
        }
        if (so_g + so_m > 1) {
            them_canh_bao(kq, chi_so,
                          "co nhieu ma G/M tren 1 dong (firmware moi da chay duoc het)");
        }

        /* ----- Cap nhat trang thai modal ----- */
        for (i = 0; i < so_g; i++) {
            if (ma_g[i] == 20) {
                he_so_dai = 25.4;
                if (!da_bao_inch) {
                    them_canh_bao(kq, chi_so, "file dung don vi INCH (G20) - toa do X "
                                              "duoc doi sang mm (x25.4)");
                    da_bao_inch = 1;
                }
            } else if (ma_g[i] == 21) {
                he_so_dai = 1.0;
            } else if (ma_g[i] == 90) {
                tuyet_doi = 1;
            } else if (ma_g[i] == 91) {
                tuyet_doi = 0;
            }
        }
        if (co_f) f_modal = gt_f;

        chi_co_f = (so_g == 0 && so_m == 0 && co_f && !co_x && !co_a);
        if (chi_co_f) {
            them_canh_bao(kq, chi_so, "dong chi co F - dat toc do modal "
                                      "(firmware moi da chap nhan)");
            continue;
        }

        /* ----- Xac dinh lenh di chuyen (co ke ca che do modal) ----- */
        co_toa_do = co_x || co_a;
        for (i = 0; i < so_g; i++) {
            if (la_ma_di_chuyen(ma_g[i])) {
                ma_dc = ma_g[i];
                g_di_chuyen_modal = ma_g[i];
            }
            if (ma_g[i] == 28 || ma_g[i] == 30) ve_goc = 1;
            if (ma_g[i] == 92) dat_goc = 1;
        }

        if (ma_dc < 0 && co_toa_do && !ve_goc && !dat_goc) {
            if (g_di_chuyen_modal < 0) {
                them_canh_bao(kq, chi_so, "co toa do nhung chua tung khai bao G0/G1 "
                                          "truoc do - bo qua dong nay");
                kq->co_loi_nang = 1;
                continue;
            }
            ma_dc = g_di_chuyen_modal;
            snprintf(tam, sizeof(tam), "dong chi co toa do (che do modal) - dung lai "
                     "G%d cua dong truoc", ma_dc);
            them_canh_bao(kq, chi_so, tam);
        }

        /* ----- Cac dong khong di chuyen: chi cap nhat trang thai ----- */
        for (i = 0; i < so_m; i++) {
            if (ma_m[i] == 3 || ma_m[i] == 4) plasma = 1;
            else if (ma_m[i] == 5 || ma_m[i] == 2 || ma_m[i] == 30) plasma = 0;
        }

        /* ----- Tinh toan quang duong ----- */
        x_moi = x;
        a_moi = a;
        if (dat_goc) {
            if (co_x) x = gt_x * he_so_dai;
            if (co_a) a = doi_a_sang_do ? gt_a / chu_vi * 360.0 : gt_a;
        } else if (ve_goc) {
            x_moi = 0.0;
            a_moi = 0.0;
            co_di_chuyen = (fabs(x_moi - x) > 1e-9) || (fabs(a_moi - a) > 1e-9);
            la_cat = 0;
        } else if (ma_dc >= 0 && co_toa_do) {
            if (co_x) {
                double gt = gt_x * he_so_dai;
                x_moi = tuyet_doi ? gt : x + gt;
            }
            if (co_a) {
                /* A la GOC (do) - khong nhan he so inch.
                 * Che do 3: gia tri trong file la mm cung -> doi sang do */
                double gt = doi_a_sang_do ? gt_a / chu_vi * 360.0 : gt_a;
                a_moi = tuyet_doi ? gt : a + gt;
            }
            co_di_chuyen = (fabs(x_moi - x) > 1e-9) || (fabs(a_moi - a) > 1e-9);
            la_cat = plasma && ma_dc != 0;
        }

        if (co_di_chuyen) {
            if (them_doan(kq, x, a, x_moi, a_moi, la_cat) != 0) return -1;
            x = x_moi;
            a = a_moi;
        }

        /* ----- Dem so buoc ma firmware se sinh ra ----- */
        if (co_di_chuyen) kq->so_buoc_firmware++;
        if (dat_goc) kq->so_buoc_firmware++;
        if (co_trong(ma_g, so_g, 4)) kq->so_buoc_firmware++;
        for (i = 0; i < so_m; i++) {
            int m = ma_m[i];
            if (m == 0 || m == 1 || m == 2 || m == 3 || m == 4 || m == 5 || m == 30)
                kq->so_buoc_firmware++;
        }

        /* ----- Sinh ban G-code chuan hoa ----- */
        for (i = 0; i < so_g; i++) {
            int g = ma_g[i];
            if (la_ma_di_chuyen(g) || g == 4 || g == 20 || g == 28 || g == 30 || g == 92)
                continue;   /* xu ly rieng ben duoi */
            snprintf(tam, sizeof(tam), "G%d", g);
            if (them_dong(kq, tam) != 0) return -1;
        }
        if (he_so_dai != 1.0 && co_trong(ma_g, so_g, 20)) {
            if (them_dong(kq, "G21") != 0) return -1;    /* da doi sang mm roi */
        }

        for (i = 0; i < so_m; i++) {
            if (ma_m[i] == 3 || ma_m[i] == 4) {          /* bat mo TRUOC khi di chuyen */
                snprintf(tam, sizeof(tam), "M%d", ma_m[i]);
                if (them_dong(kq, tam) != 0) return -1;
            }
        }

        if (dat_goc) {
            char phan[CO_DONG_G];
            snprintf(phan, sizeof(phan), "G92");
            if (co_x) {
                so_gon(sx, sizeof(sx), x);
                snprintf(phan + strlen(phan), sizeof(phan) - strlen(phan), " X%s", sx);
            }
            if (co_a) {
                /* Gui xuong theo DUNG don vi trong file goc - firmware tu doi */
                so_gon(sa, sizeof(sa), gt_a);
                snprintf(phan + strlen(phan), sizeof(phan) - strlen(phan), " A%s", sa);
            }
            if (them_dong(kq, phan) != 0) return -1;
        }

        if (co_trong(ma_g, so_g, 4)) {
            so_gon(sx, sizeof(sx), co_p ? gt_p : 0.0);
            snprintf(tam, sizeof(tam), "G4 P%s", sx);
            if (them_dong(kq, tam) != 0) return -1;
        }

        if (co_di_chuyen) {
            double f_dung = (f_modal > 0.0) ? f_modal : (la_cat ? toc_do_cat : toc_do_nhanh);
            if (ve_goc) {
                /* G28/G30 -> doi thanh lenh tuong duong ro rang, tranh phu thuoc
                 * vao che do G90/G91 dang hieu luc va toc do modal khong xac dinh */
                if (!tuyet_doi && them_dong(kq, "G90") != 0) return -1;
                so_gon(sf, sizeof(sf), toc_do_nhanh);
                snprintf(tam, sizeof(tam), "G0 X0 A0 F%s", sf);
                if (them_dong(kq, tam) != 0) return -1;
                if (!tuyet_doi && them_dong(kq, "G91") != 0) return -1;
            } else {
                /* Toa do A gui xuong phai theo DUNG don vi ma firmware dang cho
                 * doi (che do 3 = mm cung), nen doi nguoc lai neu da doi o tren */
                double a_gui = doi_a_sang_do ? a / 360.0 * chu_vi : a;
                so_gon(sx, sizeof(sx), x);
                so_gon(sa, sizeof(sa), a_gui);
                so_gon(sf, sizeof(sf), f_dung);
                snprintf(tam, sizeof(tam), "G%d X%s A%s F%s", ma_dc, sx, sa, sf);
                if (them_dong(kq, tam) != 0) return -1;
            }
        }

        for (i = 0; i < so_m; i++) {
            int m = ma_m[i];
            if (m == 5 || m == 2 || m == 30 || m == 0 || m == 1 ||
                m == 6 || m == 7 || m == 8 || m == 9) {
                snprintf(tam, sizeof(tam), "M%d", m);   /* tat mo / ket thuc SAU */
                if (them_dong(kq, tam) != 0) return -1;
            }
        }
    }

    /* Khong con canh bao "chuong trinh qua dai" - may tinh nap dan nen do dai
     * khong bi chan. Chi nhac de nguoi dung biet luc chay phai giu ket noi COM. */
    if (kq->so_buoc_firmware > SUC_CHUA_BO_DEM &&
        kq->so_canh_bao < SO_CANH_BAO_TOI_DA) {
        int i;
        for (i = kq->so_canh_bao; i > 0; i--) {
            memcpy(kq->canh_bao[i], kq->canh_bao[i - 1], CO_LOI);
        }
        snprintf(kq->canh_bao[0], CO_LOI,
                 "Bai dai %d buoc, lon hon bo dem %d buoc cua ESP32 - may se vua chay "
                 "vua nap dan. GIU NGUYEN ket noi COM, dung tat phan mem hay rut day "
                 "trong luc dang cat.", kq->so_buoc_firmware, SUC_CHUA_BO_DEM);
        kq->so_canh_bao++;
    }
    return 0;
}

/* =========================================================================
 * NEN DONG TRUOC KHI GUI
 * ========================================================================= */

/* Duong COM la tai nguyen hiem nhat cua he thong - moi byte tiet kiem duoc la
 * bo dem ESP32 day len nhanh hon bay nhieu. Ba viec, deu khong mat mat gi:
 *   - bo comment ';...' va '(...)' - may khong doc, gui xuong chi phi bang thong
 *   - bo khoang trang: "G1 X10 A45" -> "G1X10A45" (bo tach token cua firmware
 *     doc dung y het, xem tach_token_gcode trong main.c)
 *   - bo so 0 thua o duoi: "X10.500" -> "X10.5", "X10.000" -> "X10"
 * Dong hien tren man hinh van giu nguyen dinh dang de nguoi doc - chi ban khi
 * GUI moi nen. */
int nen_dong_gui(const char *dong, char *ra, size_t co_ra)
{
    char sach[512];
    size_t n = 0;
    const char *p;

    bo_chu_thich(dong, sach, sizeof(sach));

    for (p = sach; *p && n + 1 < co_ra; ) {
        if (isspace((unsigned char)*p)) { p++; continue; }
        if (isdigit((unsigned char)*p) || *p == '.') {
            /* Doc tron so roi cat 0 thua o duoi - chi voi so CO PHAN LE */
            const char *dau = p;
            int co_cham = 0;
            while (isdigit((unsigned char)*p)) p++;
            if (*p == '.') {
                co_cham = 1;
                p++;
                while (isdigit((unsigned char)*p)) p++;
            }
            {
                size_t dai = (size_t)(p - dau);
                char so[64];
                if (dai >= sizeof(so)) dai = sizeof(so) - 1;
                memcpy(so, dau, dai);
                so[dai] = '\0';
                if (co_cham) {
                    char *c = so + strlen(so) - 1;
                    while (c > so && *c == '0') *c-- = '\0';
                    if (*c == '.') *c = '\0';
                }
                {
                    size_t k;
                    for (k = 0; so[k] && n + 1 < co_ra; k++) ra[n++] = so[k];
                }
            }
            continue;
        }
        ra[n++] = *p++;
    }
    ra[n] = '\0';
    return (int)n;
}

/* ==========================================================================
 * DOI TRUC A TU DO SANG MM CUNG
 * ==========================================================================
 * FluidNC coi moi truc la truc THANG khi tinh toc do chay va gia toc. Neu de
 * truc A bang do thi lenh F o cac duong cat cheo se vo nghia - mo cat luc
 * nhanh luc cham. Vi vay ngay truoc khi gui xuong may, chu A duoc doi tu DO
 * sang MM CUNG tren mat ong:
 *
 *      mm_cung = do / 360 * pi * duong_kinh
 *
 * Ban G-code hien tren man hinh van giu don vi DO cho de doc.
 */
int doi_a_sang_mm_cung(const char *dong, double duong_kinh, char *ra, size_t co_ra)
{
    const char *p = dong;
    size_t vt = 0;
    double chu_vi = duong_kinh > 0.0 ? M_PI * duong_kinh : 0.0;

    if (co_ra == 0) return -1;
    if (chu_vi <= 0.0) {                 /* chua biet duong kinh: giu nguyen */
        snprintf(ra, co_ra, "%s", dong);
        return 0;
    }
    while (*p) {
        if ((*p == 'A' || *p == 'a')) {
            const char *ket;
            double gt = doc_so(p + 1, &ket);
            if (ket != p + 1) {
                char so[24];
                int n;
                so_gon(so, sizeof(so), gt / 360.0 * chu_vi);
                n = snprintf(ra + vt, co_ra - vt, "A%s", so);
                if (n < 0 || (size_t)n >= co_ra - vt) return -1;
                vt += (size_t)n;
                p = ket;
                continue;
            }
        }
        if (vt + 1 >= co_ra) return -1;
        ra[vt++] = *p++;
    }
    ra[vt] = '\0';
    return 0;
}
