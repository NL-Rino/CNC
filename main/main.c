/*
 * DIEU KHIEN TRUNG TAM MAY CAT ONG - ESP32 DEVKIT GOC (ESP-IDF)
 * ---------------------------------------------------------
 * GIAI DOAN HIEN TAI: 1 devkit ESP32 (ban goc), dieu khien qua USB COM
 * bang G-CODE CHUAN (khong dung cu phap tu che nua).
 *
 * QUY UOC TRUC (chuan may cat ong - ISO 841):
 *   X = truc KEO ong doc theo chieu dai (2 motor A+B dong bo) -- don vi: MM
 *       Hieu chuan: 0.2 vong = 1mm  =>  1 vong = 5mm (MM_MOI_VONG_TRUC_X)
 *   A = truc XOAY ong quanh truc X                             -- don vi: DO
 *       (Y duoc chap nhan lam ALIAS cua A de tuong thich nguoc)
 *   F = toc do RPM cua DONG CO (LUU Y: G-code cong nghiep chuan F la
 *       mm/phut cho truc thang va do/phut cho truc xoay; o day van dung
 *       RPM dong co cho ca 2 truc - can doi neu ghep voi phan mem CAM that)
 *
 * VI TRI HIEN TAI DUOC FIRMWARE TU THEO DOI CHINH XAC (dem dung so xung
 * da xuat cho TUNG truc, ke ca khi bi STOP giua chung).
 *
 * NOI SUY 2 TRUC: dung thuat toan BRESENHAM giong GRBL/LinuxCNC. Khi 1
 * dong G-code co ca X va A, hai truc chay DONG THOI theo dung ty le
 * quang duong, do lech toi da so voi duong thang ly tuong chi 0.5
 * microstep. Toc do lay theo truc CHAM NHAT de khong truc nao vuot gioi han.
 *
 * QUY TRINH VAN HANH CHUAN:
 *   1. JOG dua mo cat toi vi tri bat dau cat mong muon
 *   2. ZERO - dat diem do lam goc (0,0)
 *   3. PROG;BEGIN ... PROG;END - nap chuong trinh G-code
 *   4. RUN - chay (chuong trinh tinh tu diem goc vua dat)
 *
 * TAP LENH G-CODE HO TRO:
 *   G90 / G91            toa do tuyet doi / tuong doi
 *   G92 X.. A..           dat lai toa do (dung duoc CA trong chuong trinh)
 *   G0/G1 X.. A.. F..     di chuyen - 2 TRUC CHAY DONG THOI (noi suy
 *                          Bresenham) => duong cat cheo lien mach, cat
 *                          duoc vat goc va yen ngua
 *   G2/G3 X.. A.. F..     XAP XI thanh duong thang toi diem cuoi (CANH BAO:
 *                          chua noi suy cung tron that su)
 *   G4 P..                nghi P giay (dung lam PIERCE DELAY sau khi bat mo)
 *   G28 / G30             ve goc 0 ca 2 truc
 *   G20 / G21             don vi inch/mm - chap nhan, bo qua
 *   G17/G18/G19, G40-G43, G49, G54-G59, G80, G93/G94, G98/G99
 *                         chap nhan, bo qua (tuong thich file CAM)
 *   M0 / M1               TAM DUNG cho nguoi van hanh (tu tat mo cat),
 *                          gui RESUME de chay tiep
 *   M3 / M4               BAT mo cat plasma (dung duoc trong chuong trinh)
 *   M5                    TAT mo cat plasma
 *   M2 / M30              ket thuc chuong trinh (TU DONG tat mo cat)
 *   M6, M7/M8/M9          chap nhan, bo qua
 *   ; hoac (...)          ghi chu
 *   N.., S.., T.., I/J/K/R   duoc doc nhung bo qua
 *
 * AN TOAN MO CAT PLASMA: relay mo cat duoc TU DONG TAT khi gap STOP, PAUSE,
 * M0/M1, M2/M30, hoac khi co tin hieu EMG/LIMIT tu PLC.
 *
 * QUAN TRONG VE AN TOAN:
 *   Day la lop bao ve MUC PHAN MEM. KHONG thay the relay an toan phan cung
 *   cat nguon dong luc truc tiep tren duong EMG that.
 *
 * SO DO CHAN (ESP32 DEVKIT GOC):
 *   PUL_KEO_A = GPIO4    DIR_KEO_A = GPIO13
 *   PUL_KEO_B = GPIO14   DIR_KEO_B = GPIO16
 *   PUL_XOAY  = GPIO25   DIR_XOAY  = GPIO26
 *   RELAY_PLASMA = GPIO19
 *   PLC_OUT_READY/RUNNING/DONE/FAULT = GPIO17/18/21/22
 *   PLC_IN_START/STOP/EMG/LIMIT = GPIO23/27/32/33
 *   (Khong dung chan ENA cho ca 3 driver - dong co luon giu phanh,
 *    khong bao gio nha phanh qua phan mem)
 *
 * LUU Y KHI BUILD: dung "idf.py set-target esp32"
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us

#define UART_PC UART_NUM_0

// ================== DINH NGHIA CHAN GPIO (ESP32 DEVKIT GOC) ==================
#define PUL_KEO_A       GPIO_NUM_4
#define DIR_KEO_A       GPIO_NUM_13
#define PUL_KEO_B       GPIO_NUM_14
#define DIR_KEO_B       GPIO_NUM_16
#define PUL_XOAY        GPIO_NUM_25
#define DIR_XOAY        GPIO_NUM_26

#define RELAY_PLASMA    GPIO_NUM_19

#define PLC_IN_START    GPIO_NUM_23
#define PLC_IN_STOP     GPIO_NUM_27
#define PLC_IN_EMG      GPIO_NUM_32
#define PLC_IN_LIMIT    GPIO_NUM_33

#define PLC_OUT_READY   GPIO_NUM_17
#define PLC_OUT_RUNNING GPIO_NUM_18
#define PLC_OUT_DONE    GPIO_NUM_21
#define PLC_OUT_FAULT   GPIO_NUM_22

#define MICROSTEP_MOI_VONG    1600.0
// Truc X (KEO): 0.2 vong = 1mm  =>  1 vong = 5mm
#define MM_MOI_VONG_TRUC_X    5.0
#define CHU_KY_TOI_THIEU_US   30
#define MAX_BUOC_CHUONG_TRINH 300
#define RPM_HOME_MAC_DINH     20.0

// ----- Tang/giam toc (acceleration ramp) -----
// Dong co buoc khoi dong dot ngot tu 0 len toc do cao se BI RUNG va de MAT
// BUOC. Giai phap: xuat xung cham o dau, tang dan len toc do dat (tang toc),
// va cham dan truoc khi dung (giam toc) - dang hinh thang, giong CNC that.
#define SO_BUOC_TANG_TOC      400    // so xung dung de tang/giam toc
#define HE_SO_CHAM_LUC_DAU    4      // luc bat dau chay cham gap N lan toc do dat

static const char *TAG = "MAY_CAT_ONG";

// ================== BIEN DUNG CHUNG GIUA CAC TASK ==================
static volatile bool co_dung_khan_cap = false;  // set boi ISR (PLC/EMG/LIMIT that)

// ----- Trang thai dieu khien chuong trinh tu PC -----
typedef enum {
    CHAY_BINH_THUONG = 0,
    YEU_CAU_TAM_DUNG = 1,   // PAUSE: chay het buoc hien tai roi dung, giu vi tri
    DANG_TAM_DUNG    = 2,   // da dung, cho RESUME
    YEU_CAU_DUNG_HAN = 3,   // STOP: dung ngay + xoa het buoc con lai
} trang_thai_chay_t;

static volatile trang_thai_chay_t trang_thai_chay = CHAY_BINH_THUONG;

static QueueHandle_t hang_doi_lenh_dong_co;

typedef enum {
    LENH_DI_CHUYEN  = 0, // di chuyen PHOI HOP dong thoi X va/hoac A
    LENH_DELAY      = 1, // G4 - nghi
    LENH_PLASMA_ON  = 2, // M3/M4 - bat mo cat
    LENH_PLASMA_OFF = 3, // M5 - tat mo cat
    LENH_TAM_DUNG   = 4, // M0/M1 - cho nguoi van hanh bam RESUME
    LENH_DAT_GOC    = 5, // G92 - dat lai toa do giua chuong trinh
} loai_lenh_t;

typedef struct {
    // ----- Di chuyen phoi hop (LENH_DI_CHUYEN) -----
    // Ca 2 truc chay DONG THOI bang thuat toan Bresenham. Neu chi di 1 truc
    // thi so_buoc cua truc con lai = 0.
    bool huong_x, huong_a;        // true = chieu duong (+)
    uint32_t so_buoc_x, so_buoc_a;
    uint32_t chu_ky_us;           // chu ky moi vong lap Bresenham

    uint32_t delay_ms;            // LENH_DELAY

    // LENH_DAT_GOC (G92)
    bool dat_x, dat_a;
    double gia_tri_x, gia_tri_a;

    loai_lenh_t loai;
} lenh_dong_co_t;

// Vi tri hien tai THUC TE, don vi do - firmware tu dem chinh xac,
// cap nhat ngay ca khi buoc bi ngat giua chung boi STOP
static double vi_tri_xoay_do = 0.0;
static double vi_tri_keo_mm  = 0.0;

// ================== BO NHO CHUONG TRINH (nap theo lo) ==================
static lenh_dong_co_t chuong_trinh[MAX_BUOC_CHUONG_TRINH];
static int so_buoc_da_nap = 0;
static bool dang_nap_chuong_trinh = false;
static bool co_loi_khi_nap = false;

// ----- Trang thai modal G-code khi dang nap 1 chuong trinh -----
static bool che_do_tuyet_doi;     // true = G90, false = G91
static double feed_dang_nap;      // F hien hanh, -1 = chua dat
static double vi_tri_mo_phong_x;  // vi tri gia lap trong luc phan tich (bat dau tu vi tri thuc)
static double vi_tri_mo_phong_y;

// ================== NGAT GPIO CHO STOP / EMG / LIMIT (PLC) ==================
static void IRAM_ATTR trinh_xu_ly_ngat_an_toan(void *arg)
{
    co_dung_khan_cap = true;
}

static void cau_hinh_ngo_vao(void)
{
    gpio_config_t cfg_an_toan = {
        .pin_bit_mask = (1ULL << PLC_IN_STOP) | (1ULL << PLC_IN_EMG) | (1ULL << PLC_IN_LIMIT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg_an_toan);

    gpio_config_t cfg_start = {
        .pin_bit_mask = (1ULL << PLC_IN_START),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg_start);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PLC_IN_STOP, trinh_xu_ly_ngat_an_toan, NULL);
    gpio_isr_handler_add(PLC_IN_EMG, trinh_xu_ly_ngat_an_toan, NULL);
    gpio_isr_handler_add(PLC_IN_LIMIT, trinh_xu_ly_ngat_an_toan, NULL);
}

static void cau_hinh_ngo_ra(void)
{
    gpio_config_t cfg_ra = {
        .pin_bit_mask =
            (1ULL << PUL_KEO_A) | (1ULL << DIR_KEO_A) |
            (1ULL << PUL_KEO_B) | (1ULL << DIR_KEO_B) |
            (1ULL << PUL_XOAY)  | (1ULL << DIR_XOAY)  |
            (1ULL << RELAY_PLASMA) |
            (1ULL << PLC_OUT_READY) | (1ULL << PLC_OUT_RUNNING) |
            (1ULL << PLC_OUT_DONE)  | (1ULL << PLC_OUT_FAULT),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg_ra);

    gpio_set_level(RELAY_PLASMA, 0);
    gpio_set_level(PLC_OUT_RUNNING, 0);
    gpio_set_level(PLC_OUT_DONE, 0);
    gpio_set_level(PLC_OUT_FAULT, 0);
}

// ================== TASK: DIEU KHIEN DONG CO ==================
// Di chuyen phoi hop 2 truc DONG THOI bang thuat toan BRESENHAM - day la
// cach cac bo dieu khien CNC that (GRBL, LinuxCNC) noi suy da truc:
//   - Truc co nhieu buoc hon lam truc "troi" (dominant), quyet dinh so vong lap
//   - Moi vong lap, bo dem cua tung truc cong them so buoc cua truc do; khi
//     bo dem vuot nguong thi truc do xuat 1 xung
//   => 2 truc chay song song muot, ty le quang duong luon dung => duong cat
//      la duong CHEO LIEN MACH, khong phai bac thang
static void task_dong_co(void *param)
{
    lenh_dong_co_t lenh;

    while (1) {
        if (xQueueReceive(hang_doi_lenh_dong_co, &lenh, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (co_dung_khan_cap) {
            printf("Loi: dang o trang thai dung khan cap, bo qua buoc.\n");
            continue;
        }
        if (trang_thai_chay == YEU_CAU_DUNG_HAN) {
            continue;
        }

        // ----- Cho o day neu dang tam dung (PAUSE giua cac buoc) -----
        while (trang_thai_chay == DANG_TAM_DUNG && !co_dung_khan_cap) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (co_dung_khan_cap || trang_thai_chay == YEU_CAU_DUNG_HAN) {
            continue;
        }

        // ----- Bat / tat mo cat plasma (M3/M4/M5) -----
        if (lenh.loai == LENH_PLASMA_ON) {
            gpio_set_level(RELAY_PLASMA, 1);
            printf("PLASMA_ON: da bat mo cat.\n");
            continue;
        }
        if (lenh.loai == LENH_PLASMA_OFF) {
            gpio_set_level(RELAY_PLASMA, 0);
            printf("PLASMA_OFF: da tat mo cat.\n");
            continue;
        }

        // ----- Dat lai toa do giua chuong trinh (G92) -----
        if (lenh.loai == LENH_DAT_GOC) {
            if (lenh.dat_x) vi_tri_keo_mm = lenh.gia_tri_x;
            if (lenh.dat_a) vi_tri_xoay_do = lenh.gia_tri_a;
            printf("G92: da dat lai toa do. Vi tri: X=%.2f A=%.2f\n", vi_tri_keo_mm, vi_tri_xoay_do);
            continue;
        }

        // ----- Tam dung cho nguoi van hanh (M0/M1) -----
        if (lenh.loai == LENH_TAM_DUNG) {
            gpio_set_level(RELAY_PLASMA, 0);  // AN TOAN: tat mo cat khi dung cho
            trang_thai_chay = DANG_TAM_DUNG;
            printf("M0_PAUSED: da tat mo cat, tam dung theo lenh M0/M1 tai X=%.2f A=%.2f. "
                   "Gui RESUME de chay tiep.\n", vi_tri_keo_mm, vi_tri_xoay_do);
            while (trang_thai_chay == DANG_TAM_DUNG && !co_dung_khan_cap) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            continue;
        }

        // ----- Buoc nghi (G4 dwell) -----
        if (lenh.loai == LENH_DELAY) {
            uint32_t con_lai_ms = lenh.delay_ms;
            while (con_lai_ms > 0) {
                if (co_dung_khan_cap || trang_thai_chay == YEU_CAU_DUNG_HAN) {
                    printf("Da nhan STOP - huy buoc nghi.\n");
                    break;
                }
                uint32_t buoc_ms = con_lai_ms > 50 ? 50 : con_lai_ms;
                vTaskDelay(pdMS_TO_TICKS(buoc_ms));
                con_lai_ms -= buoc_ms;
            }
            continue;
        }

        // ================== DI CHUYEN PHOI HOP 2 TRUC (Bresenham) ==================
        uint32_t buoc_x = lenh.so_buoc_x;
        uint32_t buoc_a = lenh.so_buoc_a;
        uint32_t troi = (buoc_x > buoc_a) ? buoc_x : buoc_a;  // truc dominant
        if (troi == 0) continue;

        // Dat chieu quay cho ca 2 truc truoc khi xuat xung
        if (buoc_x > 0) {
            gpio_set_level(DIR_KEO_A, lenh.huong_x);
            gpio_set_level(DIR_KEO_B, lenh.huong_x);
        }
        if (buoc_a > 0) {
            gpio_set_level(DIR_XOAY, lenh.huong_a);
        }
        esp_rom_delay_us(10);  // thoi gian setup DIR truoc PUL

        gpio_set_level(PLC_OUT_RUNNING, 1);
        gpio_set_level(PLC_OUT_DONE, 0);

        // Bo dem Bresenham - khoi tao giua nguong de phan bo xung deu hon
        int32_t bo_dem_x = (int32_t)(troi / 2);
        int32_t bo_dem_a = (int32_t)(troi / 2);
        uint32_t da_chay_x = 0, da_chay_a = 0;
        bool bi_dung_han = false;

        // ----- Chuan bi tham so tang/giam toc (ramp hinh thang) -----
        // Vung tang toc khong duoc vuot qua 1/2 quang duong (voi doan ngan)
        uint32_t vung_ramp = SO_BUOC_TANG_TOC;
        if (vung_ramp > troi / 2) vung_ramp = troi / 2;
        uint32_t chu_ky_dat = lenh.chu_ky_us;
        uint32_t chu_ky_dau = chu_ky_dat * HE_SO_CHAM_LUC_DAU;  // cham hon luc bat dau

        for (uint32_t i = 0; i < troi; i++) {
            // Chi STOP hoac EMG moi cat NGAY giua chung.
            // PAUSE khong cat o day - buoc hien tai chay het roi moi dung.
            if (co_dung_khan_cap || trang_thai_chay == YEU_CAU_DUNG_HAN) {
                bi_dung_han = true;
                break;
            }

            // --- Quyet dinh truc nao can xuat xung o vong lap nay ---
            bool xung_x = false, xung_a = false;

            if (buoc_x > 0) {
                bo_dem_x += (int32_t)buoc_x;
                if (bo_dem_x >= (int32_t)troi) {
                    bo_dem_x -= (int32_t)troi;
                    xung_x = true;
                }
            }
            if (buoc_a > 0) {
                bo_dem_a += (int32_t)buoc_a;
                if (bo_dem_a >= (int32_t)troi) {
                    bo_dem_a -= (int32_t)troi;
                    xung_a = true;
                }
            }

            // --- Xuat xung DONG THOI cho ca 2 truc ---
            if (xung_x) {
                gpio_set_level(PUL_KEO_A, 1);
                gpio_set_level(PUL_KEO_B, 1);
            }
            if (xung_a) {
                gpio_set_level(PUL_XOAY, 1);
            }

            esp_rom_delay_us(5);  // do rong xung HIGH

            if (xung_x) {
                gpio_set_level(PUL_KEO_A, 0);
                gpio_set_level(PUL_KEO_B, 0);
                da_chay_x++;
            }
            if (xung_a) {
                gpio_set_level(PUL_XOAY, 0);
                da_chay_a++;
            }

            // ----- Tinh chu ky xung cho vong lap nay theo ramp hinh thang -----
            uint32_t chu_ky_hien_tai = chu_ky_dat;
            if (vung_ramp > 0) {
                if (i < vung_ramp) {
                    // Giai doan TANG TOC: chu ky giam dan tu chu_ky_dau -> chu_ky_dat
                    uint32_t con_lai = vung_ramp - i;
                    chu_ky_hien_tai = chu_ky_dat +
                        (uint32_t)(((uint64_t)(chu_ky_dau - chu_ky_dat) * con_lai) / vung_ramp);
                } else if (i >= troi - vung_ramp) {
                    // Giai doan GIAM TOC: chu ky tang dan tu chu_ky_dat -> chu_ky_dau
                    uint32_t den_cuoi = troi - i;
                    chu_ky_hien_tai = chu_ky_dat +
                        (uint32_t)(((uint64_t)(chu_ky_dau - chu_ky_dat) * (vung_ramp - den_cuoi + 1)) / vung_ramp);
                }
            }

            uint32_t nghi = (chu_ky_hien_tai > 5) ? (chu_ky_hien_tai - 5) : 5;
            esp_rom_delay_us(nghi);
        }

        gpio_set_level(PLC_OUT_RUNNING, 0);

        // Cap nhat vi tri THUC TE theo dung so xung da xuat cho TUNG truc
        // (chinh xac ngay ca khi bi cat giua chung boi STOP)
        if (da_chay_x > 0) {
            double mm_da_di = ((double)da_chay_x / MICROSTEP_MOI_VONG) * MM_MOI_VONG_TRUC_X;
            vi_tri_keo_mm += lenh.huong_x ? mm_da_di : -mm_da_di;
        }
        if (da_chay_a > 0) {
            double do_da_quay = ((double)da_chay_a / MICROSTEP_MOI_VONG) * 360.0;
            vi_tri_xoay_do += lenh.huong_a ? do_da_quay : -do_da_quay;
        }

        if (bi_dung_han) {
            printf("Da dung han (STOP). Vi tri: X=%.2f A=%.2f\n", vi_tri_keo_mm, vi_tri_xoay_do);
            continue;
        }

        printf("Hoan thanh. Vi tri: X=%.2f A=%.2f\n", vi_tri_keo_mm, vi_tri_xoay_do);
        gpio_set_level(PLC_OUT_DONE, 1);

        // Neu co yeu cau PAUSE: buoc nay da chay XONG HOAN TOAN, vi tri da
        // cap nhat chinh xac -> gio moi chuyen sang tam dung
        if (trang_thai_chay == YEU_CAU_TAM_DUNG) {
            trang_thai_chay = DANG_TAM_DUNG;
            gpio_set_level(RELAY_PLASMA, 0);  // AN TOAN: tat mo cat khi dung
            printf("PAUSED: da tat mo cat, tam dung o vi tri X=%.2f A=%.2f. "
                   "Gui RESUME de chay tiep (nho bat lai mo cat neu can).\n",
                   vi_tri_keo_mm, vi_tri_xoay_do);
        }
    }
}

// ================== TASK: GIAM SAT AN TOAN ==================
static void task_an_toan(void *param)
{
    bool trang_thai_truoc = false;

    while (1) {
        if (co_dung_khan_cap && !trang_thai_truoc) {
            gpio_set_level(PLC_OUT_FAULT, 1);
            gpio_set_level(RELAY_PLASMA, 0);
            printf("Loi: DUNG KHAN CAP / STOP / LIMIT tu PLC duoc kich hoat.\n");
        }
        trang_thai_truoc = co_dung_khan_cap;

        if (co_dung_khan_cap) {
            bool het_stop  = gpio_get_level(PLC_IN_STOP)  == 1;
            bool het_emg   = gpio_get_level(PLC_IN_EMG)   == 1;
            bool het_limit = gpio_get_level(PLC_IN_LIMIT) == 1;
            if (het_stop && het_emg && het_limit) {
                co_dung_khan_cap = false;
                gpio_set_level(PLC_OUT_FAULT, 0);
                printf("He thong: da het dieu kien loi, san sang hoat dong tro lai.\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ================== TIEN ICH: TAO 1 BUOC DI CHUYEN PHOI HOP ==================
// delta_x_mm : quang duong truc KEO, don vi MM  (0 = khong di truc nay)
// delta_a_do : goc quay truc XOAY, don vi DO    (0 = khong di truc nay)
// rpm        : toc do vong/phut cua dong co
//
// Ca 2 truc se chay DONG THOI. Thoi gian di chuyen duoc lay theo truc CHAM
// NHAT (truc nao can nhieu thoi gian hon o toc do rpm da cho) de khong truc
// nao bi vuot toc do gioi han - day la cach xu ly feedrate chuan cua CNC.
static bool tao_buoc_di_chuyen(double delta_x_mm, double delta_a_do,
                               double rpm, lenh_dong_co_t *ra)
{
    bool co_x = fabs(delta_x_mm) > 1e-9;
    bool co_a = fabs(delta_a_do) > 1e-9;
    if (!co_x && !co_a) return false;
    if (rpm <= 0) return false;

    // Doi sang so VONG quay cua tung dong co
    double vong_x = co_x ? (fabs(delta_x_mm) / MM_MOI_VONG_TRUC_X) : 0.0;
    double vong_a = co_a ? (fabs(delta_a_do) / 360.0) : 0.0;

    long buoc_x = (long)lround(vong_x * MICROSTEP_MOI_VONG);
    long buoc_a = (long)lround(vong_a * MICROSTEP_MOI_VONG);
    if (buoc_x < 0) buoc_x = 0;
    if (buoc_a < 0) buoc_a = 0;
    if (buoc_x == 0 && buoc_a == 0) return false;

    // Thoi gian can cho tung truc rieng le o toc do rpm -> lay truc lau hon
    double t_x = vong_x * (60.0 / rpm);
    double t_a = vong_a * (60.0 / rpm);
    double tong_thoi_gian = (t_x > t_a) ? t_x : t_a;

    long troi = (buoc_x > buoc_a) ? buoc_x : buoc_a;  // so vong lap Bresenham
    unsigned long tong_us = (unsigned long)(tong_thoi_gian * 1000000.0);
    unsigned long chu_ky_us = tong_us / (unsigned long)troi;
    if (chu_ky_us < CHU_KY_TOI_THIEU_US) chu_ky_us = CHU_KY_TOI_THIEU_US;

    ra->loai = LENH_DI_CHUYEN;
    ra->so_buoc_x = (uint32_t)buoc_x;
    ra->so_buoc_a = (uint32_t)buoc_a;
    ra->huong_x = (delta_x_mm > 0);
    ra->huong_a = (delta_a_do > 0);
    ra->chu_ky_us = (uint32_t)chu_ky_us;
    return true;
}

// Dua 1 buoc da tao san vao dich: nap vao mang chuong trinh, hoac chay ngay
static void dua_buoc_vao_dich(lenh_dong_co_t buoc, bool nap)
{
    if (nap) {
        if (so_buoc_da_nap >= MAX_BUOC_CHUONG_TRINH) {
            printf("Loi: chuong trinh vuot qua gioi han %d buoc.\n", MAX_BUOC_CHUONG_TRINH);
            co_loi_khi_nap = true;
            return;
        }
        chuong_trinh[so_buoc_da_nap++] = buoc;
    } else {
        if (xQueueSend(hang_doi_lenh_dong_co, &buoc, pdMS_TO_TICKS(100)) != pdTRUE) {
            printf("Loi: hang doi lenh day, thu lai sau.\n");
        }
    }
}

// ================== TACH TOKEN 1 DONG G-CODE ==================
// Bo comment ';' va '(...)', tra ve so token dang <CHU><SO>
static int tach_token_gcode(char *dong, char *chu, double *so, int toi_da)
{
    char *cham_phay = strchr(dong, ';');
    if (cham_phay) *cham_phay = '\0';

    char *mo_ngoac = strchr(dong, '(');
    if (mo_ngoac) {
        char *dong_ngoac = strchr(mo_ngoac, ')');
        if (dong_ngoac) memmove(mo_ngoac, dong_ngoac + 1, strlen(dong_ngoac + 1) + 1);
        else *mo_ngoac = '\0';
    }

    int dem = 0;
    char *p = dong;
    while (*p && dem < toi_da) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (isalpha((unsigned char)*p)) {
            char ky_tu = toupper((unsigned char)*p);
            p++;
            char *ket_thuc;
            double gia_tri = strtod(p, &ket_thuc);
            if (ket_thuc == p) break;  // sau chu khong co so hop le
            chu[dem] = ky_tu;
            so[dem] = gia_tri;
            dem++;
            p = ket_thuc;
        } else {
            p++;
        }
    }
    return dem;
}

// ================== XU LY 1 DONG G-CODE ==================
// nap = true: dang trong che do PROG;BEGIN...PROG;END (ghi vao mang)
// nap = false: chay ngay lap tuc (lenh don le)
static bool xu_ly_1_dong_gcode(char *dong_goc, bool nap)
{
    char dong[128];
    strncpy(dong, dong_goc, sizeof(dong) - 1);
    dong[sizeof(dong) - 1] = '\0';

    char chu[10];
    double so[10];
    int n = tach_token_gcode(dong, chu, so, 10);
    if (n == 0) return true;  // dong rong / chi co comment

    char loai = 0;
    double ma_so = 0;
    bool co_x = false, co_y = false, co_f = false, co_p = false;
    double gt_x = 0, gt_y = 0, gt_f = 0, gt_p = 0;

    for (int i = 0; i < n; i++) {
        switch (chu[i]) {
            case 'G': case 'M':
                if (loai == 0) { loai = chu[i]; ma_so = so[i]; }
                break;
            case 'X': co_x = true; gt_x = so[i]; break;
            // 'A' la ky hieu CHUAN cho truc xoay quanh truc X tren may cat ong
            // (ISO 841). 'Y' duoc giu lai lam alias cho tuong thich nguoc.
            case 'A': case 'Y': co_y = true; gt_y = so[i]; break;
            case 'F': co_f = true; gt_f = so[i]; break;
            case 'P': co_p = true; gt_p = so[i]; break;
            default: break;  // N, Z, S, I, J, K, R... bo qua
        }
    }

    if (loai == 0) {
        printf("Loi: dong '%s' khong co ma lenh G/M hop le\n", dong_goc);
        return false;
    }

    int ma = (int)lround(ma_so);

    if (loai == 'G') {
        switch (ma) {
        case 90:
            che_do_tuyet_doi = true;
            return true;

        case 91:
            che_do_tuyet_doi = false;
            return true;

        case 92: {
            if (!co_x && !co_y) {
                printf("Loi: G92 can it nhat X hoac A, dong '%s'\n", dong_goc);
                return false;
            }
            // Cap nhat vi tri mo phong ngay (de cac dong sau tinh dung)
            if (co_x) vi_tri_mo_phong_x = gt_x;
            if (co_y) vi_tri_mo_phong_y = gt_y;

            // Tao 1 buoc de ap dung dung thoi diem khi chuong trinh chay toi day
            lenh_dong_co_t buoc = {
                .loai = LENH_DAT_GOC,
                .dat_x = co_x, .gia_tri_x = gt_x,
                .dat_a = co_y, .gia_tri_a = gt_y,
            };
            dua_buoc_vao_dich(buoc, nap);
            return true;
        }

        case 0:
        case 1:
        case 2:
        case 3: {
            if (ma == 2 || ma == 3) {
                printf("Canh bao: G%d duoc xap xi thanh duong thang toi diem cuoi "
                       "(phan cung chua ho tro noi suy cung tron that su).\n", ma);
            }
            if (co_f) feed_dang_nap = gt_f;
            if (!co_x && !co_y) return true;  // chi doi toc do, khong di chuyen

            // Tinh delta CA 2 TRUC truoc, roi tao 1 lenh DUY NHAT de 2 truc
            // chay DONG THOI (Bresenham) -> duong cat cheo lien mach
            double delta_x = 0.0, delta_a = 0.0;

            if (co_x) {
                double muc_tieu = che_do_tuyet_doi ? gt_x : vi_tri_mo_phong_x + gt_x;
                delta_x = muc_tieu - vi_tri_mo_phong_x;
                vi_tri_mo_phong_x = muc_tieu;
            }
            if (co_y) {
                double muc_tieu = che_do_tuyet_doi ? gt_y : vi_tri_mo_phong_y + gt_y;
                delta_a = muc_tieu - vi_tri_mo_phong_y;
                vi_tri_mo_phong_y = muc_tieu;
            }

            if (fabs(delta_x) < 1e-9 && fabs(delta_a) < 1e-9) {
                return true;  // da o dung vi tri, khong can di
            }
            if (feed_dang_nap <= 0) {
                printf("Loi: chua khai bao F truoc lenh di chuyen, dong '%s'\n", dong_goc);
                return false;
            }

            lenh_dong_co_t buoc;
            if (tao_buoc_di_chuyen(delta_x, delta_a, feed_dang_nap, &buoc)) {
                dua_buoc_vao_dich(buoc, nap);
            }
            return true;
        }

        case 4: {
            if (!co_p) {
                printf("Loi: G4 can P (so giay), dong '%s'\n", dong_goc);
                return false;
            }
            if (gt_p < 0) {
                printf("Loi: P trong G4 phai >= 0, dong '%s'\n", dong_goc);
                return false;
            }
            lenh_dong_co_t buoc = { .loai = LENH_DELAY, .delay_ms = (uint32_t)(gt_p * 1000.0) };
            dua_buoc_vao_dich(buoc, nap);
            return true;
        }

        case 28:
        case 30: {
            double rpm_home = (feed_dang_nap > 0) ? feed_dang_nap : RPM_HOME_MAC_DINH;
            double delta_x = 0.0 - vi_tri_mo_phong_x;
            double delta_a = 0.0 - vi_tri_mo_phong_y;
            vi_tri_mo_phong_x = 0.0;
            vi_tri_mo_phong_y = 0.0;
            lenh_dong_co_t buoc;
            if (tao_buoc_di_chuyen(delta_x, delta_a, rpm_home, &buoc)) {
                dua_buoc_vao_dich(buoc, nap);
            }
            return true;
        }

        case 20:
        case 21:
            return true;  // don vi inch/mm - khong anh huong, bo qua

        // ----- Cac ma chap nhan nhung KHONG lam gi (thuong gap trong file
        // G-code xuat tu phan mem CAM, khong ap dung cho may 2 truc nay) -----
        case 17: case 18: case 19:  // chon mat phang lam viec (XY/XZ/YZ)
        case 40: case 41: case 42:  // bu duong kinh dao cat
        case 43: case 49:           // bu chieu dai dao
        case 54: case 55: case 56:  // he toa do goc lam viec 1-6
        case 57: case 58: case 59:
        case 80:                    // huy chu ky gia cong san
        case 93: case 94:           // che do dien giai feedrate
        case 98: case 99:           // che do rut dao giua cac lo
            return true;

        default:
            printf("Loi: G%d chua duoc ho tro, dong '%s'\n", ma, dong_goc);
            return false;
        }
    } else {  // loai == 'M'
        switch (ma) {
        case 0:
        case 1: {
            lenh_dong_co_t buoc = { .loai = LENH_TAM_DUNG };
            dua_buoc_vao_dich(buoc, nap);
            return true;
        }

        case 3:
        case 4: {
            lenh_dong_co_t buoc = { .loai = LENH_PLASMA_ON };
            dua_buoc_vao_dich(buoc, nap);
            return true;
        }

        case 5: {
            lenh_dong_co_t buoc = { .loai = LENH_PLASMA_OFF };
            dua_buoc_vao_dich(buoc, nap);
            return true;
        }

        case 2:
        case 30: {
            // Ket thuc chuong trinh: BAT BUOC tat mo cat de an toan
            lenh_dong_co_t buoc = { .loai = LENH_PLASMA_OFF };
            dua_buoc_vao_dich(buoc, nap);
            return true;
        }

        // ----- Cac ma chap nhan nhung KHONG lam gi (khong ap dung cho may nay) -----
        case 6:              // doi dao
        case 7: case 8: case 9:  // dung dich lam mat mist/flood/tat
            return true;

        default:
            printf("Loi: M%d chua duoc ho tro, dong '%s'\n", ma, dong_goc);
            return false;
        }
    }
}

// ================== XU LY 1 DONG LENH TU PC (dieu phoi PROG/RUN/STOP/G-code) ==================
static void xu_ly_lenh_tu_pc(char *dong)
{
    dong[strcspn(dong, "\r\n")] = '\0';
    if (strlen(dong) == 0) return;

    char dong_upper[32];
    strncpy(dong_upper, dong, sizeof(dong_upper) - 1);
    dong_upper[sizeof(dong_upper) - 1] = '\0';
    for (char *p = dong_upper; *p; p++) *p = toupper((unsigned char)*p);

    // ----- Dang trong che do NAP CHUONG TRINH -----
    if (dang_nap_chuong_trinh) {
        if (strcmp(dong_upper, "PROG;END") == 0) {
            dang_nap_chuong_trinh = false;
            if (co_loi_khi_nap) {
                printf("LOI_NAP: chuong trinh co dong bi loi, xem cac dong 'Loi:' o tren. Da huy nap.\n");
                so_buoc_da_nap = 0;
            } else {
                printf("OK_NAP: da nhan %d buoc, san sang RUN.\n", so_buoc_da_nap);
            }
            return;
        }
        if (!xu_ly_1_dong_gcode(dong, true)) {
            co_loi_khi_nap = true;
        }
        return;
    }

    // ----- Lenh dieu khien (khong o trong che do nap) -----
    if (strcmp(dong_upper, "PROG;BEGIN") == 0) {
        dang_nap_chuong_trinh = true;
        co_loi_khi_nap = false;
        so_buoc_da_nap = 0;
        che_do_tuyet_doi = true;
        feed_dang_nap = -1;
        vi_tri_mo_phong_x = vi_tri_keo_mm;   // bat dau mo phong tu VI TRI THUC hien tai
        vi_tri_mo_phong_y = vi_tri_xoay_do;
        printf("OK: san sang nhan G-code, gui PROG;END khi xong.\n");
        return;
    }

    if (strcmp(dong_upper, "RUN") == 0) {
        if (so_buoc_da_nap == 0) {
            printf("Loi: chua co chuong trinh nao duoc nap (dung PROG;BEGIN...PROG;END truoc).\n");
            return;
        }
        if (co_dung_khan_cap) {
            printf("Loi: dang o trang thai dung khan cap, khong the RUN.\n");
            return;
        }
        trang_thai_chay = CHAY_BINH_THUONG;  // reset trang thai truoc khi chay moi
        int da_day = 0;
        for (int i = 0; i < so_buoc_da_nap; i++) {
            if (xQueueSend(hang_doi_lenh_dong_co, &chuong_trinh[i], pdMS_TO_TICKS(100)) == pdTRUE) {
                da_day++;
            } else {
                printf("Loi: hang doi day, chi day duoc %d/%d buoc.\n", da_day, so_buoc_da_nap);
                break;
            }
        }
        printf("RUNNING: da day %d buoc vao hang doi, dang chay...\n", da_day);
        return;
    }

    // ----- JOG: di chuyen thu cong de dua mo cat toi vi tri mong muon -----
    // Cu phap: "JOG;<X hoac Y>;<khoang_cach>;<rpm>"
    //   X: don vi mm, Y: don vi do. Gia tri am = di chieu nguoc lai.
    //   Vi du: "JOG;X;10;30"  -> keo di 10mm, toc do 30 RPM
    //          "JOG;Y;-15;20" -> xoay lui 15 do, toc do 20 RPM
    if (strncmp(dong_upper, "JOG;", 4) == 0) {
        if (co_dung_khan_cap) {
            printf("Loi: dang o trang thai dung khan cap, khong the JOG.\n");
            return;
        }
        char truc_ky_tu;
        double khoang_cach = 0, rpm = 0;
        if (sscanf(dong + 4, "%c;%lf;%lf", &truc_ky_tu, &khoang_cach, &rpm) != 3) {
            printf("Loi: cu phap JOG sai. Vi du: JOG;X;10;30\n");
            return;
        }
        truc_ky_tu = toupper((unsigned char)truc_ky_tu);
        if (truc_ky_tu != 'X' && truc_ky_tu != 'Y' && truc_ky_tu != 'A') {
            printf("Loi: JOG chi ho tro truc X hoac A (Y la alias cua A).\n");
            return;
        }
        if (rpm <= 0) {
            printf("Loi: RPM trong JOG phai > 0.\n");
            return;
        }
        // JOG chi di 1 truc -> truc con lai dat delta = 0
        double delta_x = (truc_ky_tu == 'X') ? khoang_cach : 0.0;
        double delta_a = (truc_ky_tu == 'X') ? 0.0 : khoang_cach;
        lenh_dong_co_t buoc;
        if (tao_buoc_di_chuyen(delta_x, delta_a, rpm, &buoc)) {
            trang_thai_chay = CHAY_BINH_THUONG;  // JOG luon chay duoc
            if (xQueueSend(hang_doi_lenh_dong_co, &buoc, pdMS_TO_TICKS(100)) != pdTRUE) {
                printf("Loi: hang doi day, khong the JOG.\n");
            }
        } else {
            printf("Loi: khoang cach JOG qua nho.\n");
        }
        return;
    }

    // ----- ZERO: dat vi tri hien tai lam diem goc (0,0) -----
    // Dung sau khi da JOG mo cat toi dung vi tri bat dau cat mong muon
    if (strcmp(dong_upper, "ZERO") == 0) {
        vi_tri_keo_mm = 0.0;
        vi_tri_xoay_do = 0.0;
        vi_tri_mo_phong_x = 0.0;
        vi_tri_mo_phong_y = 0.0;
        printf("ZEROED: da dat vi tri hien tai lam diem goc. Vi tri: X=0.00 Y=0.00\n");
        return;
    }

    if (strcmp(dong_upper, "PAUSE") == 0) {
        if (trang_thai_chay == DANG_TAM_DUNG) {
            printf("PAUSED: dang tam dung san roi.\n");
        } else if (trang_thai_chay == YEU_CAU_DUNG_HAN) {
            printf("Loi: chuong trinh da bi STOP, khong the PAUSE.\n");
        } else {
            trang_thai_chay = YEU_CAU_TAM_DUNG;
            printf("He thong: da nhan PAUSE - se chay het buoc hien tai roi moi dung.\n");
        }
        return;
    }

    if (strcmp(dong_upper, "RESUME") == 0) {
        if (trang_thai_chay == DANG_TAM_DUNG || trang_thai_chay == YEU_CAU_TAM_DUNG) {
            trang_thai_chay = CHAY_BINH_THUONG;
            printf("RESUMED: chay tiep chuong trinh.\n");
        } else {
            printf("Loi: khong o trang thai tam dung, khong can RESUME.\n");
        }
        return;
    }

    if (strcmp(dong_upper, "STOP") == 0) {
        trang_thai_chay = YEU_CAU_DUNG_HAN;
        gpio_set_level(RELAY_PLASMA, 0);  // AN TOAN: tat mo cat ngay lap tuc
        xQueueReset(hang_doi_lenh_dong_co);
        so_buoc_da_nap = 0;
        printf("STOPPED: da tat mo cat, dung han va xoa toan bo chuong trinh.\n");
        return;
    }

    // ----- Lenh G-code don le (test nhanh, khong can nap chuong trinh) -----
    if (co_dung_khan_cap) {
        printf("Loi: dang o trang thai dung khan cap, khong nhan lenh moi.\n");
        return;
    }
    // Dung bien mo phong = vi tri thuc de cac lenh don le cung tinh dung
    vi_tri_mo_phong_x = vi_tri_keo_mm;
    vi_tri_mo_phong_y = vi_tri_xoay_do;
    if (feed_dang_nap == 0) feed_dang_nap = -1; // dam bao co gia tri khoi tao lan dau
    xu_ly_1_dong_gcode(dong, false);
}

// ================== CAU HINH UART DOC LENH TU PC ==================
static void cau_hinh_uart_pc(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_PC, &cfg);
    // Chi can driver de doc (RX), khong doi chan vi UART0 da noi san qua USB
    uart_driver_install(UART_PC, 1024, 0, 0, NULL, 0);
}

// ================== TASK: GIAO TIEP VOI PC QUA USB COM ==================
static void task_giao_tiep_pc(void *param)
{
    char dong[128];
    int vi_tri = 0;
    uint8_t ky_tu;

    printf("San sang. Gui G-code chuan (G0/G1/G4/G28/G90/G91/G92/M0/M3/M5...).\n");
    printf("Dung PROG;BEGIN...PROG;END de nap chuong trinh, RUN de chay, STOP de dung.\n");

    while (1) {
        // Doc tung ky tu, block THAT SU (nhuong CPU cho task/idle khac) nho
        // dung driver UART thay vi fgets(stdin) - tranh loi task watchdog
        int n = uart_read_bytes(UART_PC, &ky_tu, 1, portMAX_DELAY);
        if (n <= 0) continue;

        if (ky_tu == '\n' || ky_tu == '\r') {
            if (vi_tri > 0) {
                dong[vi_tri] = '\0';
                xu_ly_lenh_tu_pc(dong);
                vi_tri = 0;
            }
        } else if (vi_tri < (int)sizeof(dong) - 1) {
            dong[vi_tri++] = (char)ky_tu;
        }
    }
}

// ================== APP MAIN ==================
void app_main(void)
{
    ESP_LOGI(TAG, "Khoi dong dieu khien may cat ong - ESP32 devkit goc (G-code chuan qua USB COM)");

    cau_hinh_ngo_ra();
    cau_hinh_ngo_vao();
    cau_hinh_uart_pc();

    che_do_tuyet_doi = true;
    feed_dang_nap = -1;

    hang_doi_lenh_dong_co = xQueueCreate(MAX_BUOC_CHUONG_TRINH, sizeof(lenh_dong_co_t));

    gpio_set_level(PLC_OUT_READY, 1);

    xTaskCreatePinnedToCore(task_dong_co, "task_dong_co", 4096, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(task_an_toan, "task_an_toan", 2048, NULL, 15, NULL, 0);
    xTaskCreatePinnedToCore(task_giao_tiep_pc, "task_giao_tiep_pc", 4096, NULL, 5, NULL, 0);

    // ----- LUU Y QUAN TRONG VE TASK WATCHDOG -----
    // Nhan 1 duoc danh RIENG cho task_dong_co xuat xung. Vong lap xuat xung
    // phai chay LIEN TUC khong ngat quang (khong chen vTaskDelay), neu khong
    // dong co se bi dung giat ~10ms moi lan => rung may, cat xau.
    // Vi vay IDLE task cua nhan 1 khong bao gio duoc chay khi dang cat, va
    // Task Watchdog se bao loi.
    //
    // CACH XU LY DUNG (lam 1 lan bang menuconfig, KHONG go bang code):
    //   idf.py menuconfig
    //     -> Component config
    //       -> ESP System Settings
    //         -> Task Watchdog Timer
    //           -> BO CHON "Watch CPU1 Idle Task"
    //   (tuong ung: CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n)
    //
    // KHONG dung esp_task_wdt_delete(idle task) luc chay: no go task khoi
    // watchdog nhung KHONG go idle hook, khien idle task van goi reset va
    // spam loi "esp_task_wdt_reset: task not found" lien tuc moi 10ms.
}
