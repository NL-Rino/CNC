"""KET NOI ESP32 qua USB COM - mo cong, thuong luong toc do, nap dan chuong trinh.

KHONG dinh gi toi giao dien: moi thu gui ve bang mot ham goi lai (callback) duy
nhat, giao dien tu bo vao hang doi cua no. Nho vay module nay TEST duoc bang
script, khong can mo cua so.

Xung dong co la viec cua ESP32, may tinh khong dem va cung khong hoi. May tinh
chi biet VI TRI (mm / do) do ESP32 bao len.
"""

import threading
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:      # cho phep import module de test khi chua cai pyserial
    serial = None


# ESP32 LUON khoi dong o 115200 - toc do nao cung mo duoc, khong bao gio chet cong
BAUD_KHOI_DONG = 115200
# Thu nang dan tu cao xuong thap. Cho gioi han la chip USB-UART tren board
# (CP2102 / CH340), khong phai ESP32, nen phai THU chu khong dat cung.
BAUD_THU_DAN = [2000000, 1000000, 921600, 460800, 230400, 115200]

# Gui truoc bay nhieu dong roi bam CHAY ngay, vua chay vua nap tiep.
# Khop voi BUOC_DAY_TRUOC_KHI_CHAY trong firmware.
SO_DONG_NAP_TRUOC = 150
NGUONG_GUI_TIEP = 20        # chi gui tiep khi ESP32 con it nhat bay nhieu o trong
LO_GUI_TOI_DA = 250         # so dong toi da gui lien mot mach
CHO_TOI_DA_S = 60.0         # ESP32 khong voi bo dem qua lau -> coi nhu may da dung


def danh_sach_cong():
    if serial is None:
        return []
    return [c.device for c in serial.tools.list_ports.comports()]


