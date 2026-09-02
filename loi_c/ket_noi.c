#include "ket_noi.h"
#include "loi_chung.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define SO_DONG_BAY_TOI_DA 512    /* so dong toi da dang cho "ok" cung mot luc */

struct KetNoi {
    HamGoiLai gl;
    CongCom *cong;

    volatile int dang_mo;
    volatile TrangThaiMay tt;
    volatile double x_mm, a_mm;     /* vi tri may doc tu ban tin "?" */
    double duong_kinh;              /* de doi mm cung <-> do */

    /* --- Dem "ok" tra ve. Luong doc chi TANG, luong nap chi DOC. --- */
    volatile int so_ok;             /* tong so dong da duoc bao nhan */
    volatile int ma_loi;            /* != 0 khi FluidNC tra error:N */
    volatile int ma_bao_dong;       /* != 0 khi FluidNC tra ALARM:N */

    /* --- Ban sao chuong trinh dang nap (luong nap so huu) --- */
    char *bo_dong;
    const char **chi_muc;
    int so_dong_nap;
    volatile int dang_nap;
    volatile int mo_dang_bi_tat;    /* da chen 0x9E de tat mo luc tam dung */
    volatile int duc_lo_ms;         /* thoi gian cho duc lo khi chay tiep */

    char ten_cong[CO_TEN_CONG];
};

/* ====================================================================== */
/* TIEN ICH                                                               */
/* ====================================================================== */
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

const char *ten_trang_thai_may(TrangThaiMay tt)
{
    switch (tt) {
    case MAY_IDLE:  return "SAN SANG";
    case MAY_RUN:   return "DANG CHAY";
    case MAY_HOLD:  return "TAM DUNG";
    case MAY_JOG:   return "DANG NHICH";
    case MAY_HOME:  return "DANG VE GOC";
    case MAY_ALARM: return "BAO DONG";
    case MAY_DOOR:  return "CUA MO";
    case MAY_CHECK: return "CHAY THU";
    case MAY_SLEEP: return "NGU";
    default:        return "CHUA RO";
    }
}

/* FluidNC tra "error:<so>" khi tu choi mot dong G-code. */
const char *giai_thich_loi(int ma)
{
    switch (ma) {
    case 1:  return "Thieu chu cai dau lenh";
    case 2:  return "So viet sai dinh dang";
    case 3:  return "Lenh $ khong hop le";
    case 8:  return "Lenh nay chi chay duoc khi may dang ranh";
    case 9:  return "May dang bao dong - bam Mo khoa truoc";
    case 10: return "Vuot gioi han mem (chay qua tam cho phep)";
    case 11: return "Dong lenh qua dai";
    case 12: return "Toc do vuot qua kha nang cua may";
    case 15: return "Lenh nhich di qua tam cho phep";
    case 16: return "Lenh nhich viet sai";
    case 20: return "Lenh G/M nay FluidNC khong ho tro";
    case 22: return "Chua khai bao toc do chay (thieu chu F)";
    case 24: return "Hai lenh tranh nhau dieu khien cung mot truc";
    case 25: return "Mot chu bi lap lai trong cung mot dong";
    case 33: return "Diem den khong hop le";
    case 34: return "Ban kinh cung tron sai";
    case 36: return "Trong dong co chu thua khong dung toi";
    case 40: return "Luc khoi dong da co nut hoac cong tac dang bi kich";
    default: return "Xem so ma loi trong tai lieu FluidNC";
    }
}

/* FluidNC tra "ALARM:<so>" khi phai dung may. */
const char *giai_thich_bao_dong(int ma)
{
    switch (ma) {
    case 1:  return "Cham cong tac hanh trinh - lui ra roi bam Mo khoa";
    case 2:  return "Vuot gioi han mem";
    case 3:  return "Da huy giua chung";
    case 6:  return "Ve goc that bai - bi dung giua chung";
    case 8:  return "Ve goc that bai - khong roi duoc cong tac";
    case 9:  return "Ve goc that bai - khong gap cong tac";
    case 10: return "Mo cat co van de (hong quang khong mo hoac tat giua chung)";
    case 11: return "Luc khoi dong da co nut hoac cong tac dang bi kich";
    case 13: return "Dung khan cap - da dung ngay khong giam toc";
    case 14: return "May chua ve goc";
    case 15: return "Bo dieu khien vua khoi dong";
    default: return "Xem so ma bao dong trong tai lieu FluidNC";
    }
}

