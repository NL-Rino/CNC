/* KIEM TRA MO PHONG 3D VA THE XEP 2D
 *
 * Hai module nay chi DUNG RA danh sach hinh nen kiem tra duoc bang chuong
 * trinh dong lenh, khong can mo cua so. Toan bo con so o day da duoc doi
 * chieu voi ban Python cu va giong het tung chu so.
 */
#include "../loi_c/ve_3d.h"
#include "../loi_c/xep_2d.h"
#include "../loi_c/loi_chung.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int so_that_bai = 0;

static void kiem(const char *ten, int dung)
{
    printf("%-58s %s\n", ten, dung ? "DAT" : "HONG");
    if (!dung) so_that_bai++;
}

static int gan(double a, double b) { return fabs(a - b) < 1e-6; }

/* Dem so doan thang mang mot mau nhat dinh */
static int dem_mau(const KhungVe *k, unsigned mau)
{
    int i, n = 0;
    for (i = 0; i < k->so_duong; i++) if (k->duong[i].mau == mau) n++;
    return n;
}

static void thu_mo_phong_3d(void)
{
    MoPhong3D *m = mp3d_tao();
    DoanDi doan[6];
    KhungVe kv;
    double x = 0, a = 0, x_ong, goc_ong;
    int i;

    printf("\n-- MO PHONG 3D --\n");

    for (i = 0; i < 6; i++) {
        doan[i].x1 = x; doan[i].a1 = a;
        x += 12.5 + i * 3.0;
        a += 47.0 + i * 11.0;
        doan[i].x2 = x; doan[i].a2 = a;
        doan[i].la_cat = (i != 1 && i != 4);     /* 2 doan chay nhanh xen giua */
    }
    mp3d_dat_du_lieu(m, doan, 6, 76.0, 0.0);

    kiem("tu tinh chieu dai ong khi khong bao truoc",
         mp3d_chieu_dai_ong(m) > x && mp3d_chieu_dai_ong(m) < x * 1.3 + 41);
    /* doan 0, doan 2-3, doan 5 -> ba mieng cat */
    kiem("gom doan cat lien tiep thanh 3 mieng", mp3d_so_nhom(m) == 3);
    kiem("doan 2 va doan 3 cung mot mieng",
         mp3d_nhom_cua_doan(m, 2) == mp3d_nhom_cua_doan(m, 3) &&
         mp3d_nhom_cua_doan(m, 2) >= 0);
    kiem("doan chay nhanh khong thuoc mieng nao", mp3d_nhom_cua_doan(m, 1) == -1);

    /* --- Chua chay: ong dat sao cho vung cat nam giua khung --- */
    mp3d_dat_vi_tri_chay(m, -1);
    mp3d_trang_thai_ong(m, &x_ong, &goc_ong);
    kiem("chua chay thi ong chua quay", gan(goc_ong, 0.0));
    kiem("chua chay thi vung cat nam giua khung", gan(x_ong, x / 2.0));

    /* --- Dang chay: ong truot va quay toi diem dang cat --- */
    mp3d_dat_vi_tri_chay(m, 3);
    mp3d_trang_thai_ong(m, &x_ong, &goc_ong);
    kiem("dang chay thi ong truot toi diem dang cat", gan(x_ong, doan[3].x2));
    kiem("dang chay thi ong quay toi goc dang cat", gan(goc_ong, doan[3].a2));

    mp3d_dung_hinh(m, 800, 500, &kv);
    kiem("than ong ve du 48 mui * 3 (than + 2 mat dau)", kv.so_mat == 48 * 3);
    /* 6 doan nhung 2 doan dang nam khuat SAU ong nen bi bo, con 4.
     * Cong them 2 net mo + 1 cham dau mo + 3 net chu thich = 10. */
    kiem("doan nam khuat sau ong thi khong ve", kv.so_duong == 4 + 2 + 1 + 3);
    kiem("doan da cat xong to mau xanh", dem_mau(&kv, 0x3fa34du) == 1);
    /* 1 doan dang cat + 1 cham dau mo + 1 net chu thich */
    kiem("doan dang cat to mau vang", dem_mau(&kv, 0xffd23fu) == 3);
    kiem("doan chua cat con to mau do",
         dem_mau(&kv, 0xff5c4du) == 1 + 1);   /* 1 doan + 1 net chu thich */
    kiem("co dong chu bao ong dang truot va quay",
         kv.so_chu >= 2 && strstr(kv.chu[1].chu, "truot") != NULL);
    khung_ve_giai_phong(&kv);

    /* --- An bot mieng cat cho de nhin --- */
    mp3d_bat_tat_nhom(m, 1);
    kiem("an mot mieng thi dem duoc 1", mp3d_so_nhom_bi_an(m) == 1);
    mp3d_dung_hinh(m, 800, 500, &kv);
    /* Mieng 1 gom doan 2 va doan 3, ca hai deu dang thay -> bot di 2 net */
    kiem("mieng bi an thi khong ve nua", kv.so_duong == 4 + 2 + 1 + 3 - 2);
    kiem("chu thich bao dang an may mieng",
         strstr(kv.chu[0].chu, "dang an 1 mieng") != NULL);
    khung_ve_giai_phong(&kv);
    mp3d_bat_tat_nhom(m, 1);
    kiem("bam lan nua thi hien lai", mp3d_so_nhom_bi_an(m) == 0);
    mp3d_bat_tat_nhom(m, 0);
    mp3d_bat_tat_nhom(m, 2);
    mp3d_hien_lai_het(m);
    kiem("nut hien lai het xoa sach", mp3d_so_nhom_bi_an(m) == 0);

    /* --- Bam trung duong cat --- */
    mp3d_dat_vi_tri_chay(m, -1);
    mp3d_dung_hinh(m, 800, 500, &kv);
    {
        /* Lay dung diem dau cua mot duong cat roi bam trung vao do */
        int tim = -1, j;
        for (j = 0; j < kv.so_duong; j++)
            if (kv.duong[j].mau == 0xff5c4du) { tim = j; break; }
        kiem("tim duoc mot duong cat tren man hinh", tim >= 0);
        if (tim >= 0)
            kiem("bam trung duong cat thi nhan dung doan",
                 mp3d_doan_gan_diem(m, kv.duong[tim].a.x, kv.duong[tim].a.y, 14) >= 0);
        kiem("bam ra cho trong thi khong nhan doan nao",
             mp3d_doan_gan_diem(m, -500, -500, 14) == -1);
    }
    khung_ve_giai_phong(&kv);

    /* --- Khong co duong cat nao --- */
    mp3d_dat_du_lieu(m, NULL, 0, 60.0, 0.0);
    mp3d_dung_hinh(m, 800, 500, &kv);
    kiem("khong co duong cat thi bao ra man hinh",
         kv.so_chu > 0 && strstr(kv.chu[0].chu, "Chua co duong cat") != NULL);
    khung_ve_giai_phong(&kv);

    mp3d_giai_phong(m);
}

