/* KET NOI FLUIDNC qua USB COM - nap chuong trinh, dieu khien, doc vi tri.
 *
 * May chay firmware FluidNC (giao thuc GRBL). So voi firmware tu viet truoc
 * day, thay doi lon nhat la:
 *   - FluidNC tra "ok" cho TUNG DONG nhan duoc. May tinh dem so BYTE dang bay
 *     tren duong day de khong bao gio lam tran bo dem cua no (cach nay goi la
 *     "dem ky tu", chuan cua moi bo gui G-code cho GRBL).
 *   - Vi tri va trang thai lay bang cach hoi "?" dinh ky, tra ve mot dong
 *     dang <Idle|MPos:12.34,0.000,0.000,56.78|FS:0,0>
 *   - Tam dung / chay tiep / dung han la KY TU THOI GIAN THUC, chen thang vao
 *     duong day chu khong xep hang - nen an lien du bo dem con day du lieu.
 *
 * TRUC A TINH BANG MM CUNG, khong phai do. FluidNC coi moi truc la truc thang
 * khi tinh toc do, nen de hai truc cung don vi mm thi lenh F moi dung nghia
 * "toc do mo cat luot tren mat ong". Lop nay tu quy doi do <-> mm cung theo
 * duong kinh ong, ben ngoai van lam viec bang DO nhu cu.
 *
 * KHONG dinh gi toi giao dien: moi thu gui ve bang cac ham goi lai trong
 * HamGoiLai, giao dien tu bo vao hang doi cua no. Nho vay module nay TEST
 * duoc bang chuong trinh dong lenh, khong can mo cua so.
 *
 * CANH BAO: cac ham goi lai chay tren LUONG NEN (luong doc cong / luong nap),
 * khong phai luong giao dien. Ben Win32 phai PostMessage ve cua so chinh chu
 * khong duoc ve gi truc tiep trong do.
 */
#ifndef KET_NOI_H
#define KET_NOI_H

#include "cong_com.h"
#include "nen_tang.h"

/* FluidNC luon chay 115200 tren cong USB. */
#define BAUD_FLUIDNC 115200

/* So byte toi da duoc phep "dang bay" tren duong day ma chua co "ok" tra ve.
 * 127 la con so an toan voi moi ban GRBL va FluidNC. */
#define CO_DEM_NHAN_FLUIDNC 127

#define CO_DONG_NHAN 256          /* mot dong FluidNC gui len dai toi da bay nhieu */
#define NHIP_HOI_TRANG_THAI_MS 200 /* bao lau hoi "?" mot lan */

/* Ky tu thoi gian thuc cua GRBL/FluidNC */
#define RT_TRANG_THAI   '?'
#define RT_CHAY_TIEP    '~'
#define RT_TAM_DUNG     '!'
#define RT_DUNG_HAN     0x18      /* Ctrl-X, khoi dong lai bo dieu khien */
#define RT_HUY_JOG      0x85
#define RT_TAT_BAT_MO   0x9E      /* bat/tat mo cat trong luc dang tam dung */

typedef enum {
    MAY_KHONG_RO = 0, MAY_IDLE, MAY_RUN, MAY_HOLD, MAY_JOG,
    MAY_HOME, MAY_ALARM, MAY_DOOR, MAY_CHECK, MAY_SLEEP
} TrangThaiMay;

const char *ten_trang_thai_may(TrangThaiMay tt);

/* Cac ham goi lai. Bo trong (NULL) ham nao khong quan tam. */
typedef struct {
    void *ctx;
    void (*dong_may)(void *ctx, const char *dong);    /* mot dong FluidNC gui len */
    void (*nhat_ky)(void *ctx, const char *chu);      /* thong bao cua chinh phan mem */
    void (*loi_nap)(void *ctx, const char *chu);      /* nap that bai, kem ly do */
    void (*vi_tri)(void *ctx, double x_mm, double a_do);
    void (*trang_thai)(void *ctx, TrangThaiMay tt);
} HamGoiLai;

