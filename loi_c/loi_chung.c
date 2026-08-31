#include "loi_chung.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void duong_cat_khoi_tao(DuongCat *d)
{
    d->ten[0] = '\0';
    d->diem = NULL;
    d->so_diem = 0;
    d->suc_chua = 0;
    d->kin = 1;
}

void duong_cat_giai_phong(DuongCat *d)
{
    free(d->diem);
    d->diem = NULL;
    d->so_diem = 0;
    d->suc_chua = 0;
}

int duong_cat_them(DuongCat *d, double x, double a)
{
    if (d->so_diem >= d->suc_chua) {
        int moi = d->suc_chua ? d->suc_chua * 2 : 128;
        DiemCat *tam = (DiemCat *)realloc(d->diem, (size_t)moi * sizeof(DiemCat));
        if (!tam) return -1;
        d->diem = tam;
        d->suc_chua = moi;
    }
    d->diem[d->so_diem].x = x;
    d->diem[d->so_diem].a = a;
    d->so_diem++;
    return 0;
}

void duong_cat_pham_vi_x(const DuongCat *d, double *x_min, double *x_max)
{
    int i;
    if (d->so_diem == 0) {
        if (x_min) *x_min = 0.0;
        if (x_max) *x_max = 0.0;
        return;
    }
    double lo = d->diem[0].x, hi = d->diem[0].x;
    for (i = 1; i < d->so_diem; i++) {
        if (d->diem[i].x < lo) lo = d->diem[i].x;
        if (d->diem[i].x > hi) hi = d->diem[i].x;
    }
    if (x_min) *x_min = lo;
    if (x_max) *x_max = hi;
}

void dat_loi(char *loi, const char *chu, ...)
{
    va_list ds;
    if (!loi) return;
    va_start(ds, chu);
    vsnprintf(loi, CO_LOI, chu, ds);
    va_end(ds);
}

void so_gon(char *ra, size_t co_ra, double gia_tri)
{
    char *p;
    snprintf(ra, co_ra, "%.3f", gia_tri);
    if (strchr(ra, '.')) {
        p = ra + strlen(ra) - 1;
        while (p > ra && *p == '0') *p-- = '\0';
        if (*p == '.') *p = '\0';
    }
    if (ra[0] == '\0' || (ra[0] == '-' && ra[1] == '\0')) {
        snprintf(ra, co_ra, "0");
    }
}
