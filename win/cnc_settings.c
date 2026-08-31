/* CAI DAT NANG CAO - chan GPIO, so xung/vong, dao chieu truc...
 *
 * Tach RIENG khoi phan mem dung hang ngay vi day la cai dat lien quan truc
 * tiep PHAN CUNG / FIRMWARE - chi nguoi lap dat may hoac sua driver moi can
 * dung, khong dung trong luc van hanh cat hang ngay.
 *
 * Bo cuc: chia THEO THE cho de nhin va khong tran man hinh
 *   The 1 "Che do"              : chon 1 trong 3 che do lam viec + duong kinh
 *   The 2 "Truc & Driver"       : chan PUL/DIR 3 dong co + relay plasma + dao chieu
 *   The 3 "Bang dieu khien tay" : EMG + 2 cong tac hanh trinh + 7 nut + den bao
 *   The 4 "Hieu chuan"          : so xung/vong, mm/vong, dieu khien tay, ramp
 *
 * Cach hoat dong: gui lenh CFG;... qua Serial xuong ESP32. ESP32 luu cau hinh
 * vao NVS (bo nho flash noi bo), KHONG mat khi mat dien / rut usb.
 *   - Thay doi CHAN GPIO   -> can bam "Luu vao flash" RIENG "Khoi dong lai"
 *     (vi lien quan gpio_config()/ngat phan cung, doi giua luc chay khong an toan)
 *   - Thay doi HIEU CHUAN (xung/vong, mm/vong) va DAO CHIEU -> ap dung NGAY,
 *     khong can khoi dong lai, nhung van nen bam "Luu vao flash" de giu lai.
 */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "tien_ich.h"
#include "../loi_c/ket_noi.h"
#include "../loi_c/loi_chung.h"

#define TEN_PHAN_MEM "Cai dat nang cao - May Cat Ong (CHAN GPIO / HIEU CHUAN)"
#define BAUD_RATE 115200

/* Ky tu go vao o nhap de BO / TAT mot chan */
#define KY_TU_TAT_CHAN "*"

/* ====================================================================== */
/* BANG CHAN                                                              */
/* ====================================================================== */
typedef struct {
    const char *ten;        /* ten dung trong lenh CFG;PIN;<TEN>;<so> */
    const char *mo_ta;
    const char *khoa_cfg;   /* ten khoa trong dong "CFG: ..." tu ESP32 */
    const char *khoa_th;    /* ten khoa kieu tin hieu, NULL neu la ngo ra */
    int nhom;               /* 0..5 */
} ChanCauHinh;

enum { N_TRUC = 0, N_PLASMA, N_AN_TOAN, N_DI_CHUYEN, N_NUT_LENH, N_DEN, SO_NHOM };

static const ChanCauHinh CAC_CHAN[] = {
    /* --- Truc dong co --- */
    { "PUL_KEO_A", "Xung (PUL) truc keo - dong co A", "pul_keo_a", NULL, N_TRUC },
    { "DIR_KEO_A", "Chieu (DIR) truc keo - dong co A", "dir_keo_a", NULL, N_TRUC },
    { "PUL_KEO_B", "Xung (PUL) truc keo - dong co B", "pul_keo_b", NULL, N_TRUC },
    { "DIR_KEO_B", "Chieu (DIR) truc keo - dong co B", "dir_keo_b", NULL, N_TRUC },
    { "PUL_XOAY",  "Xung (PUL) truc xoay",             "pul_xoay",  NULL, N_TRUC },
    { "DIR_XOAY",  "Chieu (DIR) truc xoay",            "dir_xoay",  NULL, N_TRUC },
    /* --- Plasma --- */
    { "RELAY_PLASMA", "Relay bat/tat mo cat plasma", "relay_plasma", NULL, N_PLASMA },
    /* --- An toan --- */
    { "PLC_IN_EMG",    "EMG - dung khan cap",        "plc_in_emg",    "th_emg",         N_AN_TOAN },
    { "LIMIT_X_AM",    "Cong tac h.trinh dau X-",    "limit_x_am",    "th_limit_am",    N_AN_TOAN },
    { "LIMIT_X_DUONG", "Cong tac h.trinh dau X+",    "limit_x_duong", "th_limit_duong", N_AN_TOAN },
    /* --- 4 nut di chuyen --- */
    { "NUT_X_TIEN",   "Nut X+  (keo ong ra)",   "nut_x_tien",   "th_x_tien",   N_DI_CHUYEN },
    { "NUT_X_LUI",    "Nut X-  (keo ong vao)",  "nut_x_lui",    "th_x_lui",    N_DI_CHUYEN },
    { "NUT_A_THUAN",  "Nut A+  (xoay thuan)",   "nut_a_thuan",  "th_a_thuan",  N_DI_CHUYEN },
    { "NUT_A_NGHICH", "Nut A-  (xoay nghich)",  "nut_a_nghich", "th_a_nghich", N_DI_CHUYEN },
    /* --- Nut lenh --- */
    { "NUT_START", "Nut START (= chay tiep)",   "nut_start", "th_start", N_NUT_LENH },
    { "NUT_STOP",  "Nut STOP (= tam dung)",     "nut_stop",  "th_stop",  N_NUT_LENH },
    { "NUT_NHICH", "Nut NHICH (giu de nhich)",  "nut_nhich", "th_nhich", N_NUT_LENH },
    /* --- Den bao --- */
    { "DEN_SAN_SANG",  "Den SAN SANG",  "den_san_sang",  NULL, N_DEN },
    { "DEN_DANG_CHAY", "Den DANG CHAY", "den_dang_chay", NULL, N_DEN },
    { "DEN_XONG",      "Den XONG",      "den_xong",      NULL, N_DEN },
    { "DEN_LOI",       "Den LOI",       "den_loi",       NULL, N_DEN }
};
#define SO_CHAN ((int)(sizeof(CAC_CHAN) / sizeof(CAC_CHAN[0])))

/* ====================================================================== */
/* SO DO CHAN GOI Y SAN CHO TUNG LOAI BOARD                               */
/* ====================================================================== */
/* ESP32 devkit goc (30/38 chan): chi con 6 chan "sach" nen 4 den bao phai
 * TAT, nut NHICH va cong tac hanh trinh X+ phai dung GPIO34/35 kem dien tro
 * 10k ngoai. */
static const char *SO_DO_ESP32_GOC[SO_CHAN] = {
    "4", "13", "14", "16", "25", "26",
    "19",
    "32", "33", "35",
    "23", "27", "17", "18",
    "21", "22", "34",
    "*", "*", "*", "*"
};

/* ESP32-S3-WROOM-1 N16R8 (loai co 2 cong USB Type-C).
 * TRANH: 33-37 (PSRAM Octal cua ban R8 - dung la treo may), 19/20 (USB),
 *        43/44 (UART0 nap chuong trinh), 0/3/45/46 (chan strapping),
 *        48 (den RGB tren board), 26-32 (flash, khong dua ra chan).
 * Con lai du chan cho ca 4 den bao. Cac chan nay deu co dien tro keo len ben
 * trong nen KHONG can lap dien tro ngoai nhu ban ESP32 goc. */
static const char *SO_DO_ESP32_S3[SO_CHAN] = {
    "4", "5", "6", "7", "15", "16",
    "17",
    "18", "8", "9",
    "10", "11", "12", "13",
    "14", "21", "1",
    "2", "42", "41", "40"
};

