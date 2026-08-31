/* PHAN MEM MAY CAT ONG PLASMA CNC - ban viet bang C thuan, Win32 + GDI.
 *
 * Mot cua so lo het viec hang ngay:
 *   - THU VIEN MOI NOI: 3 kieu ghep, nhap chieu dai khuc roi bam Them - phan
 *     mem tu xep cac nhat cat noi tiep nhau tren cay ong
 *   - THE XEP 2D: nhin cay ong nam thang, keo tha tung nhat cat, co thuoc do
 *   - MO PHONG 3D: dung nhu may that - dau cat dung yen, ong quay va truot
 *   - MO FILE .NC san co tu phan mem CAM
 *   - Chay / tam dung / chay tiep (co hoi duc lo lai) / dung
 *
 * Toan bo phan tinh toan nam trong loi_c/ va da duoc kiem tra rieng bang
 * chuong trinh dong lenh; file nay chi lo phan nhin va bam.
 *
 * Cai dat phan cung (chan GPIO, so xung moi vong, dao chieu truc) nam o
 * chuong trinh rieng "cnc_settings.exe", mo tu menu Settings.
 */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tien_ich.h"
#include "gdi_ve.h"
#include "../loi_c/thu_vien_moi_noi.h"
#include "../loi_c/phan_tich_gcode.h"
#include "../loi_c/ket_noi.h"
#include "../loi_c/ve_3d.h"
#include "../loi_c/xep_2d.h"

#define TEN_PHAN_MEM "May cat ong plasma CNC"

/* Duong COM luon chay 115200 - muc on dinh nhat voi moi chip USB-UART. */
#define BAUD_CO_DINH 115200

#define SO_MUC_TOI_DA   400
#define SO_LICH_SU      60
#define SO_DONG_TOI_DA  40000       /* so dong G-code toi da cua ca bai */

/* --- Ma o dieu khien --- */
enum {
    ID_NUT_KET_NOI = 100, ID_NUT_THAM_SO,
    ID_BT_0, ID_BT_1, ID_BT_2,          /* 3 bieu tuong kieu ghep */
    ID_O_DAI_KHUC, ID_O_TS_0, ID_O_TS_1, ID_O_TS_2, ID_O_TS_3,
    ID_NUT_THEM,
    ID_JOG_X_TRU, ID_JOG_X_CONG, ID_JOG_A_TRU, ID_JOG_A_CONG,
    ID_O_TOC_DO_TAY, ID_O_BUOC_NHICH, ID_NUT_DAT_GOC,
    ID_THE_GIUA, ID_CANVAS_3D, ID_CANVAS_2D,
    ID_NUT_GOC_NHIN, ID_NUT_AN_CAT, ID_NUT_HIEN_LAI, ID_TRUOT_MO_PHONG,
    ID_NUT_VUA_KHUNG, ID_NUT_XEP_LAI, ID_O_DAI_CAY, ID_O_KHE_CAT,
    ID_O_KHOANG_CACH, ID_NUT_AP_KC,
    ID_TIEN_DO, ID_O_DUONG_KINH, ID_NUT_AP_DK,
    ID_NUT_MO_NC, ID_NUT_VE_GOC, ID_NUT_CHAY_THU, ID_NUT_BAT_MO,
    ID_NUT_CHAY, ID_NUT_TAM_DUNG, ID_NUT_CHAY_TIEP, ID_NUT_DUNG,
    ID_THE_DUOI, ID_BANG_BAI, ID_O_GCODE, ID_NUT_NAP_LAI,
    ID_NUT_LEN, ID_NUT_XUONG, ID_NUT_XOA, ID_NUT_XOA_HET,
    ID_TERM, ID_O_LENH, ID_NUT_GUI_LENH, ID_NUT_XOA_TERM,
    ID_BANG_LOI,
    ID_MENU_MO = 400, ID_MENU_LUU, ID_MENU_THOAT,
    ID_MENU_THAM_SO, ID_MENU_TOC_DO, ID_MENU_DUC_LO,
    ID_MENU_XEP_LAI, ID_MENU_KIEM_CAY,
    ID_MENU_POS, ID_MENU_BUF, ID_MENU_CFG, ID_MENU_REBOOT,
    ID_MENU_TAY, ID_MENU_CAI_DAT,
    ID_MENU_XEM_LOI, ID_MENU_XOA_LOI
};

/* --- Ban tin tu luong nen gui ve luong giao dien --- */
#define WM_ESP32    (WM_APP + 1)    /* lParam = char* (tu giai phong) */
#define WM_NHAT_KY  (WM_APP + 2)
#define WM_LOI_NAP  (WM_APP + 3)
#define WM_VI_TRI   (WM_APP + 4)    /* lParam = double[2]* (tu giai phong) */
#define WM_BAUD     (WM_APP + 5)

/* --- Trang thai may --- */
typedef enum {
    TT_CHUA_KETNOI = 0, TT_SAN_SANG, TT_DANG_NAP, TT_DANG_CHAY,
    TT_TAM_DUNG, TT_LOI, TT_DA_DUNG
} TrangThai;

static const struct { const char *chu; COLORREF mau; } BANG_TRANG_THAI[] = {
    { "CHUA KET NOI", RGB(0x86, 0x8e, 0x96) },
    { "SAN SANG",     RGB(0x2f, 0x9e, 0x44) },
    { "DANG NAP...",  RGB(0x70, 0x48, 0xe8) },
    { "DANG CHAY",    RGB(0x19, 0x71, 0xc2) },
    { "TAM DUNG",     RGB(0xe8, 0x89, 0x0c) },
    { "LOI / EMG",    RGB(0xc9, 0x2a, 0x2a) },
    { "DA DUNG",      RGB(0x49, 0x50, 0x57) }
};

static const char *TEN_CHE_DO[4] = {
    "",
    "Mode 1 - X tinh bang mm, A tinh bang do",
    "Mode 2 - nhap duong kinh, giu toc do mo cat khong doi",
    "Mode 3 - nhap duong kinh, ca hai truc tinh bang mm"
};

#define BO_LOC_FILE \
    "File G-code (*.nc;*.gcode;*.tap;*.txt)\0*.nc;*.gcode;*.tap;*.txt\0" \
    "Tat ca file (*.*)\0*.*\0\0"

/* ====================================================================== */
/* TRANG THAI CHUONG TRINH                                                */
/* ====================================================================== */
typedef struct {
    char ten[MAX_PATH];
    char **dong;
    int so_dong;
} FileNgoai;

typedef struct {
    MucBai muc[SO_MUC_TOI_DA];
    int so_muc;
    FileNgoai file_ngoai;
} AnhChup;      /* mot buoc lich su cho Ctrl+Z */

typedef struct {
    HINSTANCE hinst;
    HWND chinh;

    /* --- Ket noi --- */
    KetNoi *may;
    TrangThai trang_thai;
    char cong_com[CO_TEN_CONG];

    /* --- Tham so bai --- */
    int    che_do;
    double duong_kinh, dai_cay_ong, toc_do_cat, toc_do_nhanh;
    double toc_do_tay, buoc_nhich, thoi_gian_duc_lo;
    double dai_khuc, khe_cat, chua_dau;
    int    chay_thu;

    /* --- Bai --- */
    MucBai muc[SO_MUC_TOI_DA];
    int    so_muc;
    FileNgoai file_ngoai;
    AnhChup lich_su[SO_LICH_SU];      int so_lich_su;
    AnhChup lich_su_lam_lai[SO_LICH_SU]; int so_lam_lai;
    MucBai bang_nho[SO_MUC_TOI_DA];   int so_bang_nho;
    int dang_keo;

    const KieuGhep *kieu_dang_chon;
    KetQuaPhanTich ket_qua;
    int co_ket_qua;
    int mo_dang_bat;
    double moc_bat_dau;               /* -1 = chua chay */
    int tong_doan, doan_da_chay;
    int che_do_an_cat;

    /* --- Ve --- */
    MoPhong3D *mp;
    Xep2D *xep;
    int chuot_x, chuot_y, dang_xoay, dang_day;

    /* --- O dieu khien --- */
    HWND nut_ket_noi, nut_tham_so;
    HWND bieu_tuong[SO_KIEU_GHEP];
    HWND o_dai_khuc, o_tham_so[SO_THAM_SO_TOI_DA], nut_them;
    HWND o_toc_do_tay, o_buoc_nhich;
    HWND the_giua, canvas3d, canvas2d;
    HWND nut_an_cat, truot_mo_phong;
    HWND o_dai_cay, o_khe_cat, o_khoang_cach, o_duong_kinh;
    HWND tien_do;
    HWND nut_chay_thu, nut_bat_mo;
    HWND nut_chay, nut_tam_dung, nut_chay_tiep, nut_dung, nut_ve_goc;
    HWND the_duoi, bang_bai, o_gcode, term, o_lenh, bang_loi;
    HWND nut_thu_vien[8], nut_tay[8], nut_the3d[8], nut_the2d[8], nut_bang[8];
    HWND nut_hang[8];

    /* Vung ve tu ve tay trong WM_PAINT */
    RECT vung_trai, vung_phai;
    double x_hien, a_hien;

    char goi_y[256];
    HBRUSH nen_khung, nen_nen, nen_term;
} UngDung;

static UngDung g;

/* ====================================================================== */
/* KHAI BAO TRUOC                                                         */
/* ====================================================================== */
static void ghi(const char *the, const char *dinh_dang, ...);
static void them_loi(const char *noi_dung);
static void bai_da_doi(void);
static void sinh_gcode_bai(void);
static void ve_lai_bai(void);
static void ve_3d(void);
static void ve_2d(void);
static void cap_nhat_nut(void);
static void dat_trang_thai(TrangThai tt);
static void bo_tri(void);
static int  dang_ket_noi(void);
static int  gui_lenh(const char *lenh);
static void cap_nhat_o_nhat_cat(void);
static void ghi_lich_su(void);
static void nap_lai_bang(void);
static int  the_giua_dang_mo(void);
static void chon_kieu(const KieuGhep *k);

/* ====================================================================== */
/* TIEN ICH NHO                                                           */
/* ====================================================================== */
static double gio_may(void) { return (double)GetTickCount64() / 1000.0; }

static void file_ngoai_xoa(FileNgoai *f)
{
    int i;
    for (i = 0; i < f->so_dong; i++) free(f->dong[i]);
    free(f->dong);
    f->dong = NULL;
    f->so_dong = 0;
    f->ten[0] = '\0';
}

static int file_ngoai_chep(FileNgoai *ra, const FileNgoai *tu)
{
    int i;
    memset(ra, 0, sizeof(*ra));
    if (tu->so_dong <= 0) return 0;
    ra->dong = (char **)calloc((size_t)tu->so_dong, sizeof(char *));
    if (!ra->dong) return -1;
    for (i = 0; i < tu->so_dong; i++) {
        ra->dong[i] = _strdup(tu->dong[i] ? tu->dong[i] : "");
        if (!ra->dong[i]) { ra->so_dong = i; file_ngoai_xoa(ra); return -1; }
    }
    ra->so_dong = tu->so_dong;
    snprintf(ra->ten, sizeof(ra->ten), "%s", tu->ten);
    return 0;
}

/* Doc ca file thanh mang dong. Tra 0 = xong. */
static int doc_file_thanh_dong(const char *duong_dan, FileNgoai *ra)
{
    FILE *f = fopen(duong_dan, "rb");
    char dem[CO_DONG_G * 4];
    const char *ten;
    int suc_chua = 256;
    if (!f) return -1;
    memset(ra, 0, sizeof(*ra));
    ra->dong = (char **)calloc((size_t)suc_chua, sizeof(char *));
    if (!ra->dong) { fclose(f); return -1; }
    while (fgets(dem, sizeof(dem), f)) {
        size_t d = strlen(dem);
        while (d > 0 && (dem[d - 1] == '\n' || dem[d - 1] == '\r')) dem[--d] = '\0';
        if (ra->so_dong >= suc_chua) {
            char **moi = (char **)realloc(ra->dong, sizeof(char *) * (size_t)(suc_chua * 2));
            if (!moi) break;
            ra->dong = moi;
            suc_chua *= 2;
        }
        ra->dong[ra->so_dong] = _strdup(dem);
        if (!ra->dong[ra->so_dong]) break;
        ra->so_dong++;
    }
    fclose(f);
    ten = strrchr(duong_dan, '\\');
    if (!ten) ten = strrchr(duong_dan, '/');
    snprintf(ra->ten, sizeof(ra->ten), "%s", ten ? ten + 1 : duong_dan);
    return 0;
}

