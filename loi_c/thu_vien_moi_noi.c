#include "thu_vien_moi_noi.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double sang_rad(double do_goc) { return do_goc * M_PI / 180.0; }

/* =========================================================================
 * HAM PHU
 * ========================================================================= */

/* So diem can thiet de duong cat du muot tren ong ban kinh r.
 *
 * Lay theo CHIEU DAI CUNG chu khong lay so diem co dinh: ong D200 co chu vi
 * gap 5 lan ong D40 nen phai nhieu diem hon moi cho ra duong cong nhu nhau.
 * Muc tieu: moi doan thang khong dai qua ~0,4 mm tren mat ong.
 *
 * So diem LUON duoc lam tron len boi cua 4 de cac goc 0 / 90 / 180 / 270 roi
 * dung vao mau. Day chinh la cho SAU NHAT va NONG NHAT cua moi duong cat. Neu
 * truot qua chung thi may bo lai mot chut vat lieu ngay tai cho quan trong
 * nhat - vi du yen ngua hai ong bang nhau, day yen dang le cham truc ong chinh
 * lai con du 0,1 mm.
 */
static int so_mui_cat(int toi_thieu, double r)
{
    double chu_vi = 2.0 * M_PI * (r > 1.0 ? r : 1.0);
    int can = (int)(chu_vi / 0.4);
    int n;
    if (can > 1440) can = 1440;
    n = toi_thieu > can ? toi_thieu : can;
    return ((n + 3) / 4) * 4;
}

/* Bo tron nhung cho duong cat GAP GOC qua gat, bang phep "lan bi".
 *
 * TAI SAO CAN: khi ong nhanh va ong chinh BANG duong kinh nhau, day long yen
 * ngua la mot diem NHON that su (cong thuc rut gon thanh L = R*|cos phi|). Tai
 * diem do truc X phai doi chieu NGAY LAP TUC o het toc do cat - do duoc
 * +-0,4 mm moi buoc. Dong co buoc khong the dao chieu nhu vay: no se TRUOT BUOC
 * va vi tri sau do sai het. Ma mo plasma co be rong mach cat huu han cung khong
 * tao noi goc nhon do.
 *
 * CACH LAM: lan mot vien bi ban kinh R doc theo day rooc (phep "dong" hinh thai
 * hoc - closing). Cho nao vien bi lot vao duoc thi giu nguyen; cho nao hep hon
 * vien bi thi thay bang chinh mat vien bi.
 *   - Ket qua LUON >= duong cat goc, tuc chi de lai vat lieu chu khong cat lem
 *     them - dieu can thiet vi cat lem la hong phoi
 *   - Cho nao von da tron hon vien bi thi KHONG bi dong toi
 *   - Do o mat phang trai phang (s = r*phi) nen vien bi tron that trong khong
 *     gian, khong bi meo theo duong kinh ong
 */
static int bo_tron_day(DuongCat *d, double r, double ban_kinh)
{
    int n = d->so_diem, i, t, k;
    double chu_vi, buoc_s, *phinh, *ket_qua;

    if (ban_kinh <= 0.0 || n < 5) return 0;

    chu_vi = 2.0 * M_PI * r;
    buoc_s = chu_vi / (double)(n - 1);
    k = (int)ceil(ban_kinh / (buoc_s > 1e-9 ? buoc_s : 1e-9));
    if (k < 1) k = 1;

    phinh = (double *)malloc((size_t)n * sizeof(double));
    ket_qua = (double *)malloc((size_t)n * sizeof(double));
    if (!phinh || !ket_qua) {
        free(phinh);
        free(ket_qua);
        return -1;
    }

    /* Lay diem theo chi so VONG TRON - duong cat khep kin nen day yen nam o
     * goc 0 do cung phai duoc bo tron nhu moi cho khac */
    for (i = 0; i < n; i++) {
        double s0 = buoc_s * (double)i;
        double lon_nhat = -1e30;
        for (t = -k; t <= k; t++) {
            int j = ((i + t) % (n - 1) + (n - 1)) % (n - 1);
            double vong = floor((double)(i + t - j) / (double)(n - 1));
            double s = buoc_s * (double)j + vong * chu_vi;
            double dt = s - s0;
            double duoi = ban_kinh * ban_kinh - dt * dt;
            double vom = duoi > 0.0 ? sqrt(duoi) : 0.0;
            double gt = d->diem[j].x + vom;
            if (gt > lon_nhat) lon_nhat = gt;
        }
        phinh[i] = lon_nhat;
    }

    for (i = 0; i < n; i++) {
        double s0 = buoc_s * (double)i;
        double nho_nhat = 1e30;
        for (t = -k; t <= k; t++) {
            int j = ((i + t) % (n - 1) + (n - 1)) % (n - 1);
            double vong = floor((double)(i + t - j) / (double)(n - 1));
            double s = buoc_s * (double)j + vong * chu_vi;
            double dt = s - s0;
            double duoi = ban_kinh * ban_kinh - dt * dt;
            double vom = duoi > 0.0 ? sqrt(duoi) : 0.0;
            double gt = phinh[j] - vom;
            if (gt < nho_nhat) nho_nhat = gt;
        }
        /* Chan chan: khong bao gio cat sau hon duong cat goc */
        ket_qua[i] = nho_nhat > d->diem[i].x ? nho_nhat : d->diem[i].x;
    }

    for (i = 0; i < n; i++) d->diem[i].x = ket_qua[i];

    free(phinh);
    free(ket_qua);
    return 0;
}