/* ====================================================================== */
/* DOC BAN TIN TRANG THAI                                                 */
/* ====================================================================== */
/* "<Idle|MPos:1.5,0.000,0.000,2.5|FS:0,0>"  ->  trang thai + vi tri.
 * FluidNC bao 4 truc (X Y Z A); truc keo la so thu nhat, truc xoay so thu tu. */
int doc_dong_trang_thai(const char *dong, TrangThaiMay *tt,
                        double *x_mm, double *a_mm)
{
    static const struct { const char *ten; TrangThaiMay tt; } BANG[] = {
        { "Idle", MAY_IDLE },   { "Run", MAY_RUN },     { "Hold", MAY_HOLD },
        { "Jog", MAY_JOG },     { "Home", MAY_HOME },   { "Alarm", MAY_ALARM },
        { "Door", MAY_DOOR },   { "Check", MAY_CHECK }, { "Sleep", MAY_SLEEP },
        { "Starting", MAY_IDLE }
    };
    const char *p, *vt;
    size_t i;

    if (dong[0] != '<') return -1;
    p = dong + 1;

    if (tt) {
        *tt = MAY_KHONG_RO;
        for (i = 0; i < sizeof(BANG) / sizeof(BANG[0]); i++) {
            size_t d = strlen(BANG[i].ten);
            if (strncmp(p, BANG[i].ten, d) == 0 &&
                (p[d] == '|' || p[d] == ':' || p[d] == '>')) {
                *tt = BANG[i].tt;
                break;
            }
        }
    }

    /* MPos = vi tri may, WPos = vi tri so voi goc cua nguoi dung. Nhan ca hai. */
    vt = strstr(dong, "|MPos:");
    if (!vt) vt = strstr(dong, "|WPos:");
    if (!vt) return (tt && *tt != MAY_KHONG_RO) ? 0 : -1;
    vt = strchr(vt, ':') + 1;

    {
        double so[6];
        int n = 0;
        char *ket = NULL;
        while (n < 6) {
            double gt = strtod(vt, &ket);
            if (ket == vt) break;
            so[n++] = gt;
            vt = ket;
            if (*vt != ',') break;
            vt++;
        }
        if (n < 1) return -1;
        if (x_mm) *x_mm = so[0];
        /* Truc A la truc thu tu. Neu may chi khai bao it truc hon thi coi nhu 0. */
        if (a_mm) *a_mm = n >= 4 ? so[3] : 0.0;
    }
    return 0;
}

/* Doi mm cung tren mat ong <-> do quay */
static double mm_sang_do(const KetNoi *k, double mm)
{
    double chu_vi = PI * k->duong_kinh;
    return chu_vi > 0.0 ? mm / chu_vi * 360.0 : 0.0;
}

static double do_sang_mm(const KetNoi *k, double gt_do)
{
    return gt_do / 360.0 * PI * k->duong_kinh;
}

/* ====================================================================== */
/* XU LY MOT DONG FLUIDNC GUI LEN                                         */
/* ====================================================================== */
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

