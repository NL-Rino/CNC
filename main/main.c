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
 * SO DO CHAN MAC DINH (ESP32 DEVKIT GOC) - co the doi bang lenh CFG;PIN;...:
 *   PUL_KEO_A = GPIO4    DIR_KEO_A = GPIO13
 *   PUL_KEO_B = GPIO14   DIR_KEO_B = GPIO16
 *   PUL_XOAY  = GPIO25   DIR_XOAY  = GPIO26
 *   RELAY_PLASMA = GPIO19
 *   PLC_OUT_READY/RUNNING/DONE/FAULT = GPIO17/18/21/22
 *   PLC_IN_START/STOP/EMG/LIMIT = GPIO23/27/32/33
 *   (Khong dung chan ENA cho ca 3 driver - dong co luon giu phanh,
 *    khong bao gio nha phanh qua phan mem)
 *
 * CAI DAT NANG CAO QUA LENH CFG (xem chi tiet o dinh nghia cau_hinh_t):
 *   CFG;GET                       xem toan bo cau hinh hien tai
 *   CFG;PIN;<TEN>;<so_gpio>       doi 1 chan (can CFG;SAVE + CFG;REBOOT)
 *   CFG;CAL;MICROSTEP;<so>        doi so xung/vong dong co (ap dung ngay)
 *   CFG;CAL;MMVONG;<so>           doi mm/vong truc keo (ap dung ngay)
 *   CFG;DAO;<KEOA|KEOB|XOAY>;<0|1> dao chieu 1 truc (ap dung ngay)
 *   CFG;RAMP;CAT;<0|1>            cho doan cat tang toc dan hay khong
 *   CFG;SAVE / CFG;RESET / CFG;REBOOT   luu / xoa ve mac dinh / khoi dong lai
 *
 * TANG/GIAM TOC (RAMP) - QUAN TRONG CHO CHAT LUONG MEP CAT:
 *   - Doan CHAY KHONG TAI (G0, G28/G30, JOG): tang toc dan luc bat dau va giam
 *     toc truoc khi dung, de dong co khong rung / mat buoc.
 *   - Doan CAT (G1/G2/G3 khi mo cat dang bat sau M3): chay DUNG TOC DO NGAY TU
 *     XUNG DAU TIEN, khong tang toc dan - neu chay cham dan luc vao cat thi mep
 *     cat cho bat dau bi chay qua. (Doi lai neu dong co thieu mo-men se de mat
 *     buoc luc vao cat -> khi do bat CFG;RAMP;CAT;1)
 *   - Giua cac doan CAT lien tiep: KHONG giam toc, KHONG in log ra UART, chay
 *     thang sang doan ke tiep => duong cat lien mach, khong co vet dung o cac
 *     diem doi huong. Chi doan CAT CUOI CUNG cua chuoi moi giam toc de dung han.
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
#include "esp_system.h"    // esp_restart
#include "nvs_flash.h"
#include "nvs.h"

#define UART_PC UART_NUM_0

// ================== CHAN GPIO / HIEU CHUAN MAC DINH ==================
// Day la gia tri MAC DINH dung khi chua co cau hinh nao luu trong NVS (flash).
// Cau hinh THUC TE dang chay nam trong bien g_cfg (co the doi bang lenh
// CFG;... qua Serial va luu lai bang CFG;SAVE - xem phan CAU HINH RUNTIME).
#define DEFAULT_PUL_KEO_A       GPIO_NUM_4
#define DEFAULT_DIR_KEO_A       GPIO_NUM_13
#define DEFAULT_PUL_KEO_B       GPIO_NUM_14
#define DEFAULT_DIR_KEO_B       GPIO_NUM_16
#define DEFAULT_PUL_XOAY        GPIO_NUM_25
#define DEFAULT_DIR_XOAY        GPIO_NUM_26

#define DEFAULT_RELAY_PLASMA    GPIO_NUM_19

#define DEFAULT_PLC_IN_START    GPIO_NUM_23
#define DEFAULT_PLC_IN_STOP     GPIO_NUM_27
#define DEFAULT_PLC_IN_EMG      GPIO_NUM_32
#define DEFAULT_PLC_IN_LIMIT    GPIO_NUM_33

#define DEFAULT_PLC_OUT_READY   GPIO_NUM_17
#define DEFAULT_PLC_OUT_RUNNING GPIO_NUM_18
#define DEFAULT_PLC_OUT_DONE    GPIO_NUM_21
#define DEFAULT_PLC_OUT_FAULT   GPIO_NUM_22

// So xung dong co can de quay 1 vong (= vi buoc driver x so buoc/vong dong co,
// vi du dong co 200 buoc/vong, vi buoc 1/8 => 1600 xung/vong)
#define DEFAULT_MICROSTEP_MOI_VONG    1600.0
// Truc X (KEO): 0.2 vong = 1mm  =>  1 vong = 5mm
#define DEFAULT_MM_MOI_VONG_TRUC_X    5.0
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

// ================== CAU HINH RUNTIME (luu trong NVS - flash) ==================
// Toan bo chan GPIO va he so hieu chuan co the doi bang lenh CFG;... tu file
// setting (cnc_settings.pyw) qua Serial, khong can build lai firmware.
// - CFG;PIN;<TEN>;<so_gpio>   doi 1 chan (can CFG;SAVE + khoi dong lai ESP32
//                              de ap dung, vi lien quan gpio_config()/ISR)
// - CFG;CAL;MICROSTEP;<so>    doi so xung/vong (ap dung NGAY, khong can reboot)
// - CFG;CAL;MMVONG;<so>       doi mm/vong truc keo (ap dung NGAY)
// - CFG;DAO;<KEOA|KEOB|XOAY>;<0|1>   dao chieu 1 truc (ap dung NGAY)
// - CFG;SAVE                  luu cau hinh hien tai vao flash (NVS)
// - CFG;RESET                 xoa NVS, ve mac dinh (can khoi dong lai)
// - CFG;REBOOT                khoi dong lai ESP32 (de ap dung chan GPIO moi)
// - CFG;GET                   in toan bo cau hinh hien tai
typedef struct {
    int pin_pul_keo_a, pin_dir_keo_a;
    int pin_pul_keo_b, pin_dir_keo_b;
    int pin_pul_xoay,  pin_dir_xoay;
    int pin_relay_plasma;
    int pin_plc_in_start, pin_plc_in_stop, pin_plc_in_emg, pin_plc_in_limit;
    int pin_plc_out_ready, pin_plc_out_running, pin_plc_out_done, pin_plc_out_fault;
    double microstep_moi_vong;
    double mm_moi_vong_truc_x;
    bool dao_keo_a, dao_keo_b, dao_xoay;
    // false (mac dinh) = doan CAT chay dung toc do ngay tu xung dau, khong tang
    //                    toc dan -> mep cat dep, khong bi chay qua o diem bat dau
    // true             = van tang toc dan ca khi cat (chi bat neu dong co bi mat
    //                    buoc / ru khi vao cat, doi lai mep cat dau se xau hon)
    bool ramp_khi_cat;
} cau_hinh_t;

