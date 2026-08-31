#include "ve_3d.h"
#include "hinh_ve.h"
#include "loi_chung.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define SO_MUI_ONG 48
#define MAU_NEN 0x1b1f24u
#define MAU_ONG_R 0x8a
#define MAU_ONG_G 0x93
#define MAU_ONG_B 0x9e

struct MoPhong3D {
    CanhNhin canh_nhin;

    double duong_kinh;
    double chieu_dai_ong;
    DoanDi *doan;
    int so_doan;
    int vi_tri_chay;            /* -1 = chua chay */

    /* Nhom cac doan CAT lien tiep thanh "mieng cat" de an ca mieng chi bang
     * mot cu bam. */
    int (*nhom)[2];
    int so_nhom;
    unsigned char *nhom_bi_an;  /* 1 byte moi nhom */

    /* Diem bam gan nhat (de giao dien biet nguoi dung vua bam trung mieng nao) */
    struct { double x, y; int chi_so; } *diem_man_hinh;
    int so_diem_man_hinh, suc_chua_diem;
};

/* ================================================================ CANH NHIN */
void canh_nhin_dat_lai(CanhNhin *c)
{
    c->xoay_ngang = -32.0;
    c->xoay_doc   = 20.0;
    c->phong      = 1.0;
    c->day_ngang  = 0.0;
    c->day_doc    = 0.0;
}

/* ================================================================== TAO/HUY */
MoPhong3D *mp3d_tao(void)
{
    MoPhong3D *m = (MoPhong3D *)calloc(1, sizeof(*m));
    if (!m) return NULL;
    canh_nhin_dat_lai(&m->canh_nhin);
    m->duong_kinh = 60.0;
    m->chieu_dai_ong = 300.0;
    m->vi_tri_chay = -1;
    return m;
}

void mp3d_giai_phong(MoPhong3D *m)
{
    if (!m) return;
    free(m->doan);
    free(m->nhom);
    free(m->nhom_bi_an);
    free(m->diem_man_hinh);
    free(m);
}

CanhNhin *mp3d_canh_nhin(MoPhong3D *m) { return &m->canh_nhin; }
double mp3d_duong_kinh(const MoPhong3D *m) { return m->duong_kinh; }
double mp3d_chieu_dai_ong(const MoPhong3D *m) { return m->chieu_dai_ong; }
int    mp3d_so_doan(const MoPhong3D *m) { return m->so_doan; }
int    mp3d_vi_tri_chay(const MoPhong3D *m) { return m->vi_tri_chay; }
int    mp3d_so_nhom(const MoPhong3D *m) { return m->so_nhom; }

/* Gom cac doan CAT lien tiep thanh mot mieng - bam 1 cai la an ca mieng. */
static void chia_nhom(MoPhong3D *m)
{
    int i, dau = -1, n = 0;
    unsigned char *an_cu = m->nhom_bi_an;
    int so_nhom_cu = m->so_nhom;

    free(m->nhom);
    m->nhom = (int (*)[2])calloc((size_t)(m->so_doan + 1), sizeof(int[2]));
    m->so_nhom = 0;
    if (!m->nhom) { m->nhom_bi_an = NULL; free(an_cu); return; }

    for (i = 0; i < m->so_doan; i++) {
        if (m->doan[i].la_cat) {
            if (dau < 0) dau = i;
        } else if (dau >= 0) {
            m->nhom[n][0] = dau; m->nhom[n][1] = i - 1; n++;
            dau = -1;
        }
    }
    if (dau >= 0) { m->nhom[n][0] = dau; m->nhom[n][1] = m->so_doan - 1; n++; }
    m->so_nhom = n;

    /* Giu lai trang thai an cua cac nhom con ton tai */
    m->nhom_bi_an = (unsigned char *)calloc((size_t)(n + 1), 1);
    if (m->nhom_bi_an && an_cu)
        for (i = 0; i < n && i < so_nhom_cu; i++) m->nhom_bi_an[i] = an_cu[i];
    free(an_cu);
}

