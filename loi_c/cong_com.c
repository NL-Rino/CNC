#include "cong_com.h"
#include "loi_chung.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
/* ======================================================================
 * WINDOWS
 * ====================================================================== */
#include <windows.h>

struct CongCom { HANDLE h; };

int cong_liet_ke(char ten[][CO_TEN_CONG], int toi_da)
{
    /* Doc HKLM\HARDWARE\DEVICEMAP\SERIALCOMM - day la noi Windows ghi ten
     * cac cong noi tiep dang co that, ke ca cong ao cua CP2102 / CH340. */
    HKEY khoa;
    int n = 0;
    DWORD i = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0,
                      KEY_READ, &khoa) != ERROR_SUCCESS)
        return 0;
    for (;;) {
        char ten_gia_tri[256];
        char gia_tri[CO_TEN_CONG];
        DWORD co_ten = sizeof(ten_gia_tri), co_gt = sizeof(gia_tri), kieu;
        LONG kq = RegEnumValueA(khoa, i++, ten_gia_tri, &co_ten, NULL,
                                &kieu, (LPBYTE)gia_tri, &co_gt);
        if (kq == ERROR_NO_MORE_ITEMS) break;
        if (kq != ERROR_SUCCESS) break;
        if (kieu != REG_SZ) continue;
        if (n >= toi_da) break;
        gia_tri[sizeof(gia_tri) - 1] = '\0';
        snprintf(ten[n], CO_TEN_CONG, "%s", gia_tri);
        n++;
    }
    RegCloseKey(khoa);
    return n;
}

static int dat_thong_so(HANDLE h, int baud)
{
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) return -1;
    dcb.BaudRate = (DWORD)baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    /* KHONG bat bat tay phan cung: ESP32 dung DTR/RTS de reset, bat vao la
     * board tu khoi dong lai giua chung. Dieu tiet luu luong lam bang giao
     * thuc OK;<cho_trong> chu khong bang duong day. */
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_DISABLE;
    dcb.fRtsControl  = RTS_CONTROL_DISABLE;
    dcb.fOutX = dcb.fInX = FALSE;
    dcb.fBinary = TRUE;
    dcb.fAbortOnError = FALSE;
    return SetCommState(h, &dcb) ? 0 : -1;
}

CongCom *cong_mo(const char *ten, int baud, char *loi)
{
    char duong_dan[CO_TEN_CONG + 8];
    COMMTIMEOUTS tg;
    struct CongCom *c;
    HANDLE h;

    /* COM10 tro len bat buoc phai viet dang \\.\COM10 */
    snprintf(duong_dan, sizeof(duong_dan), "\\\\.\\%s", ten);
    h = CreateFileA(duong_dan, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                    OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        dat_loi(loi, "Khong mo duoc %s (ma loi %lu). Cong dang bi phan mem "
                     "khac giu, hoac day USB chua cam.", ten,
                (unsigned long)GetLastError());
        return NULL;
    }
    if (dat_thong_so(h, baud) != 0) {
        dat_loi(loi, "Cong %s khong nhan toc do %d baud.", ten, baud);
        CloseHandle(h);
        return NULL;
    }
    /* Doc: tra ve ngay khi co byte, cung lam nhieu nhat cho 50 ms. */
    tg.ReadIntervalTimeout         = 20;
    tg.ReadTotalTimeoutMultiplier  = 0;
    tg.ReadTotalTimeoutConstant    = 50;
    tg.WriteTotalTimeoutMultiplier = 0;
    tg.WriteTotalTimeoutConstant   = 3000;
    SetCommTimeouts(h, &tg);
    SetupComm(h, 65536, 65536);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    c = (struct CongCom *)calloc(1, sizeof(*c));
    if (!c) { CloseHandle(h); dat_loi(loi, "Het bo nho."); return NULL; }
    c->h = h;
    return c;
}

void cong_dong(CongCom *c)
{
    if (!c) return;
    if (c->h != INVALID_HANDLE_VALUE) CloseHandle(c->h);
    c->h = INVALID_HANDLE_VALUE;
    free(c);
}

int cong_dat_baud(CongCom *c, int baud)
{
    if (!c || c->h == INVALID_HANDLE_VALUE) return -1;
    return dat_thong_so(c->h, baud);
}

int cong_ghi(CongCom *c, const char *du_lieu, int n)
{
    DWORD da_ghi = 0;
    if (!c || c->h == INVALID_HANDLE_VALUE) return -1;
    if (!WriteFile(c->h, du_lieu, (DWORD)n, &da_ghi, NULL)) return -1;
    return (int)da_ghi;
}

