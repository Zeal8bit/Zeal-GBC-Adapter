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


int main (void)
{
    /* Open the serial driver to send the data to */
    uart_dev = open("#SER0", O_WRONLY);
    if (uart_dev < 0) {
        printf("Error opening serial driver, exiting...\n");
        exit();
    }

    /* Start by mapping the fixed part of the ROM. It contains info about the cartridge itself.
     * (size, MBC, ram size, etc...) */
    map_cart_phys(0);

    /* Offset 0x134 of the cartridge should contain the name of the game, only print 15 characters */
    const char* name = cart_virt + 0x134;
    uint16_t size = 15;
    write(DEV_STDOUT, name, &size);

    /* Get the cartridge type, located at offset 0x147 of the ROM. */
    uint8_t bank_num;
    uint16_t bank_size;
    const uint8_t* type_ptr = cart_virt + 0x147;
    uint8_t cart_type = *type_ptr;

    /* Previous "write" didn't output a newline, output it here before the string */
    printf("\nCartridge type: 0x%hx\n", cart_type);
    switch (cart_type) {
        case 0x3: /* MBC1 + RAM + BATT */
        case 0x10: /* ROM + RAM + BATT */
        case 0x13: /* MBC3 + RAM + BATT */
        case 0x1B: /* MBC5 + RAM + BATT */
        case 0x1E: /* MBC5 + RUMBLE + RAM + BATT */
            bank_size = GB_SRAM_BANK_SIZE;
            /* Get the cartridge RAM size, located at offset 0x149 of the ROM. Needs a little conversion. */
            const uint8_t* size_ptr = cart_virt + 0x149;
            size = 0; // in KB
            switch (*size_ptr) {
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
                    break;
            }
            bank_num = size >> 3;
            printf("Cartridge RAM size: %d KB\n", size);
            break;
        case 0x6: /* MBC2 + RAM + BATT */
            bank_size = 512;
            bank_num = 1;
            printf("Cartridge RAM size: %d B\n", bank_size);
            break;
        default:
            printf("Unsupported cart type, exiting...\n");
            exit();
    }

    /* Enable the RAM: the first 8KB of the cartridge can be used to enable the cartridge RAM by writing 0xA to it */
    cart_virt[0] = 0xA;

    /* Enable RAM banking from "Banking Mode Select" register.
     * This register can be written to when writing cartridge's address 6000–7FFF. Let's map this area to the virtual page.
     *
     * NOTE: We can only map physical pages multiple of 16KB! The solution is to map 0x4000 and write at address 0x2000.
     */
    if (cart_type == 0x3) { /* MBC1 Only */
        map_cart_phys(0x4000);
        /* We need to write 1 to it to enable RAM banking (0 disables banking) */
        cart_virt[0x2000] = 1;
    }

    /* Finally, let's use our own function to map the cartridge RAM. Let's hardcode the number of banks for the moment */
    for (uint8_t bank = 0; bank < bank_num; bank++) {
        map_cart_sram(bank);
        /* The SRAM 8KB bank is now mapped at GB_CART_VIRT_ADDR still, so we can access it with `cart_virt` array,
         * send the content to the UART. */
        size = bank_size;
        write(uart_dev, cart_virt, &size);
    }

    /* Finished using the UART, close it */
    close(uart_dev);

    /* Disable the cartridge RAM */
    map_cart_phys(0);
    cart_virt[0] = 0;

    return 0;
}
