@echo off
REM ===========================================================================
REM  DONG GOI PHAN MEM MAY CAT ONG THANH FILE .EXE CHAY DOC LAP
REM  ---------------------------------------------------------------------
REM  Sau khi chay xong se co 2 file trong thu muc "dist":
REM     dist\MayCatOng.exe        - phan mem VAN HANH hang ngay
REM     dist\MayCatOng_CaiDat.exe - phan mem CAI DAT nang cao (chan GPIO...)
REM
REM  Hai file nay COPY sang may khac chay duoc luon, KHONG can cai Python
REM  hay pyserial gi ca.
REM
REM  CACH DUNG: nhay doi chuot vao file nay (hoac go build_exe.bat trong cmd)
REM  Chi can chay tren MAY CO PYTHON. Lan dau chay se hoi tai PyInstaller.
REM ===========================================================================

echo.
echo ==========================================================
echo   DONG GOI PHAN MEM MAY CAT ONG PLASMA THANH FILE .EXE
echo ==========================================================
echo.

REM ----- Kiem tra Python -----
python --version >nul 2>&1
if errorlevel 1 (
    echo [LOI] Khong tim thay Python tren may nay.
    echo       Tai Python tai https://www.python.org/downloads/
    echo       Khi cai nho TICH vao o "Add Python to PATH".
    pause
    exit /b 1
)
echo [1/4] Python: OK
python --version

REM ----- Cai cac goi can thiet -----
echo.
echo [2/4] Kiem tra / cai cac goi can thiet (pyserial, pyinstaller)...
python -m pip install --upgrade pip --quiet
python -m pip install pyserial pyinstaller --quiet
if errorlevel 1 (
    echo [LOI] Khong cai duoc goi can thiet. Kiem tra ket noi mang.
    pause
    exit /b 1
)
echo       Xong.

REM ----- Dong goi file VAN HANH -----
echo.
echo [3/4] Dang dong goi phan mem VAN HANH (MayCatOng.exe)...
echo       (lan dau co the mat 1-2 phut, vui long doi)
python -m PyInstaller --onefile --windowed --noconfirm ^
    --name MayCatOng ^
    --hidden-import serial.tools.list_ports ^
    gcode_gui_control.pyw
if errorlevel 1 (
    echo [LOI] Dong goi that bai.
    pause
    exit /b 1
)

REM ----- Dong goi file CAI DAT -----
echo.
echo [4/4] Dang dong goi phan mem CAI DAT (MayCatOng_CaiDat.exe)...
python -m PyInstaller --onefile --windowed --noconfirm ^
    --name MayCatOng_CaiDat ^
    --hidden-import serial.tools.list_ports ^
    cnc_settings.pyw
if errorlevel 1 (
    echo [LOI] Dong goi that bai.
    pause
    exit /b 1
)

echo.
echo ==========================================================
echo   XONG! Hai file .exe nam trong thu muc "dist":
echo.
echo      dist\MayCatOng.exe          (van hanh hang ngay)
echo      dist\MayCatOng_CaiDat.exe   (cai dat nang cao)
echo.
echo   Copy 2 file nay di dau cung chay duoc, khong can Python.
echo ==========================================================
echo.
echo LUU Y: Windows Defender co the canh bao file .exe moi tao.
echo Day la canh bao chung cho moi file .exe tu PyInstaller,
echo chon "More info" -^> "Run anyway" de chay.
echo.
pause