static void xu_ly_dong(KetNoi *k, const char *dong)
{
    /* --- Ban tin trang thai: KHONG do vao nhat ky, no ve moi 0,2 giay --- */
    if (dong[0] == '<') {
        TrangThaiMay tt;
        double x, a;
        if (doc_dong_trang_thai(dong, &tt, &x, &a) == 0) {
            TrangThaiMay cu = k->tt;
            k->x_mm = x;
            k->a_mm = a;
            if (tt != MAY_KHONG_RO) k->tt = tt;
            if (k->gl.vi_tri) k->gl.vi_tri(k->gl.ctx, x, mm_sang_do(k, a));
            if (tt != cu && tt != MAY_KHONG_RO && k->gl.trang_thai)
                k->gl.trang_thai(k->gl.ctx, tt);
        }
        return;
    }

    /* --- Bao nhan tung dong --- */
    if (strcmp(dong, "ok") == 0) {
        k->so_ok++;
        return;                     /* khong lam ngap khung nhat ky */
    }
    if (strncmp(dong, "error:", 6) == 0) {
        int ma = atoi(dong + 6);
        k->so_ok++;                 /* dong nay cung da roi khoi bo dem */
        k->ma_loi = ma ? ma : -1;
    } else if (strncmp(dong, "ALARM:", 6) == 0) {
        int ma = atoi(dong + 6);
        k->ma_bao_dong = ma ? ma : -1;
        k->tt = MAY_ALARM;
        if (k->gl.trang_thai) k->gl.trang_thai(k->gl.ctx, MAY_ALARM);
    }

    if (k->gl.dong_may) k->gl.dong_may(k->gl.ctx, dong);
}

/* ====================================================================== */
/* LUONG DOC CONG                                                         */
/* ====================================================================== */
static void vong_doc(void *tham_so)
{
    KetNoi *k = (KetNoi *)tham_so;
    char dem[CO_DONG_NHAN];
    int co_dem = 0;
    double lan_hoi_cuoi = 0.0;

    while (k->dang_mo && cong_dang_mo(k->cong)) {
        char tho[512];
        int n, i;

        /* Hoi "?" dinh ky de biet vi tri va trang thai. */
        if (gio_giay() - lan_hoi_cuoi > NHIP_HOI_TRANG_THAI_MS / 1000.0) {
            char c = RT_TRANG_THAI;
            cong_ghi(k->cong, &c, 1);
            lan_hoi_cuoi = gio_giay();
        }

        n = cong_doc(k->cong, tho, (int)sizeof(tho));
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
        }
    }
    k->dang_mo = 0;
}

/* ====================================================================== */
/* GUI                                                                    */
/* ====================================================================== */
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

int ket_noi_gui_thoi_gian_thuc(KetNoi *k, unsigned char ma)
{
    char c = (char)ma;
    if (!k || !cong_dang_mo(k->cong)) return 0;
    return cong_ghi(k->cong, &c, 1) == 1;
}

/* ====================================================================== */
/* NAP BAI - dem ky tu                                                    */
/* ====================================================================== */
/* Cach "dem ky tu" (character counting), chuan cua moi bo gui G-code cho
 * GRBL: may tinh cu gui tiep chung nao TONG SO BYTE cua cac dong CHUA duoc
 * bao nhan con duoi suc chua bo dem nhan cua may. Moi "ok" tra ve la bot di
 * so byte cua dong cu nhat.
 *
 * Nho vay duong day luc nao cung day du lieu, bo lap ke hoach cua FluidNC
 * luon co san nhieu doan de nhin truoc va tinh gia toc - dieu quyet dinh
 * chat luong mep cat o cac cho gap goc. */