int cong_doc(CongCom *c, char *ra, int co_ra)
{
    DWORD da_doc = 0;
    if (!c || c->h == INVALID_HANDLE_VALUE) return -1;
    if (!ReadFile(c->h, ra, (DWORD)co_ra, &da_doc, NULL)) return -1;
    return (int)da_doc;
}

void cong_xoa_dem_vao(CongCom *c)
{
    if (c && c->h != INVALID_HANDLE_VALUE) PurgeComm(c->h, PURGE_RXCLEAR);
}

int cong_dang_mo(const CongCom *c)
{
    return c && c->h != INVALID_HANDLE_VALUE;
}

#else
/* ======================================================================
 * LINUX / POSIX - dung de chay thu va kiem tra tren may chu
 * ====================================================================== */
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <errno.h>
#include <sys/select.h>

struct CongCom { int fd; };

int cong_liet_ke(char ten[][CO_TEN_CONG], int toi_da)
{
    static const char *tien_to[] = { "ttyUSB", "ttyACM", "ttyS" };
    DIR *thu_muc = opendir("/dev");
    struct dirent *muc;
    int n = 0;
    if (!thu_muc) return 0;
    while ((muc = readdir(thu_muc)) != NULL && n < toi_da) {
        size_t i;
        for (i = 0; i < sizeof(tien_to) / sizeof(tien_to[0]); i++) {
            if (strncmp(muc->d_name, tien_to[i], strlen(tien_to[i])) == 0) {
                snprintf(ten[n], CO_TEN_CONG, "/dev/%.*s",
                         (int)(CO_TEN_CONG - 6), muc->d_name);
                n++;
                break;
            }
        }
    }
    closedir(thu_muc);
    return n;
}

static speed_t ma_baud(int baud)
{
    switch (baud) {
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 921600:  return B921600;
    case 1000000: return B1000000;
    case 2000000: return B2000000;
    default:      return 0;
    }
}

static int dat_thong_so(int fd, int baud)
{
    struct termios t;
    speed_t ma = ma_baud(baud);
    if (ma == 0) return -1;
    if (tcgetattr(fd, &t) != 0) return -1;
    cfmakeraw(&t);
    t.c_cflag |= (CLOCAL | CREAD);
    t.c_cflag &= (tcflag_t)~CRTSCTS;
    t.c_cflag &= (tcflag_t)~CSTOPB;
    t.c_cflag &= (tcflag_t)~PARENB;
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 1;          /* 0,1 giay */
    cfsetispeed(&t, ma);
    cfsetospeed(&t, ma);
    return tcsetattr(fd, TCSANOW, &t);
}

CongCom *cong_mo(const char *ten, int baud, char *loi)
{
    struct CongCom *c;
    int fd = open(ten, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        dat_loi(loi, "Khong mo duoc %s: %s", ten, strerror(errno));
        return NULL;
    }
    if (dat_thong_so(fd, baud) != 0) {
        dat_loi(loi, "Cong %s khong nhan toc do %d baud.", ten, baud);
        close(fd);
        return NULL;
    }
    tcflush(fd, TCIOFLUSH);
    c = (struct CongCom *)calloc(1, sizeof(*c));
    if (!c) { close(fd); dat_loi(loi, "Het bo nho."); return NULL; }
    c->fd = fd;
    return c;
}

void cong_dong(CongCom *c)
{
    if (!c) return;
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
    free(c);
}

int cong_dat_baud(CongCom *c, int baud)
{
    if (!c || c->fd < 0) return -1;
    return dat_thong_so(c->fd, baud);
}

int cong_ghi(CongCom *c, const char *du_lieu, int n)
{
    int da = 0;
    if (!c || c->fd < 0) return -1;
    while (da < n) {
        ssize_t r = write(c->fd, du_lieu + da, (size_t)(n - da));
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        da += (int)r;
    }
    return da;
}

int cong_doc(CongCom *c, char *ra, int co_ra)
{
    fd_set doc;
    struct timeval han;
    ssize_t r;
    if (!c || c->fd < 0) return -1;
    FD_ZERO(&doc);
    FD_SET(c->fd, &doc);
    han.tv_sec = 0;
    han.tv_usec = 50000;
    r = select(c->fd + 1, &doc, NULL, NULL, &han);
    if (r == 0) return 0;
    if (r < 0) return (errno == EINTR) ? 0 : -1;
    r = read(c->fd, ra, (size_t)co_ra);
    if (r < 0) return (errno == EINTR || errno == EAGAIN) ? 0 : -1;
    return (int)r;
}

void cong_xoa_dem_vao(CongCom *c)
{
    if (c && c->fd >= 0) tcflush(c->fd, TCIFLUSH);
}

int cong_dang_mo(const CongCom *c)
{
    return c && c->fd >= 0;
}

#endif
