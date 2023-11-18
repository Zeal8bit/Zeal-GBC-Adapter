## Gameboy (Color) cartridge adapter for Zeal 8-bit Computer

<center>
    <img src="pcb.jpg" alt="PCB final result" />
</center>

### This repository is part of [this Youtube video](https://www.youtube.com/watch?v=MxnD7FFjDEM), check it for more information and explanation about this project.

The goal of this project is to create an adapter for connecting any Gameboy or Gameboy Color cartridge to Zeal 8-bit Computer. This will offer the possibility to backup the ROM or RAM (saved data), restore the saved data or even modify the saved data in place.

This repository includes:

* An example program, written in C, to communicate with the adapter. It will read RAM banks from the cartridge and send them through UART. This has been tested with MBC1 and MBC5 cartridges.
* A Python script to run on a host that will receive the data from UART and save them inside a file
* The source code to flash to the PLD (GAL16V8)
* A Kicad (v6) project for the schematics and PCB board

**DISCLAIMER: To prevent any data loss, NEVER plug or unplug the cartridges from the adapter when it is connected to a computer/board that is powered on! I am not responsible for any potential damage caused to the Gameboy cartridge when using the adapter. Use it at your own risk.**

### Requirements

To compile this example you will need:

* python3
  * pyserial package (`pip3 install pyserial`)
* bash
* git (to clone this repo)
* make
* Zeal 8-bit OS source code. Only the `kernel_headers` directory is required, check this repository.
z88dk v2.2 (or later). Only its assembler, z80asm, is strictly required. The latest version of z80asm must be used as earlier versions don't have support for `MACRO`.
To install z88dk, please check out their GitHub project.

### Build

To build the program, define the path to Zeal 8-bit OS, this will let us find the header files used to assemble the example:

```
export ZOS_PATH=/your/path/to/Zeal8bitOS
```

Then simply use the command:

```
cd software
make
```

After compiling, the folder `bin/` in `software/` should contain the binary `dump.bin`. This file can be then loaded to Zeal 8-bit OS through UART thanks to the `load` command.

The binary can also be embedded within the romdisk that will contain both the OS and a read-only file system. For example:

```
cd $ZOS_PATH
export EXTRA_ROMDISK_FILES="/path/to/this/repo/software/bin/dump.bin"
make
```

More info about compiling [Zeal 8-bit OS here](https://github.com/Zeal8bit/Zeal-8-bit-OS#getting-started).

The resulting ROM image can then be provided to an emulator or directly flashed to the computer's ROM that will use it.

### Usage

* On the Zeal 8-bit Computer, execute the dump program from the command line:

    ```
    A:/> ./dump.bin
    ```
    The program should detect the cartridge name, the RAM banks count and their size. The output should look like:
    ```
    POKEMON BLUE
    Cartridge type: 0x3
    Cartridge RAM size: 32 KB
    Ready to send, start the dump script on the host computer
    ```

* **Close the serial monitor on the host computer!** Else, the python script below will not be able to receive characters from the UART.

* On the host, execute the python script, you can use `-h` to have more info about the parameters. For example, to save the file as `PKM.sav` from UART node `/dev/ttyUSB0` with a baudrate of 57600, the following command can be used:

    ```
    python3 dump.py -o PKM.sav -d /dev/ttyUSB0 -b57600 -v
    ```

    The output should look like this:

    ```
    Connecting to /dev/ttyUSB0 with baudrate 57600
    Dumping 4 banks of 8192 bytes, 32768 bytes in total...
    PKM.sav successfully dumped
    ```

## Troubleshooting

Upon execution of the binary on the Zeal 8-bit computer, you may encounter the `Get attr error` issue. This shows that the serial driver in the Zeal 8-bit OS kernel doesn't support setting attributes (raw) via `ioctl`. In that case, you should update your installation of the Zeal 8-bit OS to get the latest version of the serial driver.

## License

Distributed under the Creative Commons Zero v1.0 Universal License. See LICENSE file for more information.

You are free to use it for personal and commercial use, the boilerplate present in each file must not be removed.

The `zeal8bit.com` silkscreen present on the PCB shall be kept.

## Contact

For any suggestion or request, you can contact me at contact [at] zeal8bit [dot] com or on the Discord server https://discord.gg/UzEjwRvBBb