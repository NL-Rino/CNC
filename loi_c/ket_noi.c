#include "ket_noi.h"
#include "loi_chung.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

const int BAUD_THU_DAN[SO_BAUD_THU] = {
    2000000, 1000000, 921600, 460800, 230400, 115200
};

struct KetNoi {
    HamGoiLai gl;
    CongCom *cong;
    Khoa *khoa;                 /* bao ve cac o so dung chung giua hai luong */

    volatile int dang_mo;
    volatile int baud_dang_dung;

    /* --- Trang thai dieu tiet luu luong khi nap dan --- */
    volatile int cho_trong;         /* so o trong con lai trong vong dem ESP32 */
    volatile int so_dong_da_nhan;   /* so dong ESP32 da bao nhan */
    volatile int ket_qua_nap;       /* 0 chua biet, 1 OK, -1 LOI */
    volatile int dang_nap;          /* dat 0 de huy nap giua chung */
    volatile int pong;

    /* Ban sao chuong trinh dang nap (luong nap so huu) */
    char *bo_dong;                  /* cac chuoi noi tiep nhau, ket thuc bang '\0' */
    const char **chi_muc;
    int so_dong_nap;

    int baud_chon;
    char ten_cong[CO_TEN_CONG];
};

/* ------------------------------------------------------------------ TIEN ICH */
static void bao_nhat_ky(KetNoi *k, const char *dinh_dang, ...)
{
    char chu[CO_LOI];
    va_list ds;
    if (!k->gl.nhat_ky) return;
    va_start(ds, dinh_dang);
    vsnprintf(chu, sizeof(chu), dinh_dang, ds);
    va_end(ds);
    k->gl.nhat_ky(k->gl.ctx, chu);
}

static void bao_loi_nap(KetNoi *k, const char *dinh_dang, ...)
{
    char chu[CO_LOI];
    va_list ds;
    if (!k->gl.loi_nap) return;
    va_start(ds, dinh_dang);
    vsnprintf(chu, sizeof(chu), dinh_dang, ds);
    va_end(ds);
    k->gl.loi_nap(k->gl.ctx, chu);
}

int ket_noi_doc_vi_tri(const char *dong, double *x, double *a)
{
    const char *p = dong;
    int co_x = 0, co_a = 0;
    double gx = 0, ga = 0;
    while (*p) {
        if ((p[0] == 'X' || p[0] == 'A') && p[1] == '=') {
            char *ket = NULL;
            double gt = strtod(p + 2, &ket);
            if (ket != p + 2) {
                if (p[0] == 'X') { gx = gt; co_x = 1; }
                else             { ga = gt; co_a = 1; }
                p = ket;
                continue;
            }
        }
        p++;
    }
    if (!co_x || !co_a) return -1;
    if (x) *x = gx;
    if (a) *a = ga;
    return 0;
}

/* ------------------------------------------------------------------- XU LY */
/* Bo dau cach hai dau va ky tu xuong dong (tuong duong .strip() ben Python) */
static void cat_gon(char *s)
{
    char *dau = s, *cuoi;
    while (*dau == ' ' || *dau == '\t' || *dau == '\r' || *dau == '\n') dau++;
    if (dau != s) memmove(s, dau, strlen(dau) + 1);
    cuoi = s + strlen(s);
    while (cuoi > s && (cuoi[-1] == ' ' || cuoi[-1] == '\t' ||
                        cuoi[-1] == '\r' || cuoi[-1] == '\n'))
        *--cuoi = '\0';
}

/* Lay phan thu "so_thu" (dem tu 0) trong chuoi ngan cach bang dau ';'.
 * Tra 0 = doc duoc so nguyen, -1 = khong co / khong phai so. */
static int lay_phan_so(const char *dong, int so_thu, int *ra)
{
    const char *p = dong;
    int i;
    char *ket = NULL;
    long gt;
    for (i = 0; i < so_thu; i++) {
        p = strchr(p, ';');
        if (!p) return -1;
        p++;
    }
    gt = strtol(p, &ket, 10);
    if (ket == p) return -1;
    *ra = (int)gt;
    return 0;
}

