#include "hinh_ve.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void khung_ve_khoi_tao(KhungVe *k, unsigned mau_nen)
{
    memset(k, 0, sizeof(*k));
    k->mau_nen = mau_nen;
}

void khung_ve_giai_phong(KhungVe *k)
{
    if (!k) return;
    free(k->mat); free(k->hcn); free(k->duong); free(k->chu);
    memset(k, 0, sizeof(*k));
}

static int cho_them(void **mang, int so, int *suc_chua, size_t co_mot)
{
    if (so >= *suc_chua) {
        int moi = *suc_chua ? *suc_chua * 2 : 128;
        void *p = realloc(*mang, co_mot * (size_t)moi);
        if (!p) return -1;
        *mang = p;
        *suc_chua = moi;
    }
    return 0;
}

int khung_them_mat(KhungVe *k, const MatVe *m)
{
    if (cho_them((void **)&k->mat, k->so_mat, &k->suc_chua_mat, sizeof(MatVe)) != 0)
        return -1;
    k->mat[k->so_mat++] = *m;
    return 0;
}

int khung_them_hcn(KhungVe *k, double x1, double y1, double x2, double y2,
                   unsigned mau_nen, int co_nen,
                   unsigned mau_vien, int co_vien, int day)
{
    HinhChuNhat h;
    if (cho_them((void **)&k->hcn, k->so_hcn, &k->suc_chua_hcn,
                 sizeof(HinhChuNhat)) != 0)
        return -1;
    h.goc1.x = x1; h.goc1.y = y1; h.goc2.x = x2; h.goc2.y = y2;
    h.mau_nen = mau_nen; h.co_nen = co_nen;
    h.mau_vien = mau_vien; h.co_vien = co_vien;
    h.day = day;
    k->hcn[k->so_hcn++] = h;
    return 0;
}

int khung_them_duong_dac_biet(KhungVe *k, double x1, double y1,
                              double x2, double y2, unsigned mau, int day,
                              int net_dut, int mui_ten)
{
    DuongVe d;
    if (cho_them((void **)&k->duong, k->so_duong, &k->suc_chua_duong,
                 sizeof(DuongVe)) != 0)
        return -1;
    d.a.x = x1; d.a.y = y1; d.b.x = x2; d.b.y = y2;
    d.mau = mau; d.day = day; d.net_dut = net_dut; d.mui_ten = mui_ten;
    k->duong[k->so_duong++] = d;
    return 0;
}

int khung_them_duong(KhungVe *k, double x1, double y1, double x2, double y2,
                     unsigned mau, int day)
{
    return khung_them_duong_dac_biet(k, x1, y1, x2, y2, mau, day, 0, 0);
}

int khung_them_chu(KhungVe *k, double x, double y, const char *chu,
                   unsigned mau, int co_chu, int mono, int dam, KieuNeo neo)
{
    ChuVe c;
    if (cho_them((void **)&k->chu, k->so_chu, &k->suc_chua_chu, sizeof(ChuVe)) != 0)
        return -1;
    memset(&c, 0, sizeof(c));
    c.vi_tri.x = x; c.vi_tri.y = y;
    snprintf(c.chu, sizeof(c.chu), "%s", chu);
    c.mau = mau; c.co_chu = co_chu; c.mono = mono; c.dam = dam; c.neo = neo;
    k->chu[k->so_chu++] = c;
    return 0;
}

static int so_sanh_do_sau(const void *a, const void *b)
{
    double da = ((const MatVe *)a)->do_sau, db = ((const MatVe *)b)->do_sau;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

void khung_sap_theo_do_sau(KhungVe *k)
{
    qsort(k->mat, (size_t)k->so_mat, sizeof(MatVe), so_sanh_do_sau);
}
