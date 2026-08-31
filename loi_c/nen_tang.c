#include "nen_tang.h"
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>

double gio_giay(void)
{
    static LARGE_INTEGER tan_so;
    LARGE_INTEGER n;
    if (tan_so.QuadPart == 0) QueryPerformanceFrequency(&tan_so);
    QueryPerformanceCounter(&n);
    return (double)n.QuadPart / (double)tan_so.QuadPart;
}

void ngu_ms(int ms)
{
    if (ms < 0) ms = 0;
    Sleep((DWORD)ms);
}

struct Luong { HANDLE h; void (*ham)(void *); void *du_lieu; };

static DWORD WINAPI vo_luong(LPVOID tham_so)
{
    struct Luong *l = (struct Luong *)tham_so;
    l->ham(l->du_lieu);
    CloseHandle(l->h);
    free(l);
    return 0;
}

Luong *luong_chay(void (*ham)(void *), void *du_lieu)
{
    struct Luong *l = (struct Luong *)calloc(1, sizeof(*l));
    if (!l) return NULL;
    l->ham = ham;
    l->du_lieu = du_lieu;
    l->h = CreateThread(NULL, 0, vo_luong, l, 0, NULL);
    if (!l->h) { free(l); return NULL; }
    return l;
}

struct Khoa { CRITICAL_SECTION cs; };

Khoa *khoa_tao(void)
{
    struct Khoa *k = (struct Khoa *)calloc(1, sizeof(*k));
    if (!k) return NULL;
    InitializeCriticalSection(&k->cs);
    return k;
}
void khoa_giai_phong(Khoa *k) { if (k) { DeleteCriticalSection(&k->cs); free(k); } }
void khoa_vao(Khoa *k) { if (k) EnterCriticalSection(&k->cs); }
void khoa_ra(Khoa *k)  { if (k) LeaveCriticalSection(&k->cs); }

#else /* POSIX */

#include <pthread.h>
#include <time.h>
#include <errno.h>

double gio_giay(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

void ngu_ms(int ms)
{
    struct timespec t;
    if (ms < 0) ms = 0;
    t.tv_sec = ms / 1000;
    t.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&t, &t) == -1 && errno == EINTR) { }
}

struct Luong { pthread_t id; void (*ham)(void *); void *du_lieu; };

static void *vo_luong(void *tham_so)
{
    struct Luong *l = (struct Luong *)tham_so;
    l->ham(l->du_lieu);
    free(l);
    return NULL;
}

Luong *luong_chay(void (*ham)(void *), void *du_lieu)
{
    struct Luong *l = (struct Luong *)calloc(1, sizeof(*l));
    if (!l) return NULL;
    l->ham = ham;
    l->du_lieu = du_lieu;
    if (pthread_create(&l->id, NULL, vo_luong, l) != 0) { free(l); return NULL; }
    pthread_detach(l->id);
    return l;
}

struct Khoa { pthread_mutex_t m; };

Khoa *khoa_tao(void)
{
    struct Khoa *k = (struct Khoa *)calloc(1, sizeof(*k));
    if (!k) return NULL;
    pthread_mutex_init(&k->m, NULL);
    return k;
}
void khoa_giai_phong(Khoa *k) { if (k) { pthread_mutex_destroy(&k->m); free(k); } }
void khoa_vao(Khoa *k) { if (k) pthread_mutex_lock(&k->m); }
void khoa_ra(Khoa *k)  { if (k) pthread_mutex_unlock(&k->m); }

#endif
