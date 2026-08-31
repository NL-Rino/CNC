/* VE DANH SACH HINH LEN CUA SO WINDOWS (GDI).
 *
 * Lop mong duy nhat noi giua phan tinh toan (loi_c/hinh_ve.h) va Windows.
 * Ve vao anh dem roi moi day len man hinh mot lan - khong bi nhay hinh.
 */
#ifndef GDI_VE_H
#define GDI_VE_H

#include <windows.h>
#include "../loi_c/hinh_ve.h"

/* Ve ca khung hinh len hdc trong vung rong x cao. */
void gdi_ve_khung(HDC hdc, int rong, int cao, const KhungVe *k);

/* Ve co dem: tu tao anh dem, ve vao do roi day len mot lan. */
void gdi_ve_khung_co_dem(HDC hdc, int rong, int cao, const KhungVe *k);

#endif /* GDI_VE_H */