static const char *TEN_CHE_DO[4] = {
    "",
    "Che do 1 - MM va DO",
    "Che do 2 - MM va DO, giu deu toc do mo cat",
    "Che do 3 - FULL MM"
};

static const char *MO_TA_CHE_DO[4] = {
    "",
    "Truc X nhap bang mm, truc A nhap bang do. F la toc do VONG/PHUT cua "
    "dong co.\r\nThoi gian di chuyen lay theo truc cham nhat. KHONG can khai "
    "bao duong kinh ong.\r\nDay la che do goc.",
    "Van nhap X bang mm va A bang do NHUNG phai khai bao duong kinh ong. F la "
    "toc do\r\nMO CAT LUOT TREN MAT ONG, don vi mm/phut. He thong quy doi goc "
    "xoay ra chieu dai\r\ncung that roi tinh thoi gian theo quang duong that "
    "=> 2 truc phoi hop dung ty le,\r\nmo cat luot voi toc do khong doi du "
    "duong cat cheo bao nhieu.",
    "Nhu che do 2 nhung truc A cung nhap bang MM (chieu dai cung tren mat ong,"
    "\r\nkieu 'trai phang'), khong phai do. Hop voi file CAM xuat ra dang trai "
    "phang.\r\nCung can khai bao duong kinh ong."
};

/* --- Ma o dieu khien --- */
enum {
    ID_O_CONG = 100, ID_NUT_LAM_MOI, ID_NUT_KET_NOI, ID_NUT_DOC_CFG,
    ID_NUT_SO_DO_GOC, ID_NUT_SO_DO_S3,
    ID_THE = 120,
    ID_CHE_DO_1 = 130, ID_CHE_DO_2, ID_CHE_DO_3,
    ID_O_DUONG_KINH, ID_NUT_GUI_DK,
    ID_O_MICROSTEP, ID_NUT_GUI_MICROSTEP,
    ID_O_MMVONG, ID_NUT_GUI_MMVONG,
    ID_O_TOC_DO_TAY, ID_NUT_GUI_TOC_DO_TAY,
    ID_O_NHICH_MM, ID_NUT_GUI_NHICH_MM,
    ID_O_NHICH_DO, ID_NUT_GUI_NHICH_DO,
    ID_DAO_KEOA, ID_DAO_KEOB, ID_DAO_XOAY, ID_RAMP_CAT,
    ID_NUT_GUI_NHOM_TRUC, ID_NUT_GUI_NHOM_PLC,
    ID_NUT_LUU_FLASH, ID_NUT_REBOOT, ID_NUT_RESET,
    ID_LOG = 200,
    ID_CHAN_O = 300,          /* + i */
    ID_CHAN_GUI = 400,        /* + i */
    ID_CHAN_GND = 500         /* + i */
};

#define WM_ESP32   (WM_APP + 1)
#define WM_NHAT_KY (WM_APP + 2)

/* ====================================================================== */
/* TRANG THAI                                                             */
/* ====================================================================== */
static struct {
    HINSTANCE hinst;
    HWND chinh;
    KetNoi *may;
    char cong_com[CO_TEN_CONG];
    int dang_ket_noi;

    HWND o_cong, nut_ket_noi, the, log;
    HWND o_chan[SO_CHAN], nut_chan[SO_CHAN], gnd_chan[SO_CHAN];
    HWND che_do[3], o_duong_kinh;
    HWND o_microstep, o_mmvong, o_toc_do_tay, o_nhich_mm, o_nhich_do;
    HWND dao[3], ramp_cat;
    HWND nut_gui_nhom[2];
    HWND nut_khac[16];
    int  so_nut_khac;

    HBRUSH nen_khung, nen_nen;
    int che_do_hien;
} g;

/* Vi tri cac khung ve tay, tinh lai trong bo_tri() */
static RECT o_khung_nhom[SO_NHOM];
static int  y_the_trong;

static void bo_tri(void);
static void ghi_log(const char *dinh_dang, ...);

/* ====================================================================== */
/* KET NOI                                                                */
/* ====================================================================== */
static void tu_luong_dong(void *ctx, const char *dong)
{
    (void)ctx;
    PostMessageA(g.chinh, WM_ESP32, 0, (LPARAM)_strdup(dong));
}
static void tu_luong_nhat_ky(void *ctx, const char *chu)
{
    (void)ctx;
    PostMessageA(g.chinh, WM_NHAT_KY, 0, (LPARAM)_strdup(chu));
}

static int gui_qua_serial(const char *lenh)
{
    if (!g.dang_ket_noi) {
        canh_bao(g.chinh, "Chua ket noi", "Vui long ket noi Serial truoc.");
        return 0;
    }
    if (!ket_noi_gui(g.may, lenh)) {
        bao_loi(g.chinh, "Loi gui lenh", "Mat ket noi cong COM.");
        return 0;
    }
    ghi_log("[Gui] %s", lenh);
    return 1;
}

static void nap_danh_sach_cong(void)
{
    char ten[SO_CONG_TOI_DA][CO_TEN_CONG];
    int n = cong_liet_ke(ten, SO_CONG_TOI_DA), i;
    char dang_chon[CO_TEN_CONG];
    GetWindowTextA(g.o_cong, dang_chon, sizeof(dang_chon));
    SendMessageA(g.o_cong, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < n; i++)
        SendMessageA(g.o_cong, CB_ADDSTRING, 0, (LPARAM)ten[i]);
    if (!dang_chon[0] && n > 0) SetWindowTextA(g.o_cong, ten[0]);
}

static void ket_noi_may(void)
{
    char loi[CO_LOI] = "";
    GetWindowTextA(g.o_cong, g.cong_com, sizeof(g.cong_com));
    /* baud co dinh 115200: chuong trinh nay chi gui lenh cau hinh, khong nap
     * chuong trinh nen khong can nang toc do duong truyen */
    if (ket_noi_mo(g.may, g.cong_com, BAUD_RATE, loi) != 0) {
        bao_loi(g.chinh, "Loi ket noi", "Khong the ket noi toi %s:\n%s",
                g.cong_com, loi);
        return;
    }
    g.dang_ket_noi = 1;
    SetWindowTextA(g.nut_ket_noi, "Ngat ket noi");
    ghi_log("[He thong] Da ket noi toi %s", g.cong_com);
    InvalidateRect(g.chinh, NULL, TRUE);
    gui_qua_serial("CFG;GET");
}

static void ngat_ket_noi(void)
{
    ket_noi_dong(g.may);
    g.dang_ket_noi = 0;
    SetWindowTextA(g.nut_ket_noi, "Ket noi");
    ghi_log("[He thong] Da ngat ket noi");
    InvalidateRect(g.chinh, NULL, TRUE);
}

/* ====================================================================== */
/* DOC CAU HINH TU ESP32                                                  */
/* ====================================================================== */
static void cap_nhat_chu_vi(void);

