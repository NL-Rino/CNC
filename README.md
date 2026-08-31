# May cat ong plasma CNC - ESP32

Bo dieu khien may cat ong plasma 3 truc (2 dong co keo ong dong bo + 1 dong co
xoay ong), dieu khien bang G-CODE CHUAN qua cong USB COM.

## Thanh phan

Ca firmware lan phan mem may tinh deu viet bang **C**, nen doc mot chuong
trinh la hieu duoc chuong trinh kia - cung mot kieu dat ten, cung mot cach bao
loi, va bo doc so G-code o hai ben giong het nhau.

### Firmware (chay tren ESP32)

| File | Cong dung |
|---|---|
| `main/main.c` | Firmware ESP-IDF cho ESP32 (bo dieu khien) |

### Phan tinh toan (dung chung, khong dinh gi toi giao dien)

| File | Cong dung |
|---|---|
| `loi_c/thu_vien_moi_noi.c` | Toan hinh hoc sinh duong cat cho tung kieu ghep ong |
| `loi_c/phan_tich_gcode.c` | Doc / kiem tra / chuan hoa / nen G-code |
| `loi_c/ve_3d.c` | Mo phong 3D: ong quay va truot, dau cat dung yen |
| `loi_c/xep_2d.c` | The xep 2D: keo tha nhat cat doc cay ong, co thuoc do |
| `loi_c/ket_noi.c` | Duong day toi ESP32: thuong luong baud, nap dan |
| `loi_c/cong_com.c` | Mo / doc / ghi cong COM (Win32 va Linux) |
| `loi_c/nen_tang.c` | Thoi gian, luong nen, khoa (Win32 va POSIX) |
| `loi_c/hinh_ve.c` | Danh sach hinh: mat, hinh chu nhat, doan thang, chu |
| `loi_c/loi_chung.c` | Kieu du lieu chung va cach bao loi |

Hai module `ve_3d` va `xep_2d` **khong goi mot ham do hoa nao**. Chung chi dung
ra mot `KhungVe` gom cac hinh da sap xep san, lop giao dien chi viec ve lai.
Nho vay toan bo phep chieu va bo tri kiem tra duoc bang chuong trinh dong lenh,
va sau nay doi thu vien do hoa khong phai sua mot dong tinh toan nao.

### Giao dien (Windows)

| File | Cong dung |
|---|---|
| `win/may_cat_ong.c` | **Phan mem chinh** - thu vien moi noi, mo phong 3D, chay may |
| `win/cnc_settings.c` | Phan mem CAI DAT nang cao (chan GPIO, hieu chuan) |
| `win/gdi_ve.c` | Ve `KhungVe` len cua so bang GDI, co anh dem |
| `win/tien_ich.c` | Phong chu, mau, tao o dieu khien, hop thoai |

Chi dung **Win32 + GDI co san trong Windows** - khong mot thu vien ngoai nao,
khong runtime nao phai cai kem. Hai file `.exe` khoang 480 KB va 380 KB.

### Kiem tra

| File | Cong dung |
|---|---|
| `kiem_tra_c/test_thu_vien.c` | Kiem chung toan hoc cua thu vien moi noi |
| `kiem_tra_c/test_xep.c` | Kiem chung phan xep bai va do khoang cach |
| `kiem_tra_c/test_ve.c` | Kiem chung mo phong 3D va the xep 2D |
| `kiem_tra_c/test_ket_noi.c` | Nap dan that, noi thang vao firmware qua cong ao |
| `kiem_tra_c/gia_lap/` | Gia lap ESP32: chay DUNG code `main/main.c` tren may tinh |

`test_ket_noi` tao mot cap cong ao (pty), mot dau giao cho gia lap ESP32 lam
stdin/stdout, dau kia cho lop `ket_noi` mo y het mot cong COM that. Nho vay ca
duong "may tinh <-> ESP32" duoc chay that: nap dan, dieu tiet luu luong bang
`OK;<cho_trong>`, hoi `BUF`, bam RUN som khi da dem du buoc.

### Ban Python cu (giu lai lam du phong)