void mp3d_dat_du_lieu(MoPhong3D *m, const DoanDi *doan, int so_doan,
                      double duong_kinh, double chieu_dai)
{
    int i;
    free(m->doan);
    m->doan = NULL;
    m->so_doan = 0;
    if (so_doan > 0 && doan) {
        m->doan = (DoanDi *)malloc(sizeof(DoanDi) * (size_t)so_doan);
        if (m->doan) {
            memcpy(m->doan, doan, sizeof(DoanDi) * (size_t)so_doan);
            m->so_doan = so_doan;
        }
    }
    m->duong_kinh = duong_kinh > 1.0 ? duong_kinh : 1.0;
    if (chieu_dai > 0) {
        m->chieu_dai_ong = chieu_dai;
    } else {
        double x_max = 100.0;
        for (i = 0; i < m->so_doan; i++) {
            if (m->doan[i].x1 > x_max) x_max = m->doan[i].x1;
            if (m->doan[i].x2 > x_max) x_max = m->doan[i].x2;
        }
        m->chieu_dai_ong = x_max * 1.15 + 40.0;
    }
    chia_nhom(m);
}

void mp3d_dat_vi_tri_chay(MoPhong3D *m, int chi_so) { m->vi_tri_chay = chi_so; }

int mp3d_nhom_cua_doan(const MoPhong3D *m, int chi_so_doan)
{
    int k;
    for (k = 0; k < m->so_nhom; k++)
        if (m->nhom[k][0] <= chi_so_doan && chi_so_doan <= m->nhom[k][1])
            return k;
    return -1;
}

void mp3d_bat_tat_nhom(MoPhong3D *m, int nhom)
{
    if (nhom < 0 || nhom >= m->so_nhom || !m->nhom_bi_an) return;
    m->nhom_bi_an[nhom] = (unsigned char)!m->nhom_bi_an[nhom];
}

void mp3d_hien_lai_het(MoPhong3D *m)
{
    if (m->nhom_bi_an) memset(m->nhom_bi_an, 0, (size_t)(m->so_nhom + 1));
}

int mp3d_so_nhom_bi_an(const MoPhong3D *m)
{
    int i, n = 0;
    if (!m->nhom_bi_an) return 0;
    for (i = 0; i < m->so_nhom; i++) if (m->nhom_bi_an[i]) n++;
    return n;
}

static int bi_an(const MoPhong3D *m, int chi_so)
{
    int k = mp3d_nhom_cua_doan(m, chi_so);
    return k >= 0 && m->nhom_bi_an && m->nhom_bi_an[k];
}

/* ============================================================ VI TRI ONG */
static double giua_vung_cat(const MoPhong3D *m)
{
    double nho, lon;
    int i;
    if (m->so_doan <= 0) return m->chieu_dai_ong / 2.0;
    nho = lon = m->doan[0].x1;
    for (i = 0; i < m->so_doan; i++) {
        double a = m->doan[i].x1, b = m->doan[i].x2;
        if (a < nho) nho = a;
        if (b < nho) nho = b;
        if (a > lon) lon = a;
        if (b > lon) lon = b;
    }
    return (nho + lon) / 2.0;
}

/* May that giu dau cat DUNG YEN o dinh, con ong thi vua truot doc truc vua
 * quay. Vi vay diem dang cat luon nam ngay duoi dau cat.
 *
 * Khi CHUA chay thi khong co diem cat nao dang o duoi mo, luc do dat ong sao
 * cho phan co duong cat nam giua khung hinh - neu khong, ong dai 1200mm se
 * cham dau X=0 vao giua man hinh roi chay tuot ra ngoai khung. */
void mp3d_trang_thai_ong(const MoPhong3D *m, double *x_ong, double *goc_ong)
{
    int i;
    if (m->vi_tri_chay < 0 || m->so_doan <= 0) {
        if (x_ong) *x_ong = giua_vung_cat(m);
        if (goc_ong) *goc_ong = 0.0;
        return;
    }
    i = m->vi_tri_chay;
    if (i > m->so_doan - 1) i = m->so_doan - 1;
    if (x_ong) *x_ong = m->doan[i].x2;
    if (goc_ong) *goc_ong = m->doan[i].a2;
}

/* ============================================================= PHEP CHIEU */
typedef struct {
    const CanhNhin *cn;
    double tam_x, tam_y, ty_le;
} BoChieu;

