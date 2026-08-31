/* THU VIEN MOI NOI - sinh duong cat cho tung kieu ghep ong.
 *
 * May giu ong trong mam kep va XOAY no (truc A, don vi do), dong thoi keo mo
 * cat doc theo ong (truc X, don vi mm). Vi vay MOI duong cat deu quy ve mot ham
 *          A (do)  ->  X (mm)
 *
 * Toan bo cong thuc deu la hinh hoc CHINH XAC (khong xap xi), dan ngay tren
 * tung ham de sau nay con kiem lai duoc.
 */
#ifndef THU_VIEN_MOI_NOI_H
#define THU_VIEN_MOI_NOI_H

#include "loi_chung.h"

#define SO_KIEU_GHEP     3
#define SO_THAM_SO_TOI_DA 4

/* Mo ta mot o nhap cua kieu moi noi - giao dien tu dung form tu bang nay */
typedef struct {
    const char *ma;
    const char *nhan;
    double mac_dinh;
    const char *don_vi;
    double nho_nhat;      /* dung KHONG_CHAN neu khong gioi han */
    double lon_nhat;
} ThamSo;

#define KHONG_CHAN 1e30

/* Gia tri nguoi dung nhap cho mot nhat cat */
typedef struct {
    double gt[SO_THAM_SO_TOI_DA];
} GiaTriThamSo;

typedef struct {
    const char *ma;
    const char *ten;
    const char *mo_ta;
    int so_tham_so;
    ThamSo tham_so[SO_THAM_SO_TOI_DA];
} KieuGhep;

extern const KieuGhep THU_VIEN[SO_KIEU_GHEP];

const KieuGhep *kieu_theo_ma(const char *ma);
int  kieu_chi_so(const char *ma);                    /* -1 neu khong co */
void gia_tri_mac_dinh(const KieuGhep *k, GiaTriThamSo *ra);

/* Sinh duong cat cua mot kieu ghep tai vi tri x_goc doc theo ong.
 * Tra ve 0 = OK, -1 = loi (ly do ghi vao 'loi'). Ben goi phai
 * duong_cat_giai_phong() sau khi dung xong. */
int kieu_sinh(const KieuGhep *k, double duong_kinh_ong, const GiaTriThamSo *g,
              double x_goc, DuongCat *ra, char *loi);

/* ---------- Cac phep cat co ban (dung truc tiep khi can) ---------- */
int yen_ngua(double r, double r_chinh, double goc_do, double lech_tam,
             double khe_ho, double x_goc, double bo_tron, DuongCat *ra, char *loi);
int cat_vat(double r, double goc_do, double x_goc, DuongCat *ra, char *loi);

/* ---------- XEP BAI ---------- */
typedef struct {
    char ma[24];              /* ma kieu ghep */
    GiaTriThamSo gia_tri;
    double x;                 /* vi tri TAM cua nhat cat, do tu mam kep (mm) */
} MucBai;

typedef struct {
    DuongCat *duong;          /* mang so_duong phan tu */
    int so_duong;
    double tong_dung;         /* cho xa nhat cua bai tinh tu mam kep (mm) */
    char canh_bao[CO_LOI];    /* rong neu khong co */
} KetQuaXep;

void ket_qua_xep_khoi_tao(KetQuaXep *kq);
void ket_qua_xep_giai_phong(KetQuaXep *kq);

/* dai_cay_ong <= 0 nghia la khong kiem tra do dai */
int xep_bai(double duong_kinh_ong, const MucBai *cac_muc, int so_muc,
            double dai_cay_ong, KetQuaXep *ra, char *loi);

/* Vi tri dat nhat cat MOI de no nam noi tiep sau cac nhat da co */
int vi_tri_ke_tiep(double duong_kinh_ong, const MucBai *cac_muc, int so_muc,
                   double dai_khuc, double khe, double chua_dau,
                   double *ra, char *loi);

/* Khung chu nhat bao quanh mot duong cat theo truc ong */
void khung_duong_cat(const DuongCat *d, double *x_dau, double *x_cuoi, double *dai);

/* ---------- SINH G-CODE ---------- */
/* Ghi cac dong G-code vao 'ra' (mang chuoi do ben goi cap). Tra ve so dong da
 * ghi, hoac -1 neu het cho. Moi dong dai toi da 63 ky tu. */
#define CO_DONG_GCODE 96
int sinh_gcode(const DuongCat *cac_duong, int so_duong,
               double toc_do_cat, double toc_do_nhanh, double thoi_gian_duc_lo,
               int co_ve_goc, double x_ve_cho,
               const char *tieu_de[], int so_tieu_de,
               char (*ra)[CO_DONG_GCODE], int toi_da);

#endif
