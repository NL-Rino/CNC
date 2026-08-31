/* KIEM TRA LOP KET NOI (loi_c/ket_noi.c)
 *
 * Cach lam: tao mot cap cong ao (pty). Dau MASTER giao cho tien trinh gia lap
 * ESP32 (chay dung code firmware that) lam stdin/stdout, dau SLAVE la mot
 * duong dan /dev/pts/N ma lop ket_noi mo y het mot cong COM that.
 *
 * Nho vay toan bo duong di "may tinh <-> ESP32" duoc chay that: nap dan,
 * dieu tiet luu luong bang OK;<cho_trong>, hoi BUF, bam RUN som.
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
    int  so_nhat_ky;
    int  so_loi;
    int  so_dong_esp32;
    int  co_bat_dau_chay;       /* thay ban tin "BAT DAU CHAY" */
    int  co_nap_xong;
    double x_cuoi, a_cuoi;
    int  so_vi_tri;
} Bat;

static void bat_dong(void *ctx, const char *dong)
{
    Bat *b = (Bat *)ctx;
    b->so_dong_esp32++;
    (void)dong;
}
static void bat_nhat_ky(void *ctx, const char *chu)
{
    Bat *b = (Bat *)ctx;
    b->so_nhat_ky++;
    snprintf(b->nhat_ky_cuoi, sizeof(b->nhat_ky_cuoi), "%s", chu);
    if (strstr(chu, "BAT DAU CHAY")) b->co_bat_dau_chay = 1;
    if (strstr(chu, "Da nap xong")) b->co_nap_xong = 1;
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

/* ----------------------------------------------------------- CAP CONG AO */
static pid_t con = -1;

static int mo_gia_lap(const char *duong_dan_gia_lap, char *ten_slave, size_t co_ten)
{
    int chu = posix_openpt(O_RDWR | O_NOCTTY);
    struct termios t;
    if (chu < 0) return -1;
    if (grantpt(chu) != 0 || unlockpt(chu) != 0) { close(chu); return -1; }
    if (ptsname_r(chu, ten_slave, co_ten) != 0) { close(chu); return -1; }

    /* Tat het xu ly ky tu tren duong day ao: khong vong lai, khong doi \n */
    if (tcgetattr(chu, &t) == 0) {
        cfmakeraw(&t);
        tcsetattr(chu, TCSANOW, &t);
    }

    con = fork();
    if (con < 0) { close(chu); return -1; }
    if (con == 0) {
        /* Tien trinh con: dau MASTER thanh stdin/stdout cua gia lap ESP32 */
        dup2(chu, 0);
        dup2(chu, 1);
        if (chu > 2) close(chu);
        execl(duong_dan_gia_lap, duong_dan_gia_lap, "40", (char *)NULL);
        _exit(127);
    }
    close(chu);
    return 0;
}

static void dong_gia_lap(void)
{
    if (con > 0) {
        kill(con, SIGTERM);
        waitpid(con, NULL, 0);
        con = -1;
    }
}

/* ------------------------------------------------------------------ MAIN */
int main(int argc, char **argv)
{
    const char *gia_lap = argc > 1 ? argv[1] : "./gia_lap_esp32";
    char ten_slave[128];
    Bat bat;
    HamGoiLai gl;
    KetNoi *k;
    char loi[CO_LOI] = "";
    double x, a;
    int i;

    printf("== KIEM TRA LOP KET NOI ==\n\n");

    /* --- 1. Doc vi tri (thuan tuy, khong can cong) --- */
    kiem("doc vi tri binh thuong",
         ket_noi_doc_vi_tri("Vi tri: X=12.34 A=56.78", &x, &a) == 0 &&
         gan_bang(x, 12.34, 1e-9) && gan_bang(a, 56.78, 1e-9));
    kiem("doc vi tri co dau am",
         ket_noi_doc_vi_tri("Vi tri: X=-5.5 A=-0.25 (dang chay)", &x, &a) == 0 &&
         gan_bang(x, -5.5, 1e-9) && gan_bang(a, -0.25, 1e-9));
    kiem("dong khong co vi tri thi bao khong doc duoc",
         ket_noi_doc_vi_tri("OK;1200;0", &x, &a) == -1);
    kiem("thieu mot truc thi bao khong doc duoc",
         ket_noi_doc_vi_tri("Vi tri: X=10", &x, &a) == -1);

    if (access(gia_lap, X_OK) != 0) {
        printf("\n(Khong tim thay gia lap ESP32 '%s' - bo qua phan nap dan)\n",
               gia_lap);
        printf("\n%s\n", so_that_bai == 0 ? "TAT CA DAT" : "CO BAI HONG");
        return so_that_bai == 0 ? 0 : 1;
    }

    /* --- 2. Nap dan that qua cong ao --- */
    memset(&bat, 0, sizeof(bat));
    memset(&gl, 0, sizeof(gl));
    gl.ctx = &bat;
    gl.dong_esp32 = bat_dong;
    gl.nhat_ky = bat_nhat_ky;
    gl.loi_nap = bat_loi;
    gl.vi_tri = bat_vi_tri;

    if (mo_gia_lap(gia_lap, ten_slave, sizeof(ten_slave)) != 0) {
        printf("Khong tao duoc cong ao - bo qua phan nap dan.\n");
        return so_that_bai == 0 ? 0 : 1;
    }
    printf("\nCong ao: %s (gia lap ESP32 chay o tien trinh con)\n\n", ten_slave);

    k = ket_noi_tao(&gl);
    /* baud_chon = BAUD_KHOI_DONG: giu 115200, khong thuong luong (giong cai
     * dat that cua may - 115200 on dinh nhat) */
    if (ket_noi_mo(k, ten_slave, BAUD_KHOI_DONG, loi) != 0) {
        printf("Khong mo duoc cong ao: %s\n", loi);
        dong_gia_lap();
        return 1;
    }
    kiem("mo cong ao", ket_noi_dang_mo(k));
    kiem("giu nguyen 115200 baud", ket_noi_baud_dang_dung(k) == BAUD_KHOI_DONG);

    /* Sinh mot chuong trinh dai hon suc chua vong dem cua ESP32 de bat buoc
     * phai NAP DAN chu khong nap mot lan */
    {
        enum { SO_DONG = 900 };
        static char dem[SO_DONG][CO_DONG_G];
        static const char *tro[SO_DONG];
        int n = 0;
        char tho[CO_DONG_G];

        snprintf(dem[n], CO_DONG_G, "G21");           tro[n] = dem[n]; n++;
        snprintf(dem[n], CO_DONG_G, "G90");           tro[n] = dem[n]; n++;
        for (i = 0; n < SO_DONG - 1; i++) {
            snprintf(tho, sizeof(tho), "G1 X%.2f A%.2f F15",
                     (double)(i % 40) * 0.5, (double)(i % 360));
            if (nen_dong_gui(tho, dem[n], CO_DONG_G) > 0) { tro[n] = dem[n]; n++; }
        }
        snprintf(dem[n], CO_DONG_G, "M30");           tro[n] = dem[n]; n++;

        kiem("chuong trinh thu dai hon vong dem ESP32", n > SUC_CHUA_BO_DEM / 2);

        if (ket_noi_nap_va_chay(k, tro, n) != 0) {
            printf("Khong khoi dong duoc viec nap.\n");
            so_that_bai++;
        } else {
            double han = gio_giay() + 90.0;
            while (gio_giay() < han && ket_noi_dang_nap(k)) ngu_ms(20);
            kiem("nap dan ket thuc trong thoi gian cho", !ket_noi_dang_nap(k));
            kiem("khong co loi nap", bat.so_loi == 0);
            if (bat.so_loi) printf("   loi cuoi: %s\n", bat.loi_cuoi);
            kiem("da bam CHAY som (vua chay vua nap)", bat.co_bat_dau_chay);
            kiem("da nap xong toan bo", bat.co_nap_xong);
            kiem("ESP32 bao nhan du so dong",
                 ket_noi_so_dong_da_nhan(k) == n);
            if (ket_noi_so_dong_da_nhan(k) != n)
                printf("   gui %d dong, ESP32 bao nhan %d dong\n",
                       n, ket_noi_so_dong_da_nhan(k));
        }
    }

    /* --- 3. Hoi vi tri --- */
    bat.so_vi_tri = 0;
    ket_noi_gui(k, "POS");
    {
        double han = gio_giay() + 3.0;
        while (gio_giay() < han && bat.so_vi_tri == 0) ngu_ms(10);
    }
    kiem("hoi POS thi nhan duoc vi tri", bat.so_vi_tri > 0);

    kiem("co nhan duoc ban tin tu ESP32", bat.so_dong_esp32 > 0);

    ket_noi_giai_phong(k);
    dong_gia_lap();

    printf("\n%s\n", so_that_bai == 0 ? "TAT CA DAT" : "CO BAI HONG");
    return so_that_bai == 0 ? 0 : 1;
}