/* ------------------------------------------------------------------------ */
static int lan_ve_lai = 0;
static int chon_cuoi = -2;
static double keo_toi = 0.0;

static void bao_chon(void *ctx, int chi_so) { (void)ctx; chon_cuoi = chi_so; }
static void bao_keo(void *ctx, int chi_so, double x_tam)
{
    (void)ctx; (void)chi_so; keo_toi = x_tam;
}
static void bao_ve_lai(void *ctx) { (void)ctx; lan_ve_lai++; }

static void thu_xep_2d(void)
{
    HamXep2D ham;
    Xep2D *x;
    KhungNhatCat kh[3];
    KhungVe kv;
    double px_dau, px_cuoi, giua;
    int i;
    static const double dau[3]  = { 40.0, 300.0, 720.0 };
    static const double cuoi[3] = { 105.0, 366.5, 723.0 };
    static const char *ten[3]   = { "goc_90", "goc_45", "nhanh_t_90" };

    printf("\n-- THE XEP 2D --\n");

    memset(&ham, 0, sizeof(ham));
    ham.khi_chon = bao_chon;
    ham.khi_keo = bao_keo;
    ham.can_ve_lai = bao_ve_lai;
    x = xep2d_tao(&ham);

    for (i = 0; i < 3; i++) {
        kh[i].x_dau = dau[i];
        kh[i].x_cuoi = cuoi[i];
        snprintf(kh[i].ten, sizeof(kh[i].ten), "%s", ten[i]);
        kh[i].x_tam = (dau[i] + cuoi[i]) / 2.0;
    }
    xep2d_dat_du_lieu(x, kh, 3, 60.0, 1200.0);
    xep2d_dat_co_khung_hinh(x, 900, 320);

    /* --- Khoang cach do tu DAU ONG XA MAM KEP (diem goc) --- */
    kiem("khoang cach do tu diem goc toi canh gan no nhat",
         gan(xep2d_khoang_cach_tu_goc(x, 105.0), 1200.0 - 105.0));
    kiem("nhat cat sat diem goc thi khoang cach nho",
         gan(xep2d_khoang_cach_tu_goc(x, 723.0), 477.0));
    /* Doi nguoc: go khoang cach -> tam nhat cat nam o dau */
    kiem("go khoang cach roi doi nguoc ra dung tam cu",
         gan(xep2d_tu_khoang_cach(x, 1200.0 - 105.0, 72.5, 105.0), 72.5));
    kiem("doi khoang cach 100mm thi tam dich dung",
         gan(xep2d_tu_khoang_cach(x, 100.0, 72.5, 105.0), 72.5 + 995.0));

    /* --- Doi toa do di roi ve phai tra lai dung cho cu --- */
    kiem("doi mm -> pixel -> mm khong sai",
         gan(xep2d_sang_mm(x, xep2d_sang_pixel(x, 456.25)), 456.25));
    kiem("tu canh cho vua be ngang khung hinh",
         gan(xep2d_sang_pixel(x, 0.0), LE_TRAI_XEP) &&
         gan(xep2d_sang_pixel(x, 1200.0), 900.0 - LE_PHAI_XEP));

    /* --- Ve --- */
    xep2d_dung_hinh(x, 900, 320, &kv);
    kiem("chua chon thi khong ve duong do khoang cach",
         dem_mau(&kv, 0xffd23fu) == 1);      /* chi co vach DIEM GOC */
    kiem("ve du 3 nhat cat + nen + thuoc + ong + mam kep",
         kv.so_hcn == 3 + 4);
    khung_ve_giai_phong(&kv);

    /* --- Bam trung mot nhat cat --- */
    px_dau  = xep2d_sang_pixel(x, kh[1].x_dau);
    px_cuoi = xep2d_sang_pixel(x, kh[1].x_cuoi);
    giua = (px_dau + px_cuoi) / 2.0;
    lan_ve_lai = 0;
    xep2d_bam(x, giua, 160);
    kiem("bam trung nhat cat thi chon dung nhat do", xep2d_dang_chon(x) == 1);
    kiem("bam xong bao giao dien ve lai", lan_ve_lai == 1);
    kiem("bao ra ngoai chi so nhat cat vua chon", chon_cuoi == 1);

    xep2d_dung_hinh(x, 900, 320, &kv);
    kiem("dang chon thi ve them duong do khoang cach",
         dem_mau(&kv, 0xffd23fu) == 2);
    {
        int co_mui_ten = 0, j;
        for (j = 0; j < kv.so_duong; j++) if (kv.duong[j].mui_ten) co_mui_ten = 1;
        kiem("duong do khoang cach co mui ten hai dau", co_mui_ten);
    }
    khung_ve_giai_phong(&kv);

    /* --- Keo nhat cat sang cho khac --- */
    keo_toi = 0.0;
    xep2d_keo(x, giua + 100.0, 160);
    kiem("keo sang phai thi tam moi dich sang phai",
         keo_toi > kh[1].x_tam + 1.0);
    kiem("keo bao dung so mm ung voi so pixel da keo",
         gan(keo_toi, kh[1].x_tam + 100.0 / xep2d_ty_le(x)));
    xep2d_nha(x);
    keo_toi = -1.0;
    xep2d_keo(x, giua + 200.0, 160);
    kiem("nha chuot roi thi keo tiep khong an gi", gan(keo_toi, -1.0));

    /* --- Bam ra cho trong thi bo chon --- */
    xep2d_bam(x, xep2d_sang_pixel(x, 600.0), 160);
    kiem("bam ra cho trong thi bo chon", xep2d_dang_chon(x) == -1);
    kiem("bao ra ngoai la da bo chon", chon_cuoi == -1);

    /* --- Phong to giu nguyen diem duoi con tro --- */
    {
        double truoc = xep2d_sang_mm(x, 500.0);
        xep2d_phong_to(x, 1.6, 500.0);
        kiem("phong to giu nguyen diem dang nam duoi con tro",
             gan(xep2d_sang_mm(x, 500.0), truoc));
        kiem("phong to that su lam to hinh len",
             xep2d_ty_le(x) > (900.0 - LE_TRAI_XEP - LE_PHAI_XEP) / 1200.0);
    }
    /* --- Day khung nhin bang chuot giua --- */
    {
        double truoc = xep2d_sang_pixel(x, 300.0);
        xep2d_bat_dau_day(x, 400.0);
        xep2d_day_khung_nhin(x, 460.0);
        kiem("day chuot giua thi ca khung nhin truot theo",
             gan(xep2d_sang_pixel(x, 300.0), truoc + 60.0));
        xep2d_het_day(x);
        xep2d_day_khung_nhin(x, 900.0);
        kiem("tha chuot giua roi thi khong day nua",
             gan(xep2d_sang_pixel(x, 300.0), truoc + 60.0));
    }
    xep2d_vua_khung_hinh(x);
    kiem("nut vua khung hinh dua ve canh tu dong",
         gan(xep2d_sang_pixel(x, 0.0), LE_TRAI_XEP) &&
         gan(xep2d_sang_pixel(x, 1200.0), 900.0 - LE_PHAI_XEP));

    /* --- Bot nhat cat thi bo chon cu neu no khong con --- */
    xep2d_dat_dang_chon(x, 2);
    xep2d_dat_du_lieu(x, kh, 1, 60.0, 1200.0);
    kiem("bot nhat cat thi bo chon cai da mat", xep2d_dang_chon(x) == -1);

    xep2d_giai_phong(x);
}

int main(void)
{
    printf("== KIEM TRA MO PHONG 3D VA THE XEP 2D ==\n");
    thu_mo_phong_3d();
    thu_xep_2d();
    printf("\n%s\n", so_that_bai == 0 ? "TAT CA DAT" : "CO BAI HONG");
    return so_that_bai == 0 ? 0 : 1;
}
