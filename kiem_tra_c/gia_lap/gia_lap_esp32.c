/* GIA LAP ESP32 - chay DUNG code firmware that (main/main.c), doc lenh tu
 * stdin, tra loi ra stdout.
 *
 * Task dong co chay o LUONG RIENG (giong nhan 1 that tren ESP32) de kiem chung
 * vong dem SPSC va co che nap dan. Cac ham cua ESP-IDF duoc thay bang ban gia
 * o duoi day va cac tep trong stub/ - vua du de firmware bien dich va chay
 * duoc tren may tinh thuong.
 *
 * kiem_tra_c/test_ket_noi.c noi lop ket_noi cua phan mem PC vao day qua mot
 * cap cong ao (pty), nho vay ca duong "may tinh <-> ESP32" duoc chay that.
 *
 * Tham so dong lenh: he so chay nhanh hon thuc te (mac dinh 1).
 * Lenh rieng cua ban gia lap: QUIT (thoat), TONGKET (in tong so xung da xuat). */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

static unsigned long tong_xung_da_xuat = 0;
#include "main.c"

void esp_rom_delay_us(unsigned u){ (void)u; }
void esp_restart(void){}
void vTaskDelay(unsigned u){ usleep(u * 1000u); }
int xTaskCreatePinnedToCore(void(*f)(void*),const char*n,int s,void*p,int pr,void**h,int c){
    (void)f;(void)n;(void)s;(void)p;(void)pr;(void)h;(void)c;return 1;}
esp_err_t gpio_config(const gpio_config_t*c){(void)c;return 0;}
static int muc_relay = 0;
void gpio_set_level(int chan,int muc){ if(chan==g_cfg.pin_relay_plasma) muc_relay=muc; }
int gpio_get_level(int a){(void)a;return 1;}
int gpio_install_isr_service(int f){(void)f;return 0;}
int gpio_isr_handler_add(int a,void(*f)(void*),void*b){(void)a;(void)f;(void)b;return 0;}
int uart_param_config(int a,const uart_config_t*b){(void)a;(void)b;return 0;}
int uart_driver_install(int a,int b,int c,int d,void*e,int f){
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 0;}
int uart_read_bytes(int a,void*b,int c,unsigned d){(void)a;(void)b;(void)c;(void)d;return 0;}
int uart_set_baudrate(int a,unsigned b){(void)a;(void)b;return 0;}
int uart_wait_tx_done(int a,unsigned b){(void)a;(void)b;return 0;}
int uart_flush_input(int a){(void)a;return 0;}
unsigned xTaskGetTickCount(void){return 0;}
int nvs_flash_init(void){return 0;}
int nvs_flash_erase(void){return 0;}
esp_err_t nvs_open(const char*n,int m,nvs_handle_t*h){(void)n;(void)m;(void)h;return -1;}
void nvs_close(nvs_handle_t h){(void)h;}
esp_err_t nvs_get_i32(nvs_handle_t h,const char*k,int32_t*v){(void)h;(void)k;(void)v;return -1;}
esp_err_t nvs_set_i32(nvs_handle_t h,const char*k,int32_t v){(void)h;(void)k;(void)v;return 0;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char*k,uint8_t*v){(void)h;(void)k;(void)v;return -1;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char*k,uint8_t v){(void)h;(void)k;(void)v;return 0;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char*k,void*v,size_t*s){(void)h;(void)k;(void)v;(void)s;return -1;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char*k,const void*v,size_t s){(void)h;(void)k;(void)v;(void)s;return 0;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return 0;}
esp_err_t nvs_erase_all(nvs_handle_t h){(void)h;return 0;}

static volatile int con_song = 1;
static volatile long so_buoc_da_chay = 0;
static unsigned he_so_nhanh = 1;   /* chay nhanh gap may lan cho test do nhanh */

/* Nhan 1: mo phong task dong co, an theo dung vong dem that.
   Moi buoc "chay" mat 1ms de giong may that dang keo dong co. */