static void quay(const CanhNhin *cn, double x, double y, double z,
                 double *rx, double *ry, double *rz)
{
    double a = cn->xoay_ngang * PI / 180.0;
    double b = cn->xoay_doc   * PI / 180.0;
    double x1 =  x * cos(a) + z * sin(a);
    double z1 = -x * sin(a) + z * cos(a);
    double y2 =  y * cos(b) - z1 * sin(b);
    double z2 =  y * sin(b) + z1 * cos(b);
    *rx = x1; *ry = y2; *rz = z2;
}

static void chieu(const BoChieu *bc, double x, double y, double z,
                  double *mx, double *my, double *ms)
{
    double px, py, pz;
    quay(bc->cn, x, y, z, &px, &py, &pz);
    *mx = bc->tam_x + px * bc->ty_le + bc->cn->day_ngang;
    *my = bc->tam_y - py * bc->ty_le + bc->cn->day_doc;
    *ms = pz;
}

static unsigned to_bong(double sang)
{
    unsigned r, g, b;
    if (sang < 0.18) sang = 0.18;
    if (sang > 1.0)  sang = 1.0;
    r = (unsigned)(MAU_ONG_R * sang); if (r > 255) r = 255;
    g = (unsigned)(MAU_ONG_G * sang); if (g > 255) g = 255;
    b = (unsigned)(MAU_ONG_B * sang); if (b > 255) b = 255;
    return (r << 16) | (g << 8) | b;
}

/* =================================================================== VE */
/* Than ong. goc_ong = ong da QUAY bao nhieu radian quanh truc cua no. */
static void them_than_ong(KhungVe *k, const BoChieu *bc, double r, double goc_x,
                          double dai, double goc_ong)
{
    int i, j;
    for (i = 0; i < SO_MUI_ONG; i++) {
        double g1 = 2 * PI * i / SO_MUI_ONG + goc_ong;
        double g2 = 2 * PI * (i + 1) / SO_MUI_ONG + goc_ong;
        double y1 = r * cos(g1), z1 = r * sin(g1);
        double y2 = r * cos(g2), z2 = r * sin(g2);
        double gm = (g1 + g2) / 2.0;
        double sang = 0.30 + 0.70 * (cos(gm - 1.0) > 0 ? cos(gm - 1.0) : 0.0);
        double dinh[4][3] = { { goc_x, y1, z1 }, { goc_x + dai, y1, z1 },
                              { goc_x + dai, y2, z2 }, { goc_x, y2, z2 } };
        MatVe m;
        double tong = 0;
        m.so_diem = 4;
        m.mau = to_bong(sang);
        for (j = 0; j < 4; j++) {
            double s;
            chieu(bc, dinh[j][0], dinh[j][1], dinh[j][2],
                  &m.diem[j].x, &m.diem[j].y, &s);
            tong += s;
        }
        m.do_sau = tong / 4.0;
        khung_them_mat(k, &m);
    }
    /* Hai mat cat o hai dau ong */
    for (j = 0; j < 2; j++) {
        double x_dau = j == 0 ? goc_x : goc_x + dai;
        double sang  = j == 0 ? 0.30 : 0.38;
        for (i = 0; i < SO_MUI_ONG; i++) {
            double g1 = 2 * PI * i / SO_MUI_ONG + goc_ong;
            double g2 = 2 * PI * (i + 1) / SO_MUI_ONG + goc_ong;
            double dinh[3][3] = { { x_dau, 0, 0 },
                                  { x_dau, r * cos(g1), r * sin(g1) },
                                  { x_dau, r * cos(g2), r * sin(g2) } };
            MatVe m;
            double tong = 0;
            int t;
            m.so_diem = 3;
            m.mau = to_bong(sang);
            for (t = 0; t < 3; t++) {
                double s;
                chieu(bc, dinh[t][0], dinh[t][1], dinh[t][2],
                      &m.diem[t].x, &m.diem[t].y, &s);
                tong += s;
            }
            m.diem[3] = m.diem[2];
            m.do_sau = tong / 3.0;
            khung_them_mat(k, &m);
        }
    }
}