/* =========================================================================
 * CAC PHEP CAT
 * ========================================================================= */

/* YEN NGUA (fishmouth): dau ong nhanh om vao than ong chinh.
 *
 * Hinh hoc:
 *   Ong chinh: hinh tru ban kinh R, truc trung voi truc z.
 *   Ong nhanh: hinh tru ban kinh r, truc nghieng goc theta so voi truc z, cat
 *              qua truc ong chinh.
 *
 *   Diem tren mat ong nhanh, cach diem giao L doc theo truc nhanh, o goc phi:
 *       P = L*w + r*cos(phi)*u + r*sin(phi)*v
 *   voi  w = (sin t, 0, cos t)     truc ong nhanh
 *        u = (cos t, 0, -sin t)    vuong goc w, nam trong mat phang xz
 *        v = (0, 1, 0)
 *
 *   Bat P nam tren mat ong chinh  (Px^2 + Py^2 = R^2):
 *       (L sin t + r cos phi cos t)^2 + (r sin phi)^2 = R^2
 *   =>  L(phi) = [ sqrt(R^2 - r^2 sin^2 phi) - r cos phi cos t ] / sin t
 *
 *   Goc 90 do rut gon thanh  L = sqrt(R^2 - r^2 sin^2 phi).
 *
 * Chi lay CAN DUONG: do la cho ong nhanh CHAM VAO mat ong chinh. Neu lay ca
 * can am thi duong cat di qua ben kia truc ong chinh, tuc ong nhanh nuot tron
 * ong chinh chu khong ngoi len no.
 *
 * lech_tam: truc ong nhanh khong cat truc ong chinh ma lech di e (mm).
 * khe_ho: noi rong duong cat de con cho han (tru bot L).
 */
int yen_ngua(double r, double r_chinh, double goc_do, double lech_tam,
             double khe_ho, double x_goc, double bo_tron, DuongCat *ra, char *loi)
{
    int n, i;
    double t, sin_t, cos_t;

    if (r <= 0.0) { dat_loi(loi, "Ban kinh ong nhanh phai > 0"); return -1; }
    if (r_chinh <= 0.0) { dat_loi(loi, "Ban kinh ong chinh phai > 0"); return -1; }
    if (r > r_chinh) {
        dat_loi(loi, "Ong nhanh (D%.0f) khong the lon hon ong chinh (D%.0f) "
                     "- khong om vao duoc", 2 * r, 2 * r_chinh);
        return -1;
    }
    if (goc_do < 5.0 || goc_do > 175.0) {
        dat_loi(loi, "Goc yen ngua phai trong khoang 5..175 do");
        return -1;
    }

    t = sang_rad(goc_do);
    sin_t = sin(t);
    cos_t = cos(t);
    n = so_mui_cat(360, r);

    duong_cat_khoi_tao(ra);
    snprintf(ra->ten, sizeof(ra->ten), "Yen ngua %g do", goc_do);
    ra->kin = 1;

    for (i = 0; i <= n; i++) {
        double a = 360.0 * (double)i / (double)n;
        double phi = sang_rad(a);
        double y = lech_tam + r * sin(phi);
        double duoi_can = r_chinh * r_chinh - y * y;
        double L;
        if (duoi_can < 0.0) {
            dat_loi(loi, "Lech tam %.1fmm qua lon: ong nhanh tut ra ngoai ong chinh",
                    lech_tam);
            duong_cat_giai_phong(ra);
            return -1;
        }
        L = (sqrt(duoi_can) - r * cos(phi) * cos_t) / sin_t;
        if (duong_cat_them(ra, x_goc + L - khe_ho, a) != 0) {
            dat_loi(loi, "Het bo nho khi sinh duong cat");
            duong_cat_giai_phong(ra);
            return -1;
        }
    }

    if (bo_tron_day(ra, r, bo_tron) != 0) {
        dat_loi(loi, "Het bo nho khi bo tron day yen");
        duong_cat_giai_phong(ra);
        return -1;
    }
    return 0;
}

