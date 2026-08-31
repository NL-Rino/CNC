/* TIEN ICH GIAO DIEN WINDOWS - phong chu, mau, tao o dieu khien, hop thoai.
 *
 * Gom cac viec lap di lap lai khi dung Win32 tran (khong thu vien ngoai) de
 * hai chuong trinh may_cat_ong va cnc_settings dung chung.
 */
#ifndef TIEN_ICH_H
#define TIEN_ICH_H

#include <windows.h>

/* Mau dung chung, giong bang MAU cua ban Python cu */
#define MAU_NEN      RGB(0xee, 0xf1, 0xf5)
#define MAU_KHUNG    RGB(0xff, 0xff, 0xff)
#define MAU_VIEN     RGB(0xc3, 0xca, 0xd4)
#define MAU_CHU      RGB(0x1d, 0x25, 0x30)
#define MAU_CHU_MO   RGB(0x6b, 0x76, 0x86)
#define MAU_NHAN     RGB(0x2f, 0x6f, 0xb8)
#define MAU_CHAY     RGB(0x2f, 0x9e, 0x44)
#define MAU_DUNG     RGB(0xc9, 0x2a, 0x2a)
#define MAU_CHO      RGB(0xe8, 0x89, 0x0c)
#define MAU_TERM_NEN RGB(0x12, 0x16, 0x1b)
#define MAU_TERM_CHU RGB(0xc8, 0xd3, 0xde)
#define MAU_NUT      RGB(0xdf, 0xe4, 0xea)

/* Phong chu dung chung - goi tien_ich_khoi_tao mot lan khi chuong trinh chay */
extern HFONT PC_THUONG, PC_DAM, PC_NHO, PC_DEU, PC_DEU_TO, PC_TO_DAM;

void tien_ich_khoi_tao(void);
void tien_ich_don_dep(void);

/* --- Tao o dieu khien --- */
HWND tao_nhan(HWND cha, const char *chu, int id);
HWND tao_nut(HWND cha, const char *chu, int id);
HWND tao_o_nhap(HWND cha, const char *chu, int id);
HWND tao_hop_chon(HWND cha, int id);       /* ComboBox tha xuong */
HWND tao_khung(HWND cha, const char *tieu_de, int id);   /* GroupBox */
HWND tao_o_danh_dau(HWND cha, const char *chu, int id);  /* Checkbox */
HWND tao_o_tron(HWND cha, const char *chu, int id, int nhom_dau); /* Radio */

/* Dat vi tri va co cho mot o dieu khien. */
void dat_cho(HWND o, int x, int y, int rong, int cao);

/* --- Doc / ghi noi dung o nhap --- */
void   dat_chu(HWND o, const char *chu);
void   dat_chu_so(HWND o, double gt);      /* in gon: 15 chu khong phai 15.000000 */
int    lay_chu(HWND o, char *ra, int co_ra);
/* Doc so tu o nhap. Chap nhan ca dau phay thay dau cham.
 * Tra 0 = doc duoc, -1 = khong phai so. */
int    lay_so(HWND o, double *ra);
double lay_so_hoac(HWND o, double mac_dinh);

/* --- Hop thong bao --- */
void bao_loi(HWND cha, const char *tieu_de, const char *dinh_dang, ...);
void bao_tin(HWND cha, const char *tieu_de, const char *dinh_dang, ...);
void canh_bao(HWND cha, const char *tieu_de, const char *dinh_dang, ...);
int  hoi_co_khong(HWND cha, const char *tieu_de, const char *dinh_dang, ...);

/* --- Hop chon file --- */
int chon_file_mo(HWND cha, char *ra, int co_ra, const char *bo_loc,
                 const char *tieu_de);
int chon_file_luu(HWND cha, char *ra, int co_ra, const char *bo_loc,
                  const char *tieu_de, const char *duoi_mac_dinh);

/* --- Ve --- */
void ve_chu(HDC hdc, int x, int y, const char *chu, HFONT pc, COLORREF mau);
void ve_chu_phai(HDC hdc, int x_phai, int y, const char *chu, HFONT pc, COLORREF mau);

#endif /* TIEN_ICH_H */