| File | Cong dung |
|---|---|
| `may_cat_ong.pyw`, `cnc_settings.pyw` | Hai chuong trinh ban cu |
| `loi/*.py` | Phan tinh toan ban cu |
| `kiem_tra/*.py` | Bai kiem tra ban cu |

Ban Python van chay duoc va van duoc kiem tra day du trong CI. No la moc de
doi chieu: **moi con so ban C tinh ra deu da duoc so voi ban Python**, xem muc
14.

## 1. Nap firmware cho ESP32

```
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p COM3 flash monitor
```

**BAT BUOC trong `menuconfig`:** bo chon
`Component config > ESP System Settings > Task Watchdog Timer > Watch CPU1 Idle Task`
(tuong ung `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n`).

Ly do: nhan 1 duoc danh RIENG cho viec xuat xung dong co. Vong xuat xung phai
chay lien tuc khong ngat quang, neu khong dong co se bi giat ~10ms mot lan lam
rung may va cat xau. Vi vay idle task cua nhan 1 khong bao gio duoc chay khi
dang cat, va Task Watchdog se bao loi neu van bat.

## 2. Chay phan mem tren may tinh

**Cach de nhat: tai file `.exe` san.** Vao tab **Actions** tren GitHub, chon
lan chay moi nhat, tai muc **Artifacts** - hoac vao thang muc **Releases**. Co
2 file:

- `MayCatOng.exe` - phan mem van hanh hang ngay
- `MayCatOng_CaiDat.exe` - phan mem cai dat nang cao

Copy 2 file nay di dau cung chay duoc, **khong can cai Python hay bat cu thu
gi khac** - phan mem viet bang C, chi dung nhung thu co san trong Windows.

> Windows Defender co the canh bao file `.exe` moi tao. Day la canh bao chung
> cho moi file exe chua ky so, chon "More info" -> "Run anyway".

**Tu dung lay** (can `make` va bo bien dich cheo mingw-w64):

```
make            # dung 2 file .exe vao thu muc ra/
make kiem-tra   # dung va chay toan bo bai kiem tra ngay tren may Linux
make sach       # xoa het file da dung
```

Tren Linux/macOS, `make kiem-tra` chay duoc het moi bai - ke ca bai nap dan
that vao firmware qua cong ao - vi phan tinh toan khong dinh gi toi Windows.
Chi phan giao dien moi can Windows de chay.

**Ban Python cu** van con, dung khi can sua nhanh ma khong co bo bien dich
(can Python + `pip install pyserial`):

