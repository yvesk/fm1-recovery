# FM-1 recovery with an ESP32-C3 and manual USB handoff

An experimental way to get a USB-silent, black-screen M-VAVE FM-1 into its chip's ROM USB recovery mode using an ESP32-C3 Super Mini. The ESP32 sends the JieLi USB_KEY, then the FM-1's USB cable is manually moved to a computer for backup and restoration.

**Status: one successful recovery reported.** The signal and a response were measured with a logic analyser; the owner reported a successful V15 restore. This is an experimental circuit and timing-dependent handoff, not an official M-VAVE method. It may not work on every unit. The FM-1 can contain irreplaceable songs, sounds and settings: preserve a complete flash backup before writing. A failed write, wrong firmware, short circuit or unsuitable power can make recovery harder or damage hardware. No success is guaranteed.

## What this does

1. The C3 repeatedly sends `0x16EF`, most significant bit first, with clock on D+ and data on D−.
2. It releases both pins between packets and looks for both lines low. After a low pulse lasting 850–2500 µs, it stops sending the key.
3. It sends open-drain low pulses on D+ at 1 ms intervals for 6 seconds, then releases the lines. GPIO8 is driven low during this phase; on some Super Mini boards the onboard LED is active-low, but LED colour and wiring differ between boards. Do not rely on a red power LED as an acknowledgement.
4. During that phase, the USB-A cable is moved from the C3 adapter to the computer. The C3 is no longer electrically connected to the FM-1's USB data lines after the move.
5. Once the computer detects `4c4a:8057`, a separate recovery utility backs up the FM-1 and restores a compatible firmware. This C3 program never accesses or writes flash on the FM-1.

The response is a heuristic. In our captured attempt, both lines were low for about 1.8 ms followed by the C3's pulses, and a later handoff was successful. Another attempt did not enumerate. The analyser was connected at the C3, not inside the FM-1, and this does not prove the circuit works with every unit. A bench test of an earlier timing routine measured 1.000 ms between pulses and about 5 µs low; check the actual signal if timing is uncertain.

## Parts

- ESP32-C3 Super Mini and a USB data cable to power/program it.
- USB-A female breakout or socket with individually accessible wires.
- USB-A male to USB-C male **data** cable, from the socket to the FM-1. For the handoff, move its USB-A plug to a computer USB-A port or a known-working USB-A adapter.
- Two 2 kΩ pull-ups, or four 1 kΩ resistors arranged as two series pairs.
- Breadboard and jumpers; a multimeter for identifying and checking contacts. A logic analyser is strongly recommended.
- A charged FM-1 battery. The adapter deliberately does not supply USB VBUS.
- A computer with a supported recovery tool; the successful session used Apple Silicon macOS and Melodee's `fm1-recover` 1.5.1.

## Wiring

With all power and USB cables disconnected, check the socket's contact-to-wire mapping with a multimeter. Colours vary: in the successful build the socket had **blue D+**, **white D−**, **black GND** and **red VBUS**; other sockets can differ. USB-A pins are 1 = VBUS, 2 = D−, 3 = D+, 4 = GND. Identify the pins from a reliable pinout for your socket orientation; do not infer them solely from colour.

```text
ESP32-C3 3V3 -- 1k -- 1k --+-- GPIO4 -- USB-A socket D+ (pin 3)
                           +------------ logic analyser D0
ESP32-C3 3V3 -- 1k -- 1k --+-- GPIO5 -- USB-A socket D- (pin 2)
                           +------------ logic analyser D1
ESP32-C3 GND ------------------ USB-A socket GND (pin 4)
             +---------------- logic analyser GND

USB-A socket VBUS (pin 1) ---- leave disconnected and insulate
```

Only connect the pull-ups to **3.3 V**, never 5 V. Confirm the data lines are not shorted together or to VBUS. The FM-1 must be powered by its own battery during the adapter phase. Do not connect the host USB port in parallel with the C3 pins or pull-ups; move the plug so the two are physically disconnected before connecting the host. Do not plug the adapter socket into the computer. Avoid contact between exposed conductors. Unplug the C3 before changing wiring. GPIO6 and GPIO7 are not used; remove any temporary test jumpers from them.

## Build and flash the C3

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html), then from this repository:

```sh
pio run
pio run -t upload --upload-port /dev/cu.usbmodemYOURPORT
```

Use the appropriate serial port for your system. Flashing the C3 overwrites its existing application and may overwrite other flash regions. Back up its flash first if its contents matter. The board definition targets an ESP32-C3; the exact Super Mini pin labels and LED arrangement may vary. The code uses direct ESP32-C3 GPIO registers and should not be assumed compatible with other ESP32 models.