static void nap_bai(void *tham_so)
{
    KetNoi *k = (KetNoi *)tham_so;
    int tong = k->so_dong_nap;
    int da_gui = 0, moc_bao_cao = 0;
    int *do_dai;                    /* do dai tung dong da gui, de tru dan */
    long byte_tong = 0;
    double moc_cho;
    int i;

    do_dai = (int *)malloc(sizeof(int) * (size_t)tong);
    if (!do_dai) {
        bao_loi_nap(k, "Het bo nho khi chuan bi nap bai.");
        k->dang_nap = 0;
        return;
    }
    for (i = 0; i < tong; i++) byte_tong += (long)strlen(k->chi_muc[i]) + 1;

    k->so_ok = 0;
    k->ma_loi = 0;
    k->dang_nap = 1;

    bao_nhat_ky(k, "Nap bai xuong FluidNC: %d dong, %ld byte. Vua gui vua chay.",
                tong, byte_tong);

    moc_cho = gio_giay();
    while (da_gui < tong || k->so_ok < da_gui) {
        int dang_bay = 0;

        if (!k->dang_nap) {
            bao_nhat_ky(k, "Da huy nap theo yeu cau.");
            break;
        }
        if (k->ma_loi) {
            bao_loi_nap(k, "FluidNC tu choi dong %d: error:%d - %s",
                        k->so_ok, k->ma_loi, giai_thich_loi(k->ma_loi));
            break;
        }
        if (k->ma_bao_dong) {
            bao_loi_nap(k, "May bao dong giua chung: ALARM:%d - %s",
                        k->ma_bao_dong, giai_thich_bao_dong(k->ma_bao_dong));
            break;
        }

        /* So byte cua cac dong da gui ma chua co "ok" tra ve */
        for (i = k->so_ok; i < da_gui; i++) dang_bay += do_dai[i];

        if (da_gui < tong &&
            dang_bay + (int)strlen(k->chi_muc[da_gui]) + 1 <= CO_DEM_NHAN_FLUIDNC) {
            char dem[CO_DONG_NHAN + 2];
            int n = snprintf(dem, sizeof(dem), "%s\n", k->chi_muc[da_gui]);
            if (n < 0 || n >= (int)sizeof(dem)) {
                bao_loi_nap(k, "Dong %d qua dai de gui.", da_gui + 1);
                break;
            }
            if (cong_ghi(k->cong, dem, n) < 0) {
                bao_loi_nap(k, "Mat ket noi cong COM giua chung.");
                break;
            }
            do_dai[da_gui] = n;
            da_gui++;
            moc_cho = gio_giay();

            if (da_gui - moc_bao_cao >= 400) {
                moc_bao_cao = da_gui;
                bao_nhat_ky(k, "... da gui %d/%d dong", da_gui, tong);
            }
            continue;               /* thu gui tiep ngay, khong nghi */
        }

        /* Bo dem cua may da day (hoac da gui het) - cho "ok" tra ve. */
        ngu_ms(2);
        if (gio_giay() - moc_cho > 120.0) {
            bao_loi_nap(k, "FluidNC khong tra loi sau 2 phut - may co the da dung. "
                           "Da huy nap.");
            break;
        }
    }

    if (k->dang_nap && !k->ma_loi && !k->ma_bao_dong && k->so_ok >= tong)
        bao_nhat_ky(k, "Da gui xong toan bo %d dong, may dang chay not.", tong);

    free(do_dai);
    k->dang_nap = 0;
}

int ket_noi_nap_va_chay(KetNoi *k, const char *const *cac_dong, int so_dong)
{
    long tong_byte = 0;
    int n = 0, i;
    char *v;

    if (!k || k->dang_nap) return -1;
    if (k->tt == MAY_ALARM) {
        bao_loi_nap(k, "May dang bao dong. Bam 'Mo khoa' de go bao dong truoc "
                       "khi chay.");
        return -1;
    }

    /* Chan chan lan cuoi: dong rong gui xuong FluidNC cung duoc tra "ok",
     * nhung to ra vo ich va lam lech so dem tien trinh. */
    for (i = 0; i < so_dong; i++)
        if (cac_dong[i] && cac_dong[i][0]) {
            tong_byte += (long)strlen(cac_dong[i]) + 1;
            n++;
        }
    if (n == 0) return -1;
    if (n > SO_DONG_BAY_TOI_DA * 1000) return -1;

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
    k->dang_nap = 1;                /* dat truoc de khong bi bam hai lan */
    k->mo_dang_bi_tat = 0;
    if (!luong_chay(nap_bai, k)) { k->dang_nap = 0; return -1; }
    return 0;
}