/* ====================================================================== */
/* NHAT KY (tab System) VA DANH SACH LOI (tab Alarm)                      */
/* ====================================================================== */
static void term_them(const char *chu, COLORREF mau)
{
    CHARFORMAT2A cf;
    GETTEXTLENGTHEX gt;
    LONG dai;
    if (!g.term) return;
    gt.flags = GTL_DEFAULT;
    gt.codepage = CP_ACP;
    dai = (LONG)SendMessageA(g.term, EM_GETTEXTLENGTHEX, (WPARAM)&gt, 0);
    SendMessageA(g.term, EM_SETSEL, (WPARAM)dai, (LPARAM)dai);

    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = mau;
    SendMessageA(g.term, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageA(g.term, EM_REPLACESEL, FALSE, (LPARAM)chu);
    SendMessageA(g.term, EM_SCROLLCARET, 0, 0);
}

/* the: NULL thuong, "gui", "loi", "ok", "he_thong" */
static void ghi(const char *the, const char *dinh_dang, ...)
{
    char chu[1024], ca_dong[1100];
    va_list ds;
    time_t bay_gio = time(NULL);
    struct tm *t = localtime(&bay_gio);
    COLORREF mau = MAU_TERM_CHU;

    va_start(ds, dinh_dang);
    vsnprintf(chu, sizeof(chu), dinh_dang, ds);
    va_end(ds);

    if (the) {
        if      (strcmp(the, "gui") == 0)      mau = RGB(0x74, 0xc0, 0xfc);
        else if (strcmp(the, "loi") == 0)      mau = RGB(0xff, 0x87, 0x87);
        else if (strcmp(the, "ok") == 0)       mau = RGB(0x8c, 0xe9, 0x9a);
        else if (strcmp(the, "he_thong") == 0) mau = RGB(0xff, 0xd4, 0x3b);
    }
    snprintf(ca_dong, sizeof(ca_dong), "[%02d:%02d:%02d] %s\r\n",
             t->tm_hour, t->tm_min, t->tm_sec, chu);
    term_them(ca_dong, mau);
}

static void xoa_terminal(void)
{
    SetWindowTextA(g.term, "");
}

static void them_loi(const char *noi_dung)
{
    LVITEMA m;
    char gio[16], cu[CO_LOI];
    time_t bay_gio = time(NULL);
    struct tm *t = localtime(&bay_gio);
    int n;

    if (!g.bang_loi) return;
    n = (int)SendMessageA(g.bang_loi, LVM_GETITEMCOUNT, 0, 0);
    if (n > 0) {      /* khong ghi lai y het dong vua roi */
        LVITEMA doc;
        memset(&doc, 0, sizeof(doc));
        doc.iSubItem = 1;
        doc.pszText = cu;
        doc.cchTextMax = sizeof(cu);
        SendMessageA(g.bang_loi, LVM_GETITEMTEXTA, (WPARAM)(n - 1), (LPARAM)&doc);
        if (strcmp(cu, noi_dung) == 0) return;
    }
    snprintf(gio, sizeof(gio), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    memset(&m, 0, sizeof(m));
    m.mask = LVIF_TEXT;
    m.iItem = n;
    m.pszText = gio;
    SendMessageA(g.bang_loi, LVM_INSERTITEMA, 0, (LPARAM)&m);
    m.iSubItem = 1;
    m.pszText = (char *)noi_dung;
    SendMessageA(g.bang_loi, LVM_SETITEMTEXTA, (WPARAM)n, (LPARAM)&m);
    SendMessageA(g.bang_loi, LVM_ENSUREVISIBLE, (WPARAM)n, FALSE);
}

static void xoa_danh_sach_loi(void)
{
    SendMessageA(g.bang_loi, LVM_DELETEALLITEMS, 0, 0);
}

/* ====================================================================== */
/* LICH SU (Ctrl+Z / Ctrl+Y) VA BANG NHO (Ctrl+C / X / V)                 */
/* ====================================================================== */
static void anh_chup_lay(AnhChup *a)
{
    memcpy(a->muc, g.muc, sizeof(MucBai) * (size_t)g.so_muc);
    a->so_muc = g.so_muc;
    file_ngoai_chep(&a->file_ngoai, &g.file_ngoai);
}

static void anh_chup_dat(const AnhChup *a)
{
    memcpy(g.muc, a->muc, sizeof(MucBai) * (size_t)a->so_muc);
    g.so_muc = a->so_muc;
    file_ngoai_xoa(&g.file_ngoai);
    file_ngoai_chep(&g.file_ngoai, &a->file_ngoai);
}

static void xoa_lam_lai(void)
{
    int i;
    for (i = 0; i < g.so_lam_lai; i++) file_ngoai_xoa(&g.lich_su_lam_lai[i].file_ngoai);
    g.so_lam_lai = 0;
}

static void ghi_lich_su(void)
{
    if (g.so_lich_su >= SO_LICH_SU) {
        file_ngoai_xoa(&g.lich_su[0].file_ngoai);
        memmove(&g.lich_su[0], &g.lich_su[1], sizeof(AnhChup) * (SO_LICH_SU - 1));
        g.so_lich_su = SO_LICH_SU - 1;
    }
    anh_chup_lay(&g.lich_su[g.so_lich_su++]);
    xoa_lam_lai();
}

static void hoan_tac(void)
{
    if (g.so_lich_su <= 0) return;
    if (g.so_lam_lai < SO_LICH_SU) anh_chup_lay(&g.lich_su_lam_lai[g.so_lam_lai++]);
    g.so_lich_su--;
    anh_chup_dat(&g.lich_su[g.so_lich_su]);
    file_ngoai_xoa(&g.lich_su[g.so_lich_su].file_ngoai);
    bai_da_doi();
    ghi("he_thong", "Da hoan tac (Ctrl+Z).");
}

static void lam_lai(void)
{
    if (g.so_lam_lai <= 0) return;
    if (g.so_lich_su < SO_LICH_SU) anh_chup_lay(&g.lich_su[g.so_lich_su++]);
    g.so_lam_lai--;
    anh_chup_dat(&g.lich_su_lam_lai[g.so_lam_lai]);
    file_ngoai_xoa(&g.lich_su_lam_lai[g.so_lam_lai].file_ngoai);
    bai_da_doi();
    ghi("he_thong", "Da lam lai (Ctrl+Y).");
}

/* Chi so cac NHAT CAT dang duoc chon (bo qua dong file .NC o dau bang). */
static int cac_nhat_cat_dang_chon(int *ra, int toi_da)
{
    int n = 0, i = -1;
    int lech = g.file_ngoai.so_dong > 0 ? 1 : 0;
    for (;;) {
        i = (int)SendMessageA(g.bang_bai, LVM_GETNEXTITEM, (WPARAM)i,
                              (LPARAM)LVNI_SELECTED);
        if (i < 0 || n >= toi_da) break;
        if (i - lech >= 0 && i - lech < g.so_muc) ra[n++] = i - lech;
    }
    if (n == 0) {
        int c = xep2d_dang_chon(g.xep);
        if (c >= 0 && c < g.so_muc && toi_da > 0) ra[n++] = c;
    }
    return n;
}

static void sao_chep_nhat_cat(int cat_di)
{
    int chon[SO_MUC_TOI_DA], n, i;
    n = cac_nhat_cat_dang_chon(chon, SO_MUC_TOI_DA);
    if (n == 0) return;
    for (i = 0; i < n; i++) g.bang_nho[i] = g.muc[chon[i]];
    g.so_bang_nho = n;
    if (!cat_di) {
        ghi("he_thong", "Da sao chep %d nhat cat (Ctrl+C).", n);
        return;
    }
    ghi_lich_su();
    for (i = n - 1; i >= 0; i--) {
        memmove(&g.muc[chon[i]], &g.muc[chon[i] + 1],
                sizeof(MucBai) * (size_t)(g.so_muc - chon[i] - 1));
        g.so_muc--;
    }
    xep2d_dat_dang_chon(g.xep, -1);
    bai_da_doi();
    ghi("he_thong", "Da cat %d nhat cat (Ctrl+X).", n);
}

static void dan_nhat_cat(void)
{
    int i;
    if (g.so_bang_nho <= 0) return;
    ghi_lich_su();
    for (i = 0; i < g.so_bang_nho && g.so_muc < SO_MUC_TOI_DA; i++) {
        MucBai moi = g.bang_nho[i];
        double x;
        char loi[CO_LOI] = "";
        /* dat noi tiep sau bai hien co */
        if (vi_tri_ke_tiep(g.duong_kinh, g.muc, g.so_muc, g.dai_khuc, g.khe_cat,
                           g.chua_dau, &x, loi) == 0)
            moi.x = x;
        g.muc[g.so_muc++] = moi;
    }
    bai_da_doi();
    ghi("he_thong", "Da dan %d nhat cat (Ctrl+V).", g.so_bang_nho);
}

/* ====================================================================== */
/* BIEU TUONG 3 KIEU GHEP (net ve ky thuat, khong can file anh)           */
/* ====================================================================== */
static void ve_bieu_tuong(HDC hdc, const char *ma, int rong, int cao, int chon)
{
    HPEN but_net = CreatePen(PS_SOLID, 1, MAU_CHU);
    HPEN but_do  = CreatePen(PS_SOLID, 2, RGB(0xd9, 0x48, 0x0f));
    HBRUSH nen = CreateSolidBrush(chon ? RGB(0xdb, 0xe7, 0xf5) : MAU_KHUNG);
    HPEN vien = CreatePen(PS_SOLID, chon ? 2 : 1, chon ? MAU_NHAN : MAU_VIEN);
    HPEN but_cu;
    HBRUSH bru_cu;
    int m = rong / 2, n = cao / 2;

    but_cu = (HPEN)SelectObject(hdc, vien);
    bru_cu = (HBRUSH)SelectObject(hdc, nen);
    Rectangle(hdc, 1, 1, rong - 1, cao - 1);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SelectObject(hdc, but_net);

    if (strcmp(ma, "goc_90") == 0) {            /* hai ong gap vuong goc */
        POINT p[6] = { { 9, n + 15 }, { m + 5, n + 15 }, { m + 5, 9 },
                       { m - 7, 9 }, { m - 7, n + 3 }, { 9, n + 3 } };
        Polygon(hdc, p, 6);
        SelectObject(hdc, but_do);
        MoveToEx(hdc, m - 7, n + 3, NULL);
        LineTo(hdc, m + 5, n + 15);
    } else if (strcmp(ma, "goc_45") == 0) {     /* hai ong gap 45 do */
        POINT p[6] = { { 9, n + 14 }, { m + 8, n + 14 }, { rong - 10, 14 },
                       { rong - 16, 7 }, { m - 2, n + 2 }, { 9, n + 2 } };
        Polygon(hdc, p, 6);
        SelectObject(hdc, but_do);
        MoveToEx(hdc, m - 2, n + 2, NULL);
        LineTo(hdc, m + 8, n + 14);
    } else if (strcmp(ma, "nhanh_t_90") == 0) { /* nhanh dam vuong goc vao giua ong */
        Rectangle(hdc, 8, n + 7, rong - 8, n + 19);
        Ellipse(hdc, 5, n + 7, 11, n + 19);
        Rectangle(hdc, m - 7, 8, m + 7, n + 7);
        SelectObject(hdc, but_do);
        Arc(hdc, m - 7, n + 2, m + 7, n + 14, m + 7, n + 8, m - 7, n + 8);
    }

    SelectObject(hdc, but_cu);
    SelectObject(hdc, bru_cu);
    DeleteObject(but_net);
    DeleteObject(but_do);
    DeleteObject(nen);
    DeleteObject(vien);
}

static LRESULT CALLBACK thu_tuc_bieu_tuong(HWND h, UINT tin, WPARAM w, LPARAM l)
{
    int chi_so = (int)(INT_PTR)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (tin) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        ve_bieu_tuong(hdc, THU_VIEN[chi_so].ma, r.right, r.bottom,
                      g.kieu_dang_chon == &THU_VIEN[chi_so]);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        chon_kieu(&THU_VIEN[chi_so]);
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorA(NULL, IDC_HAND));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcA(h, tin, w, l);
}

/* ====================================================================== */
/* THU VIEN                                                               */
/* ====================================================================== */
static void chon_kieu(const KieuGhep *k)
{
    int i;
    g.kieu_dang_chon = k;
    for (i = 0; i < SO_KIEU_GHEP; i++) InvalidateRect(g.bieu_tuong[i], NULL, TRUE);
    /* O nhap tham so: chi hien dung so o ma kieu nay can */
    for (i = 0; i < SO_THAM_SO_TOI_DA; i++) {
        int co = i < k->so_tham_so;
        ShowWindow(g.o_tham_so[i], co ? SW_SHOW : SW_HIDE);
        if (co) dat_chu_so(g.o_tham_so[i], k->tham_so[i].mac_dinh);
    }
    snprintf(g.goi_y, sizeof(g.goi_y), "%s: %s", k->ten, k->mo_ta);
    bo_tri();
    InvalidateRect(g.chinh, &g.vung_trai, TRUE);
}

static void them_nhat_cat(void)
{
    const KieuGhep *k = g.kieu_dang_chon;
    GiaTriThamSo gt;
    DuongCat thu;
    char loi[CO_LOI] = "";
    double x;
    int i;

    if (g.so_muc >= SO_MUC_TOI_DA) {
        canh_bao(g.chinh, "Bai qua dai", "Toi da %d nhat cat trong mot bai.",
                 SO_MUC_TOI_DA);
        return;
    }
    memset(&gt, 0, sizeof(gt));
    for (i = 0; i < k->so_tham_so; i++) {
        if (lay_so(g.o_tham_so[i], &gt.gt[i]) != 0) {
            char chu[64];
            lay_chu(g.o_tham_so[i], chu, sizeof(chu));
            canh_bao(g.chinh, "Sai so lieu", "'%s' phai la so. Dang nhap: '%s'",
                     k->tham_so[i].nhan, chu);
            return;
        }
    }
    g.dai_khuc = lay_so_hoac(g.o_dai_khuc, g.dai_khuc);
    g.khe_cat  = lay_so_hoac(g.o_khe_cat, g.khe_cat);

    if (vi_tri_ke_tiep(g.duong_kinh, g.muc, g.so_muc, g.dai_khuc, g.khe_cat,
                       g.chua_dau, &x, loi) != 0) {
        canh_bao(g.chinh, "Khong lam duoc nhat cat nay", "%s", loi);
        them_loi(loi);
        return;
    }
    /* Thu sinh truoc de bat loi ngay tai day chu khong doi den luc chay */
    duong_cat_khoi_tao(&thu);
    if (kieu_sinh(k, g.duong_kinh, &gt, x, &thu, loi) != 0) {
        char day_du[CO_LOI + 64];
        snprintf(day_du, sizeof(day_du), "%s: %s", k->ten, loi);
        canh_bao(g.chinh, "Khong lam duoc nhat cat nay", "%s", loi);
        them_loi(day_du);
        duong_cat_giai_phong(&thu);
        return;
    }
    duong_cat_giai_phong(&thu);

    ghi_lich_su();
    snprintf(g.muc[g.so_muc].ma, sizeof(g.muc[g.so_muc].ma), "%s", k->ma);
    g.muc[g.so_muc].gia_tri = gt;
    g.muc[g.so_muc].x = x;
    g.so_muc++;
    bai_da_doi();
}

/* ====================================================================== */
/* BANG BAI (tab Edit)                                                    */
/* ====================================================================== */
static void nap_lai_bang(void)
{
    KetQuaXep kq;
    char loi[CO_LOI] = "";
    int co_xep = 0, i, hang = 0;

    SendMessageA(g.bang_bai, LVM_DELETEALLITEMS, 0, 0);

    if (g.file_ngoai.so_dong > 0) {
        LVITEMA m;
        char c1[MAX_PATH + 16], c3[64];
        snprintf(c1, sizeof(c1), "[FILE] %s", g.file_ngoai.ten);
        snprintf(c3, sizeof(c3), "%d dong doc tu file", g.file_ngoai.so_dong);
        memset(&m, 0, sizeof(m));
        m.mask = LVIF_TEXT;
        m.iItem = hang;
        m.pszText = c1;
        SendMessageA(g.bang_bai, LVM_INSERTITEMA, 0, (LPARAM)&m);
        m.iSubItem = 1; m.pszText = (char *)"-";
        SendMessageA(g.bang_bai, LVM_SETITEMTEXTA, (WPARAM)hang, (LPARAM)&m);
        m.iSubItem = 2; m.pszText = c3;
        SendMessageA(g.bang_bai, LVM_SETITEMTEXTA, (WPARAM)hang, (LPARAM)&m);
        hang++;
    }

    ket_qua_xep_khoi_tao(&kq);
    co_xep = (g.so_muc > 0 &&
              xep_bai(g.duong_kinh, g.muc, g.so_muc, 0.0, &kq, loi) == 0);

    for (i = 0; i < g.so_muc; i++) {
        const KieuGhep *k = kieu_theo_ma(g.muc[i].ma);
        LVITEMA m;
        char c1[128], c2[48], c3[256];
        double x1 = 0, x2 = 0, dai = 0;
        int j, vt = 0;

        if (co_xep && i < kq.so_duong)
            khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
        snprintf(c1, sizeof(c1), "%d. %s", i + 1, k ? k->ten : g.muc[i].ma);
        snprintf(c2, sizeof(c2), "%.1f mm", g.dai_cay_ong - x2);
        c3[0] = '\0';
        for (j = 0; k && j < k->so_tham_so; j++) {
            char so[24];
            so_gon(so, sizeof(so), g.muc[i].gia_tri.gt[j]);
            vt += snprintf(c3 + vt, sizeof(c3) - (size_t)vt, "%s%s=%s%s",
                           j ? "  " : "", k->tham_so[j].nhan, so,
                           k->tham_so[j].don_vi);
            if (vt >= (int)sizeof(c3) - 1) break;
        }
        memset(&m, 0, sizeof(m));
        m.mask = LVIF_TEXT;
        m.iItem = hang;
        m.pszText = c1;
        SendMessageA(g.bang_bai, LVM_INSERTITEMA, 0, (LPARAM)&m);
        m.iSubItem = 1; m.pszText = c2;
        SendMessageA(g.bang_bai, LVM_SETITEMTEXTA, (WPARAM)hang, (LPARAM)&m);
        m.iSubItem = 2; m.pszText = c3;
        SendMessageA(g.bang_bai, LVM_SETITEMTEXTA, (WPARAM)hang, (LPARAM)&m);
        hang++;
    }
    ket_qua_xep_giai_phong(&kq);
}

static void doi_cho_nhat_cat(int huong)
{
    int chon[SO_MUC_TOI_DA], n, i, j;
    MucBai tam;
    double x;
    n = cac_nhat_cat_dang_chon(chon, SO_MUC_TOI_DA);
    if (n != 1) return;
    i = chon[0];
    j = i + huong;
    if (j < 0 || j >= g.so_muc) return;
    ghi_lich_su();
    tam = g.muc[i]; g.muc[i] = g.muc[j]; g.muc[j] = tam;
    x = g.muc[i].x; g.muc[i].x = g.muc[j].x; g.muc[j].x = x;   /* giu nguyen cho */
    bai_da_doi();
}

static void xoa_nhat_cat(void)
{
    int chon[SO_MUC_TOI_DA], n, i;
    /* Dong dau bang la file .NC (neu co) - xoa dong do la bo file ra khoi bai */
    int xoa_file = g.file_ngoai.so_dong > 0 &&
                   SendMessageA(g.bang_bai, LVM_GETITEMSTATE, 0,
                                (LPARAM)LVIS_SELECTED) != 0;
    n = cac_nhat_cat_dang_chon(chon, SO_MUC_TOI_DA);
    if (n == 0 && !xoa_file) return;
    ghi_lich_su();
    if (xoa_file) {
        ghi("he_thong", "Da bo file %s khoi bai.", g.file_ngoai.ten);
        file_ngoai_xoa(&g.file_ngoai);
        SetWindowTextA(g.chinh, TEN_PHAN_MEM);
    }
    for (i = n - 1; i >= 0; i--) {
        memmove(&g.muc[chon[i]], &g.muc[chon[i] + 1],
                sizeof(MucBai) * (size_t)(g.so_muc - chon[i] - 1));
        g.so_muc--;
    }
    xep2d_dat_dang_chon(g.xep, -1);
    bai_da_doi();
}

static void xoa_het_bai(void)
{
    if (g.so_muc == 0 && g.file_ngoai.so_dong == 0) return;
    if (!hoi_co_khong(g.chinh, "Xoa het", "Xoa toan bo bai?")) return;
    ghi_lich_su();
    g.so_muc = 0;
    file_ngoai_xoa(&g.file_ngoai);
    xep2d_dat_dang_chon(g.xep, -1);
    SetWindowTextA(g.chinh, TEN_PHAN_MEM);
    bai_da_doi();
}

/* Xep lai toan bo nhat cat cho noi tiep nhau, giu nguyen thu tu va kieu. */
static void xep_lai_ca_bai(void)
{
    MucBai tam[SO_MUC_TOI_DA];
    int so_tam = 0, i;
    char loi[CO_LOI] = "";
    if (g.so_muc == 0) return;
    g.dai_khuc = lay_so_hoac(g.o_dai_khuc, g.dai_khuc);
    g.khe_cat  = lay_so_hoac(g.o_khe_cat, g.khe_cat);
    for (i = 0; i < g.so_muc; i++) {
        double x;
        if (vi_tri_ke_tiep(g.duong_kinh, tam, so_tam, g.dai_khuc, g.khe_cat,
                           g.chua_dau, &x, loi) != 0) {
            canh_bao(g.chinh, "Khong xep duoc", "%s", loi);
            return;
        }
        tam[so_tam] = g.muc[i];
        tam[so_tam].x = x;
        so_tam++;
    }
    ghi_lich_su();
    memcpy(g.muc, tam, sizeof(MucBai) * (size_t)so_tam);
    g.so_muc = so_tam;
    bai_da_doi();
    ghi("he_thong", "Da xep lai %d nhat cat, moi khuc %gmm, cach nhau %gmm.",
        so_tam, g.dai_khuc, g.khe_cat);
}

static void kiem_tra_cay_ong(void)
{
    KetQuaXep kq;
    char loi[CO_LOI] = "", cuoi[128];
    double du;
    ket_qua_xep_khoi_tao(&kq);
    if (xep_bai(g.duong_kinh, g.muc, g.so_muc, g.dai_cay_ong, &kq, loi) != 0) {
        canh_bao(g.chinh, "Bai co van de", "%s", loi);
        ket_qua_xep_giai_phong(&kq);
        return;
    }
    du = g.dai_cay_ong - kq.tong_dung;
    if (du >= 0)
        snprintf(cuoi, sizeof(cuoi), "Cay ong %g mm con thua %.1f mm.",
                 g.dai_cay_ong, du);
    else
        snprintf(cuoi, sizeof(cuoi), "THIEU %.1f mm - cay ong khong du dai!", -du);
    bao_tin(g.chinh, "Do dai cay ong",
            "Bai gom %d nhat cat.\n"
            "Chiem toi %.1f mm tinh tu mam kep.\n\n%s",
            g.so_muc, kq.tong_dung, cuoi);
    ket_qua_xep_giai_phong(&kq);
}

/* ====================================================================== */
/* SINH G-CODE + PHAN TICH                                                */
/* ====================================================================== */
/* Doc noi dung o G-code ra mang dong. Ben goi phai free(ra) va free(bo). */
static int doc_o_gcode(char **bo, char ***ra)
{
    int dai = GetWindowTextLengthA(g.o_gcode);
    char *chu = (char *)malloc((size_t)dai + 2);
    char **dong;
    int n = 0, i, dau = 0;
    *bo = NULL; *ra = NULL;
    if (!chu) return 0;
    GetWindowTextA(g.o_gcode, chu, dai + 1);
    chu[dai] = '\0';
    for (i = 0; i <= dai; i++) if (chu[i] == '\n' || chu[i] == '\0') n++;
    dong = (char **)calloc((size_t)(n > 0 ? n : 1), sizeof(char *));
    if (!dong) { free(chu); return 0; }
    n = 0;
    for (i = 0; i <= dai; i++) {
        if (chu[i] == '\n' || chu[i] == '\0') {
            int cuoi = i;
            chu[i] = '\0';
            if (cuoi > dau && chu[cuoi - 1] == '\r') chu[cuoi - 1] = '\0';
            dong[n++] = chu + dau;
            dau = i + 1;
        }
    }
    *bo = chu;
    *ra = dong;
    return n;
}

static void dat_o_gcode(char (*dong)[CO_DONG_GCODE], int so_dong,
                        const FileNgoai *truoc)
{
    size_t can = 1;
    char *bo, *v;
    int i;
    for (i = 0; i < (truoc ? truoc->so_dong : 0); i++)
        can += strlen(truoc->dong[i]) + 2;
    for (i = 0; i < so_dong; i++) can += strlen(dong[i]) + 2;
    bo = (char *)malloc(can);
    if (!bo) return;
    v = bo;
    for (i = 0; i < (truoc ? truoc->so_dong : 0); i++)
        v += sprintf(v, "%s\r\n", truoc->dong[i]);
    for (i = 0; i < so_dong; i++)
        v += sprintf(v, "%s\r\n", dong[i]);
    *v = '\0';
    SetWindowTextA(g.o_gcode, bo);
    free(bo);
}

static void sinh_gcode_bai(void)
{
    static char dong[SO_DONG_TOI_DA][CO_DONG_GCODE];
    KetQuaXep kq;
    char loi[CO_LOI] = "";
    char td1[128], td2[64], so_dk[24];
    const char *tieu_de[2];
    int n = 0;

    if (g.so_muc == 0) {
        dat_o_gcode(dong, 0, g.file_ngoai.so_dong > 0 ? &g.file_ngoai : NULL);
        ve_lai_bai();
        return;
    }

    ket_qua_xep_khoi_tao(&kq);
    if (xep_bai(g.duong_kinh, g.muc, g.so_muc, g.dai_cay_ong, &kq, loi) != 0) {
        them_loi(loi);
        canh_bao(g.chinh, "Bai co van de", "%s", loi);
        ket_qua_xep_giai_phong(&kq);
        return;
    }
    if (kq.canh_bao[0]) them_loi(kq.canh_bao);

    if (g.file_ngoai.so_dong > 0)
        snprintf(dong[n++], CO_DONG_GCODE, "(--- cac nhat cat ve tu thu vien ---)");

    so_gon(so_dk, sizeof(so_dk), g.duong_kinh);
    snprintf(td1, sizeof(td1), "%s - ong D%s", TEN_PHAN_MEM, so_dk);
    snprintf(td2, sizeof(td2), "%d nhat cat", kq.so_duong);
    tieu_de[0] = td1;
    tieu_de[1] = td2;

    n += sinh_gcode(kq.duong, kq.so_duong, g.toc_do_cat, g.toc_do_nhanh,
                    g.thoi_gian_duc_lo, 1, 0.0, tieu_de, 2,
                    dong + n, SO_DONG_TOI_DA - n);
    ket_qua_xep_giai_phong(&kq);

    dat_o_gcode(dong, n, g.file_ngoai.so_dong > 0 ? &g.file_ngoai : NULL);
    ve_lai_bai();
}

static void cap_nhat_thong_ke(void)
{
    if (!g.co_ket_qua) return;
    g.tong_doan = g.ket_qua.so_doan;
    SendMessageA(g.truot_mo_phong, TBM_SETRANGE, TRUE,
                 MAKELPARAM(0, g.tong_doan > 0 ? g.tong_doan - 1 : 1));
    InvalidateRect(g.chinh, &g.vung_phai, TRUE);
}

static void ve_lai_bai(void)
{
    char **dong;
    char *bo;
    int n, i;

    n = doc_o_gcode(&bo, &dong);
    kq_phan_tich_giai_phong(&g.ket_qua);
    g.co_ket_qua = (phan_tich_chuong_trinh((const char *const *)dong, n,
                                           g.toc_do_cat, g.toc_do_nhanh,
                                           g.che_do, g.duong_kinh,
                                           &g.ket_qua) == 0);
    free(dong);
    free(bo);
    if (!g.co_ket_qua) {
        them_loi("Khong phan tich duoc chuong trinh (het bo nho?).");
        return;
    }
    mp3d_dat_du_lieu(g.mp, g.ket_qua.doan, g.ket_qua.so_doan,
                     g.duong_kinh, g.dai_cay_ong);
    mp3d_dat_vi_tri_chay(g.mp, -1);
    ve_3d();
    ve_2d();
    cap_nhat_thong_ke();
    for (i = 0; i < g.ket_qua.so_canh_bao; i++) {
        if (g.ket_qua.co_loi_nang) them_loi(g.ket_qua.canh_bao[i]);
        else ghi(NULL, "Ghi chu: %s", g.ket_qua.canh_bao[i]);
    }
}

static void bai_da_doi(void)
{
    nap_lai_bang();
    sinh_gcode_bai();
}

/* ====================================================================== */
/* VE 3D VA VE 2D                                                         */
/* ====================================================================== */
static void ve_3d(void)   { if (g.canvas3d) InvalidateRect(g.canvas3d, NULL, FALSE); }

static void ve_2d(void)
{
    KetQuaXep kq;
    char loi[CO_LOI] = "";
    KhungNhatCat khung[SO_MUC_TOI_DA];
    int n = 0, i;

    if (g.so_muc > 0) {
        ket_qua_xep_khoi_tao(&kq);
        if (xep_bai(g.duong_kinh, g.muc, g.so_muc, 0.0, &kq, loi) == 0) {
            for (i = 0; i < kq.so_duong && i < SO_MUC_TOI_DA; i++) {
                double x1, x2, dai;
                khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
                khung[n].x_dau = x1;
                khung[n].x_cuoi = x2;
                snprintf(khung[n].ten, sizeof(khung[n].ten), "%s", kq.duong[i].ten);
                khung[n].x_tam = g.muc[i].x;
                n++;
            }
        } else {
            them_loi(loi);
        }
        ket_qua_xep_giai_phong(&kq);
    }
    xep2d_dat_du_lieu(g.xep, khung, n, g.duong_kinh, g.dai_cay_ong);
    if (g.canvas2d) InvalidateRect(g.canvas2d, NULL, FALSE);
}

static int the_giua_dang_mo(void)
{
    return (int)SendMessageA(g.the_giua, TCM_GETCURSEL, 0, 0);
}

/* ====================================================================== */
/* THE XEP 2D: goi lai tu module xep_2d                                   */
/* ====================================================================== */
static void khi_chon_khung_2d(void *ctx, int chi_so)
{
    int lech = g.file_ngoai.so_dong > 0 ? 1 : 0;
    LVITEMA m;
    (void)ctx;
    cap_nhat_o_nhat_cat();
    memset(&m, 0, sizeof(m));
    m.mask = LVIF_STATE;
    m.stateMask = LVIS_SELECTED;
    m.state = 0;
    SendMessageA(g.bang_bai, LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&m);
    if (chi_so >= 0) {
        m.state = LVIS_SELECTED;
        SendMessageA(g.bang_bai, LVM_SETITEMSTATE, (WPARAM)(chi_so + lech),
                     (LPARAM)&m);
    }
}

static void khi_keo_khung_2d(void *ctx, int chi_so, double x_tam_moi)
{
    (void)ctx;
    if (chi_so < 0 || chi_so >= g.so_muc) return;
    if (!g.dang_keo) {
        ghi_lich_su();          /* chi ghi MOT lan cho ca lan keo */
        g.dang_keo = 1;
    }
    g.muc[chi_so].x = x_tam_moi > 0.0 ? x_tam_moi : 0.0;
    ve_2d();
    cap_nhat_o_nhat_cat();
}

static void khi_can_ve_lai_2d(void *ctx)
{
    (void)ctx;
    if (g.canvas2d) InvalidateRect(g.canvas2d, NULL, FALSE);
}

static void het_keo_2d(void)
{
    xep2d_nha(g.xep);
    if (g.dang_keo) {
        g.dang_keo = 0;
        bai_da_doi();
    }
}

static void cap_nhat_o_nhat_cat(void)
{
    int i = xep2d_dang_chon(g.xep);
    KetQuaXep kq;
    char loi[CO_LOI] = "", chu[48];
    double x1, x2, dai;

    if (i < 0 || i >= g.so_muc) {
        dat_chu(g.o_khoang_cach, "");
        InvalidateRect(g.chinh, &g.vung_phai, TRUE);
        return;
    }
    ket_qua_xep_khoi_tao(&kq);
    if (xep_bai(g.duong_kinh, g.muc, g.so_muc, 0.0, &kq, loi) == 0 &&
        i < kq.so_duong) {
        khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
        snprintf(chu, sizeof(chu), "%.1f", xep2d_khoang_cach_tu_goc(g.xep, x2));
        dat_chu(g.o_khoang_cach, chu);
    }
    ket_qua_xep_giai_phong(&kq);
    InvalidateRect(g.chinh, &g.vung_phai, TRUE);
}

static void ap_khoang_cach(void)
{
    int i = xep2d_dang_chon(g.xep);
    KetQuaXep kq;
    char loi[CO_LOI] = "";
    double kc, x1, x2, dai;

    if (i < 0 || i >= g.so_muc) return;
    if (lay_so(g.o_khoang_cach, &kc) != 0) {
        canh_bao(g.chinh, "Sai so lieu", "Khoang cach phai la so.");
        return;
    }
    ket_qua_xep_khoi_tao(&kq);
    if (xep_bai(g.duong_kinh, g.muc, g.so_muc, 0.0, &kq, loi) != 0 ||
        i >= kq.so_duong) {
        ket_qua_xep_giai_phong(&kq);
        return;
    }
    khung_duong_cat(&kq.duong[i], &x1, &x2, &dai);
    ket_qua_xep_giai_phong(&kq);
    ghi_lich_su();
    g.muc[i].x = xep2d_tu_khoang_cach(g.xep, kc, g.muc[i].x, x2);
    bai_da_doi();
}

/* ====================================================================== */
/* KET NOI ESP32                                                          */
/* ====================================================================== */
/* Cac ham nay chay tren LUONG NEN -> chi duoc phep dat ban tin vao hang doi
 * cua cua so, tuyet doi khong dung toi o dieu khien nao. */
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
static void tu_luong_loi_nap(void *ctx, const char *chu)
{
    (void)ctx;
    PostMessageA(g.chinh, WM_LOI_NAP, 0, (LPARAM)_strdup(chu));
}
static void tu_luong_vi_tri(void *ctx, double x, double a)
{
    double *d = (double *)malloc(sizeof(double) * 2);
    (void)ctx;
    if (!d) return;
    d[0] = x; d[1] = a;
    PostMessageA(g.chinh, WM_VI_TRI, 0, (LPARAM)d);
}
static void tu_luong_baud(void *ctx, int baud)
{
    (void)ctx;
    PostMessageA(g.chinh, WM_BAUD, (WPARAM)baud, 0);
}

static int dang_ket_noi(void) { return ket_noi_dang_mo(g.may); }

static void cap_nhat_nhan_cong(void)
{
    InvalidateRect(g.chinh, NULL, TRUE);
}

static void ket_noi_may(void)
{
    char loi[CO_LOI] = "";
    if (!g.cong_com[0]) {
        canh_bao(g.chinh, "Chua chon cong",
                 "Chua chon cong COM.\n\nBam 'Tham so...' de chon.");
        return;
    }
    if (ket_noi_mo(g.may, g.cong_com, BAUD_CO_DINH, loi) != 0) {
        bao_loi(g.chinh, "Loi ket noi", "%s", loi);
        them_loi(loi);
        return;
    }
    SetWindowTextA(g.nut_ket_noi, "Ngat ket noi");
    dat_trang_thai(TT_SAN_SANG);
    cap_nhat_nhan_cong();
    ghi("he_thong", "Da ket noi %s o %d baud", g.cong_com, BAUD_CO_DINH);
}

static void ngat_ket_noi(void)
{
    ket_noi_dong(g.may);
    SetWindowTextA(g.nut_ket_noi, "Ket noi");
    dat_trang_thai(TT_CHUA_KETNOI);
    cap_nhat_nhan_cong();
    ghi("he_thong", "Da ngat ket noi");
}

static int gui_lenh(const char *lenh)
{
    if (!dang_ket_noi()) {
        canh_bao(g.chinh, "Chua ket noi", "Hay ket noi cong COM truoc.");
        return 0;
    }
    if (ket_noi_gui(g.may, lenh)) ghi("gui", "> %s", lenh);
    return 1;
}

static int gui_lenh_im(const char *lenh)   /* khong ghi vao nhat ky */
{
    if (!dang_ket_noi()) return 0;
    return ket_noi_gui(g.may, lenh);
}

/* ====================================================================== */
/* DIEU KHIEN MAY                                                         */
/* ====================================================================== */
static void jog(const char *truc, int dau)
{
    char lenh[64];
    double buoc, toc_do;
    if (lay_so(g.o_buoc_nhich, &buoc) != 0 || lay_so(g.o_toc_do_tay, &toc_do) != 0) {
        canh_bao(g.chinh, "Sai so lieu", "Buoc nhich va toc do tay phai la so.");
        return;
    }
    buoc *= dau;
    if (buoc == 0 || toc_do <= 0) {
        canh_bao(g.chinh, "Sai so lieu",
                 "Buoc nhich phai khac 0 va toc do tay phai lon hon 0.");
        return;
    }
    g.buoc_nhich = buoc < 0 ? -buoc : buoc;
    g.toc_do_tay = toc_do;
    snprintf(lenh, sizeof(lenh), "JOG;%s;%g;%g", truc, buoc, toc_do);
    gui_lenh(lenh);
}

static void dat_goc(void)
{
    if (hoi_co_khong(g.chinh, "Dat goc 0",
                     "Lay vi tri hien tai lam goc 0 cua ca hai truc?"))
        gui_lenh("ZERO");
}

/* Dua ca hai truc ve diem goc 0 - hai truc chay dong thoi. */
static void ve_goc(void)
{
    char lenh[64];
    if (!dang_ket_noi()) {
        canh_bao(g.chinh, "Chua ket noi", "Hay ket noi cong COM truoc.");
        return;
    }
    gui_lenh("G90");        /* bat toa do tuyet doi cho chac */
    snprintf(lenh, sizeof(lenh), "G0 X0 A0 F%g", g.toc_do_nhanh);
    gui_lenh(lenh);
}

static void bat_tat_chay_thu(void)
{
    g.chay_thu = !g.chay_thu;
    SetWindowTextA(g.nut_chay_thu, g.chay_thu ? "Chay thu: BAT" : "Chay thu");
    InvalidateRect(g.nut_chay_thu, NULL, TRUE);
    ghi("he_thong", "Che do chay thu %s",
        g.chay_thu ? "BAT - se KHONG bat mo cat" : "TAT");
}

static void bat_tat_mo(void)
{
    if (!dang_ket_noi()) {
        canh_bao(g.chinh, "Chua ket noi", "Hay ket noi cong COM truoc.");
        return;
    }
    if (!g.mo_dang_bat &&
        !hoi_co_khong(g.chinh, "Bat mo cat",
                      "BAT MO CAT PLASMA ngay bay gio?\n\n"
                      "Chi bat khi da chac chan khong co ai dung gan may."))
        return;
    g.mo_dang_bat = !g.mo_dang_bat;
    gui_lenh(g.mo_dang_bat ? "M3" : "M5");
    SetWindowTextA(g.nut_bat_mo, g.mo_dang_bat ? "TAT mo" : "Bat mo");
    InvalidateRect(g.nut_bat_mo, NULL, TRUE);
}

static void chay_bai(void)
{
    static char nen[SO_DONG_TOI_DA][CO_DONG_G];
    static const char *tro[SO_DONG_TOI_DA];
    int n = 0, i;

    if (!dang_ket_noi()) {
        canh_bao(g.chinh, "Chua ket noi", "Hay ket noi cong COM truoc.");
        return;
    }
    ve_lai_bai();
    if (!g.co_ket_qua || g.ket_qua.so_dong == 0) {
        canh_bao(g.chinh, "Bai trong", "Chua co dong G-code nao de chay.");
        return;
    }
    if (g.ket_qua.co_loi_nang &&
        !hoi_co_khong(g.chinh, "Bai co van de",
                      "Phan kiem tra truoc phat hien van de nghiem trong "
                      "(xem tab Alarm).\n\nVan chay?"))
        return;

    for (i = 0; i < g.ket_qua.so_dong && n < SO_DONG_TOI_DA; i++) {
        const char *d = g.ket_qua.dong_chuan_hoa[i];
        if (g.chay_thu && (strcmp(d, "M3") == 0 || strcmp(d, "M4") == 0)) continue;
        if (nen_dong_gui(d, nen[n], CO_DONG_G) > 0) { tro[n] = nen[n]; n++; }
    }
    if (n == 0) {
        canh_bao(g.chinh, "Bai trong", "Chua co dong G-code nao de chay.");
        return;
    }
    if (g.chay_thu) ghi("he_thong", "CHAY THU: da bo cac lenh bat mo cat.");

    g.doan_da_chay = 0;
    g.moc_bat_dau = gio_may();
    mp3d_dat_vi_tri_chay(g.mp, 0);
    dat_trang_thai(TT_DANG_NAP);
    ket_noi_nap_va_chay(g.may, tro, n);
}

static void dung_may(void)
{
    ket_noi_huy_nap(g.may);
    if (dang_ket_noi()) {
        ket_noi_gui(g.may, "STOP");
        ghi("gui", "> STOP");
    }
    g.mo_dang_bat = 0;
    SetWindowTextA(g.nut_bat_mo, "Bat mo");
    InvalidateRect(g.nut_bat_mo, NULL, TRUE);
}

/* ====================================================================== */
/* FILE                                                                   */
/* ====================================================================== */
static void mo_file_nc(void)
{
    char duong[MAX_PATH];
    FileNgoai moi;
    if (chon_file_mo(g.chinh, duong, sizeof(duong), BO_LOC_FILE,
                     "Mo file G-code") != 0)
        return;
    if (doc_file_thanh_dong(duong, &moi) != 0) {
        bao_loi(g.chinh, "Khong doc duoc file", "Khong mo duoc %s", duong);
        return;
    }
    ghi_lich_su();
    /* File hien thanh MOT DONG trong bang Edit - chon roi bam Xoa la bo no ra */
    file_ngoai_xoa(&g.file_ngoai);
    g.file_ngoai = moi;
    bai_da_doi();
    SendMessageA(g.the_duoi, TCM_SETCURSEL, 0, 0);
    bo_tri();
    ghi("he_thong", "Da mo %s (%d dong). Chon dong [FILE] trong bang roi bam "
                    "Xoa de bo ra.", moi.ten, moi.so_dong);
    {
        char tieu[MAX_PATH + 64];
        snprintf(tieu, sizeof(tieu), "%s - %s", TEN_PHAN_MEM, moi.ten);
        SetWindowTextA(g.chinh, tieu);
    }
}

static void luu_file_gcode(void)
{
    char duong[MAX_PATH] = "";
    char *bo, **dong;
    FILE *f;
    int n, i;
    if (chon_file_luu(g.chinh, duong, sizeof(duong), BO_LOC_FILE,
                      "Luu G-code", "nc") != 0)
        return;
    f = fopen(duong, "wb");
    if (!f) {
        bao_loi(g.chinh, "Khong luu duoc", "Khong ghi duoc vao %s", duong);
        return;
    }
    n = doc_o_gcode(&bo, &dong);
    for (i = 0; i < n; i++) fprintf(f, "%s\r\n", dong[i]);
    fclose(f);
    free(dong);
    free(bo);
    ghi("he_thong", "Da luu %s", duong);
}

static void mo_cai_dat_nang_cao(void)
{
    char duong[MAX_PATH], *cat;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    GetModuleFileNameA(NULL, duong, sizeof(duong));
    cat = strrchr(duong, '\\');
    if (cat) cat[1] = '\0'; else duong[0] = '\0';
    strncat(duong, "cnc_settings.exe", sizeof(duong) - strlen(duong) - 1);
    if (GetFileAttributesA(duong) == INVALID_FILE_ATTRIBUTES) {
        canh_bao(g.chinh, "Khong tim thay",
                 "Khong thay cnc_settings.exe canh phan mem nay.");
        return;
    }
    if (dang_ket_noi()) {
        if (!hoi_co_khong(g.chinh, "Dang ket noi",
                          "Chi mot phan mem duoc giu cong COM cung luc.\n\n"
                          "Ngat ket noi de mo phan cai dat nang cao?"))
            return;
        ngat_ket_noi();
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(duong, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        bao_loi(g.chinh, "Khong mo duoc", "Khong chay duoc %s", duong);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

/* ====================================================================== */
/* HOP THOAI - dung tay bang cua so con, khong can file tai nguyen .rc     */
/* ====================================================================== */
#define ID_HOP_XONG   1
#define ID_HOP_BO_QUA 2

typedef struct {
    int dang_mo;
    int ket_qua;        /* 1 = bam Xong, 0 = bo qua */
    void *rieng;
    void (*khi_lenh)(HWND hop, int ma, void *rieng);
} TrangThaiHop;

static LRESULT CALLBACK thu_tuc_hop(HWND h, UINT tin, WPARAM w, LPARAM l)
{
    TrangThaiHop *t = (TrangThaiHop *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (tin) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)w, MAU_KHUNG);
        SetTextColor((HDC)w, MAU_CHU);
        return (LRESULT)g.nen_khung;
    case WM_COMMAND:
        if (t) {
            int ma = LOWORD(w);
            if (ma == ID_HOP_BO_QUA) { t->dang_mo = 0; return 0; }
            if (t->khi_lenh) t->khi_lenh(h, ma, t->rieng);
        }
        return 0;
    case WM_CLOSE:
        if (t) t->dang_mo = 0;
        return 0;
    }
    return DefWindowProcA(h, tin, w, l);
}

/* Tao cua so hop thoai giua man hinh cua so chinh. */
static HWND hop_tao(const char *tieu_de, int rong, int cao, TrangThaiHop *t)
{
    RECT rc;
    HWND h;
    int x, y;
    GetWindowRect(g.chinh, &rc);
    x = rc.left + ((rc.right - rc.left) - rong) / 2;
    y = rc.top + ((rc.bottom - rc.top) - cao) / 2;
    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "cnc_hop", tieu_de,
                        WS_POPUP | WS_CAPTION | WS_SYSMENU,
                        x, y, rong, cao, g.chinh, NULL, g.hinst, NULL);
    if (!h) return NULL;
    t->dang_mo = 1;
    t->ket_qua = 0;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)t);
    return h;
}