```
python may_cat_ong.pyw           (phan mem chinh)
python cnc_settings.pyw          (cai dat phan cung)
```

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
| Mo .NC | Ve goc | Chay thu | Bat mo | CHAY | TAM DUNG | TIEP | DUNG   |
+----------------------------------------------------------------------+
| Edit (ve bai)  |  System (terminal)  |  Alarm (loi)                   |
+----------------------------------------------------------------------+
```

**Cong COM, toc do truyen va che do may** nam trong mot hop thoai duy nhat, mo
bang nut *Tham so...* tren thanh cong cu. Toc do truyen co dinh **115200 baud**.

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
- **System** - terminal: xem va go thang lenh xuong ESP32
- **Alarm** - chi ghi LOI THAT SU; ghi chu thuong chay xuong terminal

### Tam dung va chay tiep

Bam *TAM DUNG* thi ESP32 **giam toc va dung ngay tai cho**, dong thoi **tat mo
cat** de khong thung phoi, roi nho dung phan con lai cua doan dang cat.

Bam *CHAY TIEP* thi phan mem hoi truoc: **co can duc lo lai khong, va bao lau**.
Dung giua duong cat ma chay tiep luon thi mo cat chua xuyen qua thanh ong da
phai di chuyen, mach cat se bi dut doan. Chon "co" thi may gui `RESUME;<ms>`:
ESP32 bat mo, cho dung tung ay mili giay, roi moi cat tiep.


## 3. Quy trinh van hanh

1. Mo `MayCatOng.exe`, chon cong COM, bam **Ket noi**
2. **File > Mo file G-code** - phan mem tu doc truoc file, ve hinh duong cat
   (tab *Xem truoc duong cat* va *Mo phong 3D*) va bao loi neu file co van de
3. Dien **Toc do CAT** va **Toc do CHAY KHONG TAI**
4. Bam **JOG** dua mo cat toi diem bat dau, roi bam **DAT GOC 0 TAI DAY**
5. Bam **NAP & CHAY** - may nap chuong trinh xuong ESP32 truoc, doi ESP32 xac
   nhan nap thanh cong roi moi bat dau chay
6. Trong luc chay co the bam **PAUSE** / **RESUME**, hoac **STOP** (phim Esc)

## 4. Truc, don vi va 3 CHE DO LAM VIEC

Theo chuan ISO 841 cho may cat ong:
- `X` = truc KEO ong doc theo chieu dai (`F` = toc do)
- `A` = truc XOAY ong quanh truc X (`Y` la alias cua `A`)

Chon che do trong tab **Che do** cua phan mem cai dat:

| Che do | Truc X | Truc A | Y nghia cua F | Can duong kinh ong? |
|---|---|---|---|---|
| **1** | mm | **do** | vong/phut DONG CO | Khong |
| **2** | mm | **do** | **mm/phut MO CAT tren mat ong** | Co |
| **3** | mm | **mm cung** | **mm/phut MO CAT tren mat ong** | Co |

**Che do 1** la che do goc: thoi gian di chuyen lay theo truc CHAM NHAT. Don
gian, khong can biet ong, nhung toc do mo cat luot tren mat ong se THAY DOI tuy
duong cat nghieng nhieu hay it.

**Che do 2 va 3** giu **toc do mo cat luot tren mat ong KHONG DOI**. Cach lam:
goc xoay duoc quy doi ra chieu dai CUNG that tren mat ong

```
cung (mm) = goc (do) / 360 x 3.1416 x duong_kinh
```

roi lay quang duong THAT tren mat ong (canh huyen)

```
quang_duong = can_bac_hai( X^2 + cung^2 )
thoi_gian   = quang_duong / toc_do_mo_cat
```

Nho vay 2 truc luon phoi hop dung ty le va mo cat luot deu, du duong cat cheo
bao nhieu. Da do kiem: voi ong D60 va F2000, ca 3 kieu doan - keo doc thuan,
xoay tron thuan, va cat cheo - deu cho **cung mot toc do 2000 mm/phut**.

**Che do 3** them mot buoc: toa do truc A trong file cung nhap bang **mm cung**
(kieu "trai phang") thay vi do. Hop voi file CAM xuat ra dang trai phang.
Firmware tu doi mm cung sang do theo duong kinh da khai bao.

> Doi che do va duong kinh co hieu luc NGAY, khong can khoi dong lai ESP32.
> Nho bam *Luu vao flash* de giu lai sau khi mat dien.

## 5. Tap lenh G-code ho tro

```
G0/G1/G2/G3   di chuyen - 2 truc chay DONG THOI (noi suy Bresenham)
              G2/G3 hien duoc XAP XI thanh duong thang toi diem cuoi