static cau_hinh_t g_cfg;
static const char *NVS_NAMESPACE = "cnc_cfg";

static void cau_hinh_dat_mac_dinh(void)
{
    g_cfg.pin_pul_keo_a = DEFAULT_PUL_KEO_A;
    g_cfg.pin_dir_keo_a = DEFAULT_DIR_KEO_A;
    g_cfg.pin_pul_keo_b = DEFAULT_PUL_KEO_B;
    g_cfg.pin_dir_keo_b = DEFAULT_DIR_KEO_B;
    g_cfg.pin_pul_xoay  = DEFAULT_PUL_XOAY;
    g_cfg.pin_dir_xoay  = DEFAULT_DIR_XOAY;
    g_cfg.pin_relay_plasma = DEFAULT_RELAY_PLASMA;
    g_cfg.pin_plc_in_start = DEFAULT_PLC_IN_START;
    g_cfg.pin_plc_in_stop  = DEFAULT_PLC_IN_STOP;
    g_cfg.pin_plc_in_emg   = DEFAULT_PLC_IN_EMG;
    g_cfg.pin_plc_in_limit = DEFAULT_PLC_IN_LIMIT;
    g_cfg.pin_plc_out_ready   = DEFAULT_PLC_OUT_READY;
    g_cfg.pin_plc_out_running = DEFAULT_PLC_OUT_RUNNING;
    g_cfg.pin_plc_out_done    = DEFAULT_PLC_OUT_DONE;
    g_cfg.pin_plc_out_fault   = DEFAULT_PLC_OUT_FAULT;
    g_cfg.microstep_moi_vong = DEFAULT_MICROSTEP_MOI_VONG;
    g_cfg.mm_moi_vong_truc_x = DEFAULT_MM_MOI_VONG_TRUC_X;
    g_cfg.dao_keo_a = false;
    g_cfg.dao_keo_b = false;
    g_cfg.dao_xoay  = false;
    g_cfg.ramp_khi_cat = false;
}

// Doc 1 khoa i32 tu NVS, giu nguyen gia tri hien tai (mac dinh) neu chua co
static void nvs_doc_i32(nvs_handle_t tay_cam, const char *khoa, int *ra)
{
    int32_t gia_tri;
    if (nvs_get_i32(tay_cam, khoa, &gia_tri) == ESP_OK) *ra = (int)gia_tri;
}

static void nvs_doc_double(nvs_handle_t tay_cam, const char *khoa, double *ra)
{
    // NVS khong co kieu double truc tiep -> luu duoi dang blob co dinh 8 byte
    double gia_tri;
    size_t co_bytes = sizeof(gia_tri);
    if (nvs_get_blob(tay_cam, khoa, &gia_tri, &co_bytes) == ESP_OK && co_bytes == sizeof(gia_tri)) {
        *ra = gia_tri;
    }
}

static void nvs_doc_bool(nvs_handle_t tay_cam, const char *khoa, bool *ra)
{
    uint8_t gia_tri;
    if (nvs_get_u8(tay_cam, khoa, &gia_tri) == ESP_OK) *ra = (gia_tri != 0);
}

static void cau_hinh_doc_tu_nvs(void)
{
    cau_hinh_dat_mac_dinh();

    nvs_handle_t tay_cam;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &tay_cam) != ESP_OK) {
        return;  // chua tung luu -> dung mac dinh
    }

    nvs_doc_i32(tay_cam, "pul_keo_a", &g_cfg.pin_pul_keo_a);
    nvs_doc_i32(tay_cam, "dir_keo_a", &g_cfg.pin_dir_keo_a);
    nvs_doc_i32(tay_cam, "pul_keo_b", &g_cfg.pin_pul_keo_b);
    nvs_doc_i32(tay_cam, "dir_keo_b", &g_cfg.pin_dir_keo_b);
    nvs_doc_i32(tay_cam, "pul_xoay",  &g_cfg.pin_pul_xoay);
    nvs_doc_i32(tay_cam, "dir_xoay",  &g_cfg.pin_dir_xoay);
    nvs_doc_i32(tay_cam, "relay_plasma", &g_cfg.pin_relay_plasma);
    nvs_doc_i32(tay_cam, "plc_in_start", &g_cfg.pin_plc_in_start);
    nvs_doc_i32(tay_cam, "plc_in_stop",  &g_cfg.pin_plc_in_stop);
    nvs_doc_i32(tay_cam, "plc_in_emg",   &g_cfg.pin_plc_in_emg);
    nvs_doc_i32(tay_cam, "plc_in_limit", &g_cfg.pin_plc_in_limit);
    nvs_doc_i32(tay_cam, "plc_out_ready",   &g_cfg.pin_plc_out_ready);
    nvs_doc_i32(tay_cam, "plc_out_running", &g_cfg.pin_plc_out_running);
    nvs_doc_i32(tay_cam, "plc_out_done",    &g_cfg.pin_plc_out_done);
    nvs_doc_i32(tay_cam, "plc_out_fault",   &g_cfg.pin_plc_out_fault);
    nvs_doc_double(tay_cam, "microstep", &g_cfg.microstep_moi_vong);
    nvs_doc_double(tay_cam, "mm_vong",   &g_cfg.mm_moi_vong_truc_x);
    nvs_doc_bool(tay_cam, "dao_keo_a", &g_cfg.dao_keo_a);
    nvs_doc_bool(tay_cam, "dao_keo_b", &g_cfg.dao_keo_b);
    nvs_doc_bool(tay_cam, "dao_xoay",  &g_cfg.dao_xoay);
    nvs_doc_bool(tay_cam, "ramp_cat",  &g_cfg.ramp_khi_cat);

    nvs_close(tay_cam);
}

