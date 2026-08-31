/* THE XEP 2D - nhin cay ong nam thang, keo tha cac nhat cat doc theo no.
 *
 * Moi nhat cat duoc ve thanh MOT HINH CHU NHAT dai bang be ngang cua duong cat
 * do theo truc ong. Nhin phat la biet nhat cat an het bao nhieu ong.
 *
 * DIEM GOC de do khoang cach la DAU ONG XA MAM KEP NHAT. Khoang cach cua mot
 * nhat cat = tu diem goc toi CANH GAN DIEM GOC NHAT cua khung chu nhat do.
 *
 *     mam kep |=====================================| dau xa (DIEM GOC)
 *             0                                    dai_cay      (toa do may)
 *             <----------- khoang cach ------------>
 *                               [khung]
 *
 * Giong ve_3d: module nay chi DUNG RA danh sach hinh, khong goi ham do hoa nao.
 */
#ifndef XEP_2D_H
#define XEP_2D_H

#include "hinh_ve.h"

#define LE_TRAI_XEP  60         /* chua cho nhan thuoc ben trai */
#define LE_PHAI_XEP  20
#define CAO_THUOC    34
#define CO_TEN_KHUNG 48

typedef struct {
    double x_dau, x_cuoi;       /* toa do may, mm */
    char   ten[CO_TEN_KHUNG];
    double x_tam;
} KhungNhatCat;

typedef struct {
    double phong;               /* pixel tren mot mm */
    double day;                 /* tinh tien ngang (pixel) */
    int    tu_dong;             /* 1 = tu canh cho vua khung hinh */
} KhungNhin2D;

void khung_nhin_2d_dat_lai(KhungNhin2D *k);

typedef struct Xep2D Xep2D;

/* khi_chon(ctx, chi_so) - chi_so = -1 nghia la bo chon.
 * khi_keo(ctx, chi_so, x_tam_moi_mm) - giao dien tu quyet dinh co nhan hay khong. */
typedef struct {
    void *ctx;
    void (*khi_chon)(void *ctx, int chi_so);
    void (*khi_keo)(void *ctx, int chi_so, double x_tam_moi);
    void (*can_ve_lai)(void *ctx);
} HamXep2D;

Xep2D *xep2d_tao(const HamXep2D *ham);
void   xep2d_giai_phong(Xep2D *x);

void xep2d_dat_du_lieu(Xep2D *x, const KhungNhatCat *cac_khung, int so_khung,
                       double duong_kinh, double dai_cay_ong);
void xep2d_dat_co_khung_hinh(Xep2D *x, int rong, int cao);

int  xep2d_dang_chon(const Xep2D *x);
void xep2d_dat_dang_chon(Xep2D *x, int chi_so);
int  xep2d_so_khung(const Xep2D *x);

/* --- Doi toa do --- */
double xep2d_ty_le(const Xep2D *x);
double xep2d_sang_pixel(const Xep2D *x, double x_may);
double xep2d_sang_mm(const Xep2D *x, double px);
double xep2d_khoang_cach_tu_goc(const Xep2D *x, double x_cuoi);
/* Nguoi dung go khoang cach -> tam nhat cat nam o dau. */
double xep2d_tu_khoang_cach(const Xep2D *x, double khoang_cach,
                            double x_tam, double x_cuoi);

/* --- Ve --- */
int xep2d_dung_hinh(Xep2D *x, int rong, int cao, KhungVe *ra);

/* --- Tuong tac --- */
void xep2d_bam(Xep2D *x, double x_man, double y_man);
void xep2d_keo(Xep2D *x, double x_man, double y_man);
void xep2d_nha(Xep2D *x);
void xep2d_bat_dau_day(Xep2D *x, double x_man);
void xep2d_day_khung_nhin(Xep2D *x, double x_man);
void xep2d_het_day(Xep2D *x);
void xep2d_phong_to(Xep2D *x, double he_so, double x_man);
void xep2d_vua_khung_hinh(Xep2D *x);

#endif /* XEP_2D_H */
