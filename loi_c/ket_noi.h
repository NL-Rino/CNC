/* KET NOI ESP32 qua USB COM - mo cong, thuong luong toc do, nap dan chuong trinh.
 *
 * KHONG dinh gi toi giao dien: moi thu gui ve bang cac ham goi lai trong
 * HamGoiLai, giao dien tu bo vao hang doi cua no. Nho vay module nay TEST
 * duoc bang chuong trinh dong lenh, khong can mo cua so.
 *
 * CANH BAO: cac ham goi lai chay tren LUONG NEN (luong doc cong / luong nap),
 * khong phai luong giao dien. Ben Win32 phai PostMessage ve cua so chinh chu
 * khong duoc ve gi truc tiep trong do.
 *
 * Xung dong co la viec cua ESP32, may tinh khong dem va cung khong hoi. May
 * tinh chi biet VI TRI (mm / do) do ESP32 bao len.
 */
#ifndef KET_NOI_H
#define KET_NOI_H

#include "cong_com.h"
#include "nen_tang.h"

/* ESP32 LUON khoi dong o 115200 - toc do nao cung mo duoc, khong bao gio chet cong */
#define BAUD_KHOI_DONG 115200

/* Thu nang dan tu cao xuong thap. Cho gioi han la chip USB-UART tren board
 * (CP2102 / CH340), khong phai ESP32, nen phai THU chu khong dat cung. */
#define SO_BAUD_THU 6
extern const int BAUD_THU_DAN[SO_BAUD_THU];

/* Gui truoc bay nhieu dong roi bam CHAY ngay, vua chay vua nap tiep.
 * Khop voi BUOC_DAY_TRUOC_KHI_CHAY trong firmware. */
#define SO_DONG_NAP_TRUOC 150
#define NGUONG_GUI_TIEP   20      /* chi gui tiep khi ESP32 con it nhat bay nhieu o trong */
#define LO_GUI_TOI_DA     250     /* so dong toi da gui lien mot mach */
#define CHO_TOI_DA_S      60.0    /* ESP32 khong voi bo dem qua lau -> coi nhu may da dung */

#define CO_DONG_NHAN 256          /* mot dong ESP32 gui len dai toi da bay nhieu */

/* Cac ham goi lai. Bo trong (NULL) ham nao khong quan tam. */
typedef struct {
    void *ctx;
    void (*dong_esp32)(void *ctx, const char *dong);   /* mot dong ESP32 gui len */
    void (*nhat_ky)(void *ctx, const char *chu);       /* thong bao cua chinh phan mem */
    void (*loi_nap)(void *ctx, const char *chu);       /* nap that bai, kem ly do */
    void (*vi_tri)(void *ctx, double x_mm, double a_do);
    void (*baud)(void *ctx, int baud);                 /* da chot toc do duong truyen */
} HamGoiLai;

typedef struct KetNoi KetNoi;

/* Tao doi tuong ket noi (chua mo cong). */
KetNoi *ket_noi_tao(const HamGoiLai *goi_lai);
void    ket_noi_giai_phong(KetNoi *k);

/* Mo cong va bat dau luong doc. baud_chon = 0 nghia la thu nang dan;
 * dat baud_chon = BAUD_KHOI_DONG de giu nguyen 115200 (on dinh nhat).
 * Tra 0 = xong, -1 = loi (ly do ghi vao loi). */
int  ket_noi_mo(KetNoi *k, const char *ten_cong, int baud_chon, char *loi);
void ket_noi_dong(KetNoi *k);
int  ket_noi_dang_mo(const KetNoi *k);
int  ket_noi_baud_dang_dung(const KetNoi *k);

/* Gui mot lenh (tu them ky tu xuong dong). Tra 1 = da gui. */
int ket_noi_gui(KetNoi *k, const char *lenh);

/* ------------------------------------------------------------------ NAP DAN
 * cac_dong phai la ban DA NEN san (xem nen_dong_gui trong phan_tich_gcode).
 * Ham tra ve NGAY, viec nap chay o luong nen. Ket qua bao qua nhat_ky/loi_nap.
 * Chuoi duoc sao chep vao trong nen ben goi khong can giu lai. */
int  ket_noi_nap_va_chay(KetNoi *k, const char *const *cac_dong, int so_dong);
void ket_noi_huy_nap(KetNoi *k);
int  ket_noi_dang_nap(const KetNoi *k);

/* So dong ESP32 da bao nhan va so o trong con lai - de ve thanh tien trinh. */
int ket_noi_so_dong_da_nhan(const KetNoi *k);
int ket_noi_cho_trong(const KetNoi *k);

/* Doc "... X=12.34 A=56.78 ..." -> 0 = doc duoc, -1 = dong nay khong co vi tri.
 * Tach rieng de test duoc. */
int ket_noi_doc_vi_tri(const char *dong, double *x, double *a);

#endif /* KET_NOI_H */