static bool cau_hinh_luu_vao_nvs(void)
{
    nvs_handle_t tay_cam;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &tay_cam) != ESP_OK) return false;

    nvs_set_i32(tay_cam, "pul_keo_a", g_cfg.pin_pul_keo_a);
    nvs_set_i32(tay_cam, "dir_keo_a", g_cfg.pin_dir_keo_a);
    nvs_set_i32(tay_cam, "pul_keo_b", g_cfg.pin_pul_keo_b);
    nvs_set_i32(tay_cam, "dir_keo_b", g_cfg.pin_dir_keo_b);
    nvs_set_i32(tay_cam, "pul_xoay",  g_cfg.pin_pul_xoay);
    nvs_set_i32(tay_cam, "dir_xoay",  g_cfg.pin_dir_xoay);
    nvs_set_i32(tay_cam, "relay_plasma", g_cfg.pin_relay_plasma);
    nvs_set_i32(tay_cam, "plc_in_start", g_cfg.pin_plc_in_start);
    nvs_set_i32(tay_cam, "plc_in_stop",  g_cfg.pin_plc_in_stop);
    nvs_set_i32(tay_cam, "plc_in_emg",   g_cfg.pin_plc_in_emg);
    nvs_set_i32(tay_cam, "plc_in_limit", g_cfg.pin_plc_in_limit);
    nvs_set_i32(tay_cam, "plc_out_ready",   g_cfg.pin_plc_out_ready);
    nvs_set_i32(tay_cam, "plc_out_running", g_cfg.pin_plc_out_running);
    nvs_set_i32(tay_cam, "plc_out_done",    g_cfg.pin_plc_out_done);
    nvs_set_i32(tay_cam, "plc_out_fault",   g_cfg.pin_plc_out_fault);
    nvs_set_blob(tay_cam, "microstep", &g_cfg.microstep_moi_vong, sizeof(double));
    nvs_set_blob(tay_cam, "mm_vong",   &g_cfg.mm_moi_vong_truc_x, sizeof(double));
    nvs_set_u8(tay_cam, "dao_keo_a", g_cfg.dao_keo_a ? 1 : 0);
    nvs_set_u8(tay_cam, "dao_keo_b", g_cfg.dao_keo_b ? 1 : 0);
    nvs_set_u8(tay_cam, "dao_xoay",  g_cfg.dao_xoay  ? 1 : 0);
    nvs_set_u8(tay_cam, "ramp_cat",  g_cfg.ramp_khi_cat ? 1 : 0);

    esp_err_t loi = nvs_commit(tay_cam);
    nvs_close(tay_cam);
    return loi == ESP_OK;
}

static void cau_hinh_in_ra(void)
{
    printf("CFG: pul_keo_a=%d dir_keo_a=%d pul_keo_b=%d dir_keo_b=%d pul_xoay=%d dir_xoay=%d\n",
           g_cfg.pin_pul_keo_a, g_cfg.pin_dir_keo_a, g_cfg.pin_pul_keo_b,
           g_cfg.pin_dir_keo_b, g_cfg.pin_pul_xoay, g_cfg.pin_dir_xoay);
    printf("CFG: relay_plasma=%d\n", g_cfg.pin_relay_plasma);
    printf("CFG: plc_in_start=%d plc_in_stop=%d plc_in_emg=%d plc_in_limit=%d\n",
           g_cfg.pin_plc_in_start, g_cfg.pin_plc_in_stop, g_cfg.pin_plc_in_emg, g_cfg.pin_plc_in_limit);
    printf("CFG: plc_out_ready=%d plc_out_running=%d plc_out_done=%d plc_out_fault=%d\n",
           g_cfg.pin_plc_out_ready, g_cfg.pin_plc_out_running, g_cfg.pin_plc_out_done, g_cfg.pin_plc_out_fault);
    printf("CFG: microstep_moi_vong=%.2f mm_moi_vong_truc_x=%.4f\n",
           g_cfg.microstep_moi_vong, g_cfg.mm_moi_vong_truc_x);
    printf("CFG: dao_keo_a=%d dao_keo_b=%d dao_xoay=%d\n",
           g_cfg.dao_keo_a, g_cfg.dao_keo_b, g_cfg.dao_xoay);
    printf("CFG: ramp_khi_cat=%d\n", g_cfg.ramp_khi_cat);
}

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

// So buoc con lai cua chuong trinh dang chay. Dat = so buoc luc RUN, task dong
// co tru dan sau moi buoc. Khi ve 0 thi in "XONG_CHUONG_TRINH" - nho vay PC moi
// biet CHUONG TRINH da het (truoc day PC tuong da xong ngay sau buoc DAU TIEN
// vi chi thay dong "Hoan thanh", nen tat mat nut PAUSE giua chung).
static volatile int so_buoc_con_lai = 0;
static volatile bool dang_chay_chuong_trinh = false;

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

    // true = doan nay chay TRONG KHI mo cat plasma dang bat (G1/G2/G3 sau M3).
    // Doan cat KHONG tang toc dan: phai chay dung toc do ngay tu xung dau tien,
    // neu khong mep cat cho bat dau se bi chay qua (overburn).
    bool dang_cat;
    // true = ngay sau doan nay con 1 doan CAT nua -> KHONG giam toc, KHONG in
    // log, chay thang sang doan ke tiep de duong cat lien mach khong co vet dung
    bool noi_lien_sau;

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
// Mo phong trang thai mo cat trong luc PHAN TICH (khong phai trang thai that
// cua relay) - dung de biet moi lenh G1/G2/G3 co phai la doan DANG CAT hay khong
static bool plasma_mo_phong;
// Ma di chuyen G0/G1/G2/G3 gan nhat - dung cho CHE DO MODAL: dong chi co toa do
// (vi du "X10 A20", rat pho bien trong file CAM) se lay lai ma nay. -1 = chua co
static int g_di_chuyen_modal;
// 1.0 = dang o mm (G21), 25.4 = dang o inch (G20). Nhan vao moi toa do TRUC THANG
static double he_so_don_vi;

// ================== NGAT GPIO CHO STOP / EMG / LIMIT (PLC) ==================
static void IRAM_ATTR trinh_xu_ly_ngat_an_toan(void *arg)
{
    co_dung_khan_cap = true;
}