static void xu_ly_dong(KetNoi *k, const char *dong)
{
    /* --- Bao nhan khi nap dan: "OK;<cho_trong>;<so_dong_da_nhan>" ---
     * ESP32 bao theo LO 8 dong cho do ton bang thong; so dong lay THANG tu
     * ban tin nen bao theo lo hay tung dong deu cho ket qua nhu nhau. */
    if (strncmp(dong, "OK;", 3) == 0 || strncmp(dong, "BUF;", 4) == 0) {
        int gt;
        if (lay_phan_so(dong, 1, &gt) == 0) k->cho_trong = gt;
        if (lay_phan_so(dong, 2, &gt) == 0) k->so_dong_da_nhan = gt;
        return;                     /* khong lam ngap khung nhat ky */
    }
    if (strncmp(dong, "OK_BEGIN;", 9) == 0) {
        int gt;
        if (lay_phan_so(dong, 1, &gt) == 0) k->cho_trong = gt;
        return;
    }
    if (strncmp(dong, "PONG;", 5) == 0) {
        int gt;
        if (lay_phan_so(dong, 1, &gt) == 0) k->pong = gt;
        return;
    }
    if (strncmp(dong, "OK_NAP", 6) == 0) {
        int gt;
        k->ket_qua_nap = 1;
        if (lay_phan_so(dong, 1, &gt) == 0) k->so_dong_da_nhan = gt;
    } else if (strncmp(dong, "LOI_NAP", 7) == 0) {
        k->ket_qua_nap = -1;
    }

    if (strncmp(dong, "Vi tri:", 7) == 0) {
        double x, a;
        if (ket_noi_doc_vi_tri(dong, &x, &a) == 0 && k->gl.vi_tri)
            k->gl.vi_tri(k->gl.ctx, x, a);
    }

    if (k->gl.dong_esp32) k->gl.dong_esp32(k->gl.ctx, dong);
}

/* ----------------------------------------------------------- LUONG DOC COM */
static void vong_doc(void *tham_so)
{
    KetNoi *k = (KetNoi *)tham_so;
    char dem[CO_DONG_NHAN];
    int co_dem = 0;

    while (k->dang_mo && cong_dang_mo(k->cong)) {
        char tho[512];
        int n = cong_doc(k->cong, tho, (int)sizeof(tho));
        int i;
        if (n < 0) break;
        for (i = 0; i < n; i++) {
            char c = tho[i];
            if (c == '\n' || c == '\r') {
                if (co_dem > 0) {
                    dem[co_dem] = '\0';
                    cat_gon(dem);
                    if (dem[0]) xu_ly_dong(k, dem);
                    co_dem = 0;
                }
            } else if (co_dem < (int)sizeof(dem) - 1) {
                dem[co_dem++] = c;
            }
            /* dong dai qua muc thi phan thua bi bo - khong bao gio xay ra voi
             * ban tin cua firmware, chi de khong bao gio tran bo dem */
        }
    }
    k->dang_mo = 0;
}

/* ------------------------------------------------------------------- GUI */
int ket_noi_gui(KetNoi *k, const char *lenh)
{
    char dem[CO_DONG_NHAN + 2];
    int n;
    if (!k || !cong_dang_mo(k->cong)) return 0;
    n = snprintf(dem, sizeof(dem), "%s\n", lenh);
    if (n < 0) return 0;
    if (n >= (int)sizeof(dem)) n = (int)sizeof(dem) - 1;
    if (cong_ghi(k->cong, dem, n) < 0) {
        bao_nhat_ky(k, "Loi gui lenh: mat ket noi cong COM.");
        return 0;
    }
    return 1;
}

/* ------------------------------------------- THUONG LUONG TOC DO DUONG COM */
static int thu_mot_baud(KetNoi *k, int baud)
{
    char lenh[32];
    int lan;
    k->pong = 0;
    snprintf(lenh, sizeof(lenh), "BAUD;%d", baud);
    ket_noi_gui(k, lenh);
    ngu_ms(250);                        /* cho ESP32 tra loi va doi baud */
    if (cong_dat_baud(k->cong, baud) != 0) return 0;   /* may tinh doi theo */
    ngu_ms(150);
    cong_xoa_dem_vao(k->cong);

    for (lan = 0; lan < 3; lan++) {     /* PING vai lan phong khi rot goi */
        double han;
        k->pong = 0;
        ket_noi_gui(k, "PING");
        han = gio_giay() + 0.5;
        while (gio_giay() < han) {
            if (k->pong == baud) return 1;
            ngu_ms(10);
        }
    }
    cong_dat_baud(k->cong, BAUD_KHOI_DONG);
    cong_xoa_dem_vao(k->cong);
    return 0;
}

