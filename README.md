# May cat ong plasma CNC - FluidNC

Bo dieu khien may cat ong plasma 3 dong co (2 dong co keo ong dong bo + 1 dong
co xoay ong), dieu khien bang G-CODE CHUAN qua cong USB COM.

May chay firmware **[FluidNC](https://github.com/bdring/FluidNC)** - mot ban
firmware CNC cho ESP32 da duoc dung rong rai, co bo lap ke hoach chuyen dong
that su (nhin truoc nhieu doan, tu tinh gia toc theo goc gap), co mo plasma
kem chan bao hong quang, va co giao dien web de doi cau hinh. Kho nay giu:

- **file cau hinh may** cho FluidNC (chan GPIO, so xung, mo plasma, 7 nut bam)
- **phan mem may tinh** viet bang C: thu vien moi noi, the xep 2D, mo phong 3D,
  doc file .NC, va bo gui G-code xuong may

## Thanh phan

### Cau hinh may cho FluidNC

| File | Cong dung |
|---|---|
| `fluidnc/may_cat_ong_esp32.yaml` | Cau hinh may cho ESP32 devkit goc |
| `fluidnc/may_cat_ong_esp32s3.yaml` | Cau hinh may cho ESP32-S3-WROOM-1 N16R8 |

### Phan tinh toan (dung chung, khong dinh gi toi giao dien)

| File | Cong dung |
|---|---|
| `loi_c/thu_vien_moi_noi.c` | Toan hinh hoc sinh duong cat cho tung kieu ghep ong |
| `loi_c/phan_tich_gcode.c` | Doc / kiem tra / chuan hoa / nen G-code, doi truc A ra mm cung |
| `loi_c/ve_3d.c` | Mo phong 3D: ong quay va truot, dau cat dung yen |
| `loi_c/xep_2d.c` | The xep 2D: keo tha nhat cat doc cay ong, co thuoc do |
| `loi_c/ket_noi.c` | Bo gui G-code xuong FluidNC (giao thuc GRBL) |
| `loi_c/cong_com.c` | Mo / doc / ghi cong COM (Win32 va Linux) |
| `loi_c/nen_tang.c` | Thoi gian, luong nen, khoa (Win32 va POSIX) |
| `loi_c/hinh_ve.c` | Danh sach hinh: mat, hinh chu nhat, doan thang, chu |
| `loi_c/loi_chung.c` | Kieu du lieu chung va cach bao loi |

Hai module `ve_3d` va `xep_2d` **khong goi mot ham do hoa nao**. Chung chi dung
ra mot `KhungVe` gom cac hinh da sap xep san, lop giao dien chi viec ve lai.
Nho vay toan bo phep chieu va bo tri kiem tra duoc bang chuong trinh dong lenh.

### Giao dien (Windows)

| File | Cong dung |
|---|---|
| `win/may_cat_ong.c` | **Phan mem chinh** - thu vien moi noi, mo phong 3D, chay may |
| `win/gdi_ve.c` | Ve `KhungVe` len cua so bang GDI, co anh dem |
| `win/tien_ich.c` | Phong chu, mau, tao o dieu khien, hop thoai |

Chi dung **Win32 + GDI co san trong Windows** - khong mot thu vien ngoai nao,
khong runtime nao phai cai kem. File `.exe` khoang 480 KB.

Cai dat phan cung (chan GPIO, hieu chuan, dao chieu truc) khong con chuong
trinh rieng nua: no nam trong **giao dien web cua chinh FluidNC**, mo tu menu
*Settings* cua phan mem.

### Kiem tra

| File | Cong dung |
|---|---|
| `kiem_tra_c/test_thu_vien.c` | Kiem chung toan hoc cua thu vien moi noi |
| `kiem_tra_c/test_xep.c` | Kiem chung phan xep bai va do khoang cach |
| `kiem_tra_c/test_ve.c` | Kiem chung mo phong 3D va the xep 2D |
| `kiem_tra_c/test_ket_noi.c` | Gui bai that xuong mot ban gia lap may FluidNC |
| `kiem_tra_c/gia_lap_fluidnc.c` | Gia lap may chay FluidNC, chay tren may tinh |

`test_ket_noi` tao mot cap cong ao (pty), mot dau giao cho ban gia lap FluidNC,
dau kia cho lop `ket_noi` mo y het mot cong COM that. Ban gia lap **tu bao
hong** neu phan mem gui qua tay bo dem 127 byte cua may - do dung la loi ma
tren may that se lam mat dong lenh giua chung duong cat.

Chay het bang `make kiem-tra` (150 bai, chay duoc tren Linux, khong can may that).

## 1. Nap FluidNC vao ESP32

1. Tai ban phat hanh FluidNC moi nhat o
   <https://github.com/bdring/FluidNC/releases> roi nap theo huong dan trong do
   (`install_win.bat` tren Windows). Chon ban `wifi` neu muon dung giao dien web.
2. Chep file cau hinh cua may nay vao ESP32. Cach de nhat la qua giao dien web
   cua FluidNC: vao muc *Files*, tai len `fluidnc/may_cat_ong_esp32.yaml`
   (hoac ban `_esp32s3` neu dung board S3).
3. Bao FluidNC dung file do roi khoi dong lai. Trong o go lenh:

```
$Config/Filename=may_cat_ong_esp32.yaml
$Bye
```

4. Kiem tra may doc duoc cau hinh: go `$S` (xem trang thai) va `$Limits/Show`.
   Neu co dong `[MSG:ERR: ...]` luc khoi dong thi cau hinh sai o dau do.

### Hieu chuan

| Can dat | Lenh | Ghi chu |
|---|---|---|
| Truc keo: so xung tren mot mm | `$/axes/x/steps_per_mm=320` | = xung moi vong / mm moi vong. Vi du 1600 / 5,0 = 320 |
| Truc xoay: so xung moi vong | trong phan mem, menu *Parameters* | Phan mem tu tinh ra `steps_per_mm` theo duong kinh ong |
| Toc do va gia toc | `$/axes/x/max_rate_mm_per_min=`, `$/axes/x/acceleration_mm_per_sec2=` | Tang dan cho toi khi dong co bat dau ru roi lui lai mot nac |

Kiem tra truc keo: `$X` (go bao dong) -> nhich X 100 mm -> do bang thuoc. Neu
lech thi `steps_per_mm` moi = cu x (100 / quang duong that).

**Truc xoay khong phai chinh tay.** Phan mem tu gui lai so xung moi khi doi
duong kinh ong (xem muc 4).

## 2. Chay phan mem tren may tinh

**Cach de nhat: tai file `.exe` san.** Vao tab **Actions** tren GitHub, chon
lan chay moi nhat, tai muc **Artifacts** - hoac vao thang muc **Releases**.

Copy `MayCatOng.exe` di dau cung chay duoc, **khong can cai Python hay bat cu
thu gi khac**.

> Windows Defender co the canh bao file `.exe` moi tao. Day la canh bao chung
> cho moi file exe chua ky so, chon "More info" -> "Run anyway".

**Tu dung lay** (can `make` va bo bien dich cheo mingw-w64):

```
make            # dung ra/MayCatOng.exe
make kiem-tra   # dung va chay toan bo bai kiem tra ngay tren may Linux
make sach       # xoa het file da dung
```

Tren Linux/macOS, `make kiem-tra` chay duoc het moi bai - ke ca bai gui bai
that vao mot ban gia lap FluidNC qua cong ao. Chi phan giao dien moi can
Windows de chay.

## 2b. Phan mem chinh - mot cua so lo het

```
+----------------------------------------------------------------------+
| File | Parameters | Nesting | Diagnostics | Settings | Alarm          |
+----------------------------------------------------------------------+
| [Ket noi] | [Tham so...] | che do dang dung                        |
+---------------+------------------------------------+-----------------+
| Thu vien      | [Mo phong 3D] [Xep tren cay ong]   | Vi tri may X/A  |
| moi noi       |                                    | (hoac vi tri    |
| (3 kieu)      |   3D: ong quay + truot ra vao,     |  nhat cat o the |
|               |       dau cat dung yen             |  xep)           |
+---------------+   2D: cay ong nam thang, keo tha   +-----------------+
| Dieu khien tay|       tung nhat cat, co thuoc do   | Tien do %       |
| + toc do tay  |                                    +-----------------+
|               |                                    | Kich thuoc bai  |
|               |                                    | + DUONG KINH ONG|
+---------------+------------------------------------+-----------------+
| Mo .NC |Ve goc|Chay thu|Bat mo|Mo khoa| CHAY |TAM DUNG|TIEP|DUNG |
+----------------------------------------------------------------------+
| Edit (ve bai)  |  System (terminal)  |  Alarm (loi)                   |
+----------------------------------------------------------------------+
```

**Cong COM va cach doc file .NC** nam trong mot hop thoai duy nhat, mo bang nut
*Tham so...* tren thanh cong cu. Toc do truyen co dinh **115200 baud** (FluidNC
luon chay toc do nay tren cong USB).

**Duong kinh ong** nam ngoai man hinh chinh, o khung *Kich thuoc bai* ben phai -
doi ong la viec lam hang ngay nen khong bat vao hop thoai.
### Thu vien moi noi

Chon kieu, nhap so do, bam *Them vao bai*. Lam bao nhieu mieng tren mot cay ong
cung duoc; thu tu sap xep lai duoc bang nut Len / Xuong.

| Kieu | Dung de |
|---|---|
| Ghep goc 90 do | Noi hai ong thanh goc vuong o dau ong - moi dau vat 45 do |
| Ghep goc 45 do | Noi hai ong thanh goc 45 do - moi dau vat 22,5 do |
| Ong nhanh chu T 90 do | Dau ong cat long yen ngua de om vuong goc vao GIUA than ong chinh |

Ghep hai ong thanh mot goc thi **moi dau chi can vat nua goc do**, roi up hai
mat vat vao nhau. Vi vay goc 90 do -> vat 45 do, goc 45 do -> vat 22,5 do.

Moi lan Them, nhap **chieu dai khuc ong** - phan mem tu dat nhat cat noi tiep
sau nhat truoc, cach nhau dung khoang khe da khai bao. Cat mot cay ong ra nhieu
khuc chi la them nhieu lan.

Toan bo cong thuc deu la **hinh hoc chinh xac**, khong xap xi.
Chay `python kiem_tra/test_thu_vien.py` va `python kiem_tra/test_xep.py` de xem
toan bo phan kiem chung.

### Long yen ngua va cho gap goc o day yen

Khi ong nhanh va ong chinh **bang duong kinh nhau** thi day long yen ngua dung
la mot diem NHON that - cong thuc rut gon thanh `L = R*|cos(phi)|`, ma ham
`|cos|` gap goc tai 90 do. Do la hinh hoc dung chu khong phai loi ve. Ong chinh
cang to hon thi duong cat cang tron:

| Ong nhanh | Ong chinh | Goc gap o suon yen |
|---|---|---|
| D60 | D60 | 90 do (nhon that) |
| D60 | D70 | 3,8 do |
| D60 | D90 | 2,0 do |
| D60 | D200 | 0,7 do |

**Nhung diem nhon do MAY KHONG CAT DUOC.** Tai day yen, truc X phai doi chieu
ngay lap tuc o het toc do cat - do duoc **+-0,4 mm moi buoc** voi ong D60 vao
D60. Dong co buoc khong dao chieu nhu vay duoc: no se TRUOT BUOC va vi tri sau
do sai het. Ma mo plasma co be rong mach cat huu han cung khong tao noi goc do.

Vi vay kieu *Ong nhanh chu T* co o **Bo tron day yen**, mac dinh 2 mm:

| Ban kinh bo tron | X doi chieu | Vat lieu de lai o day yen |
|---|---|---|
| 0 mm (cong thuc chinh xac) | +-0,3993 mm/buoc | 0 |
| 1 mm | +-0,0832 mm/buoc | 0,40 mm |
| **2 mm (mac dinh)** | **+-0,0403 mm/buoc** | **0,80 mm** |
| 3 mm | +-0,0267 mm/buoc | 1,23 mm |

Cach lam la **lan mot vien bi** ban kinh do doc theo day rooc (phep dong hinh
thai hoc): cho nao vien bi lot vao duoc thi giu nguyen, cho nao hep hon vien bi
thi thay bang chinh mat vien bi. Nho vay:

- Ket qua **luon >= duong cat goc** - chi de lai vat lieu chu khong cat lem them
- Vat lieu de lai (0,8 mm) **nho hon be rong mach cat plasma** (~1,2 mm) nen thuc
  te mat luon
- Cho nao von da tron hon vien bi thi khong bi dong toi: ong chinh D70 tro len
  chi lech duoi 0,001 mm

Dat ve **0** neu muon giu dung cong thuc toan hoc.

### "Phai la mot duong sin om tron ong kia chu?" - dung, nhung la kieu KHAC

Hai hinh nay de lan, ma chon nham la cat hong phoi:

| | Duong cat | Kieu trong thu vien |
|---|---|---|
| **Mot** duong sin | `X = r*cos(A)` - len xuong mot lan tron vong ong | `goc_90` (cut ni, cat vat) |
| Sin **chinh luu** | `X = r*|cos(A)|` - len xuong HAI lan, co goc gap | `nhanh_t_90` (nhanh chu T) |

Ca hai deu nam **dung tren mat ong chinh** - da do lai: `goc_90` khop
`r*cos(A)` sai so 1,4e-14 mm, `nhanh_t_90` khop `r*|cos(A)|` sai so 1,5e-13 mm.
Khac nhau o cho di toi dau thi dung:

- **Mot sin**: ong nhanh cham mat ong chinh o mot phia roi **di xuyen qua**, thut
  sau qua duong tam ong chinh 30 mm (voi D60). Do la hinh cua hai ong **cut ngang
  roi up vao nhau** - tuc la cai cut ni goc 90 do.
- **Sin chinh luu**: ong nhanh **dung tren** mat ong chinh, om lay no. Khong cho
  nao an sau qua mat ong. Do la nhanh chu T dam vao suon ong.

Neu muon "mot duong sin om tron ong" thi chon **`goc_90`** - no san trong thu
vien, va no khong co goc nhon nao nen cung khong can bo tron.

### The XEP 2D

Bam the *Xep tren cay ong* de nhin cay ong nam thang. Moi nhat cat la mot **hinh
chu nhat dai bang be ngang cua duong cat theo truc ong**.

- Keo hinh chu nhat de doi cho, lan chuot de phong to, **keo chuot giua** de day
  khung nhin qua lai
- **Diem goc** la dau ong xa mam kep nhat. Khoang cach cua mot nhat cat do tu
  diem goc toi **canh gan diem goc nhat** cua hinh chu nhat do
- Chon mot nhat cat thi o *Vi tri may* ben phai doi thanh o nhap khoang cach -
  go so vao la nhat cat nhay dung cho. Ra khoi the nay thi o do tro lai binh thuong
- **Ctrl+Z / Ctrl+Y** hoan tac va lam lai, **Ctrl+C / Ctrl+X / Ctrl+V** sao chep,
  cat va dan nhat cat (dung duoc o ca bang Edit lan the xep)

### Ba the o duoi

- **Edit** - danh sach cac mieng trong bai, va o G-code (sua tay duoc)
- **System** - terminal: xem va go thang lenh xuong may (lenh `$` cua FluidNC)
- **Alarm** - chi ghi LOI THAT SU; ghi chu thuong chay xuong terminal

### Tam dung va chay tiep

Bam *TAM DUNG* thi may **giam toc va dung lai**, dong thoi **tat mo cat** de
khong thung phoi, roi nho dung phan con lai cua bai.

Bam *CHAY TIEP* thi phan mem hoi truoc: **co can duc lo lai khong, va bao lau**.
Dung giua duong cat ma chay tiep luon thi mo cat chua xuyen qua thanh ong da
phai di chuyen, mach cat se bi dut doan. Chi tiet cach lam o muc 7.


## 3. Quy trinh van hanh

1. Cam USB, bam **Ket noi**. Phan mem tu gui duong kinh ong xuong may.
2. Nhap duong kinh ong o khung *Kich thuoc bai* (ben phai) roi bam
   **Ap dung duong kinh**.
3. Lam bai: chon kieu moi noi -> nhap so do -> **THEM VAO BAI**. Hoac
   **Mo .NC** de doc file tu phan mem CAM.
4. Xem lai o the *Xep tren cay ong* (keo tha cho vua cay ong) va *Mo phong 3D*.
5. Ke ong vao mam kep, nhich tay toi cho bat dau, bam **Dat goc 0 tai day**.
6. **CHAY**. Muon thu truoc thi bat **Chay thu** - phan mem bo het lenh bat mo.

Neu may bao dong (bam EMG, cham cong tac hanh trinh): bam **Mo khoa**.

## 4. Truc, don vi va tai sao truc xoay tinh bang MM

| Truc | Trong G-code cua nguoi dung | Gui xuong FluidNC |
|---|---|---|
| X | mm doc theo ong | mm |
| A | **do** quay | **mm cung tren mat ong** |

FluidNC coi moi truc la truc **thang** khi tinh toc do chay va gia toc. Neu de
truc A bang do thi lenh `F` o cac duong cat cheo se vo nghia - may lay
`sqrt(dX^2 + dA^2)` voi dA tinh bang do, ra mot con so khong co y nghia vat ly,
nen mo cat luc nhanh luc cham va mep cat khong deu.

De ca hai truc cung don vi mm thi `F` chinh la **toc do mo cat luot tren mat
ong**, dung deu tren moi duong cat du cheo bao nhieu. Vi vay:

- Ban G-code hien tren man hinh van giu don vi **do** cho de doc
- Ngay truoc khi gui, chu `A` duoc doi sang mm cung: `mm = do / 360 * pi * D`
- Truc A trong FluidNC dat `steps_per_mm = xung_moi_vong / (pi * D)`

**Doi duong kinh ong thi phan mem tu gui lai so xung**:
`$/axes/a/steps_per_mm=...`. Lenh nay chi doi cau hinh dang chay, khong ghi
vao flash, nen doi bao nhieu lan cung khong hai gi.

O menu *Parameters* co muc chon **file .NC nhap vao ghi truc A bang gi**: bang
do (phan lon phan mem CAM) hay bang mm cung (kieu "trai phang"). Bai lam tu thu
vien moi noi thi luon la do. Du chon kieu nao, phan mem cung tu doi sang mm
cung truoc khi gui.

## 5. Tap lenh G-code

FluidNC hieu tap lenh cua Grbl 1.1, rong hon firmware tu viet truoc day nhieu:
`G0 G1 G2 G3 G4 G10 G17-19 G20 G21 G28 G30 G38.x G43.1 G53 G54-59 G80 G90 G91
G92 G93 G94`, `M0 M1 M2 M3 M4 M5 M6 M7 M8 M9 M30 M62-M65`.

Dang ke voi may nay:

| Lenh | Y nghia |
|---|---|
| `M3` / `M5` | Bat / tat mo cat plasma |
| `G4 P<giay>` | Cho duc lo truoc khi cat |
| `G2` / `G3` | Cung tron THAT SU (firmware cu chi xap xi thanh duong thang) |
| `G53` | Toa do may tuyet doi |
| `$J=` | Nhich tay, huy duoc giua chung |

## 6. Chat luong mep cat - viec ma FluidNC lam ho

Bo lap ke hoach cua FluidNC nhin truoc nhieu doan va tu tinh toc do tai cho
noi giua hai doan theo **goc gap** giua chung (`junction_deviation_mm`):

- Hai doan gan thang hang -> chay lien mach, khong giam toc
- Cho gap goc -> tu giam toc truoc khi toi, doi chieu roi tang toc lai

Day la khac biet lon nhat so voi firmware tu viet truoc day, thu von co y
**khong** giam toc giua cac doan cat de mep cat lien mach - va vi vay dong co
bi truot buoc o day long yen ngua (xem muc *Long yen ngua*). Voi FluidNC,
o bo tron day yen tro thanh **tuy chon** chu khong con bat buoc.

## 7. Dung may - 3 muc do

| Muc | Bam gi | May lam gi |
|---|---|---|
| Tam dung | Nut **TAM DUNG** (hoac nut STOP tren ban tay) | Giam toc roi dung, **tat mo cat**, giu nguyen cho de chay tiep |
| Dung han | Nut **DUNG (Esc)** | Khoi dong lai bo dieu khien, bo ca bai |
| Khan cap | Nut EMG | Dung ngay khong giam toc, vao trang thai bao dong |

Sau EMG hoac cham cong tac hanh trinh, phai bam **Mo khoa** (`$X`) truoc khi
chay tiep duoc.

### Tam dung tren may plasma khac may phay

Mo plasma de chay tren ong DANG DUNG YEN thi chi vai giay la thung phoi. Vi vay
khi bam Tam dung, phan mem lam ba viec theo dung thu tu:

1. Gui `!` - may giam toc roi dung
2. Doi may bao da dung han
3. Gui `0x9E` - **tat mo cat** trong luc dang dung

Luc bam **CHAY TIEP**, neu chon co duc lo lai:

1. Gui `0x9E` lan nua - bat lai mo cat, ong **van dung yen**
2. Cho dung so giay da nhap - mo duc xuyen qua thanh ong
3. Gui `~` - bat dau chay

Neu khong chon duc lo thi chi gui `~`, FluidNC tu bat lai mo roi chay.

## 8. Bang dieu khien tay 7 nut

| Nut | Chan trong cau hinh | May lam gi |
|---|---|---|
| EMG | `estop_pin` | Dung ngay, vao bao dong |
| STOP | `feed_hold_pin` | Tam dung |
| START | `cycle_start_pin` | Chay tiep |
| X+ / X- / A+ / A- | `macro0_pin` .. `macro3_pin` | Moi lan bam nhich dung mot nac |

Do lon mot nac sua trong muc `macros:` cua file cau hinh, vi du:

```yaml
macros:
  macro0: $J=G91 G21 X5 F1000     # bam mot cai la ong ra 5mm
```

> **Khac firmware cu:** truoc day GIU nut la chay lien tuc. FluidNC chi bao khi
> nut duoc **bam**, khong bao luc nha ra, nen khong lam kieu do duoc. Nut NHICH
> cu thanh thua - de trong hoac dung cho viec khac.

### Cong tac hanh trinh

Hai cong tac o hai dau truc keo, khai bao o `limit_neg_pin` va `limit_pos_pin`
cua tung dong co keo. Cham vao la may dung ngay va bao dong `ALARM:1`. Bam
**Mo khoa** roi nhich nguoc ra.

Tren ESP32 goc, GPIO35 **khong co dien tro keo len ben trong** - phai lap dien
tro 10k len 3V3 o ben ngoai (vi vay trong file cau hinh chan do khong ghi
`:pu`). Tren ESP32-S3 thi moi chan trong so do deu co dien tro keo len.

## 9. So do chan

Xem thang trong hai file `fluidnc/*.yaml` - moi chan deu co ghi chu ben canh.
Doi chan thi sua file do roi tai lai vao may, hoac doi thang trong giao dien web.

Ban ESP32-S3 tranh cac chan sau (da ghi trong file):

| Chan | Ly do |
|---|---|
| 33-37 | PSRAM Octal cua ban N16R8 - dung la treo may (board VAN dua 35/36/37 ra header) |
| 26-32 | SPI flash, khong dua ra chan |
| 19/20 | USB gan trong |
| 43/44 | UART0 nap chuong trinh |
| 0/3/45/46 | Chan strapping |
| 48 | Den RGB tren board |

## 10. Gui bai xuong may

Phan mem dung cach **dem ky tu** - chuan cua moi bo gui G-code cho GRBL:

- Cu gui tiep chung nao tong so byte cua cac dong **chua duoc bao nhan** con
  duoi 127 (suc chua bo dem nhan cua may)
- Moi `ok` tra ve la bot di so byte cua dong cu nhat

Nho vay duong day luc nao cung day du lieu, bo lap ke hoach cua FluidNC luon co
san nhieu doan de nhin truoc va tinh gia toc. Do dai bai **khong bi gioi han**.

Trong luc chay, phan mem hoi `?` moi 0,2 giay de lay vi tri va trang thai; day
la ky tu thoi gian thuc nen khong chen vao hang doi lenh, khong lam gian doan
chuoi cat.

## 11. Mo plasma va chan bao hong quang

FluidNC co san loai mo `PlasmaSpindle`, hon han cach dong/ngat relay tran:

```yaml
PlasmaSpindle:
  enable_pin: gpio.19        # relay bat mo
  arc_ok_pin: NO_PIN         # ngo bao "hong quang da mo" cua nguon plasma
  arc_wait_ms: 1500          # doi bao lau cho hong quang mo
  off_on_alarm: true
```

Neu nguon plasma cua may co ngo bao hong quang, dau vao `arc_ok_pin`. Khi do:

- Bat mo ma hong quang khong mo duoc -> may khong chay, bao dong `ALARM:10`
- Hong quang tat giua chung duong cat -> may **dung ngay** thay vi keo mot
  duong khong cat gi

Chua dau thi de `NO_PIN` - may van chay, chi la khong biet hong quang co mo hay
khong.

## 12. Gioi han hien tai

- Muc *Nesting* moi chi sap xep cac mieng noi tiep nhau tren mot cay ong theo
  chieu dai, chua toi uu xoay quanh truc de tiet kiem vat lieu
- Bam giu nut di chuyen khong con chay lien tuc, chi nhich mot nac moi lan bam
  (xem muc 8)
- Bon den bao (san sang / dang chay / xong / loi) khong con: FluidNC khong co
  ngo ra bao trang thai kieu do

## 13. Chuyen sang FluidNC - da doi chieu nhung gi

Firmware tu viet truoc day (`main/main.c`, khoang 2400 dong) da duoc bo. No van
nam trong lich su kho, o commit `915126b` neu can xem lai.

File cau hinh may duoc **doi chieu tung ten muc voi ma nguon FluidNC**: rut het
cac ten dang ky trong `handler.item(...)`, `handler.section(...)`,
`InstanceBuilder<...>(...)`, `new ControlPin(..., "ten")` va `Macro { "ten" }`
ra (300 ten), roi kiem tra moi khoa trong hai file YAML deu co that. Cach nay
da bat duoc mot loi: muc mo plasma phai ghi la `PlasmaSpindle` chu khong phai
`Plasma`.

Bo gui G-code duoc kiem tra bang cach noi thang vao mot ban gia lap may FluidNC
qua cap cong ao. Ban gia lap bat chuoc dung theo ma nguon FluidNC
(`Channel.cpp`, `Report.cpp`, `RealtimeCmd.h`, `Protocol.cpp`) va **tu bao hong
neu bi gui qua tay bo dem 127 byte**. Cac bai da chay:

| Bai | Ket qua |
|---|---|
| Gui 600 dong, dai gap nhieu lan bo dem nhan | Khong tran bo dem, may bao nhan du tung dong |
| Doi truc A tu do sang mm cung | Dung tren ca ong D60 va D200, ke ca goc am |
| Tam dung | May dung va **tat mo cat** |
| Chay tiep co duc lo | Bat lai mo, cho du thoi gian, roi moi chay |
| Nhich tay truc X va truc A | Ong dich dung quang duong, truc A quy doi dung |
| Doc ban tin trang thai | Nhan ca `MPos` lan `WPos`, doc dung Idle/Run/Hold/Alarm |

### Chua chay thu duoc phan nao

Hai cho:

1. **Giao dien** moi chi kiem duoc den muc bien dich sach `-Wall -Wextra` khong
   mot canh bao va lien ket day du - may dung de phat trien la Linux, khong co
   Windows de mo cua so len.
2. **File cau hinh** moi kiem duoc la dung ten muc va khong trung chan; chua
   nap thu vao ESP32 that. Kho FluidNC co ban dung cho Linux de chay thu, nhung
   kho phan mem cua PlatformIO bi chan tu may nay nen khong dung duoc.

Nghia la lan dau nap len may that nen lam theo thu tu: nap cau hinh -> xem log
khoi dong co bao loi khong -> `$X` roi nhich tung truc mot -> do thuoc -> moi
cat that.
