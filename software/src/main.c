/* SPDX-FileCopyrightText: 2023 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "zos_errors.h"
#include "zos_vfs.h"
#include "zos_sys.h"
#include "zos_serial.h"

/* If the standard output is the same serial driver as the one used to backup the cartridge,
 * we shall not output anything during the dump. After backing up, wait for a character before exiting. */
#define STDOUT_IS_SERIAL    1

/* Define different cartridges type */
#define MBC1_RAM_BATT       0x3
#define MBC2_RAM_BATT       0x6
#define ROM_RAM_BATT        0x10
#define MBC3_RAM_BATT       0x13
#define MBC5_RAM_BATT       0x1b
#define MBC5_RUMB_RAM_BATT  0x1e

/* Gameboy cartridge will be mapped at physical address 0x3f0000  */
#define GB_PHYS_ADDR            (0x3f0000)

/**
 * The first page is the kernel, we can't touch it, the second page is the current program,
 * the third page is available and the forth page is the stack for this current program.
 * Use the third page to map the GB cartridge data
 */
#define GB_CART_VIRT_ADDR       (0x8000)

/**
 * Each SRAM bank in the cartridge is 8KB big
 */
#define GB_SRAM_BANK_SIZE       (8*1024)

/**
 * Pointer to the cartridge virtual address
 */
uint8_t* cart_virt = (uint8_t*) GB_CART_VIRT_ADDR;

/**
 * Opened dev for the UART, needs to be closed before exiting!
 */
zos_dev_t uart_dev = 0;

/**
 * Attributes of the opened UART
 */
uint16_t uart_attr = 0;

/**
 * @brief Helper function to map cartridge given address into virtual page 3
 *
 * @param cart_addr Relative address of the cartridge to map, MUST BE A MULTIPLE OF 16KB!
 */
static void map_cart_phys(uint16_t cart_addr)
{
    zos_err_t err = map((void*) GB_CART_VIRT_ADDR, GB_PHYS_ADDR + cart_addr);
    if (err != ERR_SUCCESS) {
        printf("Error cartridge map\n");
        if (uart_dev) {
            close(uart_dev);
        }
        exit();
    }
}

/**
 * @brief Map the given cartridge SRAM bank into virtual page 3
 */
static void map_cart_sram(uint8_t bank)
{
    /* To switch the bank, write to cartridge address 0x4000 */
    map_cart_phys(0x4000);
    /* Only keep the lower 4 bits */
    cart_virt[0] = bank & 0xF;
    /* SRAM bank was mapped in the cartridge itself! Map the SRAM into the virtual page */
    map_cart_phys(0x8000);
}


/**
 * @brief Size of the cartridge RAM size, in KB
 *
 * @param size_value Size byte located in the cartridge ROM, at offset 0x149
 */
uint8_t cartridge_RAM_size(uint8_t size_value)
{
    uint8_t size = 0;
    switch (size_value) {
        case 2:
            size = 8;
            break;
        case 3:
            size = 32;
            break;
        case 4:
            size = 128;
            break;
        case 5:
            size = 64;
            break;
        default:
            size = 0;
            break;
    }
    return size;
}

static void wait_for_host(uint8_t bank_num, uint16_t bank_size)
{
    zos_err_t err;
    uint16_t size = 0;
    char msg[4] = { 0 };

    printf("Ready to send, start the dump script on the host computer\n");
    while (1) {
        /* Wait for a message from the host */
        size = 1;
        err = read(uart_dev, msg, &size);

        /* Make sure it is '!' */
        if (err != ERR_SUCCESS || msg[0] != '!') {
            printf("Invalid message from the host, please retry\n");
            continue;
        }

        /* Send the number of banks and the bank size */
        msg[0] = '=';
        msg[1] = bank_num;
        msg[2] = bank_size & 0xff;
        msg[3] = bank_size >> 8;
        size = 4;
        write(uart_dev, msg, &size);
        break;
    }
}