/* Nang baud len muc cao nhat ma may THUC SU chay duoc.
 *
 * An toan tuyet doi - khong bao gio mat lien lac:
 *   1. Gui BAUD;<n>, ESP32 tra OK_BAUD roi doi toc do
 *   2. May tinh cung doi, gui PING
 *   3. Co PONG  -> giu toc do nay
 *      Khong co -> may tinh ve 115200; ESP32 CUNG tu ve 115200 sau 4 giay
 *                  (luoi an toan nam trong firmware), roi thu muc thap hon
 */
static void thuong_luong_baud(void *tham_so)
{
    KetNoi *k = (KetNoi *)tham_so;
    int danh_sach[SO_BAUD_THU];
    int so_muc, i, da_chot = 0;

    if (k->baud_chon > 0) {
        danh_sach[0] = k->baud_chon;
        so_muc = 1;
    } else {
        memcpy(danh_sach, BAUD_THU_DAN, sizeof(danh_sach));
        so_muc = SO_BAUD_THU;
    }

    for (i = 0; i < so_muc; i++) {
        int baud = danh_sach[i];
        if (!k->dang_mo) return;
        if (baud == BAUD_KHOI_DONG) break;      /* dang o san toc do nay roi */
        if (thu_mot_baud(k, baud)) {
            k->baud_dang_dung = baud;
            if (k->gl.baud) k->gl.baud(k->gl.ctx, baud);
            bao_nhat_ky(k, "Da nang toc do duong COM len %d baud (%dx nhanh hon truoc).",
                        baud, baud / BAUD_KHOI_DONG);
            da_chot = 1;
            break;
        }
        bao_nhat_ky(k, "%d baud khong on dinh, thu muc thap hon...", baud);
        ngu_ms(4500);       /* cho ESP32 tu ve 115200 roi moi thu tiep */
    }
    if (!da_chot) {
        k->baud_dang_dung = BAUD_KHOI_DONG;
        if (k->gl.baud) k->gl.baud(k->gl.ctx, BAUD_KHOI_DONG);
        bao_nhat_ky(k, "Giu nguyen %d baud.", BAUD_KHOI_DONG);
    }
    ket_noi_gui(k, "CFG;GET");
}

