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

## 4. Truc va don vi

Theo chuan ISO 841 cho may cat ong:

- `X` = truc KEO ong doc theo chieu dai, don vi **mm**
  (hieu chuan mac dinh: 1 vong = 5mm, tuc 0.2 vong = 1mm)
- `A` = truc XOAY ong quanh truc X, don vi **do** (`Y` la alias cua `A`)
- `F` = toc do **RPM cua dong co**

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
| **EMG / LIMIT** (tu PLC) | Cat xung NGAY LAP TUC, khong giam toc | Dung han |
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

## 10. So do chan mac dinh (ESP32 devkit goc)

```
PUL_KEO_A = GPIO4    DIR_KEO_A = GPIO13
PUL_KEO_B = GPIO14   DIR_KEO_B = GPIO16
PUL_XOAY  = GPIO25   DIR_XOAY  = GPIO26
RELAY_PLASMA = GPIO19
PLC_OUT_READY/RUNNING/DONE/FAULT = GPIO17/18/21/22
PLC_IN_START/STOP/EMG/LIMIT      = GPIO23/27/32/33
```

Khong dung chan ENA cho ca 3 driver - dong co luon giu phanh, khong bao gio
nha phanh qua phan mem.

Toan bo chan tren co the doi bang phan mem cai dat ma **khong can build lai
firmware** - cau hinh duoc luu trong NVS (flash noi bo), khong mat khi mat dien.
Doi chan GPIO thi phai bam *Luu vao flash* roi *Khoi dong lai ESP32* moi co
hieu luc; doi hieu chuan va dao chieu thi co hieu luc ngay.

## 11. Gioi han hien tai

- Chuong trinh toi da **300 buoc** (`MAX_BUOC_CHUONG_TRINH` trong `main.c`).
  Phan mem van hanh se canh bao truoc neu file vuot qua gioi han nay
- `G2/G3` chua noi suy cung tron that su, dang duoc xap xi thanh duong thang
  toi diem cuoi (phan mem se in canh bao khi gap)
