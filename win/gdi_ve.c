#include "gdi_ve.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static COLORREF sang_gdi(unsigned mau)
{
    return RGB((mau >> 16) & 0xFF, (mau >> 8) & 0xFF, mau & 0xFF);
}

static int lam_tron(double v) { return (int)(v < 0 ? v - 0.5 : v + 0.5); }

/* Mui ten hai dau cho duong do khoang cach */
static void ve_mui_ten(HDC hdc, double x1, double y1, double x2, double y2)
{
    const double dai = 7.0, rong = 3.5;
    double dx = x2 - x1, dy = y2 - y1;
    double d = dx * dx + dy * dy;
    double ux, uy, vx, vy;
    POINT p[3];
    int i;
    if (d < 1e-9) return;
    d = sqrt(d);
    ux = dx / d; uy = dy / d;
    vx = -uy; vy = ux;
    for (i = 0; i < 2; i++) {
        double gx = i == 0 ? x1 : x2;
        double gy = i == 0 ? y1 : y2;
        double sx = i == 0 ? ux : -ux;
        double sy = i == 0 ? uy : -uy;
        p[0].x = lam_tron(gx);              p[0].y = lam_tron(gy);
        p[1].x = lam_tron(gx + sx * dai + vx * rong);
        p[1].y = lam_tron(gy + sy * dai + vy * rong);
        p[2].x = lam_tron(gx + sx * dai - vx * rong);
        p[2].y = lam_tron(gy + sy * dai - vy * rong);
        Polygon(hdc, p, 3);
    }
}