static void cau_hinh_ngo_vao(void)
{
    gpio_config_t cfg_an_toan = {
        .pin_bit_mask = (1ULL << g_cfg.pin_plc_in_stop) | (1ULL << g_cfg.pin_plc_in_emg) |
                        (1ULL << g_cfg.pin_plc_in_limit),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&cfg_an_toan);

    gpio_config_t cfg_start = {
        .pin_bit_mask = (1ULL << g_cfg.pin_plc_in_start),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg_start);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(g_cfg.pin_plc_in_stop, trinh_xu_ly_ngat_an_toan, NULL);
    gpio_isr_handler_add(g_cfg.pin_plc_in_emg, trinh_xu_ly_ngat_an_toan, NULL);
    gpio_isr_handler_add(g_cfg.pin_plc_in_limit, trinh_xu_ly_ngat_an_toan, NULL);
}

static void cau_hinh_ngo_ra(void)
{
    gpio_config_t cfg_ra = {
        .pin_bit_mask =
            (1ULL << g_cfg.pin_pul_keo_a) | (1ULL << g_cfg.pin_dir_keo_a) |
            (1ULL << g_cfg.pin_pul_keo_b) | (1ULL << g_cfg.pin_dir_keo_b) |
            (1ULL << g_cfg.pin_pul_xoay)  | (1ULL << g_cfg.pin_dir_xoay)  |
            (1ULL << g_cfg.pin_relay_plasma) |
            (1ULL << g_cfg.pin_plc_out_ready) | (1ULL << g_cfg.pin_plc_out_running) |
            (1ULL << g_cfg.pin_plc_out_done)  | (1ULL << g_cfg.pin_plc_out_fault),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg_ra);

    gpio_set_level(g_cfg.pin_relay_plasma, 0);
    gpio_set_level(g_cfg.pin_plc_out_running, 0);
    gpio_set_level(g_cfg.pin_plc_out_done, 0);
    gpio_set_level(g_cfg.pin_plc_out_fault, 0);
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
        // Bao PC biet chuong trinh da chay HET (khong con buoc nao trong hang doi).
        // Phai dat o DAY - truoc khi block cho lenh moi - vi ben duoi co rat nhieu
        // duong "continue" khac nhau.
        if (dang_chay_chuong_trinh && so_buoc_con_lai <= 0) {
            dang_chay_chuong_trinh = false;
            printf("XONG_CHUONG_TRINH: da chay het chuong trinh. Vi tri: X=%.2f A=%.2f\n",
                   vi_tri_keo_mm, vi_tri_xoay_do);
        }

        if (xQueueReceive(hang_doi_lenh_dong_co, &lenh, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (so_buoc_con_lai > 0) so_buoc_con_lai--;

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
            gpio_set_level(g_cfg.pin_relay_plasma, 1);
            printf("PLASMA_ON: da bat mo cat.\n");
            continue;
        }
        if (lenh.loai == LENH_PLASMA_OFF) {
            gpio_set_level(g_cfg.pin_relay_plasma, 0);
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
            gpio_set_level(g_cfg.pin_relay_plasma, 0);  // AN TOAN: tat mo cat khi dung cho
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
        // (XOR voi co dao_* de bu truong hop dong co lap nguoc chieu co khi)
        if (buoc_x > 0) {
            gpio_set_level(g_cfg.pin_dir_keo_a, lenh.huong_x ^ g_cfg.dao_keo_a);
            gpio_set_level(g_cfg.pin_dir_keo_b, lenh.huong_x ^ g_cfg.dao_keo_b);
        }
        if (buoc_a > 0) {
            gpio_set_level(g_cfg.pin_dir_xoay, lenh.huong_a ^ g_cfg.dao_xoay);
        }
        esp_rom_delay_us(10);  // thoi gian setup DIR truoc PUL

        gpio_set_level(g_cfg.pin_plc_out_running, 1);
        gpio_set_level(g_cfg.pin_plc_out_done, 0);

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

        // ----- Quyet dinh CO tang/giam toc hay khong -----
        // TANG TOC: chi ap dung cho doan CHAY KHONG TAI (G0, G28, JOG...).
        //   Doan CAT phai dat dung toc do ngay tu xung dau tien, neu chay cham
        //   dan luc vao cat thi mep cat cho bat dau bi chay qua / thung to.
        // GIAM TOC: bo khi con doan CAT ke tiep (noi_lien_sau) de duong cat
        //   lien mach, khong co vet dung o diem noi. Van giam toc o CUOI chuoi
        //   (khi that su dung han) de dong co khong bi mat buoc do dung dot ngot.
        bool cho_tang_toc = !lenh.dang_cat || g_cfg.ramp_khi_cat;
        bool cho_giam_toc = !lenh.noi_lien_sau;

        uint32_t ramp_dau  = cho_tang_toc ? vung_ramp : 0;
        uint32_t ramp_cuoi = cho_giam_toc ? vung_ramp : 0;

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
                gpio_set_level(g_cfg.pin_pul_keo_a, 1);
                gpio_set_level(g_cfg.pin_pul_keo_b, 1);
            }
            if (xung_a) {
                gpio_set_level(g_cfg.pin_pul_xoay, 1);
            }

            esp_rom_delay_us(5);  // do rong xung HIGH

            if (xung_x) {
                gpio_set_level(g_cfg.pin_pul_keo_a, 0);
                gpio_set_level(g_cfg.pin_pul_keo_b, 0);
                da_chay_x++;
            }
            if (xung_a) {
                gpio_set_level(g_cfg.pin_pul_xoay, 0);
                da_chay_a++;
            }

            // ----- Tinh chu ky xung cho vong lap nay theo ramp hinh thang -----
            uint32_t chu_ky_hien_tai = chu_ky_dat;
            if (ramp_dau > 0 && i < ramp_dau) {
                // Giai doan TANG TOC: chu ky giam dan tu chu_ky_dau -> chu_ky_dat
                uint32_t con_lai = ramp_dau - i;
                chu_ky_hien_tai = chu_ky_dat +
                    (uint32_t)(((uint64_t)(chu_ky_dau - chu_ky_dat) * con_lai) / ramp_dau);
            } else if (ramp_cuoi > 0 && i >= troi - ramp_cuoi) {
                // Giai doan GIAM TOC: chu ky tang dan tu chu_ky_dat -> chu_ky_dau
                uint32_t den_cuoi = troi - i;
                chu_ky_hien_tai = chu_ky_dat +
                    (uint32_t)(((uint64_t)(chu_ky_dau - chu_ky_dat) * (ramp_cuoi - den_cuoi + 1)) / ramp_cuoi);
            }

            uint32_t nghi = (chu_ky_hien_tai > 5) ? (chu_ky_hien_tai - 5) : 5;
            esp_rom_delay_us(nghi);
        }

        // Con noi tiep sang doan cat ke tiep hay khong. Neu nguoi van hanh vua
        // bam PAUSE thi PHAI coi nhu ket thuc chuoi de con dung lai duoc, khong
        // duoc chay thang het ca duong cat roi moi dung.
        bool ket_thuc_chuoi = !lenh.noi_lien_sau ||
                              (trang_thai_chay == YEU_CAU_TAM_DUNG);

        // Trong chuoi cat lien tuc thi GIU nguyen RUNNING, khong ha xuong roi
        // keo len lien tuc o moi diem noi
        if (ket_thuc_chuoi) {
            gpio_set_level(g_cfg.pin_plc_out_running, 0);
        }

        // Cap nhat vi tri THUC TE theo dung so xung da xuat cho TUNG truc
        // (chinh xac ngay ca khi bi cat giua chung boi STOP)
        if (da_chay_x > 0) {
            double mm_da_di = ((double)da_chay_x / g_cfg.microstep_moi_vong) * g_cfg.mm_moi_vong_truc_x;
            vi_tri_keo_mm += lenh.huong_x ? mm_da_di : -mm_da_di;
        }
        if (da_chay_a > 0) {
            double do_da_quay = ((double)da_chay_a / g_cfg.microstep_moi_vong) * 360.0;
            vi_tri_xoay_do += lenh.huong_a ? do_da_quay : -do_da_quay;
        }

        if (bi_dung_han) {
            printf("Da dung han (STOP). Vi tri: X=%.2f A=%.2f\n", vi_tri_keo_mm, vi_tri_xoay_do);
            continue;
        }

        // QUAN TRONG: KHONG in gi khi con doan cat ke tiep. printf ra UART
        // 115200 baud mat vai ms va CHAN vong xuat xung -> tao vet dung / chay
        // qua o moi diem noi duong cat. Chi bao vi tri khi chuoi cat da ket thuc.
        if (!ket_thuc_chuoi) {
            continue;
        }

        printf("Hoan thanh. Vi tri: X=%.2f A=%.2f\n", vi_tri_keo_mm, vi_tri_xoay_do);
        gpio_set_level(g_cfg.pin_plc_out_done, 1);

        // Neu co yeu cau PAUSE: buoc nay da chay XONG HOAN TOAN, vi tri da
        // cap nhat chinh xac -> gio moi chuyen sang tam dung
        if (trang_thai_chay == YEU_CAU_TAM_DUNG) {
            trang_thai_chay = DANG_TAM_DUNG;
            gpio_set_level(g_cfg.pin_relay_plasma, 0);  // AN TOAN: tat mo cat khi dung
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
            gpio_set_level(g_cfg.pin_plc_out_fault, 1);
            gpio_set_level(g_cfg.pin_relay_plasma, 0);
            printf("Loi: DUNG KHAN CAP / STOP / LIMIT tu PLC duoc kich hoat.\n");
        }
        trang_thai_truoc = co_dung_khan_cap;

        if (co_dung_khan_cap) {
            bool het_stop  = gpio_get_level(g_cfg.pin_plc_in_stop)  == 1;
            bool het_emg   = gpio_get_level(g_cfg.pin_plc_in_emg)   == 1;
            bool het_limit = gpio_get_level(g_cfg.pin_plc_in_limit) == 1;
            if (het_stop && het_emg && het_limit) {
                co_dung_khan_cap = false;
                gpio_set_level(g_cfg.pin_plc_out_fault, 0);
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
    double vong_x = co_x ? (fabs(delta_x_mm) / g_cfg.mm_moi_vong_truc_x) : 0.0;
    double vong_a = co_a ? (fabs(delta_a_do) / 360.0) : 0.0;

    long buoc_x = (long)lround(vong_x * g_cfg.microstep_moi_vong);
    long buoc_a = (long)lround(vong_a * g_cfg.microstep_moi_vong);
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
    // Mac dinh coi la doan chay khong tai; ben goi se danh dau lai neu dang cat.
    // noi_lien_sau duoc tinh sau khi nap xong ca chuong trinh (xem
    // tinh_noi_lien_chuong_trinh)
    ra->dang_cat = false;
    ra->noi_lien_sau = false;
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

// ================== NHIN TRUOC (look-ahead): NOI LIEN CAC DOAN CAT ==================
// Chay 1 lan sau khi nap xong chuong trinh. Neu doan CAT i va doan ngay sau no
// cung la doan CAT thi danh dau doan i la "noi_lien_sau": khi chay toi doan i,
// dong co se KHONG giam toc ve 0 va KHONG in log, ma chay thang sang doan ke
// tiep => duong cat lien mach, khong co vet dung o cac diem doi huong.
// Doan CAT CUOI CUNG cua chuoi van giam toc binh thuong de khong mat buoc.
static void tinh_noi_lien_chuong_trinh(void)
{
    for (int i = 0; i < so_buoc_da_nap; i++) {
        chuong_trinh[i].noi_lien_sau = false;
    }
    for (int i = 0; i + 1 < so_buoc_da_nap; i++) {
        if (chuong_trinh[i].loai == LENH_DI_CHUYEN && chuong_trinh[i].dang_cat &&
            chuong_trinh[i + 1].loai == LENH_DI_CHUYEN && chuong_trinh[i + 1].dang_cat) {
            chuong_trinh[i].noi_lien_sau = true;
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
//
// Ho tro DAY DU cu phap G-code cong nghiep de nhan duoc file tu phan mem CAM:
//   1. NHIEU ma G/M tren 1 dong  ("G90 G54 G17", "G1 X10 M3") - chay TAT CA,
//      theo dung thu tu uu tien chuan, khong phai chi ma dau tien
//   2. CHE DO MODAL: dong chi co toa do ("X10 A20") lay lai ma G di chuyen
//      cua dong truoc do
//   3. Dong chi co F ("F1000") - dat toc do, khong bao loi
//   4. G20 (inch): toa do X duoc TU DONG nhan 25.4 de doi sang mm
static bool xu_ly_1_dong_gcode(char *dong_goc, bool nap)
{
    char dong[128];
    strncpy(dong, dong_goc, sizeof(dong) - 1);
    dong[sizeof(dong) - 1] = '\0';

    char chu[24];
    double so[24];
    int n = tach_token_gcode(dong, chu, so, 24);
    if (n == 0) return true;  // dong rong / chi co comment

    // ----- Gom TAT CA ma G va M tren dong (khong chi lay ma dau tien) -----
    int ma_g[12], so_ma_g = 0;
    int ma_m[12], so_ma_m = 0;
    bool co_x = false, co_y = false, co_f = false, co_p = false;
    double gt_x = 0, gt_y = 0, gt_f = 0, gt_p = 0;

    for (int i = 0; i < n; i++) {
        switch (chu[i]) {
            case 'G':
                if (so_ma_g < 12) ma_g[so_ma_g++] = (int)lround(so[i]);
                break;
            case 'M':
                if (so_ma_m < 12) ma_m[so_ma_m++] = (int)lround(so[i]);
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

    // ================== BUOC 1: cac ma G chi doi TRANG THAI ==================
    int ma_di_chuyen = -1;   // G0/G1/G2/G3 tren dong nay
    bool co_g92 = false, co_g4 = false, co_ve_goc = false;

    for (int i = 0; i < so_ma_g; i++) {
        int ma = ma_g[i];
        switch (ma) {
        case 0: case 1: case 2: case 3:
            ma_di_chuyen = ma;
            g_di_chuyen_modal = ma;   // nho lai cho cac dong chi co toa do
            if (ma == 2 || ma == 3) {
                printf("Canh bao: G%d duoc xap xi thanh duong thang toi diem cuoi "
                       "(phan cung chua ho tro noi suy cung tron that su).\n", ma);
            }
            break;

        case 4:  co_g4 = true; break;
        case 92: co_g92 = true; break;
        case 28: case 30: co_ve_goc = true; break;

        case 90: che_do_tuyet_doi = true; break;
        case 91: che_do_tuyet_doi = false; break;

        // G20 = inch, G21 = mm. Truoc day G20 bi BO QUA nen file inch chay
        // sai 25.4 lan. Nay he_so_don_vi duoc ap vao moi toa do TRUC THANG.
        case 20: he_so_don_vi = 25.4; break;
        case 21: he_so_don_vi = 1.0;  break;

        // ----- Cac ma chap nhan nhung KHONG lam gi (thuong gap trong file
        // G-code xuat tu phan mem CAM, khong ap dung cho may 2 truc nay) -----
        case 17: case 18: case 19:  // chon mat phang lam viec (XY/XZ/YZ)
        case 40: case 41: case 42:  // bu duong kinh dao cat
        case 43: case 49:           // bu chieu dai dao
        case 54: case 55: case 56:  // he toa do goc lam viec 1-6
        case 57: case 58: case 59:
        case 61: case 64:           // che do bam duong / lam muot duong
        case 80:                    // huy chu ky gia cong san
        case 93: case 94:           // che do dien giai feedrate
        case 98: case 99:           // che do rut dao giua cac lo
            break;

        default:
            printf("Loi: G%d chua duoc ho tro, dong '%s'\n", ma, dong_goc);
            return false;
        }
    }

    // ----- Kiem tra ma M truoc khi thuc hien bat ky thay doi nao -----
    for (int i = 0; i < so_ma_m; i++) {
        int ma = ma_m[i];
        if (ma != 0 && ma != 1 && ma != 2 && ma != 3 && ma != 4 && ma != 5 &&
            ma != 6 && ma != 7 && ma != 8 && ma != 9 && ma != 30) {
            printf("Loi: M%d chua duoc ho tro, dong '%s'\n", ma, dong_goc);
            return false;
        }
    }

    // ================== BUOC 2: toc do F ==================
    // Dong chi co F ("F1000") la hop le trong G-code chuan - dat toc do modal
    if (co_f) feed_dang_nap = gt_f;

    // ================== BUOC 3: M3/M4 - bat mo cat TRUOC khi di chuyen ==================
    for (int i = 0; i < so_ma_m; i++) {
        if (ma_m[i] == 3 || ma_m[i] == 4) {
            lenh_dong_co_t buoc = { .loai = LENH_PLASMA_ON };
            plasma_mo_phong = true;
            dua_buoc_vao_dich(buoc, nap);
        }
    }

    // ================== BUOC 4: G92 - dat lai toa do ==================
    if (co_g92) {
        if (!co_x && !co_y) {
            printf("Loi: G92 can it nhat X hoac A, dong '%s'\n", dong_goc);
            return false;
        }
        double dat_x = gt_x * he_so_don_vi;   // X la truc thang -> co doi inch
        double dat_a = gt_y;                  // A la GOC (do) -> KHONG doi inch
        if (co_x) vi_tri_mo_phong_x = dat_x;
        if (co_y) vi_tri_mo_phong_y = dat_a;

        lenh_dong_co_t buoc = {
            .loai = LENH_DAT_GOC,
            .dat_x = co_x, .gia_tri_x = dat_x,
            .dat_a = co_y, .gia_tri_a = dat_a,
        };
        dua_buoc_vao_dich(buoc, nap);
    }

    // ================== BUOC 5: di chuyen ==================
    // CHE DO MODAL: dong chi co toa do (khong co G) lay lai ma di chuyen cua
    // dong truoc - day la cu phap CHUAN, hau het file CAM deu dung.
    if (ma_di_chuyen < 0 && co_ve_goc == false && co_g92 == false &&
        (co_x || co_y)) {
        if (g_di_chuyen_modal < 0) {
            printf("Loi: dong '%s' co toa do nhung chua tung khai bao G0/G1 truoc do\n",
                   dong_goc);
            return false;
        }
        ma_di_chuyen = g_di_chuyen_modal;
    }

    if (co_ve_goc) {
        double rpm_home = (feed_dang_nap > 0) ? feed_dang_nap : RPM_HOME_MAC_DINH;
        double delta_x = 0.0 - vi_tri_mo_phong_x;
        double delta_a = 0.0 - vi_tri_mo_phong_y;
        vi_tri_mo_phong_x = 0.0;
        vi_tri_mo_phong_y = 0.0;
        lenh_dong_co_t buoc;
        if (tao_buoc_di_chuyen(delta_x, delta_a, rpm_home, &buoc)) {
            dua_buoc_vao_dich(buoc, nap);   // ve goc luon la chay khong tai
        }
    } else if (ma_di_chuyen >= 0 && (co_x || co_y)) {
        // Tinh delta CA 2 TRUC truoc, roi tao 1 lenh DUY NHAT de 2 truc
        // chay DONG THOI (Bresenham) -> duong cat cheo lien mach
        double delta_x = 0.0, delta_a = 0.0;

        if (co_x) {
            double gia_tri = gt_x * he_so_don_vi;   // doi inch -> mm neu dang G20
            double muc_tieu = che_do_tuyet_doi ? gia_tri : vi_tri_mo_phong_x + gia_tri;
            delta_x = muc_tieu - vi_tri_mo_phong_x;
            vi_tri_mo_phong_x = muc_tieu;
        }
        if (co_y) {
            // A la GOC (do) - KHONG nhan he so inch
            double muc_tieu = che_do_tuyet_doi ? gt_y : vi_tri_mo_phong_y + gt_y;
            delta_a = muc_tieu - vi_tri_mo_phong_y;
            vi_tri_mo_phong_y = muc_tieu;
        }

        if (fabs(delta_x) > 1e-9 || fabs(delta_a) > 1e-9) {
            if (feed_dang_nap <= 0) {
                printf("Loi: chua khai bao F truoc lenh di chuyen, dong '%s'\n", dong_goc);
                return false;
            }
            lenh_dong_co_t buoc;
            if (tao_buoc_di_chuyen(delta_x, delta_a, feed_dang_nap, &buoc)) {
                // G0 = chay nhanh khong tai -> luon co tang/giam toc.
                // G1/G2/G3 khi mo cat dang bat = doan CAT -> khong tang toc dan.
                buoc.dang_cat = (ma_di_chuyen != 0) && plasma_mo_phong;
                dua_buoc_vao_dich(buoc, nap);
            }
        }
    }

    // ================== BUOC 6: G4 - nghi (pierce delay) ==================
    if (co_g4) {
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
    }

    // ================== BUOC 7: cac ma M con lai - SAU khi di chuyen ==================
    for (int i = 0; i < so_ma_m; i++) {
        switch (ma_m[i]) {
        case 0:
        case 1: {
            lenh_dong_co_t buoc = { .loai = LENH_TAM_DUNG };
            dua_buoc_vao_dich(buoc, nap);
            break;
        }
        case 5:
        case 2:
        case 30: {
            // M2/M30 ket thuc chuong trinh: BAT BUOC tat mo cat de an toan
            lenh_dong_co_t buoc = { .loai = LENH_PLASMA_OFF };
            plasma_mo_phong = false;
            dua_buoc_vao_dich(buoc, nap);
            break;
        }
        default: break;   // M3/M4 da xu ly o BUOC 3; M6/M7/M8/M9 bo qua
        }
    }

    return true;
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
                tinh_noi_lien_chuong_trinh();  // noi lien cac doan cat lien tiep
                printf("OK_NAP: da nhan %d buoc, san sang RUN.\n", so_buoc_da_nap);
            }
            return;
        }
        if (!xu_ly_1_dong_gcode(dong, true)) {
            co_loi_khi_nap = true;
        }
        return;
    }

    // ----- CFG: cai dat nang cao (chan GPIO, so xung/vong, dao chieu...) -----
    // Dung tu file setting (cnc_settings.pyw), xem chu thich o cau_hinh_t o dau file.
    if (strncmp(dong_upper, "CFG;", 4) == 0) {
        if (strcmp(dong_upper, "CFG;GET") == 0) {
            cau_hinh_in_ra();
            return;
        }
        if (strcmp(dong_upper, "CFG;SAVE") == 0) {
            if (cau_hinh_luu_vao_nvs()) {
                printf("OK_CFG: da luu cau hinh vao flash. Neu vua doi CHAN GPIO, "
                       "gui CFG;REBOOT de ap dung.\n");
            } else {
                printf("Loi: khong luu duoc cau hinh vao flash.\n");
            }
            return;
        }
        if (strcmp(dong_upper, "CFG;RESET") == 0) {
            nvs_handle_t tay_cam;
            if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &tay_cam) == ESP_OK) {
                nvs_erase_all(tay_cam);
                nvs_commit(tay_cam);
                nvs_close(tay_cam);
            }
            cau_hinh_dat_mac_dinh();
            printf("OK_CFG: da xoa cau hinh flash, ve mac dinh. "
                   "Gui CFG;REBOOT de ap dung chan GPIO mac dinh.\n");
            return;
        }
        if (strcmp(dong_upper, "CFG;REBOOT") == 0) {
            printf("OK_CFG: dang khoi dong lai ESP32...\n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
            return;
        }
        if (strncmp(dong_upper, "CFG;PIN;", 8) == 0) {
            char ten[32];
            int so_gpio;
            if (sscanf(dong_upper + 8, "%31[^;];%d", ten, &so_gpio) != 2) {
                printf("Loi: cu phap CFG;PIN sai. Vi du: CFG;PIN;PUL_KEO_A;4\n");
                return;
            }
            if (so_gpio < 0 || so_gpio > 39) {
                printf("Loi: so GPIO phai trong khoang 0-39.\n");
                return;
            }
            int *dich = NULL;
            if      (strcmp(ten, "PUL_KEO_A") == 0)      dich = &g_cfg.pin_pul_keo_a;
            else if (strcmp(ten, "DIR_KEO_A") == 0)      dich = &g_cfg.pin_dir_keo_a;
            else if (strcmp(ten, "PUL_KEO_B") == 0)      dich = &g_cfg.pin_pul_keo_b;
            else if (strcmp(ten, "DIR_KEO_B") == 0)      dich = &g_cfg.pin_dir_keo_b;
            else if (strcmp(ten, "PUL_XOAY") == 0)       dich = &g_cfg.pin_pul_xoay;
            else if (strcmp(ten, "DIR_XOAY") == 0)       dich = &g_cfg.pin_dir_xoay;
            else if (strcmp(ten, "RELAY_PLASMA") == 0)   dich = &g_cfg.pin_relay_plasma;
            else if (strcmp(ten, "PLC_IN_START") == 0)   dich = &g_cfg.pin_plc_in_start;
            else if (strcmp(ten, "PLC_IN_STOP") == 0)    dich = &g_cfg.pin_plc_in_stop;
            else if (strcmp(ten, "PLC_IN_EMG") == 0)     dich = &g_cfg.pin_plc_in_emg;
            else if (strcmp(ten, "PLC_IN_LIMIT") == 0)   dich = &g_cfg.pin_plc_in_limit;
            else if (strcmp(ten, "PLC_OUT_READY") == 0)  dich = &g_cfg.pin_plc_out_ready;
            else if (strcmp(ten, "PLC_OUT_RUNNING") == 0) dich = &g_cfg.pin_plc_out_running;
            else if (strcmp(ten, "PLC_OUT_DONE") == 0)   dich = &g_cfg.pin_plc_out_done;
            else if (strcmp(ten, "PLC_OUT_FAULT") == 0)  dich = &g_cfg.pin_plc_out_fault;
            if (!dich) {
                printf("Loi: ten chan '%s' khong hop le.\n", ten);
                return;
            }
            *dich = so_gpio;
            printf("OK_CFG: da dat %s = GPIO%d (chi ap dung sau khi CFG;SAVE + CFG;REBOOT).\n",
                   ten, so_gpio);
            return;
        }
        // LUU Y: do dai truyen cho strncmp phai DUNG BANG do dai chuoi tien to.
        // "CFG;CAL;MICROSTEP;" dai 18, "CFG;CAL;MMVONG;" dai 15. Truoc day dung
        // 19 va 16 nen so sanh du 1 byte ('\0' cua chuoi mau) -> LUON that bai.
        if (strncmp(dong_upper, "CFG;CAL;MICROSTEP;", 18) == 0) {
            double gia_tri = atof(dong_upper + 18);
            if (gia_tri <= 0) { printf("Loi: gia tri microstep phai > 0.\n"); return; }
            g_cfg.microstep_moi_vong = gia_tri;
            printf("OK_CFG: microstep_moi_vong = %.2f (da ap dung ngay).\n", gia_tri);
            return;
        }
        if (strncmp(dong_upper, "CFG;CAL;MMVONG;", 15) == 0) {
            double gia_tri = atof(dong_upper + 15);
            if (gia_tri <= 0) { printf("Loi: gia tri mm/vong phai > 0.\n"); return; }
            g_cfg.mm_moi_vong_truc_x = gia_tri;
            printf("OK_CFG: mm_moi_vong_truc_x = %.4f (da ap dung ngay).\n", gia_tri);
            return;
        }
        if (strncmp(dong_upper, "CFG;RAMP;CAT;", 13) == 0) {
            g_cfg.ramp_khi_cat = (atoi(dong_upper + 13) != 0);
            printf("OK_CFG: ramp_khi_cat = %d (da ap dung ngay). %s\n", g_cfg.ramp_khi_cat,
                   g_cfg.ramp_khi_cat
                       ? "Doan cat SE tang toc dan (chi dung neu dong co bi mat buoc luc vao cat)."
                       : "Doan cat chay dung toc do ngay tu xung dau (mep cat dep hon).");
            return;
        }
        if (strncmp(dong_upper, "CFG;DAO;", 8) == 0) {
            char truc[16];
            int gia_tri;
            if (sscanf(dong_upper + 8, "%15[^;];%d", truc, &gia_tri) != 2) {
                printf("Loi: cu phap CFG;DAO sai. Vi du: CFG;DAO;KEOA;1\n");
                return;
            }
            bool *dich = NULL;
            if      (strcmp(truc, "KEOA") == 0) dich = &g_cfg.dao_keo_a;
            else if (strcmp(truc, "KEOB") == 0) dich = &g_cfg.dao_keo_b;
            else if (strcmp(truc, "XOAY") == 0) dich = &g_cfg.dao_xoay;
            if (!dich) {
                printf("Loi: truc '%s' khong hop le (KEOA/KEOB/XOAY).\n", truc);
                return;
            }
            *dich = (gia_tri != 0);
            printf("OK_CFG: dao chieu %s = %d (da ap dung ngay).\n", truc, (int)*dich);
            return;
        }
        printf("Loi: lenh CFG khong hop le: %s\n", dong);
        return;
    }

    // ----- Lenh dieu khien (khong o trong che do nap) -----
    if (strcmp(dong_upper, "PROG;BEGIN") == 0) {
        dang_nap_chuong_trinh = true;
        co_loi_khi_nap = false;
        so_buoc_da_nap = 0;
        che_do_tuyet_doi = true;
        feed_dang_nap = -1;
        plasma_mo_phong = false;             // dau chuong trinh coi nhu mo cat dang tat
        g_di_chuyen_modal = -1;              // chua co ma di chuyen nao truoc do
        he_so_don_vi = 1.0;                  // mac dinh mm (G21) cho toi khi gap G20
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

        // Dat bo dem TRUOC khi day vao hang doi: task dong co bat dau chay ngay
        // khi co buoc dau tien, neu dat sau thi no da tru mat mot so buoc roi
        so_buoc_con_lai = so_buoc_da_nap;
        dang_chay_chuong_trinh = true;

        int da_day = 0;
        for (int i = 0; i < so_buoc_da_nap; i++) {
            if (xQueueSend(hang_doi_lenh_dong_co, &chuong_trinh[i], pdMS_TO_TICKS(100)) == pdTRUE) {
                da_day++;
            } else {
                printf("Loi: hang doi day, chi day duoc %d/%d buoc.\n", da_day, so_buoc_da_nap);
                so_buoc_con_lai -= (so_buoc_da_nap - da_day);  // tru phan khong day duoc
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
        gpio_set_level(g_cfg.pin_relay_plasma, 0);  // AN TOAN: tat mo cat ngay lap tuc
        xQueueReset(hang_doi_lenh_dong_co);
        so_buoc_da_nap = 0;
        so_buoc_con_lai = 0;
        dang_chay_chuong_trinh = false;  // STOP da bao roi, khong bao XONG nua
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

    // ----- Nap cau hinh (chan GPIO, hieu chuan) tu flash (NVS), neu co -----
    esp_err_t loi_nvs = nvs_flash_init();
    if (loi_nvs == ESP_ERR_NVS_NO_FREE_PAGES || loi_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        loi_nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(loi_nvs);
    cau_hinh_doc_tu_nvs();
    cau_hinh_in_ra();

    cau_hinh_ngo_ra();
    cau_hinh_ngo_vao();
    cau_hinh_uart_pc();

    che_do_tuyet_doi = true;
    feed_dang_nap = -1;
    plasma_mo_phong = false;
    g_di_chuyen_modal = -1;
    he_so_don_vi = 1.0;

    hang_doi_lenh_dong_co = xQueueCreate(MAX_BUOC_CHUONG_TRINH, sizeof(lenh_dong_co_t));

    gpio_set_level(g_cfg.pin_plc_out_ready, 1);

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