/* CAT VAT (miter): mat phang cat nghieng goc so voi truc ong.
 *
 * Diem tren mat ong o goc phi co toa do (r cos phi, r sin phi) trong mat cat
 * ngang, nen vi tri truc:
 *     X(phi) = x_goc + r * tan(beta) * cos(phi)
 * Trai phang ra day dung la MOT DUONG SIN tron ven - mat phang cat hinh tru
 * bao gio cung cho hinh sin.
 *
 * Dung de ghep CO (elbow): hai ong cung vat beta = goc_co/2 roi up vao nhau.
 */
int cat_vat(double r, double goc_do, double x_goc, DuongCat *ra, char *loi)
{
    int n, i;
    double he_so;

    if (goc_do < 0.0 || goc_do >= 89.0) {
        dat_loi(loi, "Goc vat phai trong khoang 0..89 do "
                     "(89 do tro len thi duong cat dai vo han)");
        return -1;
    }
    he_so = r * tan(sang_rad(goc_do));
    n = so_mui_cat(360, r);

    duong_cat_khoi_tao(ra);
    snprintf(ra->ten, sizeof(ra->ten), "Cat vat %g do", goc_do);
    ra->kin = 1;

    for (i = 0; i <= n; i++) {
        double a = 360.0 * (double)i / (double)n;
        if (duong_cat_them(ra, x_goc + he_so * cos(sang_rad(a)), a) != 0) {
            dat_loi(loi, "Het bo nho khi sinh duong cat");
            duong_cat_giai_phong(ra);
            return -1;
        }
    }
    return 0;
}

/* =========================================================================
 * BANG DANG KY 3 KIEU GHEP
 *
 * Ghep hai ong thanh mot goc thi MOI DAU chi can vat NUA goc do, roi up hai
 * mat vat vao nhau. Vi du goc 90 do -> moi dau vat 45 do; goc 45 do -> 22,5 do.
 * ========================================================================= */
const KieuGhep THU_VIEN[SO_KIEU_GHEP] = {
    {
        "goc_90", "Ghep goc 90 do (dau ong)",
        "Noi hai ong thanh goc vuong o dau ong. Moi dau vat 45 do (nua cua 90), "
        "up hai mat vat vao nhau la thanh goc vuong. Trai phang ra la MOT duong sin.",
        1,
        {{"a", "Goc dat mieng vat", 0.0, "do", -KHONG_CHAN, KHONG_CHAN}}
    },
    {
        "goc_45", "Ghep goc 45 do (dau ong)",
        "Noi hai ong thanh goc 45 do o dau ong. Moi dau vat 22,5 do (nua cua 45), "
        "up hai mat vat vao nhau.",
        1,
        {{"a", "Goc dat mieng vat", 0.0, "do", -KHONG_CHAN, KHONG_CHAN}}
    },
    {
        "nhanh_t_90", "Ong nhanh chu T 90 do",
        "Ong nay la ONG NHANH, dau duoc cat long yen ngua de om vuong goc vao "
        "GIUA than mot ong chinh. Ong chinh khong phai cat gi.",
        4,
        {{"d_chinh", "Duong kinh ong chinh", 60.0, "mm", 1.0, KHONG_CHAN},
         {"khe_ho", "Khe ho han", 0.0, "mm", 0.0, 10.0},
         {"bo_tron", "Bo tron day yen", 2.0, "mm", 0.0, 20.0},
         {"a", "Goc dat mieng cat", 0.0, "do", -KHONG_CHAN, KHONG_CHAN}}
    }
};

const KieuGhep *kieu_theo_ma(const char *ma)
{
    int i;
    for (i = 0; i < SO_KIEU_GHEP; i++) {
        if (strcmp(THU_VIEN[i].ma, ma) == 0) return &THU_VIEN[i];
    }
    return NULL;
}

int kieu_chi_so(const char *ma)
{
    int i;
    for (i = 0; i < SO_KIEU_GHEP; i++) {
        if (strcmp(THU_VIEN[i].ma, ma) == 0) return i;
    }
    return -1;
}