static void dat_gia_tri_tu_cfg(const char *ten, const char *gia_tri)
{
    int i;
    for (i = 0; i < SO_CHAN; i++) {
        if (strcmp(ten, CAC_CHAN[i].khoa_cfg) == 0) {
            /* Chan bi tat hien thi bang '*' cho de nhin */
            dat_chu(g.o_chan[i], strcmp(gia_tri, "-1") == 0
                                 ? KY_TU_TAT_CHAN : gia_tri);
            return;
        }
        if (CAC_CHAN[i].khoa_th && strcmp(ten, CAC_CHAN[i].khoa_th) == 0) {
            SendMessageA(g.gnd_chan[i], BM_SETCHECK,
                         (WPARAM)(strcmp(gia_tri, "1") == 0 ? BST_CHECKED
                                                            : BST_UNCHECKED), 0);
            return;
        }
    }
    if (strcmp(ten, "microstep_moi_vong") == 0)      dat_chu(g.o_microstep, gia_tri);
    else if (strcmp(ten, "mm_moi_vong_truc_x") == 0) dat_chu(g.o_mmvong, gia_tri);
    else if (strcmp(ten, "toc_do_tay_rpm") == 0)     dat_chu(g.o_toc_do_tay, gia_tri);
    else if (strcmp(ten, "nhich_mm") == 0)           dat_chu(g.o_nhich_mm, gia_tri);
    else if (strcmp(ten, "nhich_do") == 0)           dat_chu(g.o_nhich_do, gia_tri);
    else if (strcmp(ten, "duong_kinh_ong") == 0) {
        dat_chu(g.o_duong_kinh, gia_tri);
        cap_nhat_chu_vi();
    } else if (strcmp(ten, "dao_keo_a") == 0 || strcmp(ten, "dao_keo_b") == 0 ||
               strcmp(ten, "dao_xoay") == 0) {
        int k = strcmp(ten, "dao_keo_a") == 0 ? 0
              : strcmp(ten, "dao_keo_b") == 0 ? 1 : 2;
        SendMessageA(g.dao[k], BM_SETCHECK,
                     (WPARAM)(strcmp(gia_tri, "1") == 0 ? BST_CHECKED
                                                        : BST_UNCHECKED), 0);
    } else if (strcmp(ten, "ramp_khi_cat") == 0) {
        SendMessageA(g.ramp_cat, BM_SETCHECK,
                     (WPARAM)(strcmp(gia_tri, "1") == 0 ? BST_CHECKED
                                                        : BST_UNCHECKED), 0);
    } else if (strcmp(ten, "che_do") == 0) {
        int m = atoi(gia_tri);
        if (m >= 1 && m <= 3) {
            int k;
            g.che_do_hien = m;
            for (k = 0; k < 3; k++)
                SendMessageA(g.che_do[k], BM_SETCHECK,
                             (WPARAM)(k == m - 1 ? BST_CHECKED : BST_UNCHECKED), 0);
        }
    }
}

/* Doc cac dong "CFG: ten=gia_tri ten2=gia_tri2 ..." ma firmware gui ve */
static void thu_doc_cfg(const char *dong)
{
    const char *p;
    if (strncmp(dong, "CFG:", 4) != 0) return;
    p = dong + 4;
    while (*p) {
        char ten[48], gt[48];
        int n = 0;
        while (*p == ' ') p++;
        if (!*p) break;
        while (*p && *p != '=' && *p != ' ' && n < (int)sizeof(ten) - 1)
            ten[n++] = *p++;
        ten[n] = '\0';
        if (*p != '=') { while (*p && *p != ' ') p++; continue; }
        p++;
        n = 0;
        while (*p && *p != ' ' && n < (int)sizeof(gt) - 1) gt[n++] = *p++;
        gt[n] = '\0';
        dat_gia_tri_tu_cfg(ten, gt);
    }
}

/* ====================================================================== */
/* GUI LENH                                                               */
/* ====================================================================== */
/* Doi noi dung o nhap sang so chan gui xuong ESP32.
 * Chap nhan: "*" hoac de trong = BO CHAN (gui -1), hoac so 0..48.
 * Tra -2 neu khong hop le. */
static int doi_o_nhap_sang_so_chan(HWND o)
{
    char chu[32], *p;
    int so;
    lay_chu(o, chu, sizeof(chu));
    p = chu;
    while (*p == ' ' || *p == '\t') p++;
    {
        char *cuoi = p + strlen(p);
        while (cuoi > p && (cuoi[-1] == ' ' || cuoi[-1] == '\t')) *--cuoi = '\0';
    }
    if (!*p || strcmp(p, KY_TU_TAT_CHAN) == 0 || strcmp(p, "-1") == 0) return -1;
    {
        char *ket = NULL;
        long v = strtol(p, &ket, 10);
        if (ket == p || *ket) return -2;
        so = (int)v;
    }
    /* ESP32 goc: GPIO0-39. ESP32-S3: toi GPIO48 */
    return (so >= 0 && so <= 48) ? so : -2;
}

static void gui_1_chan(int i)
{
    char lenh[64];
    int so = doi_o_nhap_sang_so_chan(g.o_chan[i]);
    if (so == -2) {
        canh_bao(g.chinh, "Sai du lieu",
                 "Chan %s phai la so 0-48, hoac go '%s' de BO chan nay.",
                 CAC_CHAN[i].ten, KY_TU_TAT_CHAN);
        return;
    }
    snprintf(lenh, sizeof(lenh), "CFG;PIN;%s;%d", CAC_CHAN[i].ten, so);
    gui_qua_serial(lenh);
}

static void gui_nhom_chan(const int *nhom, int so_nhom)
{
    int i, j;
    for (i = 0; i < SO_CHAN; i++) {
        int thuoc = 0;
        for (j = 0; j < so_nhom; j++) if (CAC_CHAN[i].nhom == nhom[j]) thuoc = 1;
        if (!thuoc) continue;
        {
            int so = doi_o_nhap_sang_so_chan(g.o_chan[i]);
            char lenh[64];
            if (so == -2) continue;
            snprintf(lenh, sizeof(lenh), "CFG;PIN;%s;%d", CAC_CHAN[i].ten, so);
            gui_qua_serial(lenh);
            ngu_ms(30);
        }
    }
}

/* Dien san so do chan goi y vao cac o nhap (CHUA gui xuong ESP32).
 *
 * Nguoi dung xem lai roi tu bam "Gui tat ca chan trong the nay" o tung the.
 * Lam vay an toan hon la gui thang, vi doi nham chan dieu khien dong co khi
 * may dang cam dien co the lam dong co chay bat ngo. */
static void dien_so_do(const char *ten_board, const char *so_do[])
{
    int i;
    if (!hoi_co_khong(g.chinh, "Dien san so do chan",
                      "Dien so do chan goi y cho %s vao cac o nhap?\n\n"
                      "Chi DIEN VAO O, CHUA gui xuong ESP32. Xem lai xong hay "
                      "bam 'Gui tat ca chan trong the nay' o tung the, roi Luu "
                      "vao flash va Khoi dong lai.", ten_board))
        return;
    for (i = 0; i < SO_CHAN; i++) dat_chu(g.o_chan[i], so_do[i]);
    ghi_log("[He thong] Da dien so do chan goi y cho %s. Xem lai roi bam "
            "'Gui tat ca chan' o tung the.", ten_board);
}

