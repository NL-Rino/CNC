/* GIA LAP MAY CHAY FLUIDNC - doc lenh tu stdin, tra loi ra stdout.
 *
 * Chi bat chuoc dung phan giao thuc ma phan mem may tinh dung toi, va bat
 * chuoc theo DUNG ma nguon FluidNC:
 *   Channel.cpp      "ok"  cho moi dong nhan duoc, "error:<so>" neu tu choi
 *   Report.cpp       "<Idle|MPos:x,y,z,a|FS:0,0>" khi nhan ky tu '?'
 *   RealtimeCmd.h    '!' tam dung, '~' chay tiep, 0x18 dung han,
 *                    0x85 huy nhich, 0x9E bat/tat mo luc dang tam dung
 *   Protocol.cpp     "ALARM:<so>"
 *
 * DIEU QUAN TRONG NHAT ma ban gia lap nay canh: bo dem nhan cua may chi chua
 * duoc 127 byte. Neu phan mem may tinh dem sai va gui qua tay, ban gia lap in
 * ra "TRAN BO DEM" roi thoat voi ma loi - bai kiem tra se hong ngay. Do dung
 * la loi ma tren may that se lam mat dong lenh giua chung duong cat.
 *
 * Tham so dong lenh: he so chay nhanh hon thuc te (mac dinh 1).
 */
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>

#define CO_DEM_NHAN   127     /* giong CO_DEM_NHAN_FLUIDNC ben phan mem */
#define SO_KHOI_LAP   16      /* so doan bo lap ke hoach giu duoc */
#define SO_TRUC       4       /* X Y Z A */

typedef struct { double dich[SO_TRUC]; double giay; int bat_mo; int tat_mo; } Khoi;

static Khoi hang[SO_KHOI_LAP];
static int  hang_dau = 0, hang_duoi = 0;   /* vong tron */
/* HAI vi tri rieng biet, dung nhu GRBL that:
 *   vi_tri     - cho dong co DANG dung, dung de bao ra ban tin "?"
 *   vi_tri_lap - cho ma bo lap ke hoach da nhan het cac doan trong hang,
 *                dung de tinh quang duong cua doan MOI khi doc lenh
 * Neu chi co mot vi tri thi 16 doan dang xep hang deu tinh quang duong tu
 * cung mot cho, va toa do se sai luy tien. */
static double vi_tri[SO_TRUC];
static double vi_tri_lap[SO_TRUC];
static int  dang_tam_dung = 0, mo_dang_bat = 0, mo_bi_tat_tam = 0;
static int  con_song = 1;
static unsigned he_so_nhanh = 1;
static long  tong_dong_nhan = 0;
static pthread_mutex_t khoa = PTHREAD_MUTEX_INITIALIZER;

static int hang_day(void)  { return (hang_duoi + 1) % SO_KHOI_LAP == hang_dau; }
static int hang_rong(void) { return hang_dau == hang_duoi; }

static const char *ten_trang_thai(void)
{
    if (dang_tam_dung) return hang_rong() ? "Hold:0" : "Hold:1";
    return hang_rong() ? "Idle" : "Run";
}

static void in_trang_thai(void)
{
    pthread_mutex_lock(&khoa);
    printf("<%s|MPos:%.3f,%.3f,%.3f,%.3f|Bf:%d,%d|FS:0,0>\n",
           ten_trang_thai(), vi_tri[0], vi_tri[1], vi_tri[2], vi_tri[3],
           SO_KHOI_LAP - 1 - ((hang_duoi - hang_dau + SO_KHOI_LAP) % SO_KHOI_LAP),
           CO_DEM_NHAN);
    pthread_mutex_unlock(&khoa);
    fflush(stdout);
}

/* ------------------------------------------------------- CHAY CAC DOAN */
static void *nhan_dong_co(void *tham_so)
{
    (void)tham_so;
    while (con_song) {
        Khoi k;
        int co = 0;
        pthread_mutex_lock(&khoa);
        if (!dang_tam_dung && !hang_rong()) { k = hang[hang_dau]; co = 1; }
        pthread_mutex_unlock(&khoa);
        if (!co) { usleep(2000); continue; }

        /* Chay het doan nay roi moi bo ra khoi hang */
        {
            unsigned us = (unsigned)(k.giay * 1000000.0) / (he_so_nhanh ? he_so_nhanh : 1);
            if (us > 200000u) us = 200000u;
            usleep(us);
        }
        pthread_mutex_lock(&khoa);
        {
            int i;
            for (i = 0; i < SO_TRUC; i++) vi_tri[i] += k.dich[i];
        }
        if (k.bat_mo) mo_dang_bat = 1;
        if (k.tat_mo) mo_dang_bat = 0;
        hang_dau = (hang_dau + 1) % SO_KHOI_LAP;
        pthread_mutex_unlock(&khoa);
    }
    return NULL;
}

