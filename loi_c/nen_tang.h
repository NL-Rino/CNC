/* NEN TANG HE DIEU HANH - thoi gian, ngu, luong nen, khoa.
 *
 * Chi mot lop mong bao quanh Win32 va POSIX de phan con lai cua phan mem
 * viet mot lan chay duoc ca hai noi. Khong co gi lien quan toi may cat.
 */
#ifndef NEN_TANG_H
#define NEN_TANG_H

/* Giay tinh tu mot moc bat ky, dung de do khoang cach thoi gian. */
double gio_giay(void);

/* Ngu bay nhieu mili giay. */
void ngu_ms(int ms);

/* ------------------------------------------------------------------ LUONG */
typedef struct Luong Luong;

/* Tao mot luong nen chay ham(du_lieu). Tra NULL neu that bai.
 * Luong tu giai phong khi ham chay xong - khong can join. */
Luong *luong_chay(void (*ham)(void *), void *du_lieu);

/* ------------------------------------------------------------------- KHOA */
typedef struct Khoa Khoa;

Khoa *khoa_tao(void);
void  khoa_giai_phong(Khoa *k);
void  khoa_vao(Khoa *k);
void  khoa_ra(Khoa *k);

#endif /* NEN_TANG_H */