/* Gui kieu tin hieu cua 1 ngo vao (bat = kich bang GND). */
static void gui_tin_hieu(int i)
{
    char lenh[64];
    int bat = SendMessageA(g.gnd_chan[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
    snprintf(lenh, sizeof(lenh), "CFG;TINHIEU;%s;%d", CAC_CHAN[i].ten, bat);
    gui_qua_serial(lenh);
}

/* Gui 1 thong so dang so qua Serial, kiem tra hop le truoc. */
static void gui_so(const char *lenh_goc, HWND o)
{
    char lenh[96];
    double gt;
    if (lay_so(o, &gt) != 0 || gt <= 0) {
        canh_bao(g.chinh, "Sai du lieu", "Gia tri phai la so > 0.");
        return;
    }
    snprintf(lenh, sizeof(lenh), "%s;%g", lenh_goc, gt);
    gui_qua_serial(lenh);
}

static void cap_nhat_chu_vi(void)
{
    InvalidateRect(g.chinh, NULL, FALSE);
}

static void gui_duong_kinh(void)
{
    char lenh[64];
    double d;
    if (lay_so(g.o_duong_kinh, &d) != 0 || d <= 0) {
        canh_bao(g.chinh, "Sai du lieu", "Duong kinh ong phai la so > 0.");
        return;
    }
    snprintf(lenh, sizeof(lenh), "CFG;DUONGKINH;%g", d);
    gui_qua_serial(lenh);
    cap_nhat_chu_vi();
}

static void gui_che_do(int m)
{
    char lenh[32];
    g.che_do_hien = m;
    snprintf(lenh, sizeof(lenh), "CFG;MODE;%d", m);
    gui_qua_serial(lenh);
}

static void gui_dao(int k)
{
    static const char *ma[3] = { "KEOA", "KEOB", "XOAY" };
    char lenh[48];
    int bat = SendMessageA(g.dao[k], BM_GETCHECK, 0, 0) == BST_CHECKED;
    snprintf(lenh, sizeof(lenh), "CFG;DAO;%s;%d", ma[k], bat);
    gui_qua_serial(lenh);
}

static void gui_ramp_cat(void)
{
    char lenh[48];
    int bat = SendMessageA(g.ramp_cat, BM_GETCHECK, 0, 0) == BST_CHECKED;
    snprintf(lenh, sizeof(lenh), "CFG;RAMP;CAT;%d", bat);
    gui_qua_serial(lenh);
}

/* ====================================================================== */
/* NHAT KY                                                                */
/* ====================================================================== */
static void ghi_log(const char *dinh_dang, ...)
{
    char chu[1024], ca_dong[1100];
    va_list ds;
    GETTEXTLENGTHEX gt;
    LONG dai;
    va_start(ds, dinh_dang);
    vsnprintf(chu, sizeof(chu), dinh_dang, ds);
    va_end(ds);
    if (!g.log) return;
    snprintf(ca_dong, sizeof(ca_dong), "%s\r\n", chu);
    gt.flags = GTL_DEFAULT;
    gt.codepage = CP_ACP;
    dai = (LONG)SendMessageA(g.log, EM_GETTEXTLENGTHEX, (WPARAM)&gt, 0);
    SendMessageA(g.log, EM_SETSEL, (WPARAM)dai, (LPARAM)dai);
    SendMessageA(g.log, EM_REPLACESEL, FALSE, (LPARAM)ca_dong);
    SendMessageA(g.log, EM_SCROLLCARET, 0, 0);
}

/* ====================================================================== */
/* TAO O DIEU KHIEN                                                       */
/* ====================================================================== */
static HWND them_nut(HWND cha, const char *chu, int id)
{
    HWND h = CreateWindowExA(0, "BUTTON", chu, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                             0, 0, 10, 10, cha, (HMENU)(INT_PTR)id, g.hinst, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    return h;
}

static HWND them_o(HWND cha, int id)
{
    HWND h = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                             0, 0, 10, 10, cha, (HMENU)(INT_PTR)id, g.hinst, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    return h;
}

static HWND them_danh_dau(HWND cha, const char *chu, int id)
{
    HWND h = CreateWindowExA(0, "BUTTON", chu,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                             0, 0, 10, 10, cha, (HMENU)(INT_PTR)id, g.hinst, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    return h;
}

static HWND them_o_tron(HWND cha, const char *chu, int id, int dau_nhom)
{
    HWND h = CreateWindowExA(0, "BUTTON", chu,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                             BS_AUTORADIOBUTTON | (dau_nhom ? WS_GROUP : 0),
                             0, 0, 10, 10, cha, (HMENU)(INT_PTR)id, g.hinst, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    return h;
}

static void tao_o_dieu_khien(HWND h)
{
    static const char *ten_the[4] = { "  Che do  ", "  Truc & Driver  ",
                                      "  Bang dieu khien tay  ", "  Hieu chuan  " };
    static const char *ten_dao[3] = { "Dao chieu dong co KEO A",
                                      "Dao chieu dong co KEO B",
                                      "Dao chieu dong co XOAY" };
    TCITEMA m;
    int i;

    /* ---- Khung ket noi ---- */
    g.o_cong = CreateWindowExA(0, "COMBOBOX", "",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                               CBS_DROPDOWN | WS_VSCROLL,
                               0, 0, 120, 240, h, (HMENU)(INT_PTR)ID_O_CONG,
                               g.hinst, NULL);
    SendMessageA(g.o_cong, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    SetWindowTextA(g.o_cong, g.cong_com);
    nap_danh_sach_cong();

    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Lam moi", ID_NUT_LAM_MOI);
    g.nut_ket_noi = them_nut(h, "Ket noi", ID_NUT_KET_NOI);
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Doc cau hinh (CFG;GET)", ID_NUT_DOC_CFG);
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "ESP32 goc", ID_NUT_SO_DO_GOC);
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "ESP32-S3 N16R8", ID_NUT_SO_DO_S3);

    /* ---- The ---- */
    g.the = CreateWindowExA(0, WC_TABCONTROLA, "",
                            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                            0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_THE, g.hinst, NULL);
    SendMessageA(g.the, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    memset(&m, 0, sizeof(m));
    m.mask = TCIF_TEXT;
    for (i = 0; i < 4; i++) {
        m.pszText = (char *)ten_the[i];
        SendMessageA(g.the, TCM_INSERTITEMA, (WPARAM)i, (LPARAM)&m);
    }

    /* ---- The 1: che do ---- */
    for (i = 0; i < 3; i++)
        g.che_do[i] = them_o_tron(h, TEN_CHE_DO[i + 1], ID_CHE_DO_1 + i, i == 0);
    SendMessageA(g.che_do[0], BM_SETCHECK, BST_CHECKED, 0);
    g.o_duong_kinh = them_o(h, ID_O_DUONG_KINH);
    dat_chu(g.o_duong_kinh, "60");
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Gui", ID_NUT_GUI_DK);

    /* ---- Chan GPIO (dung chung cho the 2 va the 3) ---- */
    for (i = 0; i < SO_CHAN; i++) {
        g.o_chan[i] = them_o(h, ID_CHAN_O + i);
        g.nut_chan[i] = them_nut(h, "Gui", ID_CHAN_GUI + i);
        /* Chi ngo VAO moi co cong tac gat doi kieu tin hieu */
        if (CAC_CHAN[i].khoa_th) {
            g.gnd_chan[i] = them_danh_dau(h, "GND", ID_CHAN_GND + i);
            SendMessageA(g.gnd_chan[i], BM_SETCHECK, BST_CHECKED, 0);
        }
    }
    for (i = 0; i < 3; i++) g.dao[i] = them_danh_dau(h, ten_dao[i], ID_DAO_KEOA + i);
    g.nut_gui_nhom[0] = them_nut(h, "Gui tat ca chan trong the nay",
                                 ID_NUT_GUI_NHOM_TRUC);
    g.nut_gui_nhom[1] = them_nut(h, "Gui tat ca chan trong the nay",
                                 ID_NUT_GUI_NHOM_PLC);

    /* ---- The 4: hieu chuan ---- */
    g.o_microstep   = them_o(h, ID_O_MICROSTEP);   dat_chu(g.o_microstep, "1600");
    g.o_mmvong      = them_o(h, ID_O_MMVONG);      dat_chu(g.o_mmvong, "5.0");
    g.o_toc_do_tay  = them_o(h, ID_O_TOC_DO_TAY);  dat_chu(g.o_toc_do_tay, "30");
    g.o_nhich_mm    = them_o(h, ID_O_NHICH_MM);    dat_chu(g.o_nhich_mm, "1.0");
    g.o_nhich_do    = them_o(h, ID_O_NHICH_DO);    dat_chu(g.o_nhich_do, "1.0");
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Gui", ID_NUT_GUI_MICROSTEP);
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Gui", ID_NUT_GUI_MMVONG);
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Gui", ID_NUT_GUI_TOC_DO_TAY);
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Gui", ID_NUT_GUI_NHICH_MM);
    g.nut_khac[g.so_nut_khac++] = them_nut(h, "Gui", ID_NUT_GUI_NHICH_DO);
    g.ramp_cat = them_danh_dau(h, "Cho doan CAT tang toc dan (mac dinh TAT)",
                               ID_RAMP_CAT);

    /* ---- Hang nut luu ---- */
    them_nut(h, "LUU VAO FLASH", ID_NUT_LUU_FLASH);
    them_nut(h, "KHOI DONG LAI ESP32", ID_NUT_REBOOT);
    them_nut(h, "VE MAC DINH", ID_NUT_RESET);

    /* ---- Khung nhat ky ---- */
    g.log = CreateWindowExA(WS_EX_CLIENTEDGE, "RICHEDIT50W", "",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                            ES_READONLY | ES_AUTOVSCROLL,
                            0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_LOG,
                            g.hinst, NULL);
    if (!g.log)
        g.log = CreateWindowExA(WS_EX_CLIENTEDGE, "RichEdit20A", "",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                ES_READONLY | ES_AUTOVSCROLL,
                                0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_LOG,
                                g.hinst, NULL);
    if (g.log) {
        CHARFORMAT2A cf;
        SendMessageA(g.log, WM_SETFONT, (WPARAM)PC_DEU, TRUE);
        SendMessageA(g.log, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(0x11, 0x11, 0x11));
        memset(&cf, 0, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = RGB(0x00, 0xff, 0x00);
        SendMessageA(g.log, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    }
}

/* ====================================================================== */
/* BO TRI                                                                 */
/* ====================================================================== */
#define CAO_CANH_BAO  56
#define CAO_KET_NOI   44
#define CAO_LOG       120
#define CAO_HANG_LUU  38
#define ROG_MO_TA     190
#define ROG_TEN_CHAN  104

/* Vi tri mot hang chan trong khung nhom: tra ve toa do goc trai */
static void cho_hang_chan(const RECT *khung, int chi_so_trong_nhom, int so_trong_nhom,
                          int *x, int *y)
{
    int nua = (so_trong_nhom + 1) / 2;
    int cot = chi_so_trong_nhom < nua ? 0 : 1;
    int hang = cot == 0 ? chi_so_trong_nhom : chi_so_trong_nhom - nua;
    int rong_cot = (khung->right - khung->left - 16) / 2;
    *x = khung->left + 8 + cot * rong_cot;
    *y = khung->top + 20 + hang * 26;
}

static int so_chan_trong_nhom(int nhom)
{
    int i, n = 0;
    for (i = 0; i < SO_CHAN; i++) if (CAC_CHAN[i].nhom == nhom) n++;
    return n;
}

static void bo_tri(void)
{
    RECT rc;
    int rong, cao, y, i, the;
    int x_the, r_the, y_luu, y_log;

    if (!g.chinh) return;
    GetClientRect(g.chinh, &rc);
    rong = rc.right;
    cao = rc.bottom;
    if (rong < 300 || cao < 300) return;
    the = (int)SendMessageA(g.the, TCM_GETCURSEL, 0, 0);

    /* ---- Khung ket noi ---- */
    y = CAO_CANH_BAO + 6;
    dat_cho(g.o_cong, 80, y + 8, 120, 240);
    dat_cho(g.nut_khac[0], 208, y + 7, 70, 24);      /* Lam moi */
    dat_cho(g.nut_ket_noi, 284, y + 7, 90, 24);
    dat_cho(g.nut_khac[1], 520, y + 7, 160, 24);     /* Doc cau hinh */
    dat_cho(g.nut_khac[2], 838, y + 7, 90, 24);      /* ESP32 goc */
    dat_cho(g.nut_khac[3], 934, y + 7, 120, 24);     /* ESP32-S3 */

    /* ---- The ---- */
    y_log = cao - CAO_LOG - 4;
    y_luu = y_log - CAO_HANG_LUU - 4;
    y += CAO_KET_NOI + 4;
    dat_cho(g.the, 8, y, rong - 16, y_luu - y - 4);
    x_the = 16;
    r_the = rong - 24;
    y_the_trong = y + 26;

    /* Cac khung nhom trong the dang mo */
    for (i = 0; i < SO_NHOM; i++) {
        o_khung_nhom[i].left = o_khung_nhom[i].right = 0;
        o_khung_nhom[i].top = o_khung_nhom[i].bottom = 0;
    }
    if (the == 1) {
        int yy = y_the_trong + 6;
        int cac[3] = { N_TRUC, N_PLASMA, -1 };
        for (i = 0; i < 2; i++) {
            int n = so_chan_trong_nhom(cac[i]);
            int hang = (n + 1) / 2;
            o_khung_nhom[cac[i]].left = x_the;
            o_khung_nhom[cac[i]].right = r_the;
            o_khung_nhom[cac[i]].top = yy;
            o_khung_nhom[cac[i]].bottom = yy + 26 + hang * 26;
            yy = o_khung_nhom[cac[i]].bottom + 8;
        }
        for (i = 0; i < 3; i++) dat_cho(g.dao[i], x_the + 12 + i * 220, yy + 22, 210, 20);
        dat_cho(g.nut_gui_nhom[0], x_the, yy + 56, r_the - x_the, 28);
    } else if (the == 2) {
        int yy = y_the_trong + 4;
        int cac[4] = { N_AN_TOAN, N_DI_CHUYEN, N_NUT_LENH, N_DEN };
        for (i = 0; i < 4; i++) {
            int n = so_chan_trong_nhom(cac[i]);
            int hang = (n + 1) / 2;
            o_khung_nhom[cac[i]].left = x_the;
            o_khung_nhom[cac[i]].right = r_the;
            o_khung_nhom[cac[i]].top = yy;
            o_khung_nhom[cac[i]].bottom = yy + 26 + hang * 26;
            yy = o_khung_nhom[cac[i]].bottom + 6;
        }
        dat_cho(g.nut_gui_nhom[1], x_the, yy + 4, r_the - x_the, 28);
    }

    /* Dat cho tung o chan theo khung nhom cua no */
    for (i = 0; i < SO_CHAN; i++) {
        const RECT *k = &o_khung_nhom[CAC_CHAN[i].nhom];
        int hien = (k->right > k->left);
        int x, yc, j, chi_so = 0;
        if (hien) {
            for (j = 0; j < i; j++)
                if (CAC_CHAN[j].nhom == CAC_CHAN[i].nhom) chi_so++;
            cho_hang_chan(k, chi_so, so_chan_trong_nhom(CAC_CHAN[i].nhom), &x, &yc);
            dat_cho(g.o_chan[i], x + ROG_MO_TA + ROG_TEN_CHAN, yc, 44, 22);
            dat_cho(g.nut_chan[i], x + ROG_MO_TA + ROG_TEN_CHAN + 48, yc, 44, 22);
            if (g.gnd_chan[i])
                dat_cho(g.gnd_chan[i], x + ROG_MO_TA + ROG_TEN_CHAN + 98, yc + 2, 54, 20);
        }
        ShowWindow(g.o_chan[i], hien ? SW_SHOW : SW_HIDE);
        ShowWindow(g.nut_chan[i], hien ? SW_SHOW : SW_HIDE);
        if (g.gnd_chan[i]) ShowWindow(g.gnd_chan[i], hien ? SW_SHOW : SW_HIDE);
    }

    /* The 1: che do */
    for (i = 0; i < 3; i++) {
        dat_cho(g.che_do[i], x_the + 12, y_the_trong + 24 + i * 88, 420, 20);
        ShowWindow(g.che_do[i], the == 0 ? SW_SHOW : SW_HIDE);
    }
    dat_cho(g.o_duong_kinh, x_the + 190, y_the_trong + 300, 80, 22);
    dat_cho(g.nut_khac[4], x_the + 278, y_the_trong + 300, 60, 22);
    ShowWindow(g.o_duong_kinh, the == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.nut_khac[4], the == 0 ? SW_SHOW : SW_HIDE);

    /* The 4: hieu chuan */
    {
        HWND o[5] = { g.o_microstep, g.o_mmvong, g.o_toc_do_tay,
                      g.o_nhich_mm, g.o_nhich_do };
        int yy[5] = { 26, 58, 124, 152, 180 };
        for (i = 0; i < 5; i++) {
            dat_cho(o[i], x_the + 250, y_the_trong + yy[i], 80, 22);
            dat_cho(g.nut_khac[5 + i], x_the + 338, y_the_trong + yy[i], 60, 22);
            ShowWindow(o[i], the == 3 ? SW_SHOW : SW_HIDE);
            ShowWindow(g.nut_khac[5 + i], the == 3 ? SW_SHOW : SW_HIDE);
        }
        dat_cho(g.ramp_cat, x_the + 12, y_the_trong + 232, 400, 20);
        ShowWindow(g.ramp_cat, the == 3 ? SW_SHOW : SW_HIDE);
    }

    ShowWindow(g.nut_gui_nhom[0], the == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.nut_gui_nhom[1], the == 2 ? SW_SHOW : SW_HIDE);
    for (i = 0; i < 3; i++) ShowWindow(g.dao[i], the == 1 ? SW_SHOW : SW_HIDE);

    /* ---- Hang nut luu ---- */
    {
        int rong_nut = (rong - 16 - 8) / 3;
        dat_cho(GetDlgItem(g.chinh, ID_NUT_LUU_FLASH), 8, y_luu, rong_nut, 32);
        dat_cho(GetDlgItem(g.chinh, ID_NUT_REBOOT), 12 + rong_nut, y_luu, rong_nut, 32);
        dat_cho(GetDlgItem(g.chinh, ID_NUT_RESET), 16 + rong_nut * 2, y_luu,
                rong_nut, 32);
    }
    dat_cho(g.log, 8, y_log + 18, rong - 16, CAO_LOG - 22);
}

/* ====================================================================== */
/* VE                                                                     */
/* ====================================================================== */
static void ve_khung(HDC hdc, const RECT *r, const char *tieu_de)
{
    HBRUSH nen = CreateSolidBrush(MAU_KHUNG);
    HPEN vien = CreatePen(PS_SOLID, 1, MAU_VIEN);
    HPEN cu = (HPEN)SelectObject(hdc, vien);
    HBRUSH bcu = (HBRUSH)SelectObject(hdc, nen);
    Rectangle(hdc, r->left, r->top, r->right, r->bottom);
    SelectObject(hdc, cu);
    SelectObject(hdc, bcu);
    DeleteObject(nen);
    DeleteObject(vien);
    if (tieu_de) ve_chu(hdc, r->left + 8, r->top + 2, tieu_de, PC_DAM, MAU_CHU);
}

static void ve_nhieu_dong(HDC hdc, int x, int y, int rong, const char *chu,
                          HFONT pc, COLORREF mau)
{
    RECT r;
    HFONT cu = (HFONT)SelectObject(hdc, pc);
    r.left = x; r.top = y; r.right = x + rong; r.bottom = y + 400;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, mau);
    DrawTextA(hdc, chu, -1, &r, DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(hdc, cu);
}

static void ve_giao_dien(HDC hdc)
{
    RECT rc, r;
    int the = (int)SendMessageA(g.the, TCM_GETCURSEL, 0, 0);
    int i, x_the, y;
    char chu[256];

    GetClientRect(g.chinh, &rc);
    x_the = 16;

    /* ---- Hai dong canh bao tren cung ---- */
    ve_nhieu_dong(hdc, 8, 4, rc.right - 16,
                  "! Chi doi cau hinh khi may DUNG HAN. Doi sai chan GPIO co the "
                  "lam mat tin hieu EMG/LIMIT hoac dieu khien nham dong co.",
                  PC_DAM, RGB(0xd9, 0x53, 0x4f));
    ve_nhieu_dong(hdc, 8, 22, rc.right - 16,
                  "Go  *  vao o so chan de BO chan do.   O 'GND' ben canh ngo vao: "
                  "BAT = cap GND thi kich (nut thuong ho), TAT = mat GND moi kich "
                  "(tiep diem thuong dong - nen dung cho EMG/LIMIT vi dut day cung "
                  "tu bao ve).",
                  PC_NHO, RGB(0x0b, 0x5c, 0xad));

    /* ---- Khung ket noi ---- */
    r.left = 4; r.top = CAO_CANH_BAO;
    r.right = rc.right - 4; r.bottom = CAO_CANH_BAO + CAO_KET_NOI;
    ve_khung(hdc, &r, " Ket noi Serial ");
    ve_chu(hdc, 12, r.top + 14, "Cong COM:", PC_THUONG, MAU_CHU);
    if (g.dang_ket_noi) {
        snprintf(chu, sizeof(chu), "Da ket noi %s", g.cong_com);
        ve_chu(hdc, 382, r.top + 14, chu, PC_THUONG, MAU_CHAY);
    } else {
        ve_chu(hdc, 382, r.top + 14, "Chua ket noi", PC_THUONG, MAU_DUNG);
    }
    ve_chu(hdc, 692, r.top + 14, "Dien san so do chan:", PC_THUONG, MAU_CHU);

    /* ---- Noi dung the ---- */
    if (the == 0) {
        r.left = x_the; r.top = y_the_trong + 4;
        r.right = rc.right - 24; r.bottom = y_the_trong + 280;
        ve_khung(hdc, &r, " Chon che do lam viec (ap dung ngay) ");
        for (i = 0; i < 3; i++)
            ve_nhieu_dong(hdc, x_the + 32, y_the_trong + 46 + i * 88,
                          rc.right - 80, MO_TA_CHE_DO[i + 1], PC_NHO,
                          RGB(0x66, 0x66, 0x66));
        r.top = y_the_trong + 286; r.bottom = y_the_trong + 336;
        ve_khung(hdc, &r, " Duong kinh ong (bat buoc cho che do 2 va 3) ");
        ve_chu(hdc, x_the + 10, y_the_trong + 304, "Duong kinh ngoai ong (mm):",
               PC_THUONG, MAU_CHU);
        {
            double d;
            if (lay_so(g.o_duong_kinh, &d) == 0 && d > 0) {
                snprintf(chu, sizeof(chu),
                         "chu vi = %.2f mm   (1 do = %.4f mm cung)",
                         PI * d, PI * d / 360.0);
                ve_chu(hdc, x_the + 350, y_the_trong + 304, chu, PC_THUONG,
                       RGB(0x66, 0x66, 0x66));
            }
        }
    } else if (the == 1 || the == 2) {
        static const char *tieu_de[SO_NHOM] = {
            " Chan xung / chieu cua 3 dong co ",
            " Mo cat plasma ",
            " Ngo vao AN TOAN ",
            " 4 nut di chuyen (GIU la chay lien tuc) ",
            " Nut lenh ",
            " Den bao (go * de TAT - ESP32 goc het chan nen mac dinh TAT) "
        };
        for (i = 0; i < SO_NHOM; i++)
            if (o_khung_nhom[i].right > o_khung_nhom[i].left)
                ve_khung(hdc, &o_khung_nhom[i], tieu_de[i]);
        for (i = 0; i < SO_CHAN; i++) {
            const RECT *k = &o_khung_nhom[CAC_CHAN[i].nhom];
            int x, yc, j, chi_so = 0;
            if (k->right <= k->left) continue;
            for (j = 0; j < i; j++)
                if (CAC_CHAN[j].nhom == CAC_CHAN[i].nhom) chi_so++;
            cho_hang_chan(k, chi_so, so_chan_trong_nhom(CAC_CHAN[i].nhom), &x, &yc);
            ve_chu(hdc, x, yc + 3, CAC_CHAN[i].mo_ta, PC_NHO, MAU_CHU);
            ve_chu(hdc, x + ROG_MO_TA, yc + 4, CAC_CHAN[i].ten, PC_DEU,
                   RGB(0x66, 0x66, 0x66));
        }
        if (the == 1) {
            y = o_khung_nhom[N_PLASMA].bottom + 8;
            r.left = x_the; r.right = rc.right - 24;
            r.top = y; r.bottom = y + 48;
            ve_khung(hdc, &r,
                     " Dao chieu truc (ap dung ngay - dung khi lap motor nguoc chieu) ");
        }
    } else {
        r.left = x_the; r.right = rc.right - 24;
        r.top = y_the_trong + 4; r.bottom = y_the_trong + 92;
        ve_khung(hdc, &r, " Hieu chuan (ap dung ngay, khong can khoi dong lai) ");
        ve_chu(hdc, x_the + 10, y_the_trong + 30, "So xung / vong dong co:",
               PC_THUONG, MAU_CHU);
        ve_chu(hdc, x_the + 410, y_the_trong + 24,
               "= vi buoc driver x so buoc/vong dong co", PC_NHO,
               RGB(0x66, 0x66, 0x66));
        ve_chu(hdc, x_the + 410, y_the_trong + 38, "   (vi du 1/8 x 200 = 1600)",
               PC_NHO, RGB(0x66, 0x66, 0x66));
        ve_chu(hdc, x_the + 10, y_the_trong + 62, "mm / vong truc keo:",
               PC_THUONG, MAU_CHU);
        ve_chu(hdc, x_the + 410, y_the_trong + 56,
               "quay 1 vong thi ong di duoc bao nhieu mm", PC_NHO,
               RGB(0x66, 0x66, 0x66));
        ve_chu(hdc, x_the + 410, y_the_trong + 70,
               "   (vi du 0.2 vong = 1mm  =>  5.0)", PC_NHO, RGB(0x66, 0x66, 0x66));

        r.top = y_the_trong + 98; r.bottom = y_the_trong + 212;
        ve_khung(hdc, &r, " Bang dieu khien tay (ap dung ngay) ");
        ve_chu(hdc, x_the + 10, y_the_trong + 128,
               "Toc do khi GIU nut di chuyen (RPM):", PC_THUONG, MAU_CHU);
        ve_chu(hdc, x_the + 10, y_the_trong + 156,
               "Nhich truc X moi lan bam (mm):", PC_THUONG, MAU_CHU);
        ve_chu(hdc, x_the + 10, y_the_trong + 184,
               "Nhich truc A moi lan bam (do):", PC_THUONG, MAU_CHU);

        r.top = y_the_trong + 218; r.bottom = y_the_trong + 306;
        ve_khung(hdc, &r, " Tang toc khi CAT (ap dung ngay) ");
        ve_nhieu_dong(hdc, x_the + 12, y_the_trong + 256, rc.right - 80,
                      "TAT (khuyen dung): doan cat chay dung toc do ngay tu xung "
                      "dau, mep cat dep.\r\n"
                      "BAT: chi khi dong co bi RU / MAT BUOC luc vao cat (doi lai "
                      "mep dau xau hon).", PC_NHO, RGB(0x66, 0x66, 0x66));
        ve_nhieu_dong(hdc, x_the + 14, y_the_trong + 316, rc.right - 80,
                      "Kiem tra: ZERO -> JOG X 100mm -> do thuoc. Neu lech, sua "
                      "'mm / vong truc keo':\r\n"
                      "     gia_tri_moi = gia_tri_cu x (quang duong that / 100)",
                      PC_THUONG, RGB(0x44, 0x44, 0x44));
    }

    /* ---- Nhan khung nhat ky ---- */
    ve_chu(hdc, 12, rc.bottom - CAO_LOG - 2, " Phan hoi tu ESP32 ", PC_DAM, MAU_CHU);
}

/* ====================================================================== */
/* XU LY LENH                                                             */
/* ====================================================================== */
static void xu_ly_lenh(int ma)
{
    static const int nhom_truc[2] = { N_TRUC, N_PLASMA };
    static const int nhom_plc[4] = { N_AN_TOAN, N_DI_CHUYEN, N_NUT_LENH, N_DEN };

    if (ma >= ID_CHAN_O && ma < ID_CHAN_O + SO_CHAN) return;
    if (ma >= ID_CHAN_GUI && ma < ID_CHAN_GUI + SO_CHAN) {
        gui_1_chan(ma - ID_CHAN_GUI);
        return;
    }
    if (ma >= ID_CHAN_GND && ma < ID_CHAN_GND + SO_CHAN) {
        gui_tin_hieu(ma - ID_CHAN_GND);
        return;
    }
    switch (ma) {
    case ID_NUT_LAM_MOI: nap_danh_sach_cong(); break;
    case ID_NUT_KET_NOI:
        if (g.dang_ket_noi) ngat_ket_noi(); else ket_noi_may();
        break;
    case ID_NUT_DOC_CFG: gui_qua_serial("CFG;GET"); break;
    case ID_NUT_SO_DO_GOC:
        dien_so_do("ESP32 devkit goc", SO_DO_ESP32_GOC);
        break;
    case ID_NUT_SO_DO_S3:
        dien_so_do("ESP32-S3-WROOM-1 N16R8", SO_DO_ESP32_S3);
        break;
    case ID_CHE_DO_1: gui_che_do(1); break;
    case ID_CHE_DO_2: gui_che_do(2); break;
    case ID_CHE_DO_3: gui_che_do(3); break;
    case ID_NUT_GUI_DK: gui_duong_kinh(); break;
    case ID_O_DUONG_KINH: cap_nhat_chu_vi(); break;
    case ID_NUT_GUI_MICROSTEP: gui_so("CFG;CAL;MICROSTEP", g.o_microstep); break;
    case ID_NUT_GUI_MMVONG:    gui_so("CFG;CAL;MMVONG", g.o_mmvong); break;
    case ID_NUT_GUI_TOC_DO_TAY: gui_so("CFG;TAY;TOCDO", g.o_toc_do_tay); break;
    case ID_NUT_GUI_NHICH_MM:  gui_so("CFG;TAY;NHICHMM", g.o_nhich_mm); break;
    case ID_NUT_GUI_NHICH_DO:  gui_so("CFG;TAY;NHICHDO", g.o_nhich_do); break;
    case ID_DAO_KEOA: gui_dao(0); break;
    case ID_DAO_KEOB: gui_dao(1); break;
    case ID_DAO_XOAY: gui_dao(2); break;
    case ID_RAMP_CAT: gui_ramp_cat(); break;
    case ID_NUT_GUI_NHOM_TRUC: gui_nhom_chan(nhom_truc, 2); break;
    case ID_NUT_GUI_NHOM_PLC:  gui_nhom_chan(nhom_plc, 4); break;
    case ID_NUT_LUU_FLASH: gui_qua_serial("CFG;SAVE"); break;
    case ID_NUT_REBOOT:
        if (hoi_co_khong(g.chinh, "Xac nhan khoi dong lai",
                         "Khoi dong lai ESP32 ngay bay gio?\n\n"
                         "Chi lam dieu nay khi may DANG DUNG HAN, khong dang cat."))
            gui_qua_serial("CFG;REBOOT");
        break;
    case ID_NUT_RESET:
        if (hoi_co_khong(g.chinh, "Xac nhan ve mac dinh",
                         "Xoa TOAN BO cau hinh da luu, ve dung chan GPIO va hieu "
                         "chuan MAC DINH cua firmware?\n\n"
                         "Hanh dong nay khong hoan tac duoc."))
            gui_qua_serial("CFG;RESET");
        break;
    default:
        break;
    }
}

/* ====================================================================== */
/* THU TUC CUA SO                                                         */
/* ====================================================================== */
static LRESULT CALLBACK thu_tuc_chinh(HWND h, UINT tin, WPARAM w, LPARAM l)
{
    switch (tin) {
    case WM_COMMAND:
        if (LOWORD(w) == ID_O_DUONG_KINH && HIWORD(w) != EN_CHANGE) return 0;
        xu_ly_lenh(LOWORD(w));
        return 0;

    case WM_NOTIFY: {
        NMHDR *n = (NMHDR *)l;
        if (n->hwndFrom == g.the && n->code == (UINT)TCN_SELCHANGE) {
            bo_tri();
            InvalidateRect(h, NULL, TRUE);
        }
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)w, MAU_KHUNG);
        SetTextColor((HDC)w, MAU_CHU);
        return (LRESULT)g.nen_khung;

    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(h, &rc);
        FillRect((HDC)w, &rc, g.nen_nen);
        return 1;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        ve_giao_dien(hdc);
        EndPaint(h, &ps);
        return 0;
    }

    case WM_SIZE:
        bo_tri();
        InvalidateRect(h, NULL, TRUE);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *m = (MINMAXINFO *)l;
        m->ptMinTrackSize.x = 1130;
        m->ptMinTrackSize.y = 700;
        return 0;
    }

    case WM_ESP32:
        if (l) {
            char *dong = (char *)l;
            ghi_log("[ESP32] %s", dong);
            thu_doc_cfg(dong);
            free(dong);
        }
        return 0;
    case WM_NHAT_KY:
        if (l) { ghi_log("[He thong] %s", (const char *)l); free((void *)l); }
        return 0;

    case WM_CLOSE:
        ket_noi_dong(g.may);
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, tin, w, l);
}

/* ====================================================================== */
/* KHOI DONG                                                              */
/* ====================================================================== */
int WINAPI WinMain(HINSTANCE hi, HINSTANCE truoc, LPSTR dong_lenh, int hien)
{
    MSG tin;
    WNDCLASSA c;
    HamGoiLai gl;
    INITCOMMONCONTROLSEX icc;
    char ten_cong[SO_CONG_TOI_DA][CO_TEN_CONG];
    int so_cong;

    (void)truoc; (void)dong_lenh;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    LoadLibraryA("Msftedit.dll");
    LoadLibraryA("Riched20.dll");
    tien_ich_khoi_tao();

    memset(&g, 0, sizeof(g));
    g.hinst = hi;
    g.che_do_hien = 1;
    g.nen_khung = CreateSolidBrush(MAU_KHUNG);
    g.nen_nen = CreateSolidBrush(MAU_NEN);

    so_cong = cong_liet_ke(ten_cong, SO_CONG_TOI_DA);
    snprintf(g.cong_com, sizeof(g.cong_com), "%s",
             so_cong > 0 ? ten_cong[0] : "COM3");

    memset(&gl, 0, sizeof(gl));
    gl.dong_esp32 = tu_luong_dong;
    gl.nhat_ky = tu_luong_nhat_ky;
    g.may = ket_noi_tao(&gl);

    memset(&c, 0, sizeof(c));
    c.lpfnWndProc = thu_tuc_chinh;
    c.hInstance = hi;
    c.hCursor = LoadCursorA(NULL, IDC_ARROW);
    c.hbrBackground = g.nen_nen;
    c.lpszClassName = "cnc_cai_dat";
    RegisterClassA(&c);

    g.chinh = CreateWindowExA(0, "cnc_cai_dat", TEN_PHAN_MEM,
                              WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1140, 760,
                              NULL, NULL, hi, NULL);
    if (!g.chinh) return 1;
    tao_o_dieu_khien(g.chinh);
    bo_tri();
    ShowWindow(g.chinh, hien);
    UpdateWindow(g.chinh);
    ghi_log("[He thong] San sang. Chon cong COM roi bam Ket noi.");

    while (GetMessageA(&tin, NULL, 0, 0) > 0) {
        if (IsDialogMessageA(g.chinh, &tin)) continue;
        TranslateMessage(&tin);
        DispatchMessageA(&tin);
    }
    ket_noi_giai_phong(g.may);
    DeleteObject(g.nen_khung);
    DeleteObject(g.nen_nen);
    tien_ich_don_dep();
    return 0;
}
