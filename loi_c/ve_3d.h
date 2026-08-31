/* MO PHONG 3D - ve ong va duong cat tren mat ong.
 *
 * Module nay KHONG goi mot ham do hoa nao. No chi tinh ra mot DANH SACH HINH
 * (da giac, doan thang, chu) da sap xep san theo do sau; ben giao dien cu the
 * (GDI tren Windows) chi viec ve lai danh sach do. Nho vay toan bo phep chieu
 * va thuat toan "tho son" deu kiem tra duoc bang chuong trinh dong lenh.
 *
 * Cach lam:
 *   - dung mo hinh bang cac mat tu giac nho
 *   - sap xep theo do sau roi ve tu xa den gan (thuat toan "tho son")
 *   - mat quay ra sau bi mat truoc de len tren => trong nhu khoi dac
 *
 * DUNG NHU MAY THAT: dau cat DUNG YEN, ONG quay quanh truc va truot ra vao.
 */
#ifndef VE_3D_H
#define VE_3D_H

#include "phan_tich_gcode.h"     /* DoanDi */
#include "hinh_ve.h"

/* Goc nhin va do phong cua nguoi xem. */
typedef struct {
    double xoay_ngang;          /* do */
    double xoay_doc;            /* do */
    double phong;
    double day_ngang;           /* tinh tien khi keo chuot giua */
    double day_doc;
} CanhNhin;

void canh_nhin_dat_lai(CanhNhin *c);

typedef struct MoPhong3D MoPhong3D;

MoPhong3D *mp3d_tao(void);
void       mp3d_giai_phong(MoPhong3D *m);

CanhNhin  *mp3d_canh_nhin(MoPhong3D *m);

/* chieu_dai <= 0 nghia la tu tinh theo vung co duong cat. */
void mp3d_dat_du_lieu(MoPhong3D *m, const DoanDi *doan, int so_doan,
                      double duong_kinh, double chieu_dai);

/* -1 = chua chay (ve toan bo duong cat mau do). */
void mp3d_dat_vi_tri_chay(MoPhong3D *m, int chi_so);
int  mp3d_vi_tri_chay(const MoPhong3D *m);

double mp3d_duong_kinh(const MoPhong3D *m);
double mp3d_chieu_dai_ong(const MoPhong3D *m);
int    mp3d_so_doan(const MoPhong3D *m);

/* --- An bot duong cat cho de nhin --- */
int  mp3d_so_nhom(const MoPhong3D *m);
int  mp3d_nhom_cua_doan(const MoPhong3D *m, int chi_so_doan);
void mp3d_bat_tat_nhom(MoPhong3D *m, int nhom);
void mp3d_hien_lai_het(MoPhong3D *m);
int  mp3d_so_nhom_bi_an(const MoPhong3D *m);

/* Dung mot khung hinh. Tra 0 = xong. Goi khung_ve_giai_phong sau khi ve. */
int mp3d_dung_hinh(MoPhong3D *m, int rong, int cao, KhungVe *ra);

/* Tim doan cat gan diem bam nhat (dung sau khi da dung hinh).
 * Tra -1 neu khong co doan nao du gan. */
int mp3d_doan_gan_diem(const MoPhong3D *m, double x_man, double y_man,
                       double ban_kinh);

/* Trang thai ong: truot ra bao nhieu mm va da quay bao nhieu do. */
void mp3d_trang_thai_ong(const MoPhong3D *m, double *x_ong, double *goc_ong);

#endif /* VE_3D_H */
