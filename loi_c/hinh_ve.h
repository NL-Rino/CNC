/* DANH SACH HINH - ket qua ve, chua he lien quan toi thu vien do hoa nao.
 *
 * Cac module tinh toan (mo phong 3D, the xep 2D) khong goi thang ham ve. Chung
 * dung ra mot KhungVe gom cac mat, hinh chu nhat, doan thang va chu, roi lop
 * giao dien (GDI tren Windows) chi viec ve lai. Nho vay:
 *   - toan bo phep chieu va bo tri kiem tra duoc bang chuong trinh dong lenh
 *   - doi thu vien do hoa khong phai sua mot dong tinh toan nao
 *
 * Mau ghi dang 0xRRGGBB.
 */
#ifndef HINH_VE_H
#define HINH_VE_H

#include <stddef.h>

typedef struct { double x, y; } Diem2D;

/* Mot mat (tam giac hoac tu giac) da chieu xuong man hinh. */
typedef struct {
    Diem2D diem[4];
    int so_diem;
    unsigned mau;
    double do_sau;              /* cang lon cang gan nguoi xem */
} MatVe;

typedef struct {
    Diem2D goc1, goc2;
    unsigned mau_nen, mau_vien;
    int co_nen, co_vien;
    int day;
} HinhChuNhat;

typedef struct {
    Diem2D a, b;
    unsigned mau;
    int day;                    /* do day net, pixel */
    int net_dut;                /* 1 = ve net dut */
    int mui_ten;                /* 1 = co mui ten hai dau */
} DuongVe;

#define CO_CHU_VE 160
typedef enum { NEO_TRAI = 0, NEO_GIUA = 1, NEO_PHAI = 2 } KieuNeo;

typedef struct {
    Diem2D vi_tri;
    char chu[CO_CHU_VE];
    unsigned mau;
    int co_chu;                 /* co chu, pixel */
    int mono;                   /* 1 = chu deu (Consolas), 0 = chu thuong */
    int dam;
    KieuNeo neo;
} ChuVe;

/* Mot khung hinh hoan chinh. Thu tu ve: mat -> hinh chu nhat -> duong -> chu. */
typedef struct {
    unsigned mau_nen;
    int rong, cao;
    MatVe       *mat;   int so_mat,   suc_chua_mat;
    HinhChuNhat *hcn;   int so_hcn,   suc_chua_hcn;
    DuongVe     *duong; int so_duong, suc_chua_duong;
    ChuVe       *chu;   int so_chu,   suc_chua_chu;
} KhungVe;

void khung_ve_khoi_tao(KhungVe *k, unsigned mau_nen);
void khung_ve_giai_phong(KhungVe *k);

int khung_them_mat(KhungVe *k, const MatVe *m);
int khung_them_hcn(KhungVe *k, double x1, double y1, double x2, double y2,
                   unsigned mau_nen, int co_nen,
                   unsigned mau_vien, int co_vien, int day);
int khung_them_duong(KhungVe *k, double x1, double y1, double x2, double y2,
                     unsigned mau, int day);
int khung_them_duong_dac_biet(KhungVe *k, double x1, double y1,
                              double x2, double y2, unsigned mau, int day,
                              int net_dut, int mui_ten);
int khung_them_chu(KhungVe *k, double x, double y, const char *chu,
                   unsigned mau, int co_chu, int mono, int dam, KieuNeo neo);

/* Sap xep cac mat tu xa den gan (thuat toan "tho son"). */
void khung_sap_theo_do_sau(KhungVe *k);

#endif /* HINH_VE_H */
