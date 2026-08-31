# Dung phan mem may cat ong (ban viet bang C).
#
#   make            - dung 2 file .exe cho Windows (can mingw-w64)
#   make kiem-tra   - dung va chay toan bo bai kiem tra tren may Linux
#   make sach       - xoa het file da dung
#
# Khong can thu vien ngoai nao: chi Win32 + GDI co san trong Windows.

CC       ?= gcc
MINGW    ?= x86_64-w64-mingw32-gcc
CANH_BAO  = -Wall -Wextra
TOI_UU    = -O2

LOI      = loi_c/loi_chung.c loi_c/thu_vien_moi_noi.c loi_c/phan_tich_gcode.c \
           loi_c/nen_tang.c loi_c/cong_com.c loi_c/ket_noi.c \
           loi_c/hinh_ve.c loi_c/ve_3d.c loi_c/xep_2d.c
GIAO_DIEN = win/tien_ich.c win/gdi_ve.c
THU_VIEN_WIN = -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -ladvapi32

RA = ra

.PHONY: tat_ca kiem-tra sach

tat_ca: $(RA)/MayCatOng.exe $(RA)/MayCatOng_CaiDat.exe

$(RA):
	mkdir -p $(RA)

$(RA)/MayCatOng.exe: win/may_cat_ong.c $(GIAO_DIEN) $(LOI) | $(RA)
	$(MINGW) $(TOI_UU) $(CANH_BAO) -o $@ $^ -mwindows $(THU_VIEN_WIN)

$(RA)/MayCatOng_CaiDat.exe: win/cnc_settings.c win/tien_ich.c \
		loi_c/loi_chung.c loi_c/nen_tang.c loi_c/cong_com.c loi_c/ket_noi.c | $(RA)
	$(MINGW) $(TOI_UU) $(CANH_BAO) -o $@ $^ -mwindows $(THU_VIEN_WIN)

# Gia lap ESP32: bien dich chinh firmware that de test_ket_noi noi vao duoc
$(RA)/gia_lap_esp32: kiem_tra_c/gia_lap/gia_lap_esp32.c main/main.c | $(RA)
	$(CC) $(TOI_UU) -o $@ kiem_tra_c/gia_lap/gia_lap_esp32.c \
		-Ikiem_tra_c/gia_lap/stub -Imain -lpthread -lm

# --- Kiem tra: chay tren chinh may dang lam viec, khong can Windows ---
kiem-tra: $(RA)/gia_lap_esp32 | $(RA)
	$(CC) $(TOI_UU) $(CANH_BAO) -o $(RA)/test_thu_vien \
		kiem_tra_c/test_thu_vien.c loi_c/thu_vien_moi_noi.c loi_c/loi_chung.c -lm
	$(CC) $(TOI_UU) $(CANH_BAO) -o $(RA)/test_xep \
		kiem_tra_c/test_xep.c loi_c/thu_vien_moi_noi.c loi_c/xep_2d.c \
		loi_c/hinh_ve.c loi_c/loi_chung.c -lm
	$(CC) $(TOI_UU) $(CANH_BAO) -o $(RA)/test_ve \
		kiem_tra_c/test_ve.c $(LOI) -lm -lpthread
	$(CC) $(TOI_UU) $(CANH_BAO) -o $(RA)/test_ket_noi \
		kiem_tra_c/test_ket_noi.c $(LOI) -lm -lpthread
	$(RA)/test_thu_vien
	$(RA)/test_xep
	$(RA)/test_ve
	$(RA)/test_ket_noi $(RA)/gia_lap_esp32

sach:
	rm -rf $(RA)