typedef struct KetNoi KetNoi;

KetNoi *ket_noi_tao(const HamGoiLai *goi_lai);
void    ket_noi_giai_phong(KetNoi *k);

/* Mo cong va bat dau luong doc. Tra 0 = xong, -1 = loi (ly do ghi vao loi). */
int  ket_noi_mo(KetNoi *k, const char *ten_cong, char *loi);
void ket_noi_dong(KetNoi *k);
int  ket_noi_dang_mo(const KetNoi *k);

/* Gui mot dong lenh (tu them ky tu xuong dong). Dung cho o go lenh tay.
 * Tra 1 = da gui. */
int ket_noi_gui(KetNoi *k, const char *lenh);

/* Chen mot ky tu thoi gian thuc - an lien, khong xep hang. */
int ket_noi_gui_thoi_gian_thuc(KetNoi *k, unsigned char ma);

/* --------------------------------------------------------------- DUONG KINH
 * Doi duong kinh ong: gui lai so xung tren mot mm cung cho truc A.
 *   steps_per_mm_A = so_xung_moi_vong / (pi * duong_kinh)
 * Lenh nay chi doi cau hinh dang chay cua FluidNC, KHONG ghi vao flash. */
int  ket_noi_dat_duong_kinh(KetNoi *k, double duong_kinh_mm, double xung_moi_vong_a);
double ket_noi_duong_kinh(const KetNoi *k);

/* ------------------------------------------------------------------ NAP BAI
 * cac_dong la ban DA NEN san (xem nen_dong_gui trong phan_tich_gcode), truc A
 * da o don vi MM CUNG. Ham tra ve NGAY, viec nap chay o luong nen. */
int  ket_noi_nap_va_chay(KetNoi *k, const char *const *cac_dong, int so_dong);
void ket_noi_huy_nap(KetNoi *k);
int  ket_noi_dang_nap(const KetNoi *k);
int  ket_noi_so_dong_da_nhan(const KetNoi *k);   /* de ve thanh tien trinh */
int  ket_noi_so_dong_ca_bai(const KetNoi *k);

/* --------------------------------------------------------------- DIEU KHIEN */
void ket_noi_tam_dung(KetNoi *k);
/* thoi_gian_duc_lo_ms > 0: bat lai mo cat, cho duc xuyen qua thanh ong roi moi
 * chay tiep. = 0: chay tiep ngay. */
void ket_noi_chay_tiep(KetNoi *k, int thoi_gian_duc_lo_ms);
void ket_noi_dung_han(KetNoi *k);
void ket_noi_mo_khoa(KetNoi *k);                 /* $X - go trang thai bao dong */
void ket_noi_ve_goc(KetNoi *k);                  /* $H - chay ve cong tac goc */
void ket_noi_dat_goc(KetNoi *k);                 /* lay cho dang dung lam goc 0 */
/* truc = 'X' hoac 'A'. khoang tinh bang mm (truc X) hoac DO (truc A). */
void ket_noi_jog(KetNoi *k, char truc, double khoang, double toc_do);
void ket_noi_huy_jog(KetNoi *k);

TrangThaiMay ket_noi_trang_thai(const KetNoi *k);
void ket_noi_vi_tri(const KetNoi *k, double *x_mm, double *a_do);

/* ------------------------------------------------------------------- TACH RA
 * De test duoc rieng. */
/* Doc dong trang thai "<Idle|MPos:1.5,0,0,2.5|FS:0,0>".
 * Tra 0 = doc duoc. a_mm la MM CUNG (chua doi ra do). */
int doc_dong_trang_thai(const char *dong, TrangThaiMay *tt,
                        double *x_mm, double *a_mm);
/* Doi ma loi cua FluidNC thanh cau tieng Viet. */
const char *giai_thich_loi(int ma);
const char *giai_thich_bao_dong(int ma);

#endif /* KET_NOI_H */
