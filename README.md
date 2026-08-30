# May cat ong plasma CNC - ESP32

Bo dieu khien may cat ong plasma 3 truc (2 dong co keo ong dong bo + 1 dong co
xoay ong), dieu khien bang G-CODE CHUAN qua cong USB COM.

## Thanh phan

| File | Cong dung |
|---|---|
| `main/main.c` | Firmware ESP-IDF cho ESP32 (bo dieu khien) |
| `gcode_gui_control.pyw` | Phan mem VAN HANH hang ngay tren may tinh |
| `cnc_settings.pyw` | Phan mem CAI DAT nang cao (chan GPIO, hieu chuan) |
| `build_exe.bat` | Dong goi 2 phan mem tren thanh file `.exe` chay doc lap |

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

Cach 1 - chay truc tiep bang Python (can cai Python + `pip install pyserial`):

```
python gcode_gui_control.pyw     (van hanh)
python cnc_settings.pyw          (cai dat)
```

Cach 2 - dong goi thanh `.exe` chay doc lap (may khac khong can cai Python):

Nhay doi chuot vao `build_exe.bat`. Sau 1-2 phut se co 2 file trong thu muc `dist`:

- `dist\MayCatOng.exe` - phan mem van hanh hang ngay
- `dist\MayCatOng_CaiDat.exe` - phan mem cai dat nang cao

Copy 2 file nay di dau cung chay duoc.

> Windows Defender co the canh bao file `.exe` moi tao. Day la canh bao chung
> cho moi file dong goi bang PyInstaller, chon "More info" -> "Run anyway".

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
- Lenh `POS` in ca mm/do lan **so xung tho** de doi chieu khi nghi ngo truot buoc

Phan mem van hanh tu hoi `POS` moi 2 giay khi may dang ranh, hien so xung ngay
canh o vi tri. Neu so xung dung ma phoi lai lech thi la dong co dang TRUOT BUOC
(thieu mo-men / dong dat qua thap), khong phai loi phan mem.

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

## 12. Gioi han hien tai

- Chuong trinh toi da **300 buoc** (`MAX_BUOC_CHUONG_TRINH` trong `main.c`).
  Phan mem van hanh se canh bao truoc neu file vuot qua gioi han nay
- `G2/G3` chua noi suy cung tron that su, dang duoc xap xi thanh duong thang
  toi diem cuoi (phan mem se in canh bao khi gap)