/* Vong lap rieng: khoa cua so chinh cho toi khi hop dong. */
static int hop_chay(HWND h, TrangThaiHop *t)
{
    MSG tin;
    EnableWindow(g.chinh, FALSE);
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    while (t->dang_mo && GetMessageA(&tin, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(h, &tin)) {
            TranslateMessage(&tin);
            DispatchMessageA(&tin);
        }
    }
    EnableWindow(g.chinh, TRUE);
    SetForegroundWindow(g.chinh);
    DestroyWindow(h);
    return t->ket_qua;
}

/* ---------------------------------------------------------------------- */
/* Hop THAM SO: cong COM + toc do truyen (co dinh) + 3 che do lam viec     */
/* ---------------------------------------------------------------------- */
typedef struct {
    HWND o_cong, o_tron[3];
    int che_do;
} RiengThamSo;

#define ID_TS_LAM_MOI 20
#define ID_TS_MODE_1  21

static void nap_danh_sach_cong(HWND o_cong)
{
    char ten[SO_CONG_TOI_DA][CO_TEN_CONG];
    int n = cong_liet_ke(ten, SO_CONG_TOI_DA), i;
    char dang_chon[CO_TEN_CONG];
    GetWindowTextA(o_cong, dang_chon, sizeof(dang_chon));
    SendMessageA(o_cong, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < n; i++)
        SendMessageA(o_cong, CB_ADDSTRING, 0, (LPARAM)ten[i]);
    if (!dang_chon[0] && n > 0) SetWindowTextA(o_cong, ten[0]);
}