/* ------------------------------------------------------------- NAP DAN */
static void nap_nen(void *tham_so)
{
    KetNoi *k = (KetNoi *)tham_so;
    int tong = k->so_dong_nap;
    long byte_tong = 0;
    int da_gui = 0, da_bam_chay = 0, moc_bao_cao = 0;
    double moc_con_cho, han;
    int i;
    char *goi = NULL;
    size_t co_goi = 0;

    for (i = 0; i < tong; i++) byte_tong += (long)strlen(k->chi_muc[i]) + 1;

    k->cho_trong = 0;
    k->so_dong_da_nhan = 0;
    k->ket_qua_nap = 0;
    k->dang_nap = 1;

    bao_nhat_ky(k, "Nap dan %d baud: gui truoc %d dong roi vua chay vua nap "
                   "(tong %d dong, %ld byte).",
                k->baud_dang_dung, SO_DONG_NAP_TRUOC, tong, byte_tong);
    ket_noi_gui(k, "PROG;BEGIN");

    han = gio_giay() + 3.0;
    while (gio_giay() < han && k->cho_trong <= 0) ngu_ms(1);
    if (k->cho_trong <= 0) {
        bao_loi_nap(k, "ESP32 khong tra loi PROG;BEGIN. Kiem tra lai ket noi.");
        goto xong;
    }

    moc_con_cho = gio_giay();
    while (da_gui < tong) {
        int chua_bao_nhan, cho_thuc, lo, j;
        size_t can;
        char *v;

        if (k->ket_qua_nap == -1) {
            bao_loi_nap(k, "ESP32 tu choi chuong trinh (LOI_NAP). "
                           "Xem tab Alarm de biet dong nao sai.");
            goto xong;
        }
        if (!k->dang_nap) {
            bao_nhat_ky(k, "Da huy nap theo yeu cau.");
            goto xong;
        }

        /* Con bao nhieu dong dang bay tren duong chua duoc bao nhan.
         * Mot dong co the sinh toi 2 buoc (vd M3 + G1) nen tru gap doi. */
        chua_bao_nhan = da_gui - k->so_dong_da_nhan;
        cho_thuc = k->cho_trong - chua_bao_nhan * 2;

        if (cho_thuc < NGUONG_GUI_TIEP) {
            /* ESP32 chi bao cho trong khi tra loi mot dong. Neu may tinh
             * ngung gui va cu ngoi doi thi khong bao gio biet bo dem da
             * voi ra -> ket cung ca hai ben. Phai CHU DONG hoi bang BUF. */
            ket_noi_gui(k, "BUF");
            ngu_ms(3);
            if (gio_giay() - moc_con_cho > CHO_TOI_DA_S) {
                bao_loi_nap(k, "ESP32 khong voi bo dem sau %.0f giay "
                               "- may co the da dung. Da huy nap.", CHO_TOI_DA_S);
                goto xong;
            }
            continue;
        }
        moc_con_cho = gio_giay();

        lo = cho_thuc;
        if (lo > LO_GUI_TOI_DA) lo = LO_GUI_TOI_DA;
        if (lo > tong - da_gui) lo = tong - da_gui;

        can = 1;
        for (j = 0; j < lo; j++) can += strlen(k->chi_muc[da_gui + j]) + 1;
        if (can > co_goi) {
            char *moi = (char *)realloc(goi, can);
            if (!moi) { bao_loi_nap(k, "Het bo nho khi dong goi du lieu gui."); goto xong; }
            goi = moi;
            co_goi = can;
        }
        v = goi;
        for (j = 0; j < lo; j++) {
            size_t d = strlen(k->chi_muc[da_gui + j]);
            memcpy(v, k->chi_muc[da_gui + j], d);
            v += d;
            *v++ = '\n';
        }
        if (cong_ghi(k->cong, goi, (int)(v - goi)) < 0) {
            bao_loi_nap(k, "Loi khi gui du lieu: mat ket noi cong COM.");
            goto xong;
        }
        da_gui += lo;

        /* --- Du buoc dem dau tien -> CHAY NGAY, khong cho nap het --- */
        if (!da_bam_chay && da_gui >= (tong < SO_DONG_NAP_TRUOC ? tong : SO_DONG_NAP_TRUOC)) {
            int nguong = tong < SO_DONG_NAP_TRUOC ? tong : SO_DONG_NAP_TRUOC;
            han = gio_giay() + 5.0;
            while (gio_giay() < han && k->ket_qua_nap != -1 &&
                   k->so_dong_da_nhan < nguong)
                ngu_ms(1);
            if (k->ket_qua_nap == -1) {
                bao_loi_nap(k, "ESP32 tu choi chuong trinh (LOI_NAP).");
                goto xong;
            }
            ket_noi_gui(k, "RUN");
            da_bam_chay = 1;
            bao_nhat_ky(k, "Da dem san %d dong - BAT DAU CHAY, phan con lai "
                           "nap tiep khi dang chay.", k->so_dong_da_nhan);
        }

        if (da_gui - moc_bao_cao >= 400) {
            moc_bao_cao = da_gui;
            bao_nhat_ky(k, "... da nap %d/%d dong", da_gui, tong);
        }
    }

    /* PROG;END di SAU cac dong G-code tren cung duong truyen nen ESP32
     * chac chan xu ly no cuoi cung - khong can cho bao nhan tung dong */
    ket_noi_gui(k, "PROG;END");
    han = gio_giay() + 15.0;
    while (gio_giay() < han && k->ket_qua_nap == 0) ngu_ms(20);
    if (k->ket_qua_nap == -1) {
        bao_loi_nap(k, "ESP32 tu choi chuong trinh (LOI_NAP). "
                       "Xem tab Alarm de biet dong nao sai.");
        goto xong;
    }
    if (k->ket_qua_nap == 0) {
        bao_loi_nap(k, "ESP32 khong xac nhan nap xong sau 15 giay.");
        goto xong;
    }
    if (k->so_dong_da_nhan != tong)
        bao_nhat_ky(k, "Canh bao: da gui %d dong nhung ESP32 bao nhan %d dong.",
                    tong, k->so_dong_da_nhan);

    if (!da_bam_chay) {         /* bai qua ngan: nap xong het roi moi chay */
        ket_noi_gui(k, "RUN");
        bao_nhat_ky(k, "Da nap xong ca bai, bat dau CHAY.");
    } else {
        bao_nhat_ky(k, "Da nap xong toan bo %d dong, may dang chay tiep.", tong);
    }

xong:
    free(goi);
    k->dang_nap = 0;
}