G4 P..        nghi P giay (dung lam pierce delay sau khi bat mo cat)
G20 / G21     don vi inch / mm (G20 tu doi toa do truc thang x25.4)
G28 / G30     ve goc 0 ca 2 truc
G90 / G91     toa do tuyet doi / tuong doi
G92 X.. A..   dat lai toa do
M0 / M1       tam dung cho nguoi van hanh, gui RESUME de chay tiep
M3 / M4       BAT mo cat plasma
M5            TAT mo cat plasma
M2 / M30      ket thuc chuong trinh (TU DONG tat mo cat)
```

Cu phap chuan duoc ho tro day du: nhieu ma G/M tren 1 dong, che do modal
(dong chi co toa do), dong chi co F, chu thich `;` va `(...)`.

Cac ma `G17-19, G40-43, G49, G54-59, G61, G64, G80, G93/94, G98/99, M6-M9`
duoc chap nhan va bo qua (de tuong thich file xuat tu phan mem CAM).

## 6. Tang/giam toc - quan trong cho chat luong mep cat

- Doan **chay khong tai** (G0, G28/G30, JOG): co tang toc dan luc bat dau va
  giam toc truoc khi dung, de dong co khong rung / mat buoc
- Doan **CAT** (G1/G2/G3 khi mo cat dang bat): chay **dung toc do ngay tu xung
  dau tien**, khong tang toc dan - neu chay cham dan luc vao cat thi mep cat
  cho bat dau bi chay qua
- Giua cac doan CAT lien tiep: khong giam toc, khong in log ra UART, chay thang
  sang doan ke tiep => duong cat lien mach, khong co vet dung o diem doi huong

Neu dong co bi RU / MAT BUOC luc vao cat (thieu mo-men), bat lai tang toc bang
o chon trong tab *Hieu chuan* cua phan mem cai dat (lenh `CFG;RAMP;CAT;1`).

## 7. Dung may - 3 muc do

| Lenh | Cach dung | Chuong trinh |
|---|---|---|
| **EMG / LIMIT** | Cat xung NGAY LAP TUC, khong giam toc | Dung han |
| **STOP** (nut do / phim Esc) | Giam toc cuong buc ~150 xung roi dung | Xoa het |
| **PAUSE** | Giam toc cuong buc roi DUNG NGAY TAI CHO giua doan | Giu lai |

**PAUSE dung ngay tai cho, KHONG cho het buoc hien tai.** Phan doan con lai
duoc tra vao dau hang doi, bam RESUME la chay tiep dung cho vua dung, khong
mat xung nao. Khoang cach dung: 150 xung, o toc do cat khoang 0.5mm / 20-30ms.

Muon dung gap hon hoac em hon thi sua `SO_BUOC_DUNG_GAP` trong `main/main.c`
(nho cang de truot buoc, lon cang dung cham).

> Sau PAUSE mo cat da tat. Khi RESUME nho BAT LAI mo cat neu dang cat do dang.

## 8. Vi tri = dem xung nguyen

Firmware dem so xung da xuat cho tung truc bang so **nguyen co dau**, khong
cong don so thuc. Nho vay:

- Vi tri khong bao gio bi troi do sai so lam tron, du chay hang nghin doan
- Dung giua chung (PAUSE/STOP) van biet chinh xac dang o dau

Day la chuyen **noi bo cua ESP32**. May tinh khong dem xung va cung khong hoi:
no chi nhan lai VI TRI (mm / do) qua lenh `POS`, tu hoi moi 2 giay khi may dang
ranh. Khong hoi luc dang chay - moi lenh gui xuong deu lam ESP32 in ra UART, ma
in giua chuoi cat se chan vong xuat xung va tao vet dung tren duong cat.

## 9. An toan

Firmware TU DONG tat relay mo cat plasma khi: gap STOP, PAUSE, M0/M1, M2/M30,
hoac co tin hieu EMG/LIMIT tu PLC.

> Day chi la lop bao ve **muc phan mem**. KHONG thay the duoc relay an toan
> phan cung cat nguon dong luc truc tiep tren duong EMG that.

## 10. Bang dieu khien tay

| Nut | Tac dung |
|---|---|
| **X+ / X- / A+ / A-** | GIU la chay lien tuc o toc do da cai, NHA la dung |
| **NHICH** | GIU nut nay roi bam 1 trong 4 nut tren = nhich dung 1 nac (so mm / do da cai). Giu nguyen khong bi lap nac |
| **START** | Dang tam dung -> CHAY TIEP. Dang ranh -> chay chuong trinh da nap |
| **STOP** | Chi TAM DUNG, chuong trinh VAN GIU de bam START chay tiep. Muon huy han phai dung nut STOP tren phan mem may tinh |
| **EMG** | Dung khan cap - cat xung ngay lap tuc, cam moi cu dong |

### Cong tac hanh trinh

Co 2 cong tac cho 2 dau hanh trinh truc keo: **X-** va **X+**. Cham 1 trong 2
la chuong trinh dung ngay lap tuc.

**Van dieu khien tay duoc de LUI RA khoi cong tac**: cham dau X- thi bi cam di
tiep huong X- nhung VAN bam giu nut X+ de lui ra duoc (va nguoc lai). Truc xoay
A khong bi anh huong. Neu cam ca 2 huong thi cham cong tac xong se ket cung,
phai thao may ra moi go duoc.

> Rieng **EMG cam tuyet doi** moi cu dong, khong co ngoai le.

Nen dau ca 2 cong tac hanh trinh kieu **thuong dong** (tat o gat GND) de dut day
cung tu bao ve - xem muc "Bo chan va doi kieu tin hieu" ben duoi.

Cac nut di chuyen bi KHOA khi chuong trinh dang chay, tranh va cham.

Cai toc do giu nut va so mm/do moi nac trong tab *Hieu chuan* cua phan mem cai dat.

## 11. So do chan mac dinh (ESP32 devkit goc)

```
PUL_KEO_A = GPIO4    DIR_KEO_A = GPIO13
PUL_KEO_B = GPIO14   DIR_KEO_B = GPIO16
PUL_XOAY  = GPIO25   DIR_XOAY  = GPIO26
RELAY_PLASMA = GPIO19
EMG = GPIO32
LIMIT_X_AM    = GPIO33  (cong tac hanh trinh dau X-)
LIMIT_X_DUONG = GPIO35  <-- CAN DIEN TRO 10k LEN 3V3 BEN NGOAI

