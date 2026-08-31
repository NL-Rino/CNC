#include "tien_ich.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <commdlg.h>

HFONT PC_THUONG = NULL, PC_DAM = NULL, PC_NHO = NULL;
HFONT PC_DEU = NULL, PC_DEU_TO = NULL, PC_TO_DAM = NULL;

static HFONT tao_pc(int cao, int nang, const char *ten)
{
    return CreateFontA(cao, 0, 0, 0, nang, 0, 0, 0, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, ten);
}

void tien_ich_khoi_tao(void)
{
    if (PC_THUONG) return;
    PC_THUONG = tao_pc(-12, FW_NORMAL, "Segoe UI");
    PC_DAM    = tao_pc(-12, FW_BOLD,   "Segoe UI");
    PC_NHO    = tao_pc(-11, FW_NORMAL, "Segoe UI");
    PC_TO_DAM = tao_pc(-14, FW_BOLD,   "Segoe UI");
    PC_DEU    = tao_pc(-12, FW_NORMAL, "Consolas");
    PC_DEU_TO = tao_pc(-17, FW_NORMAL, "Consolas");
}

void tien_ich_don_dep(void)
{
    HFONT *cac[] = { &PC_THUONG, &PC_DAM, &PC_NHO, &PC_DEU, &PC_DEU_TO, &PC_TO_DAM };
    size_t i;
    for (i = 0; i < sizeof(cac) / sizeof(cac[0]); i++)
        if (*cac[i]) { DeleteObject(*cac[i]); *cac[i] = NULL; }
}

/* ---------------------------------------------------------------- TAO O */
static HWND tao(const char *lop, const char *chu, DWORD kieu, HWND cha, int id)
{
    HWND h = CreateWindowExA(0, lop, chu, WS_CHILD | WS_VISIBLE | kieu,
                             0, 0, 10, 10, cha, (HMENU)(INT_PTR)id,
                             (HINSTANCE)GetWindowLongPtrA(cha, GWLP_HINSTANCE), NULL);
    if (h) SendMessageA(h, WM_SETFONT, (WPARAM)PC_THUONG, TRUE);
    return h;
}

HWND tao_nhan(HWND cha, const char *chu, int id)
{
    return tao("STATIC", chu, SS_LEFT | SS_NOTIFY, cha, id);
}

HWND tao_nut(HWND cha, const char *chu, int id)
{
    return tao("BUTTON", chu, BS_PUSHBUTTON | WS_TABSTOP, cha, id);
}

HWND tao_o_nhap(HWND cha, const char *chu, int id)
{
    return CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", chu,
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                           0, 0, 10, 10, cha, (HMENU)(INT_PTR)id,
                           (HINSTANCE)GetWindowLongPtrA(cha, GWLP_HINSTANCE), NULL);
}

HWND tao_hop_chon(HWND cha, int id)
{
    return tao("COMBOBOX", "", CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP, cha, id);
}

HWND tao_khung(HWND cha, const char *tieu_de, int id)
{
    return tao("BUTTON", tieu_de, BS_GROUPBOX, cha, id);
}

HWND tao_o_danh_dau(HWND cha, const char *chu, int id)
{
    return tao("BUTTON", chu, BS_AUTOCHECKBOX | WS_TABSTOP, cha, id);
}

HWND tao_o_tron(HWND cha, const char *chu, int id, int nhom_dau)
{
    return tao("BUTTON", chu, BS_AUTORADIOBUTTON | WS_TABSTOP |
               (nhom_dau ? WS_GROUP : 0), cha, id);
}

void dat_cho(HWND o, int x, int y, int rong, int cao)
{
    if (o) MoveWindow(o, x, y, rong, cao, TRUE);
}

/* ------------------------------------------------------------- DOC / GHI */
void dat_chu(HWND o, const char *chu)
{
    char cu[512];
    /* Chi dat lai khi that su khac - tranh nhap nhay va mat vi tri con tro */
    if (o && GetWindowTextA(o, cu, sizeof(cu)) >= 0 && strcmp(cu, chu) != 0)
        SetWindowTextA(o, chu);
}

void dat_chu_so(HWND o, double gt)
{
    char chu[48];
    snprintf(chu, sizeof(chu), "%g", gt);
    dat_chu(o, chu);
}

int lay_chu(HWND o, char *ra, int co_ra)
{
    if (!o) { if (co_ra) ra[0] = '\0'; return 0; }
    return GetWindowTextA(o, ra, co_ra);
}