int main (void)
{
    zos_err_t err;

    /* Open the serial driver to send the data to */
    uart_dev = open("#SER0", O_WRONLY);
    if (uart_dev < 0) {
        printf("Error opening serial driver\n");
        exit();
    }

    /* Start by mapping the fixed part of the ROM. It contains info about the cartridge itself.
     * (size, MBC, ram size, etc...) */
    map_cart_phys(0);

    /* Offset 0x134 of the cartridge should contain the name of the game, only print 15 characters */
    const char* name = cart_virt + 0x134;
    uint16_t size = 15;
    write(DEV_STDOUT, name, &size);

    /* Determine the size and number of the RAM banks thanks to the cartridge type, located at offset
     * 0x147 of the ROM. */
    uint8_t bank_num = 0;
    uint16_t bank_size = GB_SRAM_BANK_SIZE;
    const uint8_t cart_type = cart_virt[0x147];

    /* Previous "write" didn't output a newline, output it here before the string */
    printf("\nCartridge type: 0x%hx\n", cart_type);
    switch (cart_type) {
        case MBC1_RAM_BATT:
        case ROM_RAM_BATT:
        case MBC3_RAM_BATT:
        case MBC5_RAM_BATT:
        case MBC5_RUMB_RAM_BATT:
            /* Cartridge RAM size pointer, located at offset 0x149 of the ROM. */
            size = cartridge_RAM_size(cart_virt[0x149]);
            bank_num = size >> 3;
            printf("Cartridge RAM size: %d KB\n", size);
            break;
        case MBC2_RAM_BATT:
            bank_size = 512;
            bank_num = 1;
            printf("Cartridge RAM size: %d B\n", bank_size);
            break;
        default:
            printf("Unsupported cart type, exiting...\n");
            goto err_close_exit;
    }

    /* Set the serial driver to RAW (to avoid \n to \r\n conversion) */
    err = ioctl(uart_dev, SERIAL_CMD_GET_ATTR, (void*) &uart_attr);
    if (err != ERR_SUCCESS) {
        printf("Get attr error %d\n", err);
        goto err_close_exit;
    }

    wait_for_host(bank_num, bank_size);

    if ((uart_attr & SERIAL_ATTR_MODE_RAW) == 0) {
        err = ioctl(uart_dev, SERIAL_CMD_SET_ATTR, (void*) (uart_attr | SERIAL_ATTR_MODE_RAW));
        if (err != ERR_SUCCESS) {
            printf("Set attr error %d\n", err);
            goto err_close_exit;
        }
    }

    /* Enable the RAM: the first 8KB of the cartridge can be used to enable the cartridge RAM by writing 0xA to it */
    cart_virt[0] = 0xA;

    /* Enable RAM banking from "Banking Mode Select" register.
     * This register can be written to when writing cartridge's address 6000–7FFF. Let's map this area to the virtual page.
     *
     * NOTE: We can only map physical pages multiple of 16KB! The solution is to map 0x4000 and write at address 0x2000.
     */
    if (cart_type == MBC1_RAM_BATT) {
        /* MBC1 Only */
        map_cart_phys(0x4000);
        /* We need to write 1 to it to enable RAM banking (0 disables banking) */
        cart_virt[0x2000] = 1;
    }

    /* Finally, let's use our own function to map the cartridge RAM. Let's hardcode the number of banks for the moment */
    for (uint8_t bank = 0; bank < bank_num; bank++) {
        /* In the case where #SER0 is the same driver as the STDOUT, we shall not write anything to STDOUT while backup is on-going */
#if !STDOUT_IS_SERIAL
        printf("Backing up bank %d...\n", bank);
#endif
        map_cart_sram(bank);
        /* The SRAM 8KB bank is now mapped at GB_CART_VIRT_ADDR still, so we can access it with `cart_virt` array,
         * send the content to the UART. */
        size = bank_size;
        err = write(uart_dev, cart_virt, &size);
        if (err != ERR_SUCCESS) {
            printf("Error %d, exiting\n", err);
            goto err_set_attr;
        }
    }

err_set_attr:
    /* Restore UART attributes before exiting */
    if ((uart_attr & SERIAL_ATTR_MODE_RAW) == 0) {
        ioctl(uart_dev, SERIAL_CMD_SET_ATTR, (void*) uart_attr);
    }

err_close_exit:
    /* Finished using the UART, close it */
    close(uart_dev);

    /* Disable the cartridge RAM */
    map_cart_phys(0);
    cart_virt[0] = 0;

    return 0;
}