void gdi_ve_khung(HDC hdc, int rong, int cao, const KhungVe *k)
{
    HBRUSH nen = CreateSolidBrush(sang_gdi(k->mau_nen));
    RECT r;
    int i, j;
    HFONT font_cu = (HFONT)GetCurrentObject(hdc, OBJ_FONT);

    r.left = 0; r.top = 0; r.right = rong; r.bottom = cao;
    FillRect(hdc, &r, nen);
    DeleteObject(nen);

    SetBkMode(hdc, TRANSPARENT);

    /* ---- Cac mat (da sap xep san tu xa den gan) ---- */
    for (i = 0; i < k->so_mat; i++) {
        POINT p[4];
        HBRUSH b = CreateSolidBrush(sang_gdi(k->mat[i].mau));
        HPEN   pn = CreatePen(PS_SOLID, 1, sang_gdi(k->mat[i].mau));
        HBRUSH bc = (HBRUSH)SelectObject(hdc, b);
        HPEN   pc = (HPEN)SelectObject(hdc, pn);
        for (j = 0; j < k->mat[i].so_diem; j++) {
            p[j].x = lam_tron(k->mat[i].diem[j].x);
            p[j].y = lam_tron(k->mat[i].diem[j].y);
        }
        Polygon(hdc, p, k->mat[i].so_diem);
        SelectObject(hdc, bc);
        SelectObject(hdc, pc);
        DeleteObject(b);
        DeleteObject(pn);
    }

    /* ---- Hinh chu nhat ---- */
    for (i = 0; i < k->so_hcn; i++) {
        const HinhChuNhat *h = &k->hcn[i];
        RECT hr;
        hr.left   = lam_tron(h->goc1.x);
        hr.top    = lam_tron(h->goc1.y);
        hr.right  = lam_tron(h->goc2.x);
        hr.bottom = lam_tron(h->goc2.y);
        if (h->co_nen) {
            HBRUSH b = CreateSolidBrush(sang_gdi(h->mau_nen));
            FillRect(hdc, &hr, b);
            DeleteObject(b);
        }
        if (h->co_vien) {
            HPEN pn = CreatePen(PS_SOLID, h->day > 0 ? h->day : 1,
                                sang_gdi(h->mau_vien));
            HPEN pc = (HPEN)SelectObject(hdc, pn);
            HBRUSH bc = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, hr.left, hr.top, hr.right, hr.bottom);
            SelectObject(hdc, pc);
            SelectObject(hdc, bc);
            DeleteObject(pn);
        }
    }

    /* ---- Doan thang ---- */
    for (i = 0; i < k->so_duong; i++) {
        const DuongVe *d = &k->duong[i];
        int day = d->day > 0 ? d->day : 1;
        LOGBRUSH lb;
        HPEN pn, pc;
        lb.lbStyle = BS_SOLID;
        lb.lbColor = sang_gdi(d->mau);
        lb.lbHatch = 0;
        if (d->net_dut)
            pn = ExtCreatePen(PS_GEOMETRIC | PS_DASH | PS_ENDCAP_FLAT, day,
                              &lb, 0, NULL);
        else
            pn = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND |
                              PS_JOIN_ROUND, day, &lb, 0, NULL);
        pc = (HPEN)SelectObject(hdc, pn);
        if (fabs(d->a.x - d->b.x) < 1e-9 && fabs(d->a.y - d->b.y) < 1e-9) {
            /* Doan dai bang 0 = mot cham tron (dau mo cat) */
            HBRUSH b = CreateSolidBrush(sang_gdi(d->mau));
            HBRUSH bc = (HBRUSH)SelectObject(hdc, b);
            int nua = day / 2;
            Ellipse(hdc, lam_tron(d->a.x) - nua, lam_tron(d->a.y) - nua,
                    lam_tron(d->a.x) + nua, lam_tron(d->a.y) + nua);
            SelectObject(hdc, bc);
            DeleteObject(b);
        } else {
            MoveToEx(hdc, lam_tron(d->a.x), lam_tron(d->a.y), NULL);
            LineTo(hdc, lam_tron(d->b.x), lam_tron(d->b.y));
            if (d->mui_ten) {
                HBRUSH b = CreateSolidBrush(sang_gdi(d->mau));
                HBRUSH bc = (HBRUSH)SelectObject(hdc, b);
                ve_mui_ten(hdc, d->a.x, d->a.y, d->b.x, d->b.y);
                SelectObject(hdc, bc);
                DeleteObject(b);
            }
        }
        SelectObject(hdc, pc);
        DeleteObject(pn);
    }

    /* ---- Chu ---- */
    for (i = 0; i < k->so_chu; i++) {
        const ChuVe *c = &k->chu[i];
        HFONT f = CreateFontA(-(c->co_chu * 96 / 72), 0, 0, 0,
                              c->dam ? FW_BOLD : FW_NORMAL, 0, 0, 0,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              c->mono ? FIXED_PITCH : DEFAULT_PITCH,
                              c->mono ? "Consolas" : "Segoe UI");
        HFONT fc = (HFONT)SelectObject(hdc, f);
        SIZE sz;
        int x = lam_tron(c->vi_tri.x), y;
        int dai = (int)strlen(c->chu);
        GetTextExtentPoint32A(hdc, c->chu, dai, &sz);
        if (c->neo == NEO_GIUA) x -= sz.cx / 2;
        else if (c->neo == NEO_PHAI) x -= sz.cx;
        y = lam_tron(c->vi_tri.y) - sz.cy / 2;      /* neo doc luon o giua */
        SetTextColor(hdc, sang_gdi(c->mau));
        TextOutA(hdc, x, y, c->chu, dai);
        SelectObject(hdc, fc);
        DeleteObject(f);
    }
    SelectObject(hdc, font_cu);
}

void gdi_ve_khung_co_dem(HDC hdc, int rong, int cao, const KhungVe *k)
{
    HDC dem = CreateCompatibleDC(hdc);
    HBITMAP anh = CreateCompatibleBitmap(hdc, rong, cao);
    HBITMAP cu = (HBITMAP)SelectObject(dem, anh);
    gdi_ve_khung(dem, rong, cao, k);
    BitBlt(hdc, 0, 0, rong, cao, dem, 0, 0, SRCCOPY);
    SelectObject(dem, cu);
    DeleteObject(anh);
    DeleteDC(dem);
}