Bang dieu khien tay (nut noi GPIO xuong GND, keo len ben trong):
NUT_X_TIEN = GPIO23  NUT_X_LUI    = GPIO27
NUT_A_THUAN= GPIO17  NUT_A_NGHICH = GPIO18
NUT_START  = GPIO21  NUT_STOP     = GPIO22
NUT_NHICH  = GPIO34  <-- CAN DIEN TRO 10k LEN 3V3 BEN NGOAI

Den bao SAN_SANG/DANG_CHAY/XONG/LOI: MAC DINH TAT (-1) vi het chan
```

### Bo chan va doi kieu tin hieu

Trong phan mem cai dat:

- **Go `*` vao o so chan** de BO chan do (khong lap thiet bi nay). Chan bi bo se
  duoc hien thi lai bang dau `*`. Firmware bo qua han chan do, khong cau hinh
  GPIO va khong doc/ghi gi len no.
- **O gat "GND"** ben canh moi ngo VAO doi kieu tin hieu:

| O gat | Y nghia | Dung cho |
|---|---|---|
| **BAT** | Cap GND vao chan = kich hoat | Nut bam **thuong ho (NO)** |
| **TAT** | Mat GND moi la kich hoat | Tiep diem **thuong dong (NC)** |

Tat ca chan vao deu bat dien tro keo len ben trong, nen chi can dau nut xuong
GND, khong can nguon ngoai.

> **KHUYEN NGHI AN TOAN cho EMG va LIMIT: dung tiep diem THUONG DONG va TAT o
> gat GND.** Khi do binh thuong mach kin (co GND), bam nut khan cap hoac
> **DUT DAY** deu lam mat GND va may tu dung. Neu de kieu thuong ho, day tin
> hieu bi dut thi nut khan cap se KHONG con tac dung ma khong ai biet.
>
> Mac dinh firmware dat TAT CA ngo vao la "kich bang GND" (thuong ho) de giu
> nguyen hanh vi cu - hay tu doi EMG/LIMIT sang thuong dong sau khi dau day.

> **GPIO34 va GPIO35 khong co dien tro keo len ben trong.** Phai lap dien tro
> 10k tu moi chan do len 3V3, neu khong nut NHICH va cong tac hanh trinh X+ se
> bao "dang kich hoat" lung tung.

ESP32 devkit chi con dung 6 chan "sach" (co keo len, khong phai chan strapping)
la 17, 18, 21, 22, 23, 27 - vua du 6 nut. Nut thu 7 (NHICH) va cong tac hanh
trinh X+ phai dung GPIO34 va GPIO35 kem dien tro keo len ben ngoai.

### Neu doi sang ESP32-S3-WROOM-1 N16R8

Board nay thoai mai chan hon nhieu, du cho ca 4 den bao va **khong can dien tro
keo len ben ngoai**. Bam nut **"ESP32-S3 N16R8"** o thanh tren cung cua phan mem
cai dat de dien san so do chan goi y (chi dien vao o, xem lai roi tu bam gui).

> **CANH BAO quan trong cho ban N16R8:** chu `R8` nghia la co 8MB PSRAM kieu
> **Octal**, va PSRAM nay chiem **GPIO33 den GPIO37**. Board VAN dua GPIO35, 36,
> 37 ra header (tren so do in la `SPIID`, `SPIICLK`, `SPIDQS` - chinh la tin
> hieu PSRAM) nhung dung chung se lam **treo may hoac hong PSRAM**. Tuyet doi
> khong dau day vao 3 chan do.

Cac chan KHONG duoc dung tren board nay:

| Chan | Ly do |
|---|---|
| 33 - 37 | PSRAM Octal cua ban R8 (33/34 khong dua ra chan) |
| 26 - 32 | SPI flash (khong dua ra chan) |
| 19, 20 | USB gan trong (cong Type-C native) |
| 43, 44 | UART0 - cong nap chuong trinh va xem log |
| 0, 3, 45, 46 | Chan strapping - keo sai muc luc khoi dong la khong boot |
| 48 | Den RGB gan san tren board |

So do chan goi y cho ESP32-S3 N16R8:

```
PUL_KEO_A = 4     DIR_KEO_A = 5
PUL_KEO_B = 6     DIR_KEO_B = 7
PUL_XOAY  = 15    DIR_XOAY  = 16
RELAY_PLASMA = 17
EMG = 18          LIMIT_X_AM = 8    LIMIT_X_DUONG = 9
NUT_X_TIEN = 10   NUT_X_LUI    = 11
NUT_A_THUAN= 12   NUT_A_NGHICH = 13
NUT_START  = 14   NUT_STOP     = 21   NUT_NHICH = 1
DEN_SAN_SANG = 2  DEN_DANG_CHAY = 42  DEN_XONG = 41  DEN_LOI = 40
```

Van con trong: GPIO39, 47 va nhom JTAG neu khong dung (40, 41, 42 dang lam den
bao, co the doi cho khac neu can go loi bang JTAG).

Nho doi target khi build: `idf.py set-target esp32s3`
Khi doi sang **ESP32-S3** (45 chan thay vi 34) se thoai mai hon: bat lai 4 den
bao bang `CFG;PIN;DEN_LOI;<so>`... va dua nut NHICH ve chan co keo len.

Khong dung chan ENA cho ca 3 driver - dong co luon giu phanh, khong bao gio
nha phanh qua phan mem.

Toan bo chan tren co the doi bang phan mem cai dat ma **khong can build lai
firmware** - cau hinh duoc luu trong NVS (flash noi bo), khong mat khi mat dien.
Doi chan GPIO thi phai bam *Luu vao flash* roi *Khoi dong lai ESP32* moi co
hieu luc; doi hieu chuan va dao chieu thi co hieu luc ngay.

## 12. Nap dan (streaming) - chay file dai bao nhieu cung duoc

Truoc day ca chuong trinh phai nap HET vao RAM ESP32 roi moi bam chay duoc,
nen do dai file bi chan cung o 300 buoc va nguoi dung phai ngoi cho nap xong.

Bay gio may tinh **nap dan**:

1. Gui `PROG;BEGIN` - ESP32 tra `OK_BEGIN;<cho_trong>;<so_buoc_nap_truoc>`
2. Gui 150 dong dau (`BUOC_DAY_TRUOC_KHI_CHAY`)
3. Gui `RUN` - **may bat dau chay ngay**, khong cho nap het
4. Vua chay vua nap tiep cho toi het bai
5. Gui `PROG;END` - ESP32 tra `OK_NAP;<so_dong>;<so_buoc>`

**Dieu tiet luu luong.** ESP32 bao nhan bang `OK;<cho_trong>;<so_dong_da_nhan>`,
trong do `cho_trong` la so o con trong trong vong dem. May tinh chi gui tiep khi
con cho, nho vay vong dem **khong bao gio tran** (mat lenh). Khi may tinh phai
ngoi doi thi no chu dong hoi bang lenh `BUF` - neu khong hoi thi ca hai ben cung
ngoi cho nhau va ket cung.

Bao nhan di theo **lo 8 dong** (`NHIP_BAO_NHAN`) khi vong dem con rong rai, va
theo **tung dong** khi vong dem sap day. Ly do: bao nhan tung dong ton them
~14 byte moi dong tren duong COM; voi file CAM chia rat nho (0.1 mm/doan) chay
3000 mm/phut thi rieng phan bao nhan da an het bang thong va lam may tinh nap
khong kip.

**Chong can bo dem.** Neu may tinh nap khong kip, may se dung yen giua duong cat
trong khi mo plasma van bat -> **thung phoi**. Firmware xu ly:

- Mo cat DANG BAT ma het buoc de chay -> **tat mo ngay lap tuc**, tam dung, bao
  `LOI_CAN_BO_DEM`. Khong cho mot mili giay nao
- Chua cat -> cho toi da `THOI_GIAN_CHO_NAP_MS` (1 giay) roi moi tam dung

### Vat kiet bang thong duong COM

Duong COM la tai nguyen hiem nhat cua he thong. Bon viec da lam:

**1. Tu thuong luong toc do.** ESP32 **luon khoi dong o 115200** nen khong bao
gio "chet cong". Vua ket noi xong, phan mem thu nang dan tu 2000000 xuong, cai
nao chay duoc thi dung:

```
PC  -> BAUD;921600      ESP32 -> OK_BAUD;921600   (roi ca hai cung doi)
PC  -> PING             ESP32 -> PONG;921600      (xac nhan con lien lac)
```

Neu PING khong ve (chip USB-UART khong chiu noi, day nhieu), may tinh quay lai
115200 va thu muc thap hon. ESP32 cung **tu quay ve 115200 sau 4 giay** neu
khong ai xac nhan - luoi an toan trong firmware, khong the mat lien lac vinh vien.
Chon tay duoc trong o *Toc do* canh cong COM.

**2. Nen dong G-code truoc khi gui** (khong doi y nghia gi):
bo comment, bo dau cach, bo so 0 thua o duoi.
`G1 X10.500 A45.000 F30` -> `G1X10.5A45F30`. **Tiet kiem ~46% byte.**
Ban hien tren man hinh van giu nguyen dinh dang de nguoi doc - chi ban khi GUI moi nen.

**3. Vong doc serial ben may tinh doc CHAN.** Truoc day no poll `in_waiting`
roi `sleep(20ms)` - nhieu nhat 50 ban tin moi giay, thanh nut co that su khi
nang baud. Nay dung `readline()` chan (pyserial nha GIL nen khong ton CPU).

**4. ESP32 doc UART theo lo 256 byte** thay vi tung byte, bo dem RX 1 KB -> 4 KB.
O 2 Mbaud, doc tung byte la 200.000 lan goi driver moi giay.

### Do thuc te

Bai 1207 dong, doan cat 0,1 mm, chay 3000 mm/phut (gia lap dung baud, tinh ca
luu luong 2 chieu):

| Baud | Tu bam CHAY den luc dong co chay | Nap xong ca bai | Can bo dem |
|---|---|---|---|
| 115200 | 313 ms | 1,6 s | 1 lan |
| 460800 | 82 ms | 0,4 s | 0 lan |
| 921600 | **44 ms** | **0,2 s** | 0 lan |
| 2000000 | **24 ms** | **0,1 s** | 0 lan |

(Truoc khi lam nap dan: phai cho nap **het** file moi bam chay duoc, 300 buoc
mat 1836 ms - va bai 1207 dong thi khong nap noi.)

115200 con **cham hon toc do cat** voi file CAM chia nho: bai tren can 1,6 giay
de nap ma chi mat 2,5 giay de cat het - nen no cham bo dem 1 lan. Tu 460800 tro
len la du du.

## 13. Gioi han hien tai

- Vong dem giu **1200 buoc** (`SUC_CHUA_BUOC`, 67 KB RAM). Day chi la **do sau
  bo dem**, khong con la gioi han do dai chuong trinh - file dai bao nhieu cung
  chay duoc
- `G2/G3` chua noi suy cung tron that su, dang duoc xap xi thanh duong thang
  toi diem cuoi (phan mem se in canh bao khi gap)
- Muc *Nesting* moi chi sap xep cac mieng noi tiep nhau tren mot cay ong theo
  chieu dai, chua toi uu xoay quanh truc de tiet kiem vat lieu

## 14. Chuyen phan mem sang C - da doi chieu nhung gi

Phan mem may tinh truoc day viet bang Python/Tkinter, nay viet lai bang C cho
dong bo voi firmware. Cach lam de chac chan khong mat mat gi: **cho cung mot
dau vao chay qua ca hai ban roi so tung ky tu.**

| Doi chieu | Cach thu | Ket qua |
|---|---|---|
| Sinh G-code | Bai 3 mieng (goc_90 @250mm, goc_45 @180mm xoay 90 do, nhanh_t_90 @220mm, khe 8mm, chua dau 20mm, cay 1200mm) | **1437 dong giong het tung ky tu**, `tong_dung` = 758.4264 mm o ca hai ban |
| Doc / chuan hoa / nen G-code | 4 bai (bai 1437 dong tren, mot file `.nc` that, mot bai dai, mot file "hanh ha" gom viet lien khong dau cach, chu thuong, inch, toa do tuong doi, so dang `.5` / `5.` / `1E5`) x 3 che do x 2 duong kinh | **Giong het** ca ban chuan hoa, ban nen, tung doan duong di va tung canh bao |
| Mo phong 3D | 4 truong hop (chua chay, dang chay o 3 vi tri khac nhau) | **Giong het** tung toa do man hinh, tung mau, tung dong chu |
| The xep 2D | 4 truong hop (chua chon, chon tung nhat cat) | **Giong het** tung toa do, ke ca thuoc do va duong do khoang cach |
| Xep bai va do khoang cach | Chay `kiem_tra/test_xep.py` va ban C `kiem_tra_c/test_xep.c` | **Moi con so giong het** (ban C in them ten nhat cat o 3 dong) |
| Nap dan xuong ESP32 | 900 dong (dai hon vong dem cua ESP32) qua cong ao vao dung firmware that | Bam CHAY som dung luc, ESP32 bao nhan du 900 dong, khong loi |

Trong luc doi chieu bo doc G-code, tim ra mot cho ban C doc thieu: dong chi co
truc, viet cach ra nhu `A 135` (khong co chu G, an theo lenh G1 dong truoc) thi
ban C bo qua. Da sua cho khop voi ban Python - va do cung la cach may CAM that
hay xuat file.

### Nhung gi giu nguyen y het

Toan bo tinh nang deu duoc chuyen sang, khong bot cai nao: thu vien 3 kieu moi
noi va bo tron day yen ngua, the xep 2D keo tha co thuoc do, mo phong 3D quay
duoc va an bot duong cat, mo file `.NC`, 3 che do lam viec, Ctrl+Z/Y/C/X/V,
nap dan, tam dung va chay tiep co hoi duc lo, dieu khien tay, va ca chuong
trinh cai dat phan cung voi 21 chan GPIO cung 2 so do chan goi y san.

### Cho khac giua hai ban

- **Nhanh hon va nhe hon**: file `.exe` khoang 480 KB thay vi ~40 MB, mo len la
  chay ngay, khong cho khoi dong Python
- **Khong can cai gi**: khong Python, khong pyserial, khong runtime
- Duong COM va bo dem noi tiep viet thang bang API cua he dieu hanh
  (`CreateFile`/`ReadFile` tren Windows, `termios` tren Linux) thay vi pyserial
- Phan ve dung GDI voi anh dem nen keo xoay mo phong khong bi nhay hinh

### Chua chay thu duoc phan nao

Phan **giao dien** moi chi kiem duoc den muc bien dich sach `-Wall -Wextra`
khong mot canh bao va lien ket day du, vi may dung de phat trien la Linux
khong co Windows de mo cua so len. Phan tinh toan ben duoi thi nguoc lai -
chay that va da doi chieu tung con so nhu bang tren.
