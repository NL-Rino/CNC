/* Kieu du lieu va tien ich dung chung cho ca loi tinh toan.
 *
 * Toan bo loi tinh toan viet bang C thuan, KHONG dinh gi toi he dieu hanh hay
 * giao dien - nho vay chay duoc ca tren Windows lan Linux, va TEST duoc bang
 * script khong can mo cua so.
 *
 * QUY UOC BAO LOI: ham tra ve 0 la thanh cong, -1 la loi. Khi loi thi ly do
 * duoc ghi vao bo dem 'loi' ma ben goi cap - khong co ngoai le nhu Python nen
 * moi cho goi deu phai kiem tra tra ve.
 */
#ifndef LOI_CHUNG_H
#define LOI_CHUNG_H

#include <stddef.h>

#define CO_LOI 256        /* do dai bo dem thong bao loi */

/* M_PI khong nam trong chuan C nen mot so trinh dich khong co - dinh nghia
 * o day de ca phan mem dung chung mot hang so. */
#define PI 3.14159265358979323846

/* Mot diem tren duong cat: X doc truc ong (mm), A goc xoay ong (do) */
typedef struct {
    double x;
    double a;
} DiemCat;

/* Mot duong cat tren mat ong. diem[] duoc cap phat dong. */
typedef struct {
    char ten[48];
    DiemCat *diem;
    int so_diem;
    int suc_chua;
    int kin;              /* 1 = duong khep kin (quay het mot vong) */
} DuongCat;

void duong_cat_khoi_tao(DuongCat *d);
void duong_cat_giai_phong(DuongCat *d);
int  duong_cat_them(DuongCat *d, double x, double a);   /* 0 = OK, -1 = het bo nho */
void duong_cat_pham_vi_x(const DuongCat *d, double *x_min, double *x_max);

/* Ghi thong bao loi (an toan khi loi == NULL) */
void dat_loi(char *loi, const char *chu, ...);

/* In so gon: 10.0 -> "10", 10.500 -> "10.5" */
void so_gon(char *ra, size_t co_ra, double gia_tri);

#endif