/* ------------------------------------------------------------ DOC LENH */
/* Doc mot so trong dong G-code. Khong dung strtod: "X1E5" phai ra 1, khong
 * phai 100000 (giong het bo doc cua FluidNC va cua phan mem may tinh). */
static double doc_so(const char *p, const char **ket)
{
    char tam[48];
    const char *dau = p, *dau_so;
    size_t n;
    if (*p == '+' || *p == '-') p++;
    dau_so = p;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
    if (p == dau_so) { *ket = dau; return 0.0; }
    n = (size_t)(p - dau);
    if (n >= sizeof(tam)) n = sizeof(tam) - 1;
    memcpy(tam, dau, n);
    tam[n] = '\0';
    *ket = p;
    return atof(tam);
}

/* Tra 0 neu nhan dong, so ma loi neu tu choi. */
static int xu_ly_dong_gcode(const char *dong)
{
    double dich[SO_TRUC] = { 0, 0, 0, 0 };
    int    co_truc[SO_TRUC] = { 0, 0, 0, 0 };   /* dong nay co ghi truc do khong */
    double f = 0.0, dwell = 0.0;
    int co_di_chuyen = 0, bat_mo = 0, tat_mo = 0, tuyet_doi = 1;
    static int modal_tuyet_doi = 1;
    const char *p = dong;

    if (!*dong) return 0;
    if (*dong == '(' || *dong == ';') return 0;
    /* "$J=G91 G21 X10 F600" la lenh NHICH - phan sau dau = la G-code that */
    if (dong[0] == '$' && (dong[1] == 'J' || dong[1] == 'j') && dong[2] == '=') {
        p = dong + 3;
    } else if (*dong == '$') {
        return 0;                        /* lenh cai dat khac: nhan het */
    }

    tuyet_doi = modal_tuyet_doi;
    while (*p) {
        char chu;
        const char *ket;
        double gt;
        if (!isalpha((unsigned char)*p)) { p++; continue; }
        chu = (char)toupper((unsigned char)*p);
        p++;
        while (*p == ' ') p++;
        gt = doc_so(p, &ket);
        if (ket == p) continue;
        p = ket;
        switch (chu) {
        case 'G':
            if (gt == 90) { tuyet_doi = 1; modal_tuyet_doi = 1; }
            else if (gt == 91) { tuyet_doi = 0; modal_tuyet_doi = 0; }
            else if (gt == 0 || gt == 1) co_di_chuyen = 1;
            else if (gt == 4) co_di_chuyen = 1;
            break;
        case 'M':
            if (gt == 3 || gt == 4) bat_mo = 1;
            else if (gt == 5) tat_mo = 1;
            break;
        case 'X': dich[0] = gt; co_truc[0] = 1; co_di_chuyen = 1; break;
        case 'A': dich[3] = gt; co_truc[3] = 1; co_di_chuyen = 1; break;
        case 'F': f = gt; break;
        case 'P': dwell = gt; break;
        default: break;
        }
    }

    if (!co_di_chuyen && !bat_mo && !tat_mo) return 0;

    {
        Khoi k;
        double dai = 0.0;
        int i;
        memset(&k, 0, sizeof(k));
        /* Truc nao KHONG duoc ghi trong dong nay thi giu nguyen cho, khong dich */
        pthread_mutex_lock(&khoa);
        for (i = 0; i < SO_TRUC; i++) {
            k.dich[i] = co_truc[i]
                        ? (tuyet_doi ? dich[i] - vi_tri_lap[i] : dich[i]) : 0.0;
            vi_tri_lap[i] += k.dich[i];
        }
        pthread_mutex_unlock(&khoa);
        for (i = 0; i < SO_TRUC; i++) dai += k.dich[i] * k.dich[i];
        dai = sqrt(dai);
        k.giay = f > 0.0 ? dai / (f / 60.0) : (dwell > 0 ? dwell : 0.01);
        if (k.giay > 2.0) k.giay = 2.0;
        k.bat_mo = bat_mo;
        k.tat_mo = tat_mo;

        /* Bo lap ke hoach day thi CHUA tra "ok" - dung nhu GRBL that */
        for (;;) {
            int day;
            pthread_mutex_lock(&khoa);
            day = hang_day();
            if (!day) {
                hang[hang_duoi] = k;
                hang_duoi = (hang_duoi + 1) % SO_KHOI_LAP;
            }
            pthread_mutex_unlock(&khoa);
            if (!day) break;
            usleep(1000);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    char dem[512];
    int co_dem = 0;
    int byte_chua_bao_nhan = 0;
    pthread_t t;

    if (argc > 1) he_so_nhanh = (unsigned)atoi(argv[1]);
    if (he_so_nhanh < 1) he_so_nhanh = 1;
    setvbuf(stdout, NULL, _IOLBF, 0);

    pthread_create(&t, NULL, nhan_dong_co, NULL);

    /* FluidNC noi loi chao khi vua khoi dong */
    printf("\n[MSG:INFO: FluidNC (gia lap) ready]\nGrbl 3.9 [FluidNC v3.9.9 (gia lap)]\n");
    fflush(stdout);

    for (;;) {
        unsigned char c;
        ssize_t n = read(0, &c, 1);
        if (n <= 0) break;

        /* --- Ky tu thoi gian thuc: nhat ra ngay, khong tinh vao bo dem --- */
        if (c == '?') { in_trang_thai(); continue; }
        if (c == '!') { pthread_mutex_lock(&khoa); dang_tam_dung = 1;
                        pthread_mutex_unlock(&khoa); continue; }
        if (c == '~') { pthread_mutex_lock(&khoa);
                        if (mo_bi_tat_tam) { mo_dang_bat = 1; mo_bi_tat_tam = 0; }
                        dang_tam_dung = 0; pthread_mutex_unlock(&khoa); continue; }
        if (c == 0x18) {                       /* dung han, khoi dong lai */
            pthread_mutex_lock(&khoa);
            hang_dau = hang_duoi = 0;
            memcpy(vi_tri_lap, vi_tri, sizeof(vi_tri_lap));
            dang_tam_dung = 0; mo_dang_bat = 0; mo_bi_tat_tam = 0;
            pthread_mutex_unlock(&khoa);
            printf("\n[MSG:INFO: FluidNC (gia lap) ready]\nGrbl 3.9 [FluidNC v3.9.9 (gia lap)]\n");
            fflush(stdout);
            byte_chua_bao_nhan = 0;
            co_dem = 0;
            continue;
        }
        if (c == 0x85) {                       /* huy nhich */
            pthread_mutex_lock(&khoa);
            hang_dau = hang_duoi = 0;
            memcpy(vi_tri_lap, vi_tri, sizeof(vi_tri_lap));
            pthread_mutex_unlock(&khoa);
            continue;
        }
        if (c == 0x9E) {                       /* bat/tat mo luc dang tam dung */
            pthread_mutex_lock(&khoa);
            if (dang_tam_dung) {
                if (mo_bi_tat_tam) { mo_dang_bat = 1; mo_bi_tat_tam = 0; }
                else if (mo_dang_bat) { mo_dang_bat = 0; mo_bi_tat_tam = 1; }
            }
            pthread_mutex_unlock(&khoa);
            continue;
        }
        if (c >= 0x80) continue;               /* cac lenh dieu chinh khac */

        /* --- Byte thuong: vao bo dem nhan --- */
        byte_chua_bao_nhan++;
        if (byte_chua_bao_nhan > CO_DEM_NHAN) {
            printf("TRAN BO DEM: may tinh gui %d byte ma chua doi bao nhan "
                   "(bo dem chi chua %d)\n", byte_chua_bao_nhan, CO_DEM_NHAN);
            fflush(stdout);
            return 1;
        }

        if (c == '\n' || c == '\r') {
            int ma;
            dem[co_dem] = '\0';
            ma = xu_ly_dong_gcode(dem);
            if (ma == 0) printf("ok\n");
            else         printf("error:%d\n", ma);
            fflush(stdout);
            tong_dong_nhan++;
            byte_chua_bao_nhan = 0;            /* ca dong da roi khoi bo dem */
            co_dem = 0;
        } else if (co_dem < (int)sizeof(dem) - 1) {
            dem[co_dem++] = (char)c;
        }
    }
    con_song = 0;
    pthread_join(t, NULL);
    return 0;
}
