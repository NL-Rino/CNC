/* KIEM TRA LOP KET NOI FLUIDNC (loi_c/ket_noi.c)
 *
 * Cach lam: tao mot cap cong ao (pty). Dau MASTER giao cho tien trinh gia lap
 * may FluidNC lam stdin/stdout, dau SLAVE la mot duong dan /dev/pts/N ma lop
 * ket_noi mo y het mot cong COM that.
 *
 * Nho vay toan bo duong di "may tinh <-> may cat" duoc chay that: dem ky tu
 * de khong tran bo dem, doi "ok" tung dong, hoi "?" lay vi tri, tam dung va
 * tat mo cat, chay tiep co duc lo lai.
 *
 * Ban gia lap TU BAO HONG neu phan mem gui qua tay bo dem 127 byte - do la
 * loi ma tren may that se lam mat dong lenh giua chung duong cat.
 *
 * Bai nay chi chay tren Linux (can pty). Tren Windows lop ket_noi duoc kiem
 * tra bang may that.
 */
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include "../loi_c/ket_noi.h"
#include "../loi_c/phan_tich_gcode.h"
#include "../loi_c/loi_chung.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>

static int so_that_bai = 0;

static int gan_bang(double a, double b, double sai_so)
{
    double d = a - b;
    return (d < 0 ? -d : d) <= sai_so;
}

static void kiem(const char *ten, int dung)
{
    printf("%-58s %s\n", ten, dung ? "DAT" : "HONG");
    if (!dung) so_that_bai++;
}

/* ------------------------------------------------------- BAT CAC BAN TIN */
typedef struct {
    char nhat_ky_cuoi[CO_LOI];
    char loi_cuoi[CO_LOI];
    int  so_loi, so_dong_may, so_vi_tri;
    int  co_gui_xong, co_tat_mo, co_duc_lo;
    double x_cuoi, a_cuoi;
    TrangThaiMay tt_cuoi;
    int  thay_run, thay_hold, thay_idle;
} Bat;

static void bat_dong(void *ctx, const char *dong)
{
    Bat *b = (Bat *)ctx;
    b->so_dong_may++;
    (void)dong;
}
static void bat_nhat_ky(void *ctx, const char *chu)
{
    Bat *b = (Bat *)ctx;
    snprintf(b->nhat_ky_cuoi, sizeof(b->nhat_ky_cuoi), "%s", chu);
    if (strstr(chu, "Da gui xong")) b->co_gui_xong = 1;
    if (strstr(chu, "tat mo cat")) b->co_tat_mo = 1;
    if (strstr(chu, "cho duc lo")) b->co_duc_lo = 1;
}
static void bat_loi(void *ctx, const char *chu)
{
    Bat *b = (Bat *)ctx;
    b->so_loi++;
    snprintf(b->loi_cuoi, sizeof(b->loi_cuoi), "%s", chu);
}
static void bat_vi_tri(void *ctx, double x, double a)
{
    Bat *b = (Bat *)ctx;
    b->x_cuoi = x; b->a_cuoi = a; b->so_vi_tri++;
}
static void bat_trang_thai(void *ctx, TrangThaiMay tt)
{
    Bat *b = (Bat *)ctx;
    b->tt_cuoi = tt;
    if (tt == MAY_RUN)  b->thay_run = 1;
    if (tt == MAY_HOLD) b->thay_hold = 1;
    if (tt == MAY_IDLE) b->thay_idle = 1;
}

/* ----------------------------------------------------------- CAP CONG AO */
static pid_t con = -1;

static int mo_gia_lap(const char *duong_dan, char *ten_slave, size_t co_ten)
{
    int chu = posix_openpt(O_RDWR | O_NOCTTY);
    struct termios t;
    if (chu < 0) return -1;
    if (grantpt(chu) != 0 || unlockpt(chu) != 0) { close(chu); return -1; }
    if (ptsname_r(chu, ten_slave, co_ten) != 0) { close(chu); return -1; }
    if (tcgetattr(chu, &t) == 0) { cfmakeraw(&t); tcsetattr(chu, TCSANOW, &t); }

    con = fork();
    if (con < 0) { close(chu); return -1; }
    if (con == 0) {
        dup2(chu, 0);
        dup2(chu, 1);
        if (chu > 2) close(chu);
        execl(duong_dan, duong_dan, "40", (char *)NULL);
        _exit(127);
    }
    close(chu);
    return 0;
}