static void *nhan_1(void *p)
{
    (void)p;
    lenh_dong_co_t lenh;
    int cho_nap_ms = 0;
    while (con_song) {
        if (!vong_dem_lay(&lenh)) {
            if (dang_chay_chuong_trinh) {
                if (het_chuong_trinh) {
                    dang_chay_chuong_trinh = false;
                    printf("XONG_CHUONG_TRINH: da chay het chuong trinh. Vi tri: X=%.2f A=%.2f\n",
                           doc_vi_tri_keo_mm(), doc_vi_tri_xoay_do());
                    fflush(stdout);
                } else if (trang_thai_chay == CHAY_BINH_THUONG) {
                    if (plasma_dang_bat) {
                        dat_plasma(0);
                        trang_thai_chay = DANG_TAM_DUNG;
                        printf("LOI_CAN_BO_DEM: may tinh nap khong kip, da TAT MO CAT.\n");
                        fflush(stdout);
                    } else {
                        cho_nap_ms += 20;
                        if (cho_nap_ms >= THOI_GIAN_CHO_NAP_MS) {
                            trang_thai_chay = DANG_TAM_DUNG;
                            printf("LOI_CAN_BO_DEM: may tinh khong nap tiep.\n");
                            fflush(stdout);
                            cho_nap_ms = 0;
                        }
                    }
                }
            }
            usleep(20000);
            continue;
        }
        cho_nap_ms = 0;
        /* Giong het task_dong_co that: EMG hoac da STOP thi BO buoc nay */
        if (co_dung_khan_cap || trang_thai_chay == YEU_CAU_DUNG_HAN) continue;
        while (trang_thai_chay == DANG_TAM_DUNG && !co_dung_khan_cap) usleep(20000);
        so_buoc_da_chay++;
        if (lenh.loai == LENH_PLASMA_ON)  dat_plasma(1);
        if (lenh.loai == LENH_PLASMA_OFF) dat_plasma(0);
        if (lenh.loai == LENH_DI_CHUYEN) {
            uint32_t troi = lenh.so_buoc_x > lenh.so_buoc_a ? lenh.so_buoc_x : lenh.so_buoc_a;
            int32_t bx = troi/2, ba = troi/2;
            for (uint32_t i = 0; i < troi; i++) {
                if (lenh.so_buoc_x){bx+=(int32_t)lenh.so_buoc_x; if(bx>=(int32_t)troi){bx-=(int32_t)troi;tong_xung_keo += lenh.huong_x?1:-1;}}
                if (lenh.so_buoc_a){ba+=(int32_t)lenh.so_buoc_a; if(ba>=(int32_t)troi){ba-=(int32_t)troi;tong_xung_xoay += lenh.huong_a?1:-1;}}
            }
            /* Thoi gian THAT su cua doan nay tren may: so xung x chu ky */
            unsigned long us = (unsigned long)troi * lenh.chu_ky_us;
            if (us > 2000000UL) us = 2000000UL;
            usleep(us / he_so_nhanh);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc > 1) he_so_nhanh = (unsigned)atoi(argv[1]);
    if (he_so_nhanh < 1) he_so_nhanh = 1;
    setvbuf(stdout, NULL, _IOLBF, 0);
    cau_hinh_dat_mac_dinh();
    che_do_tuyet_doi = true; feed_dang_nap = -1; plasma_mo_phong = false;
    g_di_chuyen_modal = -1; he_so_don_vi = 1.0;

    pthread_t t;
    pthread_create(&t, NULL, nhan_1, NULL);

    char dong[256];
    while (fgets(dong, sizeof dong, stdin)) {
        if (strncmp(dong, "QUIT", 4) == 0) break;
        if (strncmp(dong, "TONGKET", 7) == 0) {
            /* cho dong co chay het */
            while (vong_dem_dang_co() > 0) usleep(5000);
            printf("TONGKET;%ld;%ld;%d\n", tong_xung_keo, so_buoc_da_chay, muc_relay);
            fflush(stdout);
            continue;
        }
        xu_ly_lenh_tu_pc(dong);
        fflush(stdout);
    }
    con_song = 0;
    pthread_join(t, NULL);
    return 0;
}