void gia_tri_mac_dinh(const KieuGhep *k, GiaTriThamSo *ra)
{
    int i;
    for (i = 0; i < SO_THAM_SO_TOI_DA; i++) ra->gt[i] = 0.0;
    for (i = 0; i < k->so_tham_so; i++) ra->gt[i] = k->tham_so[i].mac_dinh;
}

/* Xoay ca duong cat quanh truc ong (de dat mieng cat huong khac) */
static void xoay_duong(DuongCat *d, double a_lech)
{
    int i;
    if (a_lech == 0.0) return;
    for (i = 0; i < d->so_diem; i++) d->diem[i].a += a_lech;
}

static int kiem_tham_so(const KieuGhep *k, const GiaTriThamSo *g, char *loi)
{
    int i;
    for (i = 0; i < k->so_tham_so; i++) {
        const ThamSo *ts = &k->tham_so[i];
        if (ts->nho_nhat > -KHONG_CHAN && g->gt[i] < ts->nho_nhat) {
            dat_loi(loi, "%s phai >= %g", ts->nhan, ts->nho_nhat);
            return -1;
        }
        if (ts->lon_nhat < KHONG_CHAN && g->gt[i] > ts->lon_nhat) {
            dat_loi(loi, "%s phai <= %g", ts->nhan, ts->lon_nhat);
            return -1;
        }
    }
    return 0;
}

int kieu_sinh(const KieuGhep *k, double duong_kinh_ong, const GiaTriThamSo *g,
              double x_goc, DuongCat *ra, char *loi)
{
    double r = duong_kinh_ong / 2.0;
    int kq;

    if (kiem_tham_so(k, g, loi) != 0) return -1;

    if (strcmp(k->ma, "goc_90") == 0) {
        kq = cat_vat(r, 45.0, x_goc, ra, loi);
        if (kq == 0) xoay_duong(ra, g->gt[0]);
        return kq;
    }
    if (strcmp(k->ma, "goc_45") == 0) {
        kq = cat_vat(r, 22.5, x_goc, ra, loi);
        if (kq == 0) xoay_duong(ra, g->gt[0]);
        return kq;
    }
    if (strcmp(k->ma, "nhanh_t_90") == 0) {
        kq = yen_ngua(r, g->gt[0] / 2.0, 90.0, 0.0, g->gt[1], x_goc, g->gt[2], ra, loi);
        if (kq == 0) xoay_duong(ra, g->gt[3]);
        return kq;
    }
    dat_loi(loi, "Khong biet kieu ghep '%s'", k->ma);
    return -1;
}

/* =========================================================================
 * XEP BAI
 * ========================================================================= */
void ket_qua_xep_khoi_tao(KetQuaXep *kq)
{
    kq->duong = NULL;
    kq->so_duong = 0;
    kq->tong_dung = 0.0;
    kq->canh_bao[0] = '\0';
}

void ket_qua_xep_giai_phong(KetQuaXep *kq)
{
    int i;
    for (i = 0; i < kq->so_duong; i++) duong_cat_giai_phong(&kq->duong[i]);
    free(kq->duong);
    kq->duong = NULL;
    kq->so_duong = 0;
}

void khung_duong_cat(const DuongCat *d, double *x_dau, double *x_cuoi, double *dai)
{
    double lo, hi;
    duong_cat_pham_vi_x(d, &lo, &hi);
    if (x_dau) *x_dau = lo;
    if (x_cuoi) *x_cuoi = hi;
    if (dai) *dai = hi - lo;
}

/* "x" cua moi muc la vi tri TAM cua nhat cat, do tu MAM KEP. Voi mat cat vat
 * thi tam la tam ong; voi long yen ngua thi la diem hai truc ong gap nhau. Do
 * theo tam chu khong theo mep vi mep dai va mep ngan khac nhau, con tam thi
 * khong doi. */