int lay_so(HWND o, double *ra)
{
    char chu[64], *p, *ket = NULL;
    double gt;
    lay_chu(o, chu, sizeof(chu));
    for (p = chu; *p; p++) if (*p == ',') *p = '.';     /* nguoi Viet hay go dau phay */
    p = chu;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return -1;
    gt = strtod(p, &ket);
    if (ket == p) return -1;
    while (*ket == ' ' || *ket == '\t') ket++;
    if (*ket) return -1;
    if (ra) *ra = gt;
    return 0;
}

double lay_so_hoac(HWND o, double mac_dinh)
{
    double gt;
    if (lay_so(o, &gt) != 0 || gt <= 0) return mac_dinh;
    return gt;
}

/* ------------------------------------------------------------ THONG BAO */
static void hop(HWND cha, const char *tieu_de, UINT co, const char *dinh_dang,
                va_list ds)
{
    char chu[1024];
    vsnprintf(chu, sizeof(chu), dinh_dang, ds);
    MessageBoxA(cha, chu, tieu_de, co);
}

void bao_loi(HWND cha, const char *tieu_de, const char *dinh_dang, ...)
{
    va_list ds; va_start(ds, dinh_dang);
    hop(cha, tieu_de, MB_ICONERROR | MB_OK, dinh_dang, ds);
    va_end(ds);
}

void bao_tin(HWND cha, const char *tieu_de, const char *dinh_dang, ...)
{
    va_list ds; va_start(ds, dinh_dang);
    hop(cha, tieu_de, MB_ICONINFORMATION | MB_OK, dinh_dang, ds);
    va_end(ds);
}

void canh_bao(HWND cha, const char *tieu_de, const char *dinh_dang, ...)
{
    va_list ds; va_start(ds, dinh_dang);
    hop(cha, tieu_de, MB_ICONWARNING | MB_OK, dinh_dang, ds);
    va_end(ds);
}

int hoi_co_khong(HWND cha, const char *tieu_de, const char *dinh_dang, ...)
{
    char chu[1024];
    va_list ds;
    va_start(ds, dinh_dang);
    vsnprintf(chu, sizeof(chu), dinh_dang, ds);
    va_end(ds);
    return MessageBoxA(cha, chu, tieu_de, MB_ICONQUESTION | MB_YESNO) == IDYES;
}

/* --------------------------------------------------------------- FILE */
static void dat_chung(OPENFILENAMEA *o, HWND cha, char *ra, int co_ra,
                      const char *bo_loc, const char *tieu_de)
{
    memset(o, 0, sizeof(*o));
    o->lStructSize = sizeof(*o);
    o->hwndOwner = cha;
    o->lpstrFilter = bo_loc;
    o->lpstrFile = ra;
    o->nMaxFile = (DWORD)co_ra;
    o->lpstrTitle = tieu_de;
}

int chon_file_mo(HWND cha, char *ra, int co_ra, const char *bo_loc,
                 const char *tieu_de)
{
    OPENFILENAMEA o;
    ra[0] = '\0';
    dat_chung(&o, cha, ra, co_ra, bo_loc, tieu_de);
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameA(&o) ? 0 : -1;
}

int chon_file_luu(HWND cha, char *ra, int co_ra, const char *bo_loc,
                  const char *tieu_de, const char *duoi_mac_dinh)
{
    OPENFILENAMEA o;
    dat_chung(&o, cha, ra, co_ra, bo_loc, tieu_de);
    o.lpstrDefExt = duoi_mac_dinh;
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    return GetSaveFileNameA(&o) ? 0 : -1;
}

/* ------------------------------------------------------------------ VE */
void ve_chu(HDC hdc, int x, int y, const char *chu, HFONT pc, COLORREF mau)
{
    HFONT cu = (HFONT)SelectObject(hdc, pc);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, mau);
    TextOutA(hdc, x, y, chu, (int)strlen(chu));
    SelectObject(hdc, cu);
}

void ve_chu_phai(HDC hdc, int x_phai, int y, const char *chu, HFONT pc, COLORREF mau)
{
    HFONT cu = (HFONT)SelectObject(hdc, pc);
    SIZE sz;
    GetTextExtentPoint32A(hdc, chu, (int)strlen(chu), &sz);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, mau);
    TextOutA(hdc, x_phai - sz.cx, y, chu, (int)strlen(chu));
    SelectObject(hdc, cu);
}