static void dong_gia_lap(void)
{
    if (con > 0) {
        int tt = 0;
        kill(con, SIGTERM);
        waitpid(con, &tt, 0);
        con = -1;
    }
}

/* Ban gia lap con song khong? No TU THOAT neu phan mem lam tran bo dem. */
static int gia_lap_con_song(void)
{
    int tt = 0;
    pid_t r;
    if (con <= 0) return 0;
    r = waitpid(con, &tt, WNOHANG);
    if (r == 0) return 1;
    con = -1;
    return 0;
}

/* ------------------------------------------------------------------ MAIN */
int main(int argc, char **argv)
{
    const char *gia_lap = argc > 1 ? argv[1] : "./gia_lap_fluidnc";
    char ten_slave[128];
    Bat bat;
    HamGoiLai gl;
    KetNoi *k;
    char loi[CO_LOI] = "";
    TrangThaiMay tt;
    double x, a;

    printf("== KIEM TRA LOP KET NOI FLUIDNC ==\n\n");

    /* --- 1. Doc ban tin trang thai (thuan tuy, khong can cong) --- */
    kiem("doc <Idle|MPos:...> ra dung trang thai va vi tri",
         doc_dong_trang_thai("<Idle|MPos:12.340,0.000,0.000,56.780|FS:0,0>",
                             &tt, &x, &a) == 0 &&
         tt == MAY_IDLE && gan_bang(x, 12.34, 1e-9) && gan_bang(a, 56.78, 1e-9));
    kiem("doc trang thai dang chay",
         doc_dong_trang_thai("<Run|MPos:1.0,0.0,0.0,2.0|FS:500,0>", &tt, &x, &a) == 0 &&
         tt == MAY_RUN);
    kiem("doc Hold:0 ra tam dung",
         doc_dong_trang_thai("<Hold:0|MPos:1.0,0.0,0.0,2.0|FS:0,0>", &tt, &x, &a) == 0 &&
         tt == MAY_HOLD);
    kiem("doc bao dong",
         doc_dong_trang_thai("<Alarm|MPos:0.0,0.0,0.0,0.0|FS:0,0>", &tt, &x, &a) == 0 &&
         tt == MAY_ALARM);
    kiem("nhan ca WPos thay cho MPos",
         doc_dong_trang_thai("<Idle|WPos:-5.500,0.000,0.000,-1.250|FS:0,0>",
                             &tt, &x, &a) == 0 &&
         gan_bang(x, -5.5, 1e-9) && gan_bang(a, -1.25, 1e-9));
    kiem("dong khong phai ban tin trang thai thi bao khong doc duoc",
         doc_dong_trang_thai("ok", &tt, &x, &a) == -1);
    kiem("ma loi duoc dich ra tieng Viet",
         strstr(giai_thich_loi(9), "bao dong") != NULL);
    kiem("ma bao dong duoc dich ra tieng Viet",
         strstr(giai_thich_bao_dong(1), "cong tac hanh trinh") != NULL);

    /* --- 1b. Doi truc A tu DO sang MM CUNG truoc khi gui xuong may ---
     * FluidNC coi moi truc la truc thang khi tinh toc do, nen truc xoay phai
     * di xuong bang mm cung thi lenh F moi dung nghia "toc do mo cat". */
    {
        char ra[CO_DONG_G];
        /* Ong D60: chu vi 188.4956 mm, 90 do = 47.124 mm cung */
        doi_a_sang_mm_cung("G1X10A90F800", 60.0, ra, sizeof(ra));
        kiem("D60: A90 do -> 47.124 mm cung", strcmp(ra, "G1X10A47.124F800") == 0);
        doi_a_sang_mm_cung("G1X10A360F800", 60.0, ra, sizeof(ra));
        kiem("D60: A360 do -> tron mot vong chu vi",
             strcmp(ra, "G1X10A188.496F800") == 0);
        doi_a_sang_mm_cung("G1X10A-90F800", 60.0, ra, sizeof(ra));
        kiem("goc am van doi dung", strcmp(ra, "G1X10A-47.124F800") == 0);
        doi_a_sang_mm_cung("G1A90", 200.0, ra, sizeof(ra));
        kiem("ong to hon thi cung mot goc ra nhieu mm hon",
             strcmp(ra, "G1A157.08") == 0);
        doi_a_sang_mm_cung("G0X5", 60.0, ra, sizeof(ra));
        kiem("dong khong co chu A thi giu nguyen", strcmp(ra, "G0X5") == 0);
        doi_a_sang_mm_cung("M3", 60.0, ra, sizeof(ra));
        kiem("lenh bat mo giu nguyen", strcmp(ra, "M3") == 0);
        doi_a_sang_mm_cung("G1X10A90F800", 0.0, ra, sizeof(ra));
        kiem("chua biet duong kinh thi khong dam vao dong lenh",
             strcmp(ra, "G1X10A90F800") == 0);
    }

    if (access(gia_lap, X_OK) != 0) {
        printf("\n(Khong tim thay gia lap '%s' - bo qua phan nap bai)\n", gia_lap);
        printf("\n%s\n", so_that_bai == 0 ? "TAT CA DAT" : "CO BAI HONG");
        return so_that_bai == 0 ? 0 : 1;
    }

    /* --- 2. Nap bai that qua cong ao --- */
    memset(&bat, 0, sizeof(bat));
    memset(&gl, 0, sizeof(gl));
    gl.ctx = &bat;
    gl.dong_may = bat_dong;
    gl.nhat_ky = bat_nhat_ky;
    gl.loi_nap = bat_loi;
    gl.vi_tri = bat_vi_tri;
    gl.trang_thai = bat_trang_thai;

    if (mo_gia_lap(gia_lap, ten_slave, sizeof(ten_slave)) != 0) {
        printf("Khong tao duoc cong ao - bo qua phan nap bai.\n");
        return so_that_bai == 0 ? 0 : 1;
    }
    printf("\nCong ao: %s (gia lap FluidNC chay o tien trinh con)\n\n", ten_slave);

    k = ket_noi_tao(&gl);
    if (ket_noi_mo(k, ten_slave, loi) != 0) {
        printf("Khong mo duoc cong ao: %s\n", loi);
        dong_gia_lap();
        return 1;
    }
    kiem("mo cong ao", ket_noi_dang_mo(k));

    /* Doi vai nhip de co ban tin "?" dau tien tra ve */
    { double han = gio_giay() + 3.0;
      while (gio_giay() < han && bat.so_vi_tri == 0) ngu_ms(20); }
    kiem("tu hoi vi tri dinh ky, khong can ai bam", bat.so_vi_tri > 0);
    kiem("doc duoc trang thai may", ket_noi_trang_thai(k) == MAY_IDLE);

    /* --- 3. Doi duong kinh ong -> gui lai so xung cho truc xoay --- */
    kiem("dat duong kinh ong gui duoc xuong may",
         ket_noi_dat_duong_kinh(k, 60.0, 1600.0) == 0);
    kiem("nho lai duong kinh vua dat",
         gan_bang(ket_noi_duong_kinh(k), 60.0, 1e-9));

    /* --- 4. Nap mot bai dai hon suc chua bo dem nhan --- */
    {
        enum { SO_DONG = 600 };
        static char dem[SO_DONG][CO_DONG_G];
        static const char *tro[SO_DONG];
        int n = 0, i;
        char tho[CO_DONG_G];

        snprintf(dem[n], CO_DONG_G, "G21");  tro[n] = dem[n]; n++;
        snprintf(dem[n], CO_DONG_G, "G90");  tro[n] = dem[n]; n++;
        snprintf(dem[n], CO_DONG_G, "M3");   tro[n] = dem[n]; n++;
        for (i = 0; n < SO_DONG - 1; i++) {
            snprintf(tho, sizeof(tho), "G1 X%.2f A%.2f F800",
                     (double)(i % 40) * 0.5, (double)(i % 90) * 0.5);
            if (nen_dong_gui(tho, dem[n], CO_DONG_G) > 0) { tro[n] = dem[n]; n++; }
        }
        snprintf(dem[n], CO_DONG_G, "M5");   tro[n] = dem[n]; n++;

        kiem("bai thu dai hon nhieu lan bo dem nhan cua may",
             n * 8 > CO_DEM_NHAN_FLUIDNC);

        if (ket_noi_nap_va_chay(k, tro, n) != 0) {
            printf("Khong khoi dong duoc viec nap.\n");
            so_that_bai++;
        } else {
            double han = gio_giay() + 120.0;
            while (gio_giay() < han && ket_noi_dang_nap(k)) ngu_ms(20);
            kiem("nap xong trong thoi gian cho", !ket_noi_dang_nap(k));
            kiem("khong co loi nap", bat.so_loi == 0);
            if (bat.so_loi) printf("   loi cuoi: %s\n", bat.loi_cuoi);
            kiem("KHONG lam tran bo dem nhan cua may", gia_lap_con_song());
            kiem("da gui het ca bai", bat.co_gui_xong);
            kiem("may bao nhan du tung dong",
                 ket_noi_so_dong_da_nhan(k) == ket_noi_so_dong_ca_bai(k));
            if (ket_noi_so_dong_da_nhan(k) != ket_noi_so_dong_ca_bai(k))
                printf("   gui %d dong, may bao nhan %d dong\n",
                       ket_noi_so_dong_ca_bai(k), ket_noi_so_dong_da_nhan(k));
            kiem("thay may chuyen sang trang thai dang chay", bat.thay_run);
            kiem("ong da dich chuyen that", fabs(bat.x_cuoi) > 0.001);
        }
    }

    /* --- 5. Tam dung: phai TAT MO CAT de khong thung phoi --- */
    if (gia_lap_con_song()) {
        static const char *bai_dai[] = {
            "G21", "G90", "M3", "G1X0A0F300", "G1X200A360F300", "M5"
        };
        ket_noi_nap_va_chay(k, bai_dai, 6);
        ngu_ms(400);
        ket_noi_tam_dung(k);
        /* Tam dung chay o luong nen (cho may dung han roi moi tat mo), nen
         * phai doi chu khong xet ngay */
        { double han = gio_giay() + 6.0;
          while (gio_giay() < han && !bat.co_tat_mo) ngu_ms(20); }
        kiem("bam tam dung thi may dung lai", bat.thay_hold);
        kiem("tam dung thi TAT mo cat", bat.co_tat_mo);

        /* --- 6. Chay tiep co duc lo lai --- */
        bat.co_duc_lo = 0;
        ket_noi_chay_tiep(k, 300);
        { double han = gio_giay() + 3.0;
          while (gio_giay() < han && !bat.co_duc_lo) ngu_ms(20); }
        kiem("chay tiep co cho duc lo truoc", bat.co_duc_lo);
        ngu_ms(600);   /* cho het thoi gian duc lo roi may moi chay lai */
        kiem("chay tiep xong thi may chay lai",
             ket_noi_trang_thai(k) == MAY_RUN || ket_noi_trang_thai(k) == MAY_IDLE);

        /* --- 7. Dung han --- */
        ket_noi_dung_han(k);
        ngu_ms(300);
        kiem("bam dung thi thoi nap", !ket_noi_dang_nap(k));
        kiem("gia lap van song sau khi dung han", gia_lap_con_song());
    }

    /* --- 8. Nhich tay --- */
    if (gia_lap_con_song()) {
        double x1, x2, a1, a2;
        ngu_ms(300);
        ket_noi_vi_tri(k, &x1, &a1);
        ket_noi_jog(k, 'X', 10.0, 600.0);
        ngu_ms(800);
        ket_noi_vi_tri(k, &x2, &a2);
        kiem("nhich truc X thi ong dich ra", x2 > x1 + 1.0);

        ket_noi_vi_tri(k, &x1, &a1);
        ket_noi_jog(k, 'A', 90.0, 600.0);
        ngu_ms(800);
        ket_noi_vi_tri(k, &x2, &a2);
        kiem("nhich truc A tinh bang DO, quy doi dung ra mm cung",
             a2 > a1 + 45.0);
    }

    kiem("co nhan duoc ban tin tu may", bat.so_dong_may > 0);

    ket_noi_giai_phong(k);
    dong_gia_lap();

    printf("\n%s\n", so_that_bai == 0 ? "TAT CA DAT" : "CO BAI HONG");
    return so_that_bai == 0 ? 0 : 1;
}