static void them_diem_bam(MoPhong3D *m, double x, double y, int chi_so)
{
    if (m->so_diem_man_hinh >= m->suc_chua_diem) {
        int moi = m->suc_chua_diem ? m->suc_chua_diem * 2 : 256;
        void *p = realloc(m->diem_man_hinh, sizeof(*m->diem_man_hinh) * (size_t)moi);
        if (!p) return;
        m->diem_man_hinh = p;
        m->suc_chua_diem = moi;
    }
    m->diem_man_hinh[m->so_diem_man_hinh].x = x;
    m->diem_man_hinh[m->so_diem_man_hinh].y = y;
    m->diem_man_hinh[m->so_diem_man_hinh].chi_so = chi_so;
    m->so_diem_man_hinh++;
}

static void ve_duong_cat(MoPhong3D *m, KhungVe *k, const BoChieu *bc,
                         double r, double goc_x, double goc_ong)
{
    double r_ve = r * 1.012;
    int i;
    for (i = 0; i < m->so_doan; i++) {
        const DoanDi *d = &m->doan[i];
        double g1, g2, x1, y1, s1, x2, y2, s2;
        unsigned mau;
        int day;
        if (bi_an(m, i)) continue;
        g1 = d->a1 * PI / 180.0 + goc_ong;
        g2 = d->a2 * PI / 180.0 + goc_ong;
        chieu(bc, goc_x + d->x1, r_ve * cos(g1), r_ve * sin(g1), &x1, &y1, &s1);
        chieu(bc, goc_x + d->x2, r_ve * cos(g2), r_ve * sin(g2), &x2, &y2, &s2);
        if (s1 < -r * 0.15 && s2 < -r * 0.15) continue;   /* nam khuat sau ong */
        if (!d->la_cat)                      { mau = 0x5a6472u; day = 1; }
        else if (m->vi_tri_chay < 0)         { mau = 0xff5c4du; day = 2; }
        else if (i < m->vi_tri_chay)         { mau = 0x3fa34du; day = 2; }  /* da cat xong */
        else if (i == m->vi_tri_chay)        { mau = 0xffd23fu; day = 3; }  /* dang cat */
        else                                 { mau = 0xff5c4du; day = 2; }
        khung_them_duong(k, x1, y1, x2, y2, mau, day);
        if (d->la_cat) them_diem_bam(m, x1, y1, i);
    }
    if (m->so_doan == 0)
        khung_them_chu(k, bc->tam_x, bc->tam_y, "Chua co duong cat nao",
                       0x6c7581u, 11, 0, 0, NEO_GIUA);
}

/* Dau cat DUNG YEN o dinh khung hinh - dung nhu may that. */
static void ve_dau_cat(KhungVe *k, const BoChieu *bc, double r)
{
    double cao_mo = r * 1.9;
    double cx, cy, cs, dx, dy, ds, tx, ty, ts;
    chieu(bc, 0.0, r * 1.02,      0.0, &cx, &cy, &cs);
    chieu(bc, 0.0, cao_mo,        0.0, &dx, &dy, &ds);
    chieu(bc, 0.0, cao_mo * 1.6,  0.0, &tx, &ty, &ts);
    khung_them_duong(k, dx, dy, tx, ty, 0xc8ced6u, 8);
    khung_them_duong(k, cx, cy, dx, dy, 0xe8703au, 3);
    /* Dau mo: mot cham tron mau vang. Ghi thanh doan thang dai bang 0 va
     * rat day - lop ve chi viec dung but dau tron, cham dung o day mo. */
    khung_them_duong(k, cx, cy, cx, cy, 0xffd23fu, 10);
}