void ket_noi_huy_nap(KetNoi *k) { if (k) k->dang_nap = 0; }
int  ket_noi_dang_nap(const KetNoi *k) { return k ? k->dang_nap : 0; }
int  ket_noi_so_dong_da_nhan(const KetNoi *k) { return k ? k->so_ok : 0; }
int  ket_noi_so_dong_ca_bai(const KetNoi *k) { return k ? k->so_dong_nap : 0; }
int  ket_noi_dang_mo(const KetNoi *k) { return k ? k->dang_mo : 0; }
TrangThaiMay ket_noi_trang_thai(const KetNoi *k) { return k ? k->tt : MAY_KHONG_RO; }
double ket_noi_duong_kinh(const KetNoi *k) { return k ? k->duong_kinh : 0.0; }

void ket_noi_vi_tri(const KetNoi *k, double *x_mm, double *a_do)
{
    if (!k) return;
    if (x_mm) *x_mm = k->x_mm;
    if (a_do) *a_do = mm_sang_do(k, k->a_mm);
}

/* ====================================================================== */
/* DIEU KHIEN MAY                                                         */
/* ====================================================================== */
/* Tam dung va chay tiep deu phai CHO (cho may dung han, cho duc lo). Neu cho
 * ngay trong ham thi ben Win32 se chan luon luong giao dien - cua so dong
 * bang toi 30 giay. Vi vay ca hai viec deu day sang mot luong nen. */
static void viec_tam_dung(void *tham_so)
{
    KetNoi *k = (KetNoi *)tham_so;
    double han = gio_giay() + 3.0;
    /* Cho may dung han roi TAT MO CAT. Neu de mo chay tren ong dang dung yen
     * thi chi vai giay la thung phoi. */
    while (gio_giay() < han && k->tt != MAY_HOLD) ngu_ms(20);
    if (ket_noi_gui_thoi_gian_thuc(k, RT_TAT_BAT_MO)) {
        k->mo_dang_bi_tat = 1;
        bao_nhat_ky(k, "Da tam dung va tat mo cat.");
    }
}

void ket_noi_tam_dung(KetNoi *k)
{
    if (!k) return;
    ket_noi_gui_thoi_gian_thuc(k, RT_TAM_DUNG);
    if (!luong_chay(viec_tam_dung, k)) viec_tam_dung(k);   /* cung duong */
}

static void viec_chay_tiep(void *tham_so)
{
    KetNoi *k = (KetNoi *)tham_so;
    if (k->duc_lo_ms > 0 && k->mo_dang_bi_tat) {
        /* Bat lai mo cat trong khi ong VAN DUNG YEN, cho duc xuyen qua thanh
         * ong roi moi cho chay - neu khong mach cat se bi dut doan. */
        ket_noi_gui_thoi_gian_thuc(k, RT_TAT_BAT_MO);
        k->mo_dang_bi_tat = 0;
        bao_nhat_ky(k, "Bat lai mo cat, cho duc lo %.2f giay...",
                    k->duc_lo_ms / 1000.0);
        ngu_ms(k->duc_lo_ms);
    }
    k->mo_dang_bi_tat = 0;
    ket_noi_gui_thoi_gian_thuc(k, RT_CHAY_TIEP);
}

void ket_noi_chay_tiep(KetNoi *k, int thoi_gian_duc_lo_ms)
{
    if (!k) return;
    k->duc_lo_ms = thoi_gian_duc_lo_ms;
    if (!luong_chay(viec_chay_tiep, k)) viec_chay_tiep(k);
}

void ket_noi_dung_han(KetNoi *k)
{
    if (!k) return;
    k->dang_nap = 0;
    ket_noi_gui_thoi_gian_thuc(k, RT_DUNG_HAN);
    k->mo_dang_bi_tat = 0;
    bao_nhat_ky(k, "Da dung han (khoi dong lai bo dieu khien).");
}

void ket_noi_mo_khoa(KetNoi *k)
{
    if (!k) return;
    k->ma_bao_dong = 0;
    ket_noi_gui(k, "$X");
}