After programming, with the FM-1 disconnected, check at the socket that GPIO4 outputs clock and GPIO5 outputs data. A logic analyser should show the sequence `0001011011101111` sampled on rising clock edges. Our test measured around 46 kHz. Verify that both lines are released high between packets. A digital capture does not replace voltage or continuity checks.

## Restore the FM-1

1. Charge the FM-1. Switch it **off**. Connect its USB-C port via the USB-A-to-C cable to the prepared C3 adapter socket. Connect the C3 to the computer for power. Never connect VBUS from the adapter to the FM-1.
2. Prepare a **verified, compatible** firmware package and the recovery utility before starting. For example, the [Melodee releases](https://github.com/keremimo/melodee/releases) include `fm1-recover` builds for macOS, Windows and Linux. Check the instructions and hashes for the version you download. Melodee's 1.5.1 tool accepted the unmodified official M-VAVE V15 package `FM-1_015` with SHA-256 `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`. Its offline `--check` rejected the V16 package in our session; do not assume that an arbitrary `.fwsc` is supported.
3. In a terminal start a **read-only** device search. On macOS USB access may require administrator rights:

   ```sh
   sudo ./fm1-recover --info --wait 60
   ```

   Do not share or enter your administrator password into a chat.
4. While the search waits, switch on the FM-1. After approximately two seconds, quickly unplug the USB-A **male cable** from the C3 socket and plug it into the computer. Keep the FM-1 switched on. Timing is experimental; the first attempt in our session failed and a second worked. The C3's power connection to the computer is separate from the moved FM-1 cable.
5. The expected result is USB `4c4a:8057` and an inquiry reporting `WL82 / UBOOT1.00`. macOS may display `WL80UBOOT1.00` as the USB product name. If the system asks to initialise or format a disk, cancel. If no device appears, stop and investigate rather than guessing flash addresses.
6. With the **compatible package already checked**, follow the recovery tool's documentation to save and verify a full flash backup. For `fm1-recover` 1.5.1, a package without `--write` loads a temporary helper into RAM and saves the backup, but does not write flash. Store a copy somewhere safe. The documentation says to power-cycle before using USB again after a backup-only session; for a device that needs USB_KEY, that means repeating the C3 entry and handoff before restoring.
7. Follow the tool's documented restore procedure. Its `--write` operation makes a fresh backup, writes the application region and verifies the complete flash. Keep power and USB stable; do not unplug or power off during a write. In our session the owner reported that V15 restoration succeeded. Check the tool's success report and then test boot, controls and audio.

Example syntax from the recovery tool, **only after reading its documentation**:

```sh
sudo ./fm1-recover /path/to/verified-v15.fwsc --check
sudo ./fm1-recover /path/to/verified-v15.fwsc --backup-dir /path/to/backups
# After the required power-cycle and new ROM-mode entry:
sudo ./fm1-recover /path/to/verified-v15.fwsc --backup-dir /path/to/backups --write --wait 60
```

Start the last command while the FM-1 is off and still connected to the adapter, then switch it on and hand off its cable while the command waits. The write starts automatically when the device is found. Do not proceed with an unverified package or without a retained backup. Do not erase the entire flash or write the boot region below `0x4000`; the documented recovery tool preserves the protected regions. Even when saved-data bytes are retained during flashing, stock firmware may change them after boot.

## Troubleshooting and limitations

- No response: check the battery, verified D+/D− pin mapping, ground, data cable, solder joints, pull-ups and C3 output at the socket. The key is only accepted during the chip's boot window. Reset the C3 and power-cycle the FM-1 for another attempt.
- Response but no USB device: the ROM may leave recovery when the pulses stop or host handoff takes too long. Try the handoff again; the timing is not proven reliable. See the protocol sources below.
- `Access denied` on macOS: the documented tool may require `sudo`. Grant access only to software you have verified and trust.
- The USB-A connector on the C3 is **only a handoff socket**, not a USB host. ESP32-C3's USB Serial/JTAG peripheral is not a general USB host.
- Recovery tools do not guarantee repair of hardware damage or damaged boot regions. If in doubt, preserve the backup and seek device-specific assistance.

## References and credit

- [JieLi USB_KEY research by kagaimiq](https://kagaimiq.github.io/jielie/isp/usb/usb-key.html): signalling, acknowledgement and host handoff.
- [FM-1 Transporter by kurogedelic](https://github.com/kurogedelic/FM-1-transporter): proven RP2040 recovery approach, protocol and calibration pulses.
- [MvaveFM1Unbricker by Quixotic7](https://github.com/Quixotic7/MvaveFM1Unbricker): recovery research and verified V15 hashes.
- [Melodee by keremimo](https://github.com/keremimo/melodee): macOS-capable recovery utility used in this session.
- [M-VAVE FM-1 downloads](https://www.m-vave.com/product?id=fm-1): manufacturer firmware and tools. Firmware packages are not included in this repository.