static void lenh_hop_tham_so(HWND h, int ma, void *rieng)
{
    RiengThamSo *r = (RiengThamSo *)rieng;
    TrangThaiHop *t = (TrangThaiHop *)GetWindowLongPtrA(h, GWLP_USERDATA);
    int i;
    if (ma == ID_TS_LAM_MOI) { nap_danh_sach_cong(r->o_cong); return; }
    for (i = 0; i < 3; i++)
        if (ma == ID_TS_MODE_1 + i) { r->che_do = i + 1; return; }
    if (ma != ID_HOP_XONG) return;

    GetWindowTextA(r->o_cong, g.cong_com, sizeof(g.cong_com));
    g.che_do = r->che_do;
    if (dang_ket_noi()) {
        char lenh[64];
        snprintf(lenh, sizeof(lenh), "CFG;MODE;%d", g.che_do);
        gui_lenh(lenh);
        if (g.che_do == 2 || g.che_do == 3) {
            snprintf(lenh, sizeof(lenh), "CFG;DUONGKINH;%g", g.duong_kinh);
            gui_lenh(lenh);
        }
    }
    t->ket_qua = 1;
    t->dang_mo = 0;
}

static void hop_tham_so(void)
{
    TrangThaiHop t;
    RiengThamSo r;
    HWND h, nut;
    char chu[160];
    int i;

    memset(&t, 0, sizeof(t));
    memset(&r, 0, sizeof(r));
    r.che_do = g.che_do;
    t.rieng = &r;
    t.khi_lenh = lenh_hop_tham_so;

    h = hop_tao("Tham so may", 470, 320, &t);
    if (!h) return;

    /* Win32 khong co bo tri tu dong: tao roi dat cho tung o */
    {
        HWND nhan = CreateWindowExA(0, "STATIC", "Cong COM:",
                                    WS_CHILD | WS_VISIBLE, 12, 16, 90, 18,
                                    h, NULL, g.hinst, NULL);
        SendMessageA(nhan, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    }
    r.o_cong = CreateWindowExA(0, "COMBOBOX", "",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                               CBS_DROPDOWN | WS_VSCROLL,
                               110, 12, 150, 240, h, NULL, g.hinst, NULL);
    SendMessageA(r.o_cong, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    SetWindowTextA(r.o_cong, g.cong_com);
    nap_danh_sach_cong(r.o_cong);

    nut = CreateWindowExA(0, "BUTTON", "Lam moi",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                          272, 12, 80, 24, h,
                          (HMENU)(INT_PTR)ID_TS_LAM_MOI, g.hinst, NULL);
    SendMessageA(nut, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);

    snprintf(chu, sizeof(chu), "%d baud  (co dinh - on dinh nhat)", BAUD_CO_DINH);
    {
        HWND a = CreateWindowExA(0, "STATIC", "Toc do truyen:",
                                 WS_CHILD | WS_VISIBLE, 12, 48, 96, 18,
                                 h, NULL, g.hinst, NULL);
        HWND b = CreateWindowExA(0, "STATIC", chu, WS_CHILD | WS_VISIBLE,
                                 110, 48, 320, 18, h, NULL, g.hinst, NULL);
        SendMessageA(a, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
        SendMessageA(b, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    }
    {
        HWND a = CreateWindowExA(0, "STATIC", "Che do lam viec:",
                                 WS_CHILD | WS_VISIBLE, 12, 82, 200, 18,
                                 h, NULL, g.hinst, NULL);
        SendMessageA(a, WM_SETFONT, (WPARAM)PC_DAM, TRUE);
    }
    for (i = 0; i < 3; i++) {
        r.o_tron[i] = CreateWindowExA(0, "BUTTON", TEN_CHE_DO[i + 1],
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                      BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0),
                                      24, 106 + i * 24, 420, 20, h,
                                      (HMENU)(INT_PTR)(ID_TS_MODE_1 + i),
                                      g.hinst, NULL);
        SendMessageA(r.o_tron[i], WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
        SendMessageA(r.o_tron[i], BM_SETCHECK,
                     (WPARAM)(g.che_do == i + 1 ? BST_CHECKED : BST_UNCHECKED), 0);
    }
    {
        HWND a = CreateWindowExA(0, "STATIC",
                                 "Mode 2 va 3 can duong kinh ong - nhap o khung "
                                 "\"Kich thuoc bai\" ben phai man hinh chinh.\r\n"
                                 "Doi cong COM chi co hieu luc o lan ket noi sau.",
                                 WS_CHILD | WS_VISIBLE, 12, 188, 440, 44,
                                 h, NULL, g.hinst, NULL);
        SendMessageA(a, WM_SETFONT, (WPARAM)PC_NHO, TRUE);
    }
    nut = CreateWindowExA(0, "BUTTON", "Xong",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          140, 244, 90, 28, h,
                          (HMENU)(INT_PTR)ID_HOP_XONG, g.hinst, NULL);
    SendMessageA(nut, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    nut = CreateWindowExA(0, "BUTTON", "Bo qua",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                          240, 244, 90, 28, h,
                          (HMENU)(INT_PTR)ID_HOP_BO_QUA, g.hinst, NULL);
    SendMessageA(nut, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);

    if (hop_chay(h, &t)) {
        cap_nhat_nhan_cong();
        ve_lai_bai();
    }
}

/* ---------------------------------------------------------------------- */
/* Hop NHAP SO chung cho Toc do / Duc lo / Dieu khien tay                  */
/* ---------------------------------------------------------------------- */
#define SO_O_NHAP_TOI_DA 4
typedef struct {
    HWND o[SO_O_NHAP_TOI_DA];
    const char *nhan[SO_O_NHAP_TOI_DA];
    double *bien[SO_O_NHAP_TOI_DA];
    int so_o;
} RiengNhap;

static void lenh_hop_nhap(HWND h, int ma, void *rieng)
{
    RiengNhap *r = (RiengNhap *)rieng;
    TrangThaiHop *t = (TrangThaiHop *)GetWindowLongPtrA(h, GWLP_USERDATA);
    double tam[SO_O_NHAP_TOI_DA];
    int i;
    if (ma != ID_HOP_XONG) return;
    for (i = 0; i < r->so_o; i++) {
        if (lay_so(r->o[i], &tam[i]) != 0) {
            canh_bao(h, "Sai so lieu", "'%s' phai la so.", r->nhan[i]);
            return;
        }
        if (tam[i] <= 0) {
            canh_bao(h, "Sai so lieu", "'%s' phai lon hon 0.", r->nhan[i]);
            return;
        }
    }
    for (i = 0; i < r->so_o; i++) *r->bien[i] = tam[i];
    t->ket_qua = 1;
    t->dang_mo = 0;
}

static int hop_nhap(const char *tieu_de, const char *nhan[], double *bien[],
                    const char *don_vi[], int so_o)
{
    TrangThaiHop t;
    RiengNhap r;
    HWND h, nut;
    int i, cao = 60 + so_o * 32;

    memset(&t, 0, sizeof(t));
    memset(&r, 0, sizeof(r));
    r.so_o = so_o;
    t.rieng = &r;
    t.khi_lenh = lenh_hop_nhap;

    h = hop_tao(tieu_de, 420, cao + 40, &t);
    if (!h) return 0;
    for (i = 0; i < so_o; i++) {
        HWND a = CreateWindowExA(0, "STATIC", nhan[i], WS_CHILD | WS_VISIBLE,
                                 12, 16 + i * 32, 230, 20, h, NULL, g.hinst, NULL);
        HWND b = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                 248, 14 + i * 32, 80, 22, h, NULL, g.hinst, NULL);
        HWND c = CreateWindowExA(0, "STATIC", don_vi[i], WS_CHILD | WS_VISIBLE,
                                 334, 16 + i * 32, 70, 20, h, NULL, g.hinst, NULL);
        SendMessageA(a, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
        SendMessageA(b, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
        SendMessageA(c, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
        dat_chu_so(b, *bien[i]);
        r.o[i] = b;
        r.nhan[i] = nhan[i];
        r.bien[i] = bien[i];
    }
    nut = CreateWindowExA(0, "BUTTON", "Luu",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          110, cao - 4, 90, 28, h,
                          (HMENU)(INT_PTR)ID_HOP_XONG, g.hinst, NULL);
    SendMessageA(nut, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    nut = CreateWindowExA(0, "BUTTON", "Bo qua",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                          210, cao - 4, 90, 28, h,
                          (HMENU)(INT_PTR)ID_HOP_BO_QUA, g.hinst, NULL);
    SendMessageA(nut, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    return hop_chay(h, &t);
}

static void hop_toc_do(void)
{
    const char *nhan[2] = { "Toc do CAT", "Toc do CHAY KHONG TAI (G0)" };
    const char *don_vi[2] = { "RPM", "RPM" };
    double *bien[2];
    bien[0] = &g.toc_do_cat;
    bien[1] = &g.toc_do_nhanh;
    if (hop_nhap("Toc do", nhan, bien, don_vi, 2)) sinh_gcode_bai();
}

static void hop_duc_lo(void)
{
    const char *nhan[1] = { "Thoi gian duc lo truoc khi cat" };
    const char *don_vi[1] = { "giay" };
    double *bien[1];
    bien[0] = &g.thoi_gian_duc_lo;
    if (hop_nhap("Duc lo", nhan, bien, don_vi, 1)) sinh_gcode_bai();
}

static void hop_dieu_khien_tay(void)
{
    const char *nhan[2] = { "Toc do di chuyen tay", "Buoc nhich moi lan bam" };
    const char *don_vi[2] = { "RPM", "mm hoac do" };
    double *bien[2];
    bien[0] = &g.toc_do_tay;
    bien[1] = &g.buoc_nhich;
    if (hop_nhap("Dieu khien tay", nhan, bien, don_vi, 2)) {
        dat_chu_so(g.o_toc_do_tay, g.toc_do_tay);
        dat_chu_so(g.o_buoc_nhich, g.buoc_nhich);
    }
}

/* ---------------------------------------------------------------------- */
/* Hop CHAY TIEP: hoi truoc co can duc lo lai khong                        */
/* ---------------------------------------------------------------------- */
#define ID_CT_CO_DUC 30
typedef struct { HWND o_danh_dau, o_thoi_gian; } RiengChayTiep;

static void lenh_hop_chay_tiep(HWND h, int ma, void *rieng)
{
    RiengChayTiep *r = (RiengChayTiep *)rieng;
    TrangThaiHop *t = (TrangThaiHop *)GetWindowLongPtrA(h, GWLP_USERDATA);
    int co_duc = SendMessageA(r->o_danh_dau, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (ma == ID_CT_CO_DUC) {
        EnableWindow(r->o_thoi_gian, co_duc);
        return;
    }
    if (ma != ID_HOP_XONG) return;
    if (co_duc) {
        double giay;
        char lenh[48];
        if (lay_so(r->o_thoi_gian, &giay) != 0) {
            canh_bao(h, "Sai so lieu", "Thoi gian duc lo phai la so.");
            return;
        }
        if (!(giay > 0 && giay <= 30)) {
            canh_bao(h, "Sai so lieu",
                     "Thoi gian duc lo phai trong khoang 0..30 giay.");
            return;
        }
        t->dang_mo = 0;
        snprintf(lenh, sizeof(lenh), "RESUME;%d", (int)(giay * 1000));
        gui_lenh(lenh);
        ghi("he_thong", "Chay tiep, duc lo %g giay truoc.", giay);
    } else {
        t->dang_mo = 0;
        gui_lenh("RESUME");
        ghi("he_thong", "Chay tiep, KHONG duc lo lai.");
    }
    t->ket_qua = 1;
}

static void hop_chay_tiep(void)
{
    TrangThaiHop t;
    RiengChayTiep r;
    HWND h, w;

    memset(&t, 0, sizeof(t));
    memset(&r, 0, sizeof(r));
    t.rieng = &r;
    t.khi_lenh = lenh_hop_chay_tiep;

    h = hop_tao("Chay tiep", 480, 260, &t);
    if (!h) return;

    w = CreateWindowExA(0, "STATIC",
                        "Luc tam dung, may da TAT mo cat de khong thung phoi.\r\n\r\n"
                        "Neu dang dung giua duong cat thi phai bat mo va cho duc "
                        "xuyen qua thanh ong roi moi chay tiep, neu khong mach cat "
                        "se bi dut doan.",
                        WS_CHILD | WS_VISIBLE, 14, 12, 440, 80, h, NULL,
                        g.hinst, NULL);
    SendMessageA(w, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);

    r.o_danh_dau = CreateWindowExA(0, "BUTTON",
                                   "Bat mo va cho duc lo truoc khi chay tiep",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                   BS_AUTOCHECKBOX,
                                   14, 100, 400, 22, h,
                                   (HMENU)(INT_PTR)ID_CT_CO_DUC, g.hinst, NULL);
    SendMessageA(r.o_danh_dau, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    SendMessageA(r.o_danh_dau, BM_SETCHECK, BST_CHECKED, 0);

    w = CreateWindowExA(0, "STATIC", "Thoi gian duc lo:", WS_CHILD | WS_VISIBLE,
                        34, 132, 110, 20, h, NULL, g.hinst, NULL);
    SendMessageA(w, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    r.o_thoi_gian = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    ES_AUTOHSCROLL,
                                    150, 130, 70, 22, h, NULL, g.hinst, NULL);
    SendMessageA(r.o_thoi_gian, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    dat_chu_so(r.o_thoi_gian, g.thoi_gian_duc_lo);
    w = CreateWindowExA(0, "STATIC", "giay", WS_CHILD | WS_VISIBLE,
                        228, 132, 60, 20, h, NULL, g.hinst, NULL);
    SendMessageA(w, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);

    w = CreateWindowExA(0, "BUTTON", "CHAY TIEP",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        90, 174, 140, 32, h, (HMENU)(INT_PTR)ID_HOP_XONG,
                        g.hinst, NULL);
    SendMessageA(w, WM_SETFONT, (WPARAM)PC_DAM, TRUE);
    w = CreateWindowExA(0, "BUTTON", "Khong chay nua",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        244, 174, 140, 32, h, (HMENU)(INT_PTR)ID_HOP_BO_QUA,
                        g.hinst, NULL);
    SendMessageA(w, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);

    SetFocus(r.o_thoi_gian);
    hop_chay(h, &t);
}

/* ====================================================================== */
/* CUA SO VE 3D                                                           */
/* ====================================================================== */
static LRESULT CALLBACK thu_tuc_canvas3d(HWND h, UINT tin, WPARAM w, LPARAM l)
{
    switch (tin) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT r;
        KhungVe kv;
        GetClientRect(h, &r);
        mp3d_dung_hinh(g.mp, r.right, r.bottom, &kv);
        gdi_ve_khung_co_dem(hdc, r.right, r.bottom, &kv);
        khung_ve_giai_phong(&kv);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        SetFocus(h);
        if (g.che_do_an_cat) {
            int chi_so = mp3d_doan_gan_diem(g.mp, LOWORD(l), HIWORD(l), 14);
            if (chi_so >= 0) {
                mp3d_bat_tat_nhom(g.mp, mp3d_nhom_cua_doan(g.mp, chi_so));
                ve_3d();
            }
            return 0;
        }
        g.chuot_x = (short)LOWORD(l);
        g.chuot_y = (short)HIWORD(l);
        g.dang_xoay = 1;
        SetCapture(h);
        return 0;
    case WM_MBUTTONDOWN:
        g.chuot_x = (short)LOWORD(l);
        g.chuot_y = (short)HIWORD(l);
        g.dang_day = 1;
        SetCapture(h);
        return 0;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(l), y = (short)HIWORD(l);
        CanhNhin *cn = mp3d_canh_nhin(g.mp);
        if (g.dang_xoay && !g.che_do_an_cat) {
            cn->xoay_ngang += (x - g.chuot_x) * 0.5;
            cn->xoay_doc   -= (y - g.chuot_y) * 0.5;
            if (cn->xoay_doc >  88) cn->xoay_doc =  88;
            if (cn->xoay_doc < -88) cn->xoay_doc = -88;
            g.chuot_x = x; g.chuot_y = y;
            ve_3d();
        } else if (g.dang_day) {
            cn->day_ngang += x - g.chuot_x;
            cn->day_doc   += y - g.chuot_y;
            g.chuot_x = x; g.chuot_y = y;
            ve_3d();
        }
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
        g.dang_xoay = 0;
        g.dang_day = 0;
        ReleaseCapture();
        return 0;
    case WM_MOUSEWHEEL: {
        CanhNhin *cn = mp3d_canh_nhin(g.mp);
        double he_so = (short)HIWORD(w) > 0 ? 1.15 : 1.0 / 1.15;
        cn->phong *= he_so;
        if (cn->phong < 0.25) cn->phong = 0.25;
        if (cn->phong > 8.0)  cn->phong = 8.0;
        ve_3d();
        return 0;
    }
    case WM_SETCURSOR:
        if (g.che_do_an_cat) { SetCursor(LoadCursorA(NULL, IDC_CROSS)); return TRUE; }
        break;
    }
    return DefWindowProcA(h, tin, w, l);
}

/* ====================================================================== */
/* CUA SO THE XEP 2D                                                      */
/* ====================================================================== */
static LRESULT CALLBACK thu_tuc_canvas2d(HWND h, UINT tin, WPARAM w, LPARAM l)
{
    switch (tin) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT r;
        KhungVe kv;
        GetClientRect(h, &r);
        xep2d_dung_hinh(g.xep, r.right, r.bottom, &kv);
        gdi_ve_khung_co_dem(hdc, r.right, r.bottom, &kv);
        khung_ve_giai_phong(&kv);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        SetFocus(h);
        xep2d_bam(g.xep, (short)LOWORD(l), (short)HIWORD(l));
        SetCapture(h);
        return 0;
    case WM_MOUSEMOVE:
        if (w & MK_LBUTTON) xep2d_keo(g.xep, (short)LOWORD(l), (short)HIWORD(l));
        else if (w & MK_MBUTTON) xep2d_day_khung_nhin(g.xep, (short)LOWORD(l));
        return 0;
    case WM_LBUTTONUP:
        het_keo_2d();
        ReleaseCapture();
        return 0;
    case WM_MBUTTONDOWN:
        xep2d_bat_dau_day(g.xep, (short)LOWORD(l));
        SetCapture(h);
        return 0;
    case WM_MBUTTONUP:
        xep2d_het_day(g.xep);
        ReleaseCapture();
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p;
        p.x = (short)LOWORD(l);
        p.y = (short)HIWORD(l);
        ScreenToClient(h, &p);
        xep2d_phong_to(g.xep, (short)HIWORD(w) > 0 ? 1.2 : 1.0 / 1.2, p.x);
        return 0;
    }
    }
    return DefWindowProcA(h, tin, w, l);
}

/* ====================================================================== */
/* MENU                                                                   */
/* ====================================================================== */
static void tao_menu(HWND h)
{
    HMENU thanh = CreateMenu(), m;

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_MENU_MO,  "Mo file .NC...\tCtrl+O");
    AppendMenuA(m, MF_STRING, ID_MENU_LUU, "Luu G-code...\tCtrl+S");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, ID_MENU_THOAT, "Thoat");
    AppendMenuA(thanh, MF_POPUP, (UINT_PTR)m, "File");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_MENU_THAM_SO, "Cong COM va che do may...");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, ID_MENU_TOC_DO, "Toc do cat va toc do khong tai...");
    AppendMenuA(m, MF_STRING, ID_MENU_DUC_LO, "Thoi gian duc lo...");
    AppendMenuA(thanh, MF_POPUP, (UINT_PTR)m, "Parameters");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_MENU_XEP_LAI,  "Xep lai ca bai cho noi tiep");
    AppendMenuA(m, MF_STRING, ID_MENU_KIEM_CAY, "Kiem tra do dai cay ong");
    AppendMenuA(thanh, MF_POPUP, (UINT_PTR)m, "Nesting");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_MENU_POS, "Hoi vi tri hien tai");
    AppendMenuA(m, MF_STRING, ID_MENU_BUF, "Hoi bo dem con trong");
    AppendMenuA(m, MF_STRING, ID_MENU_CFG, "Doc cau hinh ESP32");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, ID_MENU_REBOOT, "Khoi dong lai ESP32");
    AppendMenuA(thanh, MF_POPUP, (UINT_PTR)m, "Diagnostics");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_MENU_TAY, "Buoc nhich va toc do tay...");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, ID_MENU_CAI_DAT,
                "Cai dat phan cung (chan GPIO, hieu chuan)...");
    AppendMenuA(thanh, MF_POPUP, (UINT_PTR)m, "Settings");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_MENU_XEM_LOI, "Xem danh sach loi");
    AppendMenuA(m, MF_STRING, ID_MENU_XOA_LOI, "Xoa danh sach loi");
    AppendMenuA(thanh, MF_POPUP, (UINT_PTR)m, "Alarm");

    SetMenu(h, thanh);
}

