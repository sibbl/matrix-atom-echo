# macOS USB Development

## 1. Install ESP-IDF

```bash
brew install cmake ninja dfu-util python3
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd ~/esp/esp-idf
./install.sh esp32
. ./export.sh
```

If you are using an ESP32-S3 board instead:

```bash
./install.sh esp32s3
```

If ESP-IDF tooling reports `bad CPU type in executable` on Apple Silicon, install Rosetta:

```bash
/usr/sbin/softwareupdate --install-rosetta --agree-to-license
```

## 2. Configure your shell

Add this to `~/.zshrc` if desired:

```bash
export IDF_PATH="$HOME/esp/esp-idf"
alias get_idf='. "$IDF_PATH/export.sh"'
export ESP_TARGET="${ESP_TARGET:-esp32}"
```

Then for each new terminal:

```bash
get_idf
```

## 3. Find the USB serial port

Connect the ESP32 via USB and run:

```bash
ls /dev/cu.*
```

Then unplug and replug the board and compare the list.

Common examples:

- `/dev/cu.usbmodem1101`
- `/dev/cu.usbserial-0001`
- `/dev/cu.SLAB_USBtoUART`
- `/dev/cu.wchusbserial110`

Set the port in your shell:

```bash
export ESPPORT="/dev/cu.usbmodem1101"
```

## 4. Build, flash, and monitor

```bash
cd firmware
idf.py set-target "$ESP_TARGET"
idf.py -p "$ESPPORT" flash monitor
```

`idf.py flash` automatically builds before flashing, so a separate `idf.py build` is not required for the normal development loop.

Exit the monitor with `Ctrl + ]`.

## 5. Continuous flash loop

Normal development loop:

```bash
cd firmware
idf.py -p "$ESPPORT" flash monitor
```

For config changes:

```bash
idf.py menuconfig
idf.py -p "$ESPPORT" flash monitor
```

For a clean rebuild:

```bash
idf.py fullclean
idf.py -p "$ESPPORT" flash monitor
```

The repo also includes a convenience wrapper:

```bash
./scripts/dev-macos.sh
```

## 6. Troubleshooting

### Port not visible

Run:

```bash
ls /dev/cu.*
```

Then unplug and replug the device.

If the board uses a USB-UART chip, macOS may need a driver depending on the chip. Common chips include CP210x, CH34x, and FTDI.

### Failed to connect

Try:

```bash
idf.py -p "$ESPPORT" -b 115200 flash monitor
```

Or hold the board's `BOOT` button while the flash command starts.

If `esptool` reports a chip mismatch such as `This chip is ESP32, not ESP32-S3`, switch the target:

```bash
export ESP_TARGET="esp32"
idf.py set-target "$ESP_TARGET"
```

### Permission denied

On macOS this is less common than Linux, but another serial monitor may have the port open. Close other terminal monitors, Arduino IDE, PlatformIO, or serial console tools.

### Garbled monitor output

Start with:

```bash
idf.py -p "$ESPPORT" monitor
```

If logs are still garbled, inspect the serial settings and `menuconfig`.

### Machine-specific ports

Do not commit local `ESPPORT` values or hardcoded `/dev/cu.*` device names.