static void ve_chu_thich(const MoPhong3D *m, KhungVe *k, int rong, int cao,
                         double x_ong, double goc_ong)
{
    char chu[CO_CHU_VE], so_dk[24];
    int so_an = mp3d_so_nhom_bi_an(m);
    double y;
    int i;
    static const struct { const char *ten; unsigned mau; int lech; } ghi_chu[] = {
        { "cat", 0xff5c4du, 0 }, { "chay nhanh", 0x5a6472u, 52 },
        { "dang cat", 0xffd23fu, 150 }
    };

    so_gon(so_dk, sizeof(so_dk), m->duong_kinh);
    if (so_an > 0)
        snprintf(chu, sizeof(chu),
                 "Ong D%s  |  dai %.0fmm  |  %d doan  |  dang an %d mieng",
                 so_dk, m->chieu_dai_ong, m->so_doan, so_an);
    else
        snprintf(chu, sizeof(chu), "Ong D%s  |  dai %.0fmm  |  %d doan",
                 so_dk, m->chieu_dai_ong, m->so_doan);
    khung_them_chu(k, 10, 12, chu, 0x9aa4b0u, 9, 1, 0, NEO_TRAI);

    if (m->vi_tri_chay >= 0) {
        snprintf(chu, sizeof(chu), "Ong: truot X=%.1fmm   quay A=%.1fdo",
                 x_ong, goc_ong);
        khung_them_chu(k, 10, 28, chu, 0xffd23fu, 9, 1, 0, NEO_TRAI);
    }

    y = cao - 14;
    for (i = 0; i < 3; i++) {
        khung_them_duong(k, 10 + ghi_chu[i].lech, y, 26 + ghi_chu[i].lech, y,
                         ghi_chu[i].mau, 3);
        khung_them_chu(k, 30 + ghi_chu[i].lech, y, ghi_chu[i].ten,
                       0x9aa4b0u, 8, 0, 0, NEO_TRAI);
    }
    khung_them_chu(k, rong - 10, cao - 14,
                   "keo trai = xoay  |  keo giua = day  |  lan = phong to",
                   0x5a6472u, 8, 0, 0, NEO_PHAI);
}

int mp3d_dung_hinh(MoPhong3D *m, int rong, int cao, KhungVe *ra)
{
    BoChieu bc;
    double r, dai, ty_le, x_ong, goc_ong, goc_ong_rad, goc_x;

    khung_ve_khoi_tao(ra, MAU_NEN);
    ra->rong = rong > 0 ? rong : 640;
    ra->cao  = cao  > 0 ? cao  : 400;
    m->so_diem_man_hinh = 0;

    r   = m->duong_kinh / 2.0;
    dai = m->chieu_dai_ong;
    ty_le = ra->rong / (dai * 1.35 + 1);
    {
        double kia = ra->cao / (m->duong_kinh * 3.0 + 1);
        if (kia < ty_le) ty_le = kia;
    }
    ty_le *= m->canh_nhin.phong;

    bc.cn = &m->canh_nhin;
    bc.tam_x = ra->rong / 2.0;
    bc.tam_y = ra->cao / 2.0;
    bc.ty_le = ty_le;

    /* ----- ONG TRUOT RA VAO: mo cat dung yen o giua khung hinh -----
     * Toa do ve = toa do tren ong TRU di vi tri dang cat, nen diem dang cat
     * luon roi vao giua man hinh, con ca cay ong thi truot qua. */
    mp3d_trang_thai_ong(m, &x_ong, &goc_ong);
    goc_ong_rad = goc_ong * PI / 180.0;
    goc_x = -x_ong;             /* X=0 cua ong nam o day tren man hinh */

    them_than_ong(ra, &bc, r, goc_x, dai, goc_ong_rad);
    /* Ve tu xa den gan - mat quay ra sau bi mat truoc de len tren */
    khung_sap_theo_do_sau(ra);

    ve_duong_cat(m, ra, &bc, r, goc_x, goc_ong_rad);
    ve_dau_cat(ra, &bc, r);
    ve_chu_thich(m, ra, ra->rong, ra->cao, x_ong, goc_ong);
    return 0;
}

int mp3d_doan_gan_diem(const MoPhong3D *m, double x_man, double y_man,
                       double ban_kinh)
{
    int i, gan_nhat = -1;
    double cach_nhat = ban_kinh * ban_kinh;
    for (i = 0; i < m->so_diem_man_hinh; i++) {
        double dx = m->diem_man_hinh[i].x - x_man;
        double dy = m->diem_man_hinh[i].y - y_man;
        double d = dx * dx + dy * dy;
        if (d < cach_nhat) { gan_nhat = m->diem_man_hinh[i].chi_so; cach_nhat = d; }
    }
    return gan_nhat;
}
