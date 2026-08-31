/* PHAN TICH G-CODE - chay tren may tinh, TRUOC khi gui xuong ESP32.
 *
 * Doc truoc ca chuong trinh de:
 *   - dung lai duong di (ve hinh xem truoc + mo phong 3D)
 *   - bao truoc nhung dong firmware se tu choi, KHONG de may dung giua duong cat
 *   - chuan hoa ve dang firmware chac chan hieu
 *   - nen lai truoc khi day xuong day COM
 */
#ifndef PHAN_TICH_GCODE_H
#define PHAN_TICH_GCODE_H

#include "loi_chung.h"

/* Do sau vong dem cua firmware (#define SUC_CHUA_BUOC). Chi de bao cho nguoi
 * dung biet, KHONG con la gioi han do dai chuong trinh - may tinh nap dan. */
#define SUC_CHUA_BO_DEM 1200

#define CO_DONG_G   96
#define SO_CANH_BAO_TOI_DA 60

/* Mot doan duong di: tu (x1,a1) toi (x2,a2). la_cat = dang bat mo plasma. */
typedef struct {
    double x1, a1, x2, a2;
    int la_cat;
} DoanDi;

typedef struct {
    DoanDi *doan;
    int so_doan, suc_chua_doan;

    char (*dong_chuan_hoa)[CO_DONG_G];
    int so_dong, suc_chua_dong;

    char canh_bao[SO_CANH_BAO_TOI_DA][CO_LOI];
    int so_canh_bao;

    int so_buoc_firmware;
    int co_loi_nang;
} KetQuaPhanTich;

void kq_phan_tich_khoi_tao(KetQuaPhanTich *kq);
void kq_phan_tich_giai_phong(KetQuaPhanTich *kq);

/* che_do: 1/2/3. duong_kinh chi dung o che do 3 (truc A trong file la mm cung). */
int phan_tich_chuong_trinh(const char *const *cac_dong, int so_dong,
                           double toc_do_cat, double toc_do_nhanh,
                           int che_do, double duong_kinh,
                           KetQuaPhanTich *ra);

/* Nen mot dong G-code truoc khi day xuong day COM (KHONG doi y nghia):
 * bo comment, bo dau cach, bo so 0 thua. Tra ve do dai chuoi ket qua. */
int nen_dong_gui(const char *dong, char *ra, size_t co_ra);

#endif