void ket_noi_ve_goc(KetNoi *k) { if (k) ket_noi_gui(k, "$H"); }

/* Lay cho dang dung lam goc 0 cua ca hai truc. */
void ket_noi_dat_goc(KetNoi *k)
{
    if (!k) return;
    ket_noi_gui(k, "G10 L20 P0 X0 A0");
}

void ket_noi_jog(KetNoi *k, char truc, double khoang, double toc_do)
{
    char lenh[96];
    if (!k) return;
    /* Truc A nguoi dung nhap bang DO, may lam viec bang MM CUNG */
    if (truc == 'A' || truc == 'a') khoang = do_sang_mm(k, khoang);
    snprintf(lenh, sizeof(lenh), "$J=G91 G21 %c%.4f F%.1f",
             truc == 'a' ? 'A' : truc, khoang, toc_do);
    ket_noi_gui(k, lenh);
}

void ket_noi_huy_jog(KetNoi *k)
{
    if (k) ket_noi_gui_thoi_gian_thuc(k, RT_HUY_JOG);
}

/* ====================================================================== */
/* DUONG KINH ONG                                                         */
/* ====================================================================== */
int ket_noi_dat_duong_kinh(KetNoi *k, double duong_kinh_mm, double xung_moi_vong_a)
{
    char lenh[96];
    double xung_moi_mm;
    if (!k || duong_kinh_mm <= 0 || xung_moi_vong_a <= 0) return -1;
    k->duong_kinh = duong_kinh_mm;
    if (!cong_dang_mo(k->cong)) return 0;   /* nho lai, gui khi ket noi */

    /* Truc A tinh bang mm cung tren mat ong, nen so xung tren mot mm phu thuoc
     * duong kinh: xung_moi_vong / chu_vi */
    xung_moi_mm = xung_moi_vong_a / (PI * duong_kinh_mm);
    snprintf(lenh, sizeof(lenh), "$/axes/a/steps_per_mm=%.4f", xung_moi_mm);
    if (!ket_noi_gui(k, lenh)) return -1;
    bao_nhat_ky(k, "Ong D%g: truc xoay dat lai %.4f xung tren mot mm cung.",
                duong_kinh_mm, xung_moi_mm);
    return 0;
}

/* ====================================================================== */
/* MO / DONG                                                              */
/* ====================================================================== */
KetNoi *ket_noi_tao(const HamGoiLai *goi_lai)
{
    KetNoi *k = (KetNoi *)calloc(1, sizeof(*k));
    if (!k) return NULL;
    if (goi_lai) k->gl = *goi_lai;
    k->duong_kinh = 60.0;
    k->tt = MAY_KHONG_RO;
    return k;
}

void ket_noi_giai_phong(KetNoi *k)
{
    if (!k) return;
    ket_noi_dong(k);
    ngu_ms(150);                /* cho luong doc thoat khoi vong lap */
    free(k->bo_dong);
    free(k->chi_muc);
    free(k);
}

int ket_noi_mo(KetNoi *k, const char *ten_cong, char *loi)
{
    if (!k) return -1;
    if (k->dang_mo) { dat_loi(loi, "Cong dang mo san."); return -1; }
    k->cong = cong_mo(ten_cong, BAUD_FLUIDNC, loi);
    if (!k->cong) return -1;
    snprintf(k->ten_cong, sizeof(k->ten_cong), "%s", ten_cong);
    k->dang_mo = 1;
    k->tt = MAY_KHONG_RO;
    k->ma_loi = 0;
    k->ma_bao_dong = 0;
    if (!luong_chay(vong_doc, k)) {
        dat_loi(loi, "Khong tao duoc luong doc cong COM.");
        cong_dong(k->cong);
        k->cong = NULL;
        k->dang_mo = 0;
        return -1;
    }
    /* Mo cong Serial lam ESP32 khoi dong lai - cho no noi xong loi chao. */
    ngu_ms(2000);
    cong_xoa_dem_vao(k->cong);
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
    k->tt = MAY_KHONG_RO;
}
