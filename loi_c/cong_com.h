/* CONG COM - mo / doc / ghi / doi toc do mot cong noi tiep.
 *
 * Windows dung API Win32 (COM1..COM256), Linux dung termios (/dev/ttyUSB*).
 * Tat ca ham deu tra ve -1 khi loi va ghi ly do vao "loi" neu co truyen vao.
 */
#ifndef CONG_COM_H
#define CONG_COM_H

#include <stddef.h>

#define CO_TEN_CONG 64
#define SO_CONG_TOI_DA 64

typedef struct CongCom CongCom;

/* Liet ke cac cong dang co. Tra ve so cong tim duoc. */
int cong_liet_ke(char ten[][CO_TEN_CONG], int toi_da);

/* Mo cong. Tra NULL neu that bai (ly do ghi vao loi). */
CongCom *cong_mo(const char *ten, int baud, char *loi);

void cong_dong(CongCom *c);

/* Doi toc do truyen tren cong dang mo. 0 = xong, -1 = khong doi duoc. */
int cong_dat_baud(CongCom *c, int baud);

/* Ghi n byte. Tra so byte da ghi, -1 neu loi. */
int cong_ghi(CongCom *c, const char *du_lieu, int n);

/* Doc toi da co_ra byte, cho nhieu nhat khoang 50 ms.
 * Tra so byte doc duoc (0 khi het gio), -1 neu loi/cong dut. */
int cong_doc(CongCom *c, char *ra, int co_ra);

/* Vut bo du lieu dang nam trong bo dem nhan. */
void cong_xoa_dem_vao(CongCom *c);

/* Cong con mo hay khong. */
int cong_dang_mo(const CongCom *c);

#endif /* CONG_COM_H */