int ket_noi_nap_va_chay(KetNoi *k, const char *const *cac_dong, int so_dong)
{
    long tong_byte = 0;
    int n = 0, i;
    char *v;

    if (!k || k->dang_nap) return -1;

    /* Chan chan lan cuoi: dong rong xuong toi ESP32 se bi bo qua KHONG kem
     * bao nhan, lam lech so dem hai ben */
    for (i = 0; i < so_dong; i++)
        if (cac_dong[i] && cac_dong[i][0]) {
            tong_byte += (long)strlen(cac_dong[i]) + 1;
            n++;
        }
    if (n == 0) return -1;

    free(k->bo_dong);
    free(k->chi_muc);
    k->bo_dong = (char *)malloc((size_t)tong_byte);
    k->chi_muc = (const char **)malloc(sizeof(char *) * (size_t)n);
    if (!k->bo_dong || !k->chi_muc) {
        free(k->bo_dong); free(k->chi_muc);
        k->bo_dong = NULL; k->chi_muc = NULL;
        return -1;
    }
    v = k->bo_dong;
    n = 0;
    for (i = 0; i < so_dong; i++) {
        size_t d;
        if (!cac_dong[i] || !cac_dong[i][0]) continue;
        d = strlen(cac_dong[i]) + 1;
        memcpy(v, cac_dong[i], d);
        k->chi_muc[n++] = v;
        v += d;
    }
    k->so_dong_nap = n;
    k->dang_nap = 1;            /* dat truoc de khong bi bam hai lan */
    if (!luong_chay(nap_nen, k)) { k->dang_nap = 0; return -1; }
    return 0;
}

void ket_noi_huy_nap(KetNoi *k) { if (k) k->dang_nap = 0; }
int  ket_noi_dang_nap(const KetNoi *k) { return k ? k->dang_nap : 0; }
int  ket_noi_so_dong_da_nhan(const KetNoi *k) { return k ? k->so_dong_da_nhan : 0; }
int  ket_noi_cho_trong(const KetNoi *k) { return k ? k->cho_trong : 0; }
int  ket_noi_dang_mo(const KetNoi *k) { return k ? k->dang_mo : 0; }
int  ket_noi_baud_dang_dung(const KetNoi *k) { return k ? k->baud_dang_dung : BAUD_KHOI_DONG; }

/* ------------------------------------------------------------- MO / DONG */
KetNoi *ket_noi_tao(const HamGoiLai *goi_lai)
{
    KetNoi *k = (KetNoi *)calloc(1, sizeof(*k));
    if (!k) return NULL;
    if (goi_lai) k->gl = *goi_lai;
    k->khoa = khoa_tao();
    k->baud_dang_dung = BAUD_KHOI_DONG;
    return k;
}

void ket_noi_giai_phong(KetNoi *k)
{
    if (!k) return;
    ket_noi_dong(k);
    ngu_ms(120);                /* cho luong doc thoat khoi vong lap */
    khoa_giai_phong(k->khoa);
    free(k->bo_dong);
    free(k->chi_muc);
    free(k);
}

int ket_noi_mo(KetNoi *k, const char *ten_cong, int baud_chon, char *loi)
{
    if (!k) return -1;
    if (k->dang_mo) { dat_loi(loi, "Cong dang mo san."); return -1; }
    k->cong = cong_mo(ten_cong, BAUD_KHOI_DONG, loi);
    if (!k->cong) return -1;
    snprintf(k->ten_cong, sizeof(k->ten_cong), "%s", ten_cong);
    ngu_ms(2000);               /* ESP32 khoi dong lai khi cong Serial vua mo */
    k->dang_mo = 1;
    k->baud_dang_dung = BAUD_KHOI_DONG;
    k->baud_chon = baud_chon;
    if (!luong_chay(vong_doc, k)) {
        dat_loi(loi, "Khong tao duoc luong doc cong COM.");
        cong_dong(k->cong);
        k->cong = NULL;
        k->dang_mo = 0;
        return -1;
    }
    if (!luong_chay(thuong_luong_baud, k))
        bao_nhat_ky(k, "Khong tao duoc luong thuong luong baud, giu %d.",
                    BAUD_KHOI_DONG);
    return 0;
}

void ket_noi_dong(KetNoi *k)
{
    if (!k) return;
    k->dang_mo = 0;
    k->dang_nap = 0;
    if (k->cong) {
        cong_dong(k->cong);
        k->cong = NULL;
    }
}