int xep_bai(double duong_kinh_ong, const MucBai *cac_muc, int so_muc,
            double dai_cay_ong, KetQuaXep *ra, char *loi)
{
    int i;

    ket_qua_xep_khoi_tao(ra);
    if (so_muc <= 0) return 0;

    ra->duong = (DuongCat *)calloc((size_t)so_muc, sizeof(DuongCat));
    if (!ra->duong) { dat_loi(loi, "Het bo nho khi xep bai"); return -1; }

    for (i = 0; i < so_muc; i++) {
        const KieuGhep *k = kieu_theo_ma(cac_muc[i].ma);
        char loi_con[CO_LOI];
        double x_cuoi;
        if (!k) {
            dat_loi(loi, "Nhat cat %d: khong biet kieu ghep '%s'", i + 1, cac_muc[i].ma);
            ket_qua_xep_giai_phong(ra);
            return -1;
        }
        loi_con[0] = '\0';
        if (kieu_sinh(k, duong_kinh_ong, &cac_muc[i].gia_tri, cac_muc[i].x,
                      &ra->duong[i], loi_con) != 0) {
            dat_loi(loi, "Nhat cat %d (%s): %s", i + 1, k->ten, loi_con);
            ra->so_duong = i;          /* chi giai phong nhung cai da sinh xong */
            ket_qua_xep_giai_phong(ra);
            return -1;
        }
        ra->so_duong = i + 1;
        /* Cay ong bi chiem toi cho XA NHAT cua nhat cat, khong phai toi tam no */
        khung_duong_cat(&ra->duong[i], NULL, &x_cuoi, NULL);
        if (x_cuoi > ra->tong_dung) ra->tong_dung = x_cuoi;
    }

    if (dai_cay_ong > 0.0 && ra->tong_dung > dai_cay_ong) {
        snprintf(ra->canh_bao, sizeof(ra->canh_bao),
                 "Bai can %.1f mm nhung cay ong chi dai %g mm - THIEU %.1f mm.",
                 ra->tong_dung, dai_cay_ong, ra->tong_dung - dai_cay_ong);
    }
    return 0;
}

/* Khuc ong nam giua nhat cat truoc va nhat cat moi phai dai dung dai_khuc, con
 * khe la phan vat lieu mat di cho mach cat va cho kep. */
int vi_tri_ke_tiep(double duong_kinh_ong, const MucBai *cac_muc, int so_muc,
                   double dai_khuc, double khe, double chua_dau,
                   double *ra, char *loi)
{
    KetQuaXep truoc;
    if (dai_khuc <= 0.0) {
        dat_loi(loi, "Chieu dai khuc phai lon hon 0");
        return -1;
    }
    if (khe < 0.0) {
        dat_loi(loi, "Khoang cach giua cac nhat cat khong the am");
        return -1;
    }
    if (so_muc <= 0) {
        *ra = chua_dau + dai_khuc;
        return 0;
    }
    if (xep_bai(duong_kinh_ong, cac_muc, so_muc, 0.0, &truoc, loi) != 0) return -1;
    *ra = truoc.tong_dung + khe + dai_khuc;
    ket_qua_xep_giai_phong(&truoc);
    return 0;
}

/* =========================================================================
 * SINH G-CODE
 * ========================================================================= */
int sinh_gcode(const DuongCat *cac_duong, int so_duong,
               double toc_do_cat, double toc_do_nhanh, double thoi_gian_duc_lo,
               int co_ve_goc, double x_ve_cho,
               const char *tieu_de[], int so_tieu_de,
               char (*ra)[CO_DONG_GCODE], int toi_da)
{
    int n = 0, i, j;
    char sx[32], sa[32];

    #define THEM(...) do { \
        if (n >= toi_da) return -1; \
        snprintf(ra[n], CO_DONG_GCODE, __VA_ARGS__); \
        n++; \
    } while (0)

    for (i = 0; i < so_tieu_de; i++) THEM("(%s)", tieu_de[i]);
    THEM("G21");
    THEM("G90");

    for (i = 0; i < so_duong; i++) {
        const DuongCat *d = &cac_duong[i];
        if (d->so_diem == 0) continue;
        so_gon(sx, sizeof(sx), d->diem[0].x);
        so_gon(sa, sizeof(sa), d->diem[0].a);
        THEM("(%s)", d->ten);
        THEM("G0 X%s A%s F%g", sx, sa, toc_do_nhanh);
        THEM("M3");
        if (thoi_gian_duc_lo > 0.0) THEM("G4 P%g", thoi_gian_duc_lo);
        /* Dat F ngay tren dong G1 DAU TIEN chu khong de mot dong "F..." rieng:
         * dong F dung mot minh la G-code hop le nhung de rieng thi ton them mot
         * dong tren duong COM, va mot so bo dieu khien khac khong nhan. */
        for (j = 1; j < d->so_diem; j++) {
            so_gon(sx, sizeof(sx), d->diem[j].x);
            so_gon(sa, sizeof(sa), d->diem[j].a);
            if (j == 1) THEM("G1 X%s A%s F%g", sx, sa, toc_do_cat);
            else        THEM("G1 X%s A%s", sx, sa);
        }
        THEM("M5");
    }

    if (co_ve_goc) {
        so_gon(sx, sizeof(sx), x_ve_cho);
        THEM("G0 X%s A0 F%g", sx, toc_do_nhanh);
    }
    THEM("M30");
    #undef THEM
    return n;
}