/* ====================================================================== */
/* TAO CAC O DIEU KHIEN                                                   */
/* ====================================================================== */
static HWND tao_the(HWND cha, int id, const char *cac_the[], int so_the)
{
    HWND h = CreateWindowExA(0, WC_TABCONTROLA, "",
                             WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                             0, 0, 10, 10, cha, (HMENU)(INT_PTR)id, g.hinst, NULL);
    TCITEMA m;
    int i;
    SendMessageA(h, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    memset(&m, 0, sizeof(m));
    m.mask = TCIF_TEXT;
    for (i = 0; i < so_the; i++) {
        m.pszText = (char *)cac_the[i];
        SendMessageA(h, TCM_INSERTITEMA, (WPARAM)i, (LPARAM)&m);
    }
    return h;
}

static HWND tao_bang(HWND cha, int id, const char *cot[], const int rong[],
                     int so_cot)
{
    HWND h = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                             LVS_REPORT | LVS_SHOWSELALWAYS,
                             0, 0, 10, 10, cha, (HMENU)(INT_PTR)id, g.hinst, NULL);
    LVCOLUMNA c;
    int i;
    SendMessageA(h, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    SendMessageA(h, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                 LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    memset(&c, 0, sizeof(c));
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    for (i = 0; i < so_cot; i++) {
        c.pszText = (char *)cot[i];
        c.cx = rong[i];
        SendMessageA(h, LVM_INSERTCOLUMNA, (WPARAM)i, (LPARAM)&c);
    }
    return h;
}

static void tao_o_dieu_khien(HWND h)
{
    static const char *the_giua[]  = { "  Mo phong 3D  ", "  Xep tren cay ong  " };
    static const char *the_duoi[]  = { "  Edit (ve bai)  ", "  System (terminal)  ",
                                       "  Alarm (loi)  " };
    static const char *cot_bai[]   = { "Noi dung", "Cach dau xa", "So do" };
    static const int  rong_bai[]   = { 200, 92, 320 };
    static const char *cot_loi[]   = { "Thoi diem", "Noi dung" };
    static const int  rong_loi[]   = { 90, 880 };
    static const char *ten_tay[4]  = { "X-", "X+", "A nguoc", "A thuan" };
    static const char *ten_hang[8] = { "Mo .NC", "Ve goc 0", "Chay thu", "Bat mo",
                                       "CHAY", "TAM DUNG", "CHAY TIEP",
                                       "DUNG (Esc)" };
    int i;

    /* ---- Thanh tren ---- */
    g.nut_ket_noi = tao_nut(h, "Ket noi", ID_NUT_KET_NOI);
    g.nut_tham_so = tao_nut(h, "Tham so...", ID_NUT_THAM_SO);

    /* ---- Cot trai: thu vien ---- */
    for (i = 0; i < SO_KIEU_GHEP; i++) {
        g.bieu_tuong[i] = CreateWindowExA(0, "cnc_bieu_tuong", "",
                                          WS_CHILD | WS_VISIBLE,
                                          0, 0, 68, 42, h,
                                          (HMENU)(INT_PTR)(ID_BT_0 + i),
                                          g.hinst, NULL);
        SetWindowLongPtrA(g.bieu_tuong[i], GWLP_USERDATA, (LONG_PTR)i);
    }
    g.o_dai_khuc = tao_o_nhap(h, "", ID_O_DAI_KHUC);
    for (i = 0; i < SO_THAM_SO_TOI_DA; i++)
        g.o_tham_so[i] = tao_o_nhap(h, "", ID_O_TS_0 + i);
    g.nut_them = tao_nut(h, "THEM VAO BAI", ID_NUT_THEM);

    /* ---- Cot trai: dieu khien tay ---- */
    for (i = 0; i < 4; i++)
        g.nut_tay[i] = tao_nut(h, ten_tay[i], ID_JOG_X_TRU + i);
    g.o_toc_do_tay = tao_o_nhap(h, "", ID_O_TOC_DO_TAY);
    g.o_buoc_nhich = tao_o_nhap(h, "", ID_O_BUOC_NHICH);
    g.nut_tay[4] = tao_nut(h, "DAT GOC 0 TAI DAY", ID_NUT_DAT_GOC);

    /* ---- Giua: the 3D / xep 2D ---- */
    g.the_giua = tao_the(h, ID_THE_GIUA, the_giua, 2);
    g.canvas3d = CreateWindowExA(0, "cnc_canvas3d", "", WS_CHILD | WS_VISIBLE,
                                 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_CANVAS_3D,
                                 g.hinst, NULL);
    g.canvas2d = CreateWindowExA(0, "cnc_canvas2d", "", WS_CHILD,
                                 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_CANVAS_2D,
                                 g.hinst, NULL);
    g.nut_the3d[0] = tao_nut(h, "Goc nhin mac dinh", ID_NUT_GOC_NHIN);
    g.nut_an_cat   = tao_nut(h, "An bot duong cat", ID_NUT_AN_CAT);
    g.nut_the3d[1] = g.nut_an_cat;
    g.nut_the3d[2] = tao_nut(h, "Hien lai het", ID_NUT_HIEN_LAI);
    g.truot_mo_phong = CreateWindowExA(0, TRACKBAR_CLASSA, "",
                                       WS_CHILD | WS_VISIBLE | TBS_HORZ |
                                       TBS_NOTICKS,
                                       0, 0, 10, 10, h,
                                       (HMENU)(INT_PTR)ID_TRUOT_MO_PHONG,
                                       g.hinst, NULL);
    g.nut_the2d[0] = tao_nut(h, "Vua khung hinh", ID_NUT_VUA_KHUNG);
    g.nut_the2d[1] = tao_nut(h, "Xep lai noi tiep", ID_NUT_XEP_LAI);
    g.o_dai_cay = tao_o_nhap(h, "", ID_O_DAI_CAY);
    g.o_khe_cat = tao_o_nhap(h, "", ID_O_KHE_CAT);

    /* ---- Cot phai ---- */
    g.o_khoang_cach = tao_o_nhap(h, "", ID_O_KHOANG_CACH);
    g.nut_thu_vien[1] = tao_nut(h, "Ap dung", ID_NUT_AP_KC);
    g.tien_do = CreateWindowExA(0, PROGRESS_CLASSA, "",
                                WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, h,
                                (HMENU)(INT_PTR)ID_TIEN_DO, g.hinst, NULL);
    SendMessageA(g.tien_do, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    g.o_duong_kinh = tao_o_nhap(h, "", ID_O_DUONG_KINH);
    g.nut_thu_vien[2] = tao_nut(h, "Ap dung duong kinh", ID_NUT_AP_DK);

    /* ---- Hang nut lon ---- */
    for (i = 0; i < 8; i++)
        g.nut_hang[i] = tao_nut(h, ten_hang[i], ID_NUT_MO_NC + i);
    g.nut_ve_goc   = g.nut_hang[1];
    g.nut_chay_thu = g.nut_hang[2];
    g.nut_bat_mo   = g.nut_hang[3];
    g.nut_chay     = g.nut_hang[4];
    g.nut_tam_dung = g.nut_hang[5];
    g.nut_chay_tiep = g.nut_hang[6];
    g.nut_dung     = g.nut_hang[7];
    for (i = 4; i < 8; i++)
        SendMessageA(g.nut_hang[i], WM_SETFONT, (WPARAM)PC_DAM, TRUE);

    /* ---- The duoi ---- */
    g.the_duoi = tao_the(h, ID_THE_DUOI, the_duoi, 3);
    g.bang_bai = tao_bang(h, ID_BANG_BAI, cot_bai, rong_bai, 3);
    for (i = 0; i < 4; i++) {
        static const char *ten[4] = { "Len", "Xuong", "Xoa", "Xoa het" };
        g.nut_bang[i] = tao_nut(h, ten[i], ID_NUT_LEN + i);
    }
    g.o_gcode = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                                ES_AUTOHSCROLL | ES_WANTRETURN,
                                0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_O_GCODE,
                                g.hinst, NULL);
    SendMessageA(g.o_gcode, WM_SETFONT, (WPARAM)PC_DEU, TRUE);
    SendMessageA(g.o_gcode, EM_SETLIMITTEXT, (WPARAM)(4 * 1024 * 1024), 0);
    g.nut_bang[4] = tao_nut(h, "NAP LAI G-CODE (F5)", ID_NUT_NAP_LAI);

    g.term = CreateWindowExA(WS_EX_CLIENTEDGE, "RICHEDIT50W", "",
                             WS_CHILD | WS_VSCROLL | ES_MULTILINE |
                             ES_READONLY | ES_AUTOVSCROLL,
                             0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_TERM,
                             g.hinst, NULL);
    if (!g.term)        /* may cu chi co RichEdit 2.0 */
        g.term = CreateWindowExA(WS_EX_CLIENTEDGE, "RichEdit20A", "",
                                 WS_CHILD | WS_VSCROLL | ES_MULTILINE |
                                 ES_READONLY | ES_AUTOVSCROLL,
                                 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_TERM,
                                 g.hinst, NULL);
    if (g.term) {
        SendMessageA(g.term, WM_SETFONT, (WPARAM)PC_DEU, TRUE);
        SendMessageA(g.term, EM_SETBKGNDCOLOR, 0, (LPARAM)MAU_TERM_NEN);
        SendMessageA(g.term, EM_SETLIMITTEXT, (WPARAM)(2 * 1024 * 1024), 0);
    }
    g.o_lenh = tao_o_nhap(h, "", ID_O_LENH);
    ShowWindow(g.o_lenh, SW_HIDE);
    SendMessageA(g.o_lenh, WM_SETFONT, (WPARAM)PC_DEU, TRUE);
    g.nut_bang[5] = tao_nut(h, "Gui", ID_NUT_GUI_LENH);
    g.nut_bang[6] = tao_nut(h, "Xoa man hinh", ID_NUT_XOA_TERM);

    g.bang_loi = tao_bang(h, ID_BANG_LOI, cot_loi, rong_loi, 2);
    ShowWindow(g.bang_loi, SW_HIDE);
    ShowWindow(g.nut_bang[5], SW_HIDE);
    ShowWindow(g.nut_bang[6], SW_HIDE);
}

/* ====================================================================== */
/* BO TRI - Win32 khong tu xep, phai tinh tung o mot                      */
/* ====================================================================== */
#define X_LE           6
#define ROG_TRAI       250
#define ROG_PHAI       214
#define CAO_TREN       40
#define CAO_HANG_NUT   40
#define CAO_THE_DUOI   180
#define CAO_TRANG_THAI 24
#define CAO_TAY        156

static RECT o_vi_tri, o_tien_do, o_kich_thuoc, o_thu_vien, o_tay;
static RECT o_the_giua_trong, o_the_duoi_trong;

static void bo_tri(void)
{
    RECT rc;
    int rong, cao, y0, y_trang_thai, y_the_duoi, y_hang_nut, than_cuoi;
    int x_phai, x_giua, rong_giua, y, i;
    const KieuGhep *k = g.kieu_dang_chon;
    int so_hang_ts = 1 + (k ? k->so_tham_so : 0);

    if (!g.chinh) return;
    GetClientRect(g.chinh, &rc);
    rong = rc.right;
    cao = rc.bottom;
    if (rong < 200 || cao < 200) return;

    y0 = CAO_TREN + 4;
    y_trang_thai = cao - CAO_TRANG_THAI;
    y_the_duoi = y_trang_thai - CAO_THE_DUOI;
    y_hang_nut = y_the_duoi - CAO_HANG_NUT - 4;
    than_cuoi = y_hang_nut - 4;
    x_phai = rong - X_LE - ROG_PHAI;
    x_giua = X_LE + ROG_TRAI + 6;
    rong_giua = x_phai - 6 - x_giua;

    /* ---------------- Thanh tren ---------------- */
    dat_cho(g.nut_ket_noi, X_LE + 8, 8, 92, 26);
    dat_cho(g.nut_tham_so, X_LE + 330, 8, 100, 26);

    /* ---------------- Cot trai ---------------- */
    o_tay.left = X_LE; o_tay.right = X_LE + ROG_TRAI;
    o_tay.top = than_cuoi - CAO_TAY; o_tay.bottom = than_cuoi;
    o_thu_vien.left = X_LE; o_thu_vien.right = X_LE + ROG_TRAI;
    o_thu_vien.top = y0; o_thu_vien.bottom = o_tay.top - 6;

    for (i = 0; i < SO_KIEU_GHEP; i++)
        dat_cho(g.bieu_tuong[i], X_LE + 10 + i * 74, y0 + 22, 68, 42);

    /* Nut THEM sat day khung thu vien, luoi tham so nam ngay tren no */
    y = o_thu_vien.bottom - 12 - 30;
    dat_cho(g.nut_them, X_LE + 10, y, ROG_TRAI - 20, 30);
    y -= 6 + so_hang_ts * 26;
    dat_cho(g.o_dai_khuc, X_LE + 150, y + 2, 62, 22);
    for (i = 0; i < SO_THAM_SO_TOI_DA; i++)
        dat_cho(g.o_tham_so[i], X_LE + 150, y + 2 + (i + 1) * 26, 62, 22);

    /* Dieu khien tay */
    for (i = 0; i < 4; i++)
        dat_cho(g.nut_tay[i], X_LE + 10 + (i % 2) * 116, o_tay.top + 22 + (i / 2) * 30,
                110, 26);
    dat_cho(g.o_toc_do_tay, X_LE + 56,  o_tay.top + 86, 46, 22);
    dat_cho(g.o_buoc_nhich, X_LE + 168, o_tay.top + 86, 46, 22);
    dat_cho(g.nut_tay[4], X_LE + 10, o_tay.top + 116, ROG_TRAI - 20, 28);

    /* ---------------- Giua: the 3D / xep 2D ---------------- */
    dat_cho(g.the_giua, x_giua, y0, rong_giua, than_cuoi - y0);
    o_the_giua_trong.left = x_giua + 4;
    o_the_giua_trong.top = y0 + 26;
    o_the_giua_trong.right = x_giua + rong_giua - 4;
    o_the_giua_trong.bottom = than_cuoi - 4;
    y = o_the_giua_trong.bottom - 30;
    dat_cho(g.canvas3d, o_the_giua_trong.left, o_the_giua_trong.top,
            o_the_giua_trong.right - o_the_giua_trong.left,
            y - o_the_giua_trong.top - 4);
    dat_cho(g.canvas2d, o_the_giua_trong.left, o_the_giua_trong.top,
            o_the_giua_trong.right - o_the_giua_trong.left,
            y - o_the_giua_trong.top - 4);
    dat_cho(g.nut_the3d[0], o_the_giua_trong.left, y, 126, 26);
    dat_cho(g.nut_an_cat,   o_the_giua_trong.left + 130, y, 126, 26);
    dat_cho(g.nut_the3d[2], o_the_giua_trong.left + 260, y, 96, 26);
    dat_cho(g.truot_mo_phong, o_the_giua_trong.left + 362, y,
            o_the_giua_trong.right - o_the_giua_trong.left - 366, 26);
    dat_cho(g.nut_the2d[0], o_the_giua_trong.left, y, 116, 26);
    dat_cho(g.nut_the2d[1], o_the_giua_trong.left + 120, y, 126, 26);
    dat_cho(g.o_dai_cay, o_the_giua_trong.left + 330, y + 2, 62, 22);
    dat_cho(g.o_khe_cat, o_the_giua_trong.left + 480, y + 2, 50, 22);

    /* ---------------- Cot phai ---------------- */
    o_vi_tri.left = x_phai; o_vi_tri.right = x_phai + ROG_PHAI;
    o_vi_tri.top = y0;
    o_vi_tri.bottom = y0 + (the_giua_dang_mo() == 1 ? 126 : 92);
    dat_cho(g.o_khoang_cach, x_phai + 10, o_vi_tri.top + 42, 130, 24);
    dat_cho(g.nut_thu_vien[1], x_phai + 10, o_vi_tri.top + 70, ROG_PHAI - 20, 24);

    o_tien_do.left = x_phai; o_tien_do.right = x_phai + ROG_PHAI;
    o_tien_do.top = o_vi_tri.bottom + 6;
    o_tien_do.bottom = o_tien_do.top + 100;
    dat_cho(g.tien_do, x_phai + 10, o_tien_do.top + 22, ROG_PHAI - 20, 16);

    o_kich_thuoc.left = x_phai; o_kich_thuoc.right = x_phai + ROG_PHAI;
    o_kich_thuoc.top = o_tien_do.bottom + 6;
    o_kich_thuoc.bottom = o_kich_thuoc.top + 130;
    dat_cho(g.o_duong_kinh, x_phai + 116, o_kich_thuoc.top + 20, 56, 22);
    dat_cho(g.nut_thu_vien[2], x_phai + 10, o_kich_thuoc.top + 48, ROG_PHAI - 20, 24);

    g.vung_trai.left = 0; g.vung_trai.top = 0;
    g.vung_trai.right = X_LE + ROG_TRAI + 4; g.vung_trai.bottom = than_cuoi;
    g.vung_phai.left = x_phai - 4; g.vung_phai.top = y0 - 4;
    g.vung_phai.right = rong; g.vung_phai.bottom = than_cuoi;

    /* ---------------- Hang nut lon ---------------- */
    {
        int rong_nut = (rong - X_LE * 2 - 7 * 4) / 8;
        for (i = 0; i < 8; i++)
            dat_cho(g.nut_hang[i], X_LE + i * (rong_nut + 4), y_hang_nut,
                    rong_nut, CAO_HANG_NUT);
    }

    /* ---------------- The duoi ---------------- */
    dat_cho(g.the_duoi, X_LE, y_the_duoi, rong - X_LE * 2, CAO_THE_DUOI);
    o_the_duoi_trong.left = X_LE + 4;
    o_the_duoi_trong.top = y_the_duoi + 26;
    o_the_duoi_trong.right = rong - X_LE - 4;
    o_the_duoi_trong.bottom = y_the_duoi + CAO_THE_DUOI - 4;
    {
        int t = o_the_duoi_trong.top, b = o_the_duoi_trong.bottom;
        int l = o_the_duoi_trong.left, r = o_the_duoi_trong.right;
        int giua = l + (r - l) / 2;
        /* Edit: bang bai + 4 nut o giua, o G-code ben phai */
        dat_cho(g.bang_bai, l, t, giua - l - 86, b - t);
        for (i = 0; i < 4; i++)
            dat_cho(g.nut_bang[i], giua - 82 + (i % 2) * 40, t + (i / 2) * 26,
                    38, 24);
        dat_cho(g.nut_bang[4], r - 150, t, 150, 22);
        dat_cho(g.o_gcode, giua + 4, t + 26, r - giua - 4, b - t - 26);
        /* System: khung nhat ky + dong lenh */
        dat_cho(g.term, l, t, r - l, b - t - 28);
        dat_cho(g.o_lenh, l + 14, b - 24, r - l - 200, 22);
        dat_cho(g.nut_bang[5], r - 180, b - 25, 50, 24);
        dat_cho(g.nut_bang[6], r - 126, b - 25, 110, 24);
        /* Alarm */
        dat_cho(g.bang_loi, l, t, r - l, b - t);
    }
}

/* Hien / an cac o theo the dang mo. */
static void doi_the_giua(void)
{
    int the = the_giua_dang_mo();
    int la_3d = (the == 0);
    int i;
    ShowWindow(g.canvas3d, la_3d ? SW_SHOW : SW_HIDE);
    ShowWindow(g.canvas2d, la_3d ? SW_HIDE : SW_SHOW);
    for (i = 0; i < 3; i++) ShowWindow(g.nut_the3d[i], la_3d ? SW_SHOW : SW_HIDE);
    ShowWindow(g.truot_mo_phong, la_3d ? SW_SHOW : SW_HIDE);
    for (i = 0; i < 2; i++) ShowWindow(g.nut_the2d[i], la_3d ? SW_HIDE : SW_SHOW);
    ShowWindow(g.o_dai_cay, la_3d ? SW_HIDE : SW_SHOW);
    ShowWindow(g.o_khe_cat, la_3d ? SW_HIDE : SW_SHOW);
    /* Sang the XEP thi o ben phai doi thanh o nhap vi tri nhat cat */
    ShowWindow(g.o_khoang_cach, la_3d ? SW_HIDE : SW_SHOW);
    ShowWindow(g.nut_thu_vien[1], la_3d ? SW_HIDE : SW_SHOW);
    bo_tri();
    if (la_3d) ve_3d(); else { ve_2d(); cap_nhat_o_nhat_cat(); }
    InvalidateRect(g.chinh, NULL, TRUE);
}

static void doi_the_duoi(void)
{
    int the = (int)SendMessageA(g.the_duoi, TCM_GETCURSEL, 0, 0);
    int i;
    ShowWindow(g.bang_bai, the == 0 ? SW_SHOW : SW_HIDE);
    for (i = 0; i < 5; i++) ShowWindow(g.nut_bang[i], the == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.o_gcode, the == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.term,   the == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.o_lenh, the == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.nut_bang[5], the == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.nut_bang[6], the == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g.bang_loi, the == 2 ? SW_SHOW : SW_HIDE);
    InvalidateRect(g.chinh, NULL, TRUE);
}

/* ====================================================================== */
/* VE CAC KHUNG THONG TIN (phan khong phai o dieu khien)                  */
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
    if (tieu_de) ve_chu(hdc, r->left + 8, r->top + 3, tieu_de, PC_DAM, MAU_CHU);
}

/* Mot dong "ten .... gia tri" trong cac khung ben phai */
static void ve_dong(HDC hdc, const RECT *r, int y, const char *ten,
                    const char *gia_tri)
{
    ve_chu(hdc, r->left + 10, y, ten, PC_NHO, MAU_CHU_MO);
    ve_chu_phai(hdc, r->right - 10, y, gia_tri, PC_DAM, MAU_CHU);
}

/* O hien so lon (X, A) */
static void ve_o_so(HDC hdc, int x, int y, int rong, const char *ten,
                    double gia_tri, const char *don_vi)
{
    RECT o;
    char chu[32];
    HBRUSH nen = CreateSolidBrush(RGB(0xf6, 0xf8, 0xfa));
    o.left = x + 22; o.top = y; o.right = x + rong - 30; o.bottom = y + 26;
    FillRect(hdc, &o, nen);
    DeleteObject(nen);
    DrawEdge(hdc, &o, EDGE_SUNKEN, BF_RECT);
    ve_chu(hdc, x, y + 5, ten, PC_DAM, MAU_CHU);
    snprintf(chu, sizeof(chu), "%.2f", gia_tri);
    ve_chu_phai(hdc, o.right - 6, y + 4, chu, PC_DEU_TO, MAU_CHU);
    ve_chu(hdc, o.right + 6, y + 5, don_vi, PC_NHO, MAU_CHU_MO);
}

static void ve_bang_dieu_khien(HDC hdc)
{
    RECT rc, r;
    char chu[256], so[32];
    const KieuGhep *k = g.kieu_dang_chon;
    int y, i;

    GetClientRect(g.chinh, &rc);

    /* ---- Thanh tren ---- */
    r = rc; r.bottom = CAO_TREN;
    ve_khung(hdc, &r, NULL);
    if (dang_ket_noi())
        snprintf(chu, sizeof(chu), "%s - %d baud", g.cong_com, BAUD_CO_DINH);
    else
        snprintf(chu, sizeof(chu), "Chua ket noi (%s)", g.cong_com);
    ve_chu(hdc, X_LE + 108, 14, chu, PC_NHO,
           dang_ket_noi() ? MAU_CHAY : MAU_CHU_MO);
    ve_chu(hdc, X_LE + 442, 13, TEN_CHE_DO[g.che_do], PC_DAM, MAU_NHAN);

    /* ---- Khung thu vien ---- */
    ve_khung(hdc, &o_thu_vien, " Thu vien moi noi ");
    if (k) {
        ve_chu(hdc, X_LE + 12, o_thu_vien.top + 68, k->ten, PC_DAM, MAU_NHAN);
        /* Mo ta dai - de Windows tu ngat dong trong khung */
        r.left = X_LE + 12; r.top = o_thu_vien.top + 88;
        r.right = X_LE + ROG_TRAI - 10;
        r.bottom = o_thu_vien.bottom - 120;
        {
            HFONT cu = (HFONT)SelectObject(hdc, PC_NHO);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, MAU_CHU_MO);
            DrawTextA(hdc, k->mo_ta, -1, &r, DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(hdc, cu);
        }
        /* Nhan cua luoi tham so */
        y = o_thu_vien.bottom - 12 - 30 - 6 - (1 + k->so_tham_so) * 26;
        ve_chu(hdc, X_LE + 12, y + 6, "Chieu dai khuc ong", PC_NHO, MAU_CHU);
        ve_chu(hdc, X_LE + 216, y + 6, "mm", PC_NHO, MAU_CHU_MO);
        for (i = 0; i < k->so_tham_so; i++) {
            ve_chu(hdc, X_LE + 12, y + 6 + (i + 1) * 26, k->tham_so[i].nhan,
                   PC_NHO, MAU_CHU);
            ve_chu(hdc, X_LE + 216, y + 6 + (i + 1) * 26, k->tham_so[i].don_vi,
                   PC_NHO, MAU_CHU_MO);
        }
    }

    /* ---- Khung dieu khien tay ---- */
    ve_khung(hdc, &o_tay, " Dieu khien tay ");
    ve_chu(hdc, X_LE + 12,  o_tay.top + 90, "Toc do:", PC_NHO, MAU_CHU);
    ve_chu(hdc, X_LE + 106, o_tay.top + 90, "RPM", PC_NHO, MAU_CHU_MO);
    ve_chu(hdc, X_LE + 132, o_tay.top + 90, "Buoc:", PC_NHO, MAU_CHU);
    ve_chu(hdc, X_LE + 218, o_tay.top + 90, "mm", PC_NHO, MAU_CHU_MO);

    /* ---- Cot phai: vi tri may HOAC vi tri nhat cat ---- */
    if (the_giua_dang_mo() == 1) {
        int i2 = xep2d_dang_chon(g.xep);
        ve_khung(hdc, &o_vi_tri, " Vi tri nhat cat ");
        ve_chu(hdc, o_vi_tri.left + 10, o_vi_tri.top + 24,
               "Cach dau ong xa nhat:", PC_NHO, MAU_CHU_MO);
        ve_chu(hdc, o_vi_tri.left + 146, o_vi_tri.top + 47, "mm", PC_NHO, MAU_CHU_MO);
        if (i2 >= 0 && i2 < g.so_muc) {
            KetQuaXep kq;
            char loi[CO_LOI] = "";
            const KieuGhep *kk = kieu_theo_ma(g.muc[i2].ma);
            double x1 = 0, x2 = 0, dai = 0;
            ket_qua_xep_khoi_tao(&kq);
            if (xep_bai(g.duong_kinh, g.muc, g.so_muc, 0.0, &kq, loi) == 0 &&
                i2 < kq.so_duong)
                khung_duong_cat(&kq.duong[i2], &x1, &x2, &dai);
            ket_qua_xep_giai_phong(&kq);
            snprintf(chu, sizeof(chu), "Nhat %d: %s", i2 + 1,
                     kk ? kk->ten : g.muc[i2].ma);
            ve_chu(hdc, o_vi_tri.left + 10, o_vi_tri.top + 96, chu, PC_NHO, MAU_CHU_MO);
            snprintf(chu, sizeof(chu), "Rong %.1f mm, cach mam kep %.1f..%.1f mm",
                     dai, x1, x2);
            ve_chu(hdc, o_vi_tri.left + 10, o_vi_tri.top + 110, chu, PC_NHO,
                   MAU_CHU_MO);
        } else {
            ve_chu(hdc, o_vi_tri.left + 10, o_vi_tri.top + 100,
                   "Chua chon nhat cat", PC_NHO, MAU_CHU_MO);
        }
    } else {
        ve_khung(hdc, &o_vi_tri, " Vi tri may ");
        ve_o_so(hdc, o_vi_tri.left + 10, o_vi_tri.top + 22, ROG_PHAI - 20,
                "X", g.x_hien, "mm");
        ve_o_so(hdc, o_vi_tri.left + 10, o_vi_tri.top + 54, ROG_PHAI - 20,
                "A", g.a_hien, "do");
    }

    /* ---- Tien do ---- */
    ve_khung(hdc, &o_tien_do, " Tien do ");
    {
        double pt = g.tong_doan > 0
                    ? 100.0 * g.doan_da_chay / g.tong_doan : 0.0;
        int giay = g.moc_bat_dau >= 0 ? (int)(gio_may() - g.moc_bat_dau) : 0;
        if (pt > 100) pt = 100;
        snprintf(chu, sizeof(chu), "%.0f%%", pt);
        ve_chu_phai(hdc, o_tien_do.right - 10, o_tien_do.top + 40, chu,
                    PC_NHO, MAU_CHU_MO);
        snprintf(chu, sizeof(chu), "%g / %g RPM", g.toc_do_cat, g.toc_do_nhanh);
        ve_dong(hdc, &o_tien_do, o_tien_do.top + 58, "Toc do cat", chu);
        snprintf(chu, sizeof(chu), "%02d:%02d:%02d",
                 giay / 3600, (giay / 60) % 60, giay % 60);
        ve_dong(hdc, &o_tien_do, o_tien_do.top + 76, "Thoi gian chay", chu);
    }

    /* ---- Kich thuoc bai ---- */
    ve_khung(hdc, &o_kich_thuoc, " Kich thuoc bai ");
    ve_chu(hdc, o_kich_thuoc.left + 10, o_kich_thuoc.top + 24,
           "Duong kinh ong:", PC_NHO, MAU_CHU);
    ve_chu(hdc, o_kich_thuoc.left + 176, o_kich_thuoc.top + 24, "mm",
           PC_NHO, MAU_CHU_MO);
    if (g.co_ket_qua && g.ket_qua.so_doan > 0) {
        double nho = g.ket_qua.doan[0].x1, lon = nho;
        for (i = 0; i < g.ket_qua.so_doan; i++) {
            double a = g.ket_qua.doan[i].x1, b = g.ket_qua.doan[i].x2;
            if (a < nho) nho = a;
            if (b < nho) nho = b;
            if (a > lon) lon = a;
            if (b > lon) lon = b;
        }
        snprintf(chu, sizeof(chu), "%.0f .. %.0f mm", nho, lon);
    } else {
        snprintf(chu, sizeof(chu), "-");
    }
    ve_dong(hdc, &o_kich_thuoc, o_kich_thuoc.top + 78, "Bai chiem doc ong", chu);
    snprintf(so, sizeof(so), "%d", g.co_ket_qua ? g.ket_qua.so_dong : 0);
    ve_dong(hdc, &o_kich_thuoc, o_kich_thuoc.top + 96, "So dong G-code", so);
    snprintf(so, sizeof(so), "%d / %d",
             g.co_ket_qua ? g.ket_qua.so_buoc_firmware : 0, SUC_CHUA_BO_DEM);
    ve_dong(hdc, &o_kich_thuoc, o_kich_thuoc.top + 114, "Buoc / bo dem", so);

    /* ---- Nhan phu cho the xep 2D ---- */
    if (the_giua_dang_mo() == 1) {
        int y2 = o_the_giua_trong.bottom - 30;
        ve_chu(hdc, o_the_giua_trong.left + 252, y2 + 6, "Dai cay ong:",
               PC_NHO, MAU_CHU);
        ve_chu(hdc, o_the_giua_trong.left + 396, y2 + 6, "mm", PC_NHO, MAU_CHU_MO);
        ve_chu(hdc, o_the_giua_trong.left + 420, y2 + 6, "Cach nhau:",
               PC_NHO, MAU_CHU);
        ve_chu(hdc, o_the_giua_trong.left + 534, y2 + 6, "mm", PC_NHO, MAU_CHU_MO);
    }

    /* ---- Thanh trang thai ---- */
    {
        RECT t;
        HBRUSH b;
        t.left = 0; t.top = rc.bottom - CAO_TRANG_THAI;
        t.right = rc.right; t.bottom = rc.bottom;
        b = CreateSolidBrush(MAU_NUT);
        FillRect(hdc, &t, b);
        DeleteObject(b);
        t.right = 150;
        b = CreateSolidBrush(BANG_TRANG_THAI[g.trang_thai].mau);
        FillRect(hdc, &t, b);
        DeleteObject(b);
        ve_chu(hdc, 10, t.top + 4, BANG_TRANG_THAI[g.trang_thai].chu,
               PC_DAM, RGB(255, 255, 255));
        ve_chu(hdc, 160, t.top + 5, g.goi_y, PC_NHO, MAU_CHU_MO);
    }
}

/* ====================================================================== */
/* TRANG THAI VA NUT                                                      */
/* ====================================================================== */
static void cap_nhat_nut(void)
{
    int noi = dang_ket_noi();
    int chay = (g.trang_thai == TT_DANG_CHAY || g.trang_thai == TT_DANG_NAP);
    int tam_dung = (g.trang_thai == TT_TAM_DUNG);
    EnableWindow(g.nut_chay,      noi && !chay && g.trang_thai != TT_LOI);
    EnableWindow(g.nut_tam_dung,  g.trang_thai == TT_DANG_CHAY);
    EnableWindow(g.nut_chay_tiep, tam_dung);
    EnableWindow(g.nut_dung,      noi);
    EnableWindow(g.nut_ve_goc,    noi && !chay && !tam_dung);
    EnableWindow(g.nut_bat_mo,    noi && !chay);
}

static void dat_trang_thai(TrangThai tt)
{
    g.trang_thai = tt;
    cap_nhat_nut();
    InvalidateRect(g.chinh, NULL, FALSE);
}

static void cap_nhat_tien_do(void)
{
    int pt;
    if (g.tong_doan <= 0) return;
    pt = (int)(100.0 * g.doan_da_chay / g.tong_doan);
    if (pt > 100) pt = 100;
    SendMessageA(g.tien_do, PBM_SETPOS, (WPARAM)pt, 0);
    mp3d_dat_vi_tri_chay(g.mp, g.doan_da_chay < g.tong_doan
                               ? g.doan_da_chay : g.tong_doan - 1);
    if (the_giua_dang_mo() == 0) ve_3d();
    InvalidateRect(g.chinh, &g.vung_phai, FALSE);
}

static void xong_bai(void)
{
    g.doan_da_chay = g.tong_doan;
    cap_nhat_tien_do();
    g.moc_bat_dau = -1;
    g.mo_dang_bat = 0;
    SetWindowTextA(g.nut_bat_mo, "Bat mo");
}

static void doc_cau_hinh(const char *dong)
{
    const char *p = dong + 4;
    while (*p) {
        char ten[32];
        int n = 0;
        while (*p == ' ') p++;
        while (*p && *p != '=' && *p != ' ' && n < (int)sizeof(ten) - 1)
            ten[n++] = *p++;
        ten[n] = '\0';
        if (*p != '=') { while (*p && *p != ' ') p++; continue; }
        p++;
        if (strcmp(ten, "che_do") == 0) {
            int m = atoi(p);
            if (m >= 1 && m <= 3) g.che_do = m;
        } else if (strcmp(ten, "duong_kinh_ong") == 0) {
            double d = atof(p);
            if (d > 0) {
                g.duong_kinh = d;
                dat_chu_so(g.o_duong_kinh, d);
            }
        }
        while (*p && *p != ' ') p++;
    }
    InvalidateRect(g.chinh, NULL, TRUE);
}

static void tu_esp32(const char *dong)
{
    const char *the = NULL;
    if (strncmp(dong, "Loi:", 4) == 0 || strncmp(dong, "LOI_", 4) == 0) {
        the = "loi";
        them_loi(dong);
    } else if (strncmp(dong, "OK", 2) == 0 || strncmp(dong, "RUNNING", 7) == 0 ||
               strncmp(dong, "ZEROED", 6) == 0 || strncmp(dong, "RESUMED", 7) == 0 ||
               strncmp(dong, "Hoan thanh", 10) == 0 ||
               strncmp(dong, "PLASMA_", 7) == 0 || strncmp(dong, "XONG_", 5) == 0 ||
               strncmp(dong, "DUC_LO", 6) == 0) {
        the = "ok";
    }
    ghi(the, "%s", dong);

    if (strncmp(dong, "Loi: DUNG KHAN CAP", 18) == 0) {
        dat_trang_thai(TT_LOI);
    } else if (strncmp(dong, "RUNNING", 7) == 0 || strncmp(dong, "RESUMED", 7) == 0) {
        dat_trang_thai(TT_DANG_CHAY);
    } else if (strncmp(dong, "PAUSED", 6) == 0 || strncmp(dong, "M0_PAUSED", 9) == 0) {
        dat_trang_thai(TT_TAM_DUNG);
    } else if (strncmp(dong, "STOPPED", 7) == 0 ||
               strncmp(dong, "Da dung han", 11) == 0) {
        dat_trang_thai(TT_DA_DUNG);
    } else if (strncmp(dong, "XONG_CHUONG_TRINH", 17) == 0) {
        dat_trang_thai(TT_SAN_SANG);
        xong_bai();
    } else if (strncmp(dong, "He thong: da het dieu kien loi", 30) == 0) {
        dat_trang_thai(TT_SAN_SANG);
    } else if (strncmp(dong, "PLASMA_ON", 9) == 0) {
        g.mo_dang_bat = 1;
        SetWindowTextA(g.nut_bat_mo, "TAT mo");
    } else if (strncmp(dong, "PLASMA_OFF", 10) == 0) {
        g.mo_dang_bat = 0;
        SetWindowTextA(g.nut_bat_mo, "Bat mo");
    }

    if (strncmp(dong, "Hoan thanh", 10) == 0) {
        g.doan_da_chay++;
        cap_nhat_tien_do();
    }
    if (strncmp(dong, "CFG:", 4) == 0) doc_cau_hinh(dong);
}

/* ====================================================================== */
/* XU LY LENH                                                             */
/* ====================================================================== */
static void ap_duong_kinh(void)
{
    double d;
    if (lay_so(g.o_duong_kinh, &d) != 0) {
        canh_bao(g.chinh, "Sai so lieu", "Duong kinh phai la so.");
        return;
    }
    if (d <= 0) {
        canh_bao(g.chinh, "Sai so lieu", "Duong kinh phai lon hon 0.");
        return;
    }
    g.duong_kinh = d;
    if (dang_ket_noi()) {
        char lenh[64];
        snprintf(lenh, sizeof(lenh), "CFG;DUONGKINH;%g", d);
        gui_lenh(lenh);
    }
    bai_da_doi();
}

static void gui_lenh_go(void)
{
    char lenh[CO_DONG_NHAN];
    lay_chu(g.o_lenh, lenh, sizeof(lenh));
    if (lenh[0] && gui_lenh(lenh)) SetWindowTextA(g.o_lenh, "");
}

static void bat_tat_an_cat(void)
{
    g.che_do_an_cat = !g.che_do_an_cat;
    SetWindowTextA(g.nut_an_cat, g.che_do_an_cat ? "Dang an: bam duong cat"
                                                 : "An bot duong cat");
    snprintf(g.goi_y, sizeof(g.goi_y), "%s",
             g.che_do_an_cat
             ? "Bam vao mot duong cat trong mo phong de an no di cho de nhin."
             : "");
    InvalidateRect(g.chinh, NULL, FALSE);
}

static void xu_ly_lenh(int ma, int bao)
{
    switch (ma) {
    case ID_NUT_KET_NOI:
        if (dang_ket_noi()) ngat_ket_noi(); else ket_noi_may();
        break;
    case ID_NUT_THAM_SO: case ID_MENU_THAM_SO: hop_tham_so(); break;
    case ID_MENU_TOC_DO: hop_toc_do(); break;
    case ID_MENU_DUC_LO: hop_duc_lo(); break;
    case ID_MENU_TAY:    hop_dieu_khien_tay(); break;
    case ID_MENU_CAI_DAT: mo_cai_dat_nang_cao(); break;
    case ID_NUT_THEM:    them_nhat_cat(); break;
    case ID_JOG_X_TRU:   jog("X", -1); break;
    case ID_JOG_X_CONG:  jog("X", +1); break;
    case ID_JOG_A_TRU:   jog("A", -1); break;
    case ID_JOG_A_CONG:  jog("A", +1); break;
    case ID_NUT_DAT_GOC: dat_goc(); break;
    case ID_NUT_GOC_NHIN:
        canh_nhin_dat_lai(mp3d_canh_nhin(g.mp));
        ve_3d();
        break;
    case ID_NUT_AN_CAT:  bat_tat_an_cat(); break;
    case ID_NUT_HIEN_LAI: mp3d_hien_lai_het(g.mp); ve_3d(); break;
    case ID_NUT_VUA_KHUNG: xep2d_vua_khung_hinh(g.xep); break;
    case ID_NUT_XEP_LAI: case ID_MENU_XEP_LAI: xep_lai_ca_bai(); break;
    case ID_MENU_KIEM_CAY: kiem_tra_cay_ong(); break;
    case ID_NUT_AP_KC:   ap_khoang_cach(); break;
    case ID_NUT_AP_DK:   ap_duong_kinh(); break;
    case ID_NUT_MO_NC: case ID_MENU_MO: mo_file_nc(); break;
    case ID_MENU_LUU:    luu_file_gcode(); break;
    case ID_MENU_THOAT:  PostMessageA(g.chinh, WM_CLOSE, 0, 0); break;
    case ID_NUT_VE_GOC:  ve_goc(); break;
    case ID_NUT_CHAY_THU: bat_tat_chay_thu(); break;
    case ID_NUT_BAT_MO:  bat_tat_mo(); break;
    case ID_NUT_CHAY:    chay_bai(); break;
    case ID_NUT_TAM_DUNG: gui_lenh("PAUSE"); break;
    case ID_NUT_CHAY_TIEP: hop_chay_tiep(); break;
    case ID_NUT_DUNG:    dung_may(); break;
    case ID_NUT_LEN:     doi_cho_nhat_cat(-1); break;
    case ID_NUT_XUONG:   doi_cho_nhat_cat(+1); break;
    case ID_NUT_XOA:     xoa_nhat_cat(); break;
    case ID_NUT_XOA_HET: xoa_het_bai(); break;
    case ID_NUT_NAP_LAI: ve_lai_bai(); break;
    case ID_NUT_GUI_LENH: gui_lenh_go(); break;
    case ID_NUT_XOA_TERM: xoa_terminal(); break;
    case ID_MENU_POS:    gui_lenh("POS"); break;
    case ID_MENU_BUF:    gui_lenh("BUF"); break;
    case ID_MENU_CFG:    gui_lenh("CFG;GET"); break;
    case ID_MENU_REBOOT:
        if (hoi_co_khong(g.chinh, "Khoi dong lai", "Khoi dong lai ESP32?"))
            gui_lenh("CFG;REBOOT");
        break;
    case ID_MENU_XEM_LOI:
        SendMessageA(g.the_duoi, TCM_SETCURSEL, 2, 0);
        doi_the_duoi();
        break;
    case ID_MENU_XOA_LOI: xoa_danh_sach_loi(); break;
    case ID_O_DAI_CAY:
        if (bao == EN_KILLFOCUS) {
            g.dai_cay_ong = lay_so_hoac(g.o_dai_cay, g.dai_cay_ong);
            ve_lai_bai();
        }
        break;
    case ID_O_KHE_CAT:
        if (bao == EN_KILLFOCUS) g.khe_cat = lay_so_hoac(g.o_khe_cat, g.khe_cat);
        break;
    case ID_O_DAI_KHUC:
        if (bao == EN_KILLFOCUS) g.dai_khuc = lay_so_hoac(g.o_dai_khuc, g.dai_khuc);
        break;
    default:
        break;
    }
}

/* True neu con tro dang o trong mot o nhap - de khong cuop phim tat cua no. */
static int dang_go_chu(void)
{
    HWND o = GetFocus();
    char lop[32];
    if (!o) return 0;
    GetClassNameA(o, lop, sizeof(lop));
    return _stricmp(lop, "EDIT") == 0 || _stricmp(lop, "COMBOBOX") == 0 ||
           strncmp(lop, "RICHEDIT", 8) == 0 || strncmp(lop, "RichEdit", 8) == 0;
}

/* ====================================================================== */
/* THU TUC CUA SO CHINH                                                   */
/* ====================================================================== */
static LRESULT CALLBACK thu_tuc_chinh(HWND h, UINT tin, WPARAM w, LPARAM l)
{
    switch (tin) {
    case WM_COMMAND:
        xu_ly_lenh(LOWORD(w), HIWORD(w));
        return 0;

    case WM_NOTIFY: {
        NMHDR *n = (NMHDR *)l;
        if (n->hwndFrom == g.the_giua && n->code == (UINT)TCN_SELCHANGE) {
            doi_the_giua();
        } else if (n->hwndFrom == g.the_duoi && n->code == (UINT)TCN_SELCHANGE) {
            doi_the_duoi();
        } else if (n->hwndFrom == g.bang_bai && n->code == (UINT)LVN_ITEMCHANGED) {
            NMLISTVIEW *lv = (NMLISTVIEW *)l;
            if (lv->uChanged & LVIF_STATE) {
                int chon[SO_MUC_TOI_DA];
                int so = cac_nhat_cat_dang_chon(chon, SO_MUC_TOI_DA);
                xep2d_dat_dang_chon(g.xep, so > 0 ? chon[0] : -1);
                cap_nhat_o_nhat_cat();
                if (the_giua_dang_mo() == 1)
                    InvalidateRect(g.canvas2d, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_HSCROLL:
        if ((HWND)l == g.truot_mo_phong && g.co_ket_qua && g.ket_qua.so_doan > 0) {
            int v = (int)SendMessageA(g.truot_mo_phong, TBM_GETPOS, 0, 0);
            mp3d_dat_vi_tri_chay(g.mp, v);
            ve_3d();
        }
        return 0;

    case WM_CTLCOLORSTATIC:
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
        ve_bang_dieu_khien(hdc);
        EndPaint(h, &ps);
        return 0;
    }

    case WM_SIZE:
        bo_tri();
        InvalidateRect(h, NULL, TRUE);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *m = (MINMAXINFO *)l;
        m->ptMinTrackSize.x = 1040;
        m->ptMinTrackSize.y = 700;
        return 0;
    }

    /* ---- Ban tin tu luong nen ---- */
    case WM_ESP32:
        if (l) { tu_esp32((const char *)l); free((void *)l); }
        return 0;
    case WM_NHAT_KY:
        if (l) { ghi("he_thong", "%s", (const char *)l); free((void *)l); }
        return 0;
    case WM_LOI_NAP:
        if (l) {
            char *chu = (char *)l;
            char day_du[CO_LOI + 32];
            dat_trang_thai(TT_SAN_SANG);
            snprintf(day_du, sizeof(day_du), "NAP THAT BAI: %s", chu);
            them_loi(day_du);
            bao_loi(h, "Nap that bai", "%s\n\nMay CHUA chay.", chu);
            free(chu);
        }
        return 0;
    case WM_VI_TRI:
        if (l) {
            double *d = (double *)l;
            g.x_hien = d[0];
            g.a_hien = d[1];
            free(d);
            if (the_giua_dang_mo() == 0)
                InvalidateRect(h, &g.vung_phai, FALSE);
        }
        return 0;
    case WM_BAUD:
        return 0;

    case WM_TIMER:
        if (w == 1) {
            /* Hoi POS moi 2 giay khi may DANG RANH de o vi tri luon dung.
             * KHONG hoi luc dang chay: moi lenh gui xuong deu lam ESP32 in ra
             * UART, ma in giua chuoi cat se chan vong xuat xung -> tao vet
             * dung tren duong cat. */
            if (dang_ket_noi() && (g.trang_thai == TT_SAN_SANG ||
                                   g.trang_thai == TT_DA_DUNG ||
                                   g.trang_thai == TT_TAM_DUNG))
                gui_lenh_im("POS");
        } else if (w == 2) {
            if (g.moc_bat_dau >= 0)
                InvalidateRect(h, &g.vung_phai, FALSE);
        }
        return 0;

    case WM_CLOSE:
        if ((g.trang_thai == TT_DANG_CHAY || g.trang_thai == TT_DANG_NAP) &&
            !hoi_co_khong(h, "Dang chay", "May DANG CHAY. Van thoat phan mem?"))
            return 0;
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
/* PHIM TAT                                                               */
/* ====================================================================== */
static int phim_tat(MSG *tin)
{
    int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (tin->message != WM_KEYDOWN) return 0;
    if (tin->wParam == VK_F5) { ve_lai_bai(); return 1; }
    if (tin->wParam == VK_ESCAPE) { dung_may(); return 1; }
    if (ctrl) {
        switch (tin->wParam) {
        case 'O': mo_file_nc(); return 1;
        case 'S': luu_file_gcode(); return 1;
        }
    }
    /* Cac phim lam viec voi NHAT CAT dang chon - nhuong lai cho o nhap chu */
    if (dang_go_chu()) return 0;
    if (ctrl) {
        switch (tin->wParam) {
        case 'Z': hoan_tac(); return 1;
        case 'Y': lam_lai(); return 1;
        case 'C': sao_chep_nhat_cat(0); return 1;
        case 'X': sao_chep_nhat_cat(1); return 1;
        case 'V': dan_nhat_cat(); return 1;
        }
    }
    if (tin->wParam == VK_DELETE) { xoa_nhat_cat(); return 1; }
    return 0;
}

/* ====================================================================== */
/* KHOI DONG                                                              */
/* ====================================================================== */
static void dang_ky_lop(HINSTANCE hi, const char *ten, WNDPROC thu_tuc,
                        HBRUSH nen)
{
    WNDCLASSA c;
    memset(&c, 0, sizeof(c));
    c.lpfnWndProc = thu_tuc;
    c.hInstance = hi;
    c.hCursor = LoadCursorA(NULL, IDC_ARROW);
    c.hbrBackground = nen;
    c.lpszClassName = ten;
    RegisterClassA(&c);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE truoc, LPSTR dong_lenh, int hien)
{
    MSG tin;
    HamGoiLai gl;
    INITCOMMONCONTROLSEX icc;
    char ten_cong[SO_CONG_TOI_DA][CO_TEN_CONG];
    int so_cong;

    (void)truoc; (void)dong_lenh;

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS |
                ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    LoadLibraryA("Msftedit.dll");       /* RichEdit 4.1 cho khung nhat ky */
    LoadLibraryA("Riched20.dll");
    tien_ich_khoi_tao();

    memset(&g, 0, sizeof(g));
    g.hinst = hi;
    g.che_do = 1;
    g.duong_kinh = 60.0;
    g.dai_cay_ong = 1000.0;
    g.toc_do_cat = 15.0;
    g.toc_do_nhanh = 60.0;
    g.toc_do_tay = 30.0;
    g.buoc_nhich = 1.0;
    g.thoi_gian_duc_lo = 0.8;
    g.dai_khuc = 200.0;
    g.khe_cat = 8.0;
    g.chua_dau = 20.0;
    g.moc_bat_dau = -1;
    g.kieu_dang_chon = &THU_VIEN[0];
    g.trang_thai = TT_CHUA_KETNOI;
    kq_phan_tich_khoi_tao(&g.ket_qua);

    g.nen_khung = CreateSolidBrush(MAU_KHUNG);
    g.nen_nen   = CreateSolidBrush(MAU_NEN);
    g.nen_term  = CreateSolidBrush(MAU_TERM_NEN);

    so_cong = cong_liet_ke(ten_cong, SO_CONG_TOI_DA);
    snprintf(g.cong_com, sizeof(g.cong_com), "%s",
             so_cong > 0 ? ten_cong[0] : "COM3");

    g.mp = mp3d_tao();
    {
        HamXep2D hx;
        memset(&hx, 0, sizeof(hx));
        hx.khi_chon = khi_chon_khung_2d;
        hx.khi_keo = khi_keo_khung_2d;
        hx.can_ve_lai = khi_can_ve_lai_2d;
        g.xep = xep2d_tao(&hx);
    }
    memset(&gl, 0, sizeof(gl));
    gl.dong_esp32 = tu_luong_dong;
    gl.nhat_ky = tu_luong_nhat_ky;
    gl.loi_nap = tu_luong_loi_nap;
    gl.vi_tri = tu_luong_vi_tri;
    gl.baud = tu_luong_baud;
    g.may = ket_noi_tao(&gl);

    dang_ky_lop(hi, "cnc_chinh", thu_tuc_chinh, g.nen_nen);
    dang_ky_lop(hi, "cnc_hop", thu_tuc_hop, g.nen_khung);
    dang_ky_lop(hi, "cnc_bieu_tuong", thu_tuc_bieu_tuong, g.nen_khung);
    dang_ky_lop(hi, "cnc_canvas3d", thu_tuc_canvas3d, NULL);
    dang_ky_lop(hi, "cnc_canvas2d", thu_tuc_canvas2d, NULL);

    g.chinh = CreateWindowExA(0, "cnc_chinh", TEN_PHAN_MEM,
                              WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1180, 760,
                              NULL, NULL, hi, NULL);
    if (!g.chinh) return 1;
    tao_menu(g.chinh);
    tao_o_dieu_khien(g.chinh);

    dat_chu_so(g.o_dai_khuc, g.dai_khuc);
    dat_chu_so(g.o_toc_do_tay, g.toc_do_tay);
    dat_chu_so(g.o_buoc_nhich, g.buoc_nhich);
    dat_chu_so(g.o_dai_cay, g.dai_cay_ong);
    dat_chu_so(g.o_khe_cat, g.khe_cat);
    dat_chu_so(g.o_duong_kinh, g.duong_kinh);

    chon_kieu(&THU_VIEN[0]);
    doi_the_giua();
    doi_the_duoi();
    cap_nhat_nut();
    bo_tri();
    ShowWindow(g.chinh, hien);
    UpdateWindow(g.chinh);

    ghi("he_thong", "%s - san sang. Chon cong COM o 'Tham so...' roi bam Ket noi.",
        TEN_PHAN_MEM);
    SetTimer(g.chinh, 1, 2000, NULL);   /* hoi vi tri dinh ky */
    SetTimer(g.chinh, 2, 500, NULL);    /* dong ho thoi gian chay */

    while (GetMessageA(&tin, NULL, 0, 0) > 0) {
        if (phim_tat(&tin)) continue;
        if (IsDialogMessageA(g.chinh, &tin)) continue;
        TranslateMessage(&tin);
        DispatchMessageA(&tin);
    }

    KillTimer(g.chinh, 1);
    KillTimer(g.chinh, 2);
    ket_noi_giai_phong(g.may);
    mp3d_giai_phong(g.mp);
    xep2d_giai_phong(g.xep);
    kq_phan_tich_giai_phong(&g.ket_qua);
    file_ngoai_xoa(&g.file_ngoai);
    DeleteObject(g.nen_khung);
    DeleteObject(g.nen_nen);
    DeleteObject(g.nen_term);
    tien_ich_don_dep();
    return 0;
}