class KetNoiESP32:
    """Mot duong day toi ESP32. Moi ham cong khai deu goi duoc tu luong giao dien."""

    def __init__(self, bao):
        """bao(loai, noi_dung) - ham goi lai, PHAI an toan khi goi tu luong nen.

        Cac loai:
          "esp32"    mot dong ESP32 gui len
          "nhat_ky"  thong bao cua chinh phan mem
          "loi_nap"  nap that bai, kem ly do
          "vi_tri"   (x_mm, a_do)
          "baud"     da chot toc do duong truyen
        """
        self.bao = bao
        self.ser = None
        self.dang_mo = False
        self.baud_dang_dung = BAUD_KHOI_DONG

        # --- Trang thai dieu tiet luu luong khi nap dan ---
        self.cho_trong = 0          # so o trong con lai trong vong dem ESP32
        self.so_dong_da_nhan = 0    # so dong ESP32 da bao nhan
        self.ket_qua_nap = None     # "OK" / "LOI"
        self.dang_nap = False       # dat False de huy nap giua chung
        self._pong = None

        self._luong_doc = None

    # ------------------------------------------------------------------
    # MO / DONG
    # ------------------------------------------------------------------
    def mo(self, cong, baud_chon=None):
        if serial is None:
            raise RuntimeError("Chua cai pyserial. Chay: pip install pyserial")
        # timeout ngan: vong doc goi readline() chan that su, ban tin ve la xu ly
        # ngay, con khi im lang thi 0,05s quay lai kiem tra co dung khong
        self.ser = serial.Serial(cong, BAUD_KHOI_DONG, timeout=0.05)
        time.sleep(2.0)          # ESP32 khoi dong lai khi cong Serial vua mo
        self.dang_mo = True
        self.baud_dang_dung = BAUD_KHOI_DONG
        self._luong_doc = threading.Thread(target=self._vong_doc, daemon=True)
        self._luong_doc.start()
        threading.Thread(target=self._thuong_luong_baud, args=(baud_chon,),
                         daemon=True).start()

    def dong(self):
        self.dang_mo = False
        self.dang_nap = False
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
        except Exception:
            pass

    def gui(self, lenh):
        if not (self.ser and self.ser.is_open):
            return False
        try:
            self.ser.write((lenh + "\n").encode())
            return True
        except Exception as loi:
            self.bao("nhat_ky", f"Loi gui lenh: {loi}")
            return False

    # ------------------------------------------------------------------
    # VONG DOC (luong nen)
    # ------------------------------------------------------------------
    def _vong_doc(self):
        while self.dang_mo and self.ser and self.ser.is_open:
            try:
                dong = self.ser.readline().decode(errors="ignore").strip()
            except Exception:
                break
            if not dong:
                continue
            self._xu_ly_dong(dong)

    def _xu_ly_dong(self, dong):
        # --- Bao nhan khi nap dan: "OK;<cho_trong>;<so_dong_da_nhan>" ---
        # ESP32 bao theo LO 8 dong cho do ton bang thong; so dong lay THANG tu
        # ban tin nen bao theo lo hay tung dong deu cho ket qua nhu nhau.
        if dong.startswith("OK;") or dong.startswith("BUF;"):
            phan = dong.split(";")
            try:
                self.cho_trong = int(phan[1])
                self.so_dong_da_nhan = int(phan[2])
            except (IndexError, ValueError):
                pass
            return                      # khong lam ngap khung nhat ky
        if dong.startswith("OK_BEGIN;"):
            try:
                self.cho_trong = int(dong.split(";")[1])
            except (IndexError, ValueError):
                pass
            return
        if dong.startswith("PONG;"):
            try:
                self._pong = int(dong.split(";")[1])
            except (IndexError, ValueError):
                pass
            return
        if dong.startswith("OK_NAP"):
            self.ket_qua_nap = "OK"
            try:                        # "OK_NAP;<so_dong>;<so_buoc>: ..."
                self.so_dong_da_nhan = int(dong.split(";")[1])
            except (IndexError, ValueError):
                pass
        elif dong.startswith("LOI_NAP"):
            self.ket_qua_nap = "LOI"

        if dong.startswith("Vi tri:"):
            vi_tri = self._doc_vi_tri(dong)
            if vi_tri:
                self.bao("vi_tri", vi_tri)

        self.bao("esp32", dong)

    @staticmethod
    def _doc_vi_tri(dong):
        """Doc "... X=12.34 A=56.78 ..." -> (12.34, 56.78)"""
        x = a = None
        for tu in dong.replace(",", " ").split():
            if tu.startswith("X="):
                try:
                    x = float(tu[2:])
                except ValueError:
                    pass
            elif tu.startswith("A="):
                try:
                    a = float(tu[2:])
                except ValueError:
                    pass
        return (x, a) if x is not None and a is not None else None

    # ------------------------------------------------------------------
    # THUONG LUONG TOC DO DUONG COM
    # ------------------------------------------------------------------
    def _thuong_luong_baud(self, baud_chon):
        """Nang baud len muc cao nhat ma may THUC SU chay duoc.

        An toan tuyet doi - khong bao gio mat lien lac:
          1. Gui BAUD;<n>, ESP32 tra OK_BAUD roi doi toc do
          2. May tinh cung doi, gui PING
          3. Co PONG  -> giu toc do nay
             Khong co -> may tinh ve 115200; ESP32 CUNG tu ve 115200 sau 4 giay
                         (luoi an toan nam trong firmware), roi thu muc thap hon
        """
        danh_sach = [baud_chon] if baud_chon else BAUD_THU_DAN
        for baud in danh_sach:
            if not self.dang_mo:
                return
            if baud == BAUD_KHOI_DONG:
                break
            if self._thu_mot_baud(baud):
                self.baud_dang_dung = baud
                self.bao("baud", baud)
                self.bao("nhat_ky", f"Da nang toc do duong COM len {baud} baud "
                                    f"({baud // BAUD_KHOI_DONG}x nhanh hon truoc).")
                break
            self.bao("nhat_ky", f"{baud} baud khong on dinh, thu muc thap hon...")
            time.sleep(4.5)      # cho ESP32 tu ve 115200 roi moi thu tiep
        else:
            self.bao("baud", BAUD_KHOI_DONG)
            self.bao("nhat_ky", f"Giu nguyen {BAUD_KHOI_DONG} baud.")
        self.gui("CFG;GET")

    def _thu_mot_baud(self, baud):
        try:
            self._pong = None
            self.gui(f"BAUD;{baud}")
            self.ser.flush()
            time.sleep(0.25)                 # cho ESP32 tra loi va doi baud
            self.ser.baudrate = baud         # may tinh doi theo
            time.sleep(0.15)
            self.ser.reset_input_buffer()

            for _ in range(3):               # PING vai lan phong khi rot goi
                self._pong = None
                self.gui("PING")
                self.ser.flush()
                han = time.time() + 0.5
                while time.time() < han:
                    if self._pong == baud:
                        return True
                    time.sleep(0.01)
            self.ser.baudrate = BAUD_KHOI_DONG
            self.ser.reset_input_buffer()
            return False
        except Exception:
            try:
                self.ser.baudrate = BAUD_KHOI_DONG
            except Exception:
                pass
            return False

    # ------------------------------------------------------------------
    # NAP DAN (streaming)
    # ------------------------------------------------------------------
    def nap_va_chay(self, cac_dong_nen):
        """Chay o LUONG NEN. cac_dong_nen phai la ban DA NEN san."""
        threading.Thread(target=self._nap_nen, args=(cac_dong_nen,),
                         daemon=True).start()

    def huy_nap(self):
        self.dang_nap = False

    def _nap_nen(self, cac_dong):
        # Chan chan lan cuoi: dong rong xuong toi ESP32 se bi bo qua KHONG kem
        # bao nhan, lam lech so dem hai ben
        cac_dong = [d for d in cac_dong if d]
        tong = len(cac_dong)
        byte_tong = sum(len(d) + 1 for d in cac_dong)
        try:
            self.cho_trong = 0
            self.so_dong_da_nhan = 0
            self.ket_qua_nap = None
            self.dang_nap = True

            self.bao("nhat_ky",
                     f"Nap dan {self.baud_dang_dung} baud: gui truoc {SO_DONG_NAP_TRUOC} "
                     f"dong roi vua chay vua nap (tong {tong} dong, {byte_tong} byte).")
            self.gui("PROG;BEGIN")

            han = time.time() + 3.0
            while time.time() < han and self.cho_trong <= 0:
                time.sleep(0.001)
            if self.cho_trong <= 0:
                self.bao("loi_nap", "ESP32 khong tra loi PROG;BEGIN. Kiem tra lai ket noi.")
                return

            da_gui = 0
            da_bam_chay = False
            moc_bao_cao = 0
            moc_con_cho = time.time()

            while da_gui < tong:
                if self.ket_qua_nap == "LOI":
                    self.bao("loi_nap", "ESP32 tu choi chuong trinh (LOI_NAP). "
                                        "Xem tab Alarm de biet dong nao sai.")
                    return
                if not self.dang_nap:
                    self.bao("nhat_ky", "Da huy nap theo yeu cau.")
                    return

                # Con bao nhieu dong dang bay tren duong chua duoc bao nhan.
                # Mot dong co the sinh toi 2 buoc (vd M3 + G1) nen tru gap doi.
                chua_bao_nhan = da_gui - self.so_dong_da_nhan
                cho_thuc = self.cho_trong - chua_bao_nhan * 2

                if cho_thuc < NGUONG_GUI_TIEP:
                    # ESP32 chi bao cho trong khi tra loi mot dong. Neu may tinh
                    # ngung gui va cu ngoi doi thi khong bao gio biet bo dem da
                    # voi ra -> ket cung ca hai ben. Phai CHU DONG hoi bang BUF.
                    self.gui("BUF")
                    time.sleep(0.003)
                    if time.time() - moc_con_cho > CHO_TOI_DA_S:
                        self.bao("loi_nap",
                                 f"ESP32 khong voi bo dem sau {CHO_TOI_DA_S:.0f} giay "
                                 f"- may co the da dung. Da huy nap.")
                        return
                    continue
                moc_con_cho = time.time()

                lo = min(cho_thuc, LO_GUI_TOI_DA, tong - da_gui)
                goi = "".join(d + "\n" for d in cac_dong[da_gui:da_gui + lo])
                self.ser.write(goi.encode())
                da_gui += lo

                # --- Du buoc dem dau tien -> CHAY NGAY, khong cho nap het ---
                if not da_bam_chay and da_gui >= min(SO_DONG_NAP_TRUOC, tong):
                    han = time.time() + 5.0
                    while (time.time() < han and self.ket_qua_nap != "LOI" and
                           self.so_dong_da_nhan < min(SO_DONG_NAP_TRUOC, tong)):
                        time.sleep(0.001)
                    if self.ket_qua_nap == "LOI":
                        self.bao("loi_nap", "ESP32 tu choi chuong trinh (LOI_NAP).")
                        return
                    self.gui("RUN")
                    da_bam_chay = True
                    self.bao("nhat_ky", f"Da dem san {self.so_dong_da_nhan} dong - "
                                        f"BAT DAU CHAY, phan con lai nap tiep khi dang chay.")

                if da_gui - moc_bao_cao >= 400:
                    moc_bao_cao = da_gui
                    self.bao("nhat_ky", f"... da nap {da_gui}/{tong} dong")

            # PROG;END di SAU cac dong G-code tren cung duong truyen nen ESP32
            # chac chan xu ly no cuoi cung - khong can cho bao nhan tung dong
            self.gui("PROG;END")
            han = time.time() + 15.0
            while time.time() < han and self.ket_qua_nap is None:
                time.sleep(0.02)
            if self.ket_qua_nap == "LOI":
                self.bao("loi_nap", "ESP32 tu choi chuong trinh (LOI_NAP). "
                                    "Xem tab Alarm de biet dong nao sai.")
                return
            if self.ket_qua_nap is None:
                self.bao("loi_nap", "ESP32 khong xac nhan nap xong sau 15 giay.")
                return
            if self.so_dong_da_nhan != tong:
                self.bao("nhat_ky", f"Canh bao: da gui {tong} dong nhung ESP32 bao "
                                    f"nhan {self.so_dong_da_nhan} dong.")

            if not da_bam_chay:       # bai qua ngan: nap xong het roi moi chay
                self.gui("RUN")
                self.bao("nhat_ky", "Da nap xong ca bai, bat dau CHAY.")
            else:
                self.bao("nhat_ky", f"Da nap xong toan bo {tong} dong, may dang chay tiep.")
        except Exception as loi:
            self.bao("loi_nap", f"Loi khi gui du lieu: {loi}")
        finally:
            self.dang_nap = False
