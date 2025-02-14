#include <stdint.h>
#include "stdio.h"
#include "memory.h"
#include <hal/hal.h>
#include <arch/i686/irq.h>
#include <debug.h>
#include <boot/bootparams.h>
#include <hal/io.h>

#define MAX_INPUT_LENGTH 100
char input_buffer[MAX_INPUT_LENGTH];
int input_index = 0;

extern void set_cursor_position(int x, int y);

void get_cursor_position(int *x, int *y) {
    uint16_t pos = 0;
    
    outb(0x3D4, 0x0E);
    pos = inb(0x3D5) << 8;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);

    *x = pos % 80;
    *y = pos / 80;
}

extern void _init();

void crash_me();

void remap_pic()
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}

void unmask_irq1()
{
    uint8_t mask = inb(0x21) & ~(1 << 1);
    outb(0x21, mask);
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return (unsigned char)(*str1) - (unsigned char)(*str2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (n == (size_t)-1) ? 0 : (*(unsigned char*)s1 - *(unsigned char*)s2);
}

void clear_screen() {
    set_cursor_position(0, 0);
    for (int i = 0; i < 25; i++) {
        for (int j = 0; j < 80; j++) {
            put_char(' ');  // fill screen with spaces to empty it
        }
    }
    set_cursor_position(0, -1);  // reset cursor position
}

void process_command(char* command) {
    if (strcmp(command, "crash") == 0) {
        log_crit("Main", "User has crashed the system!");
        crash_me();
    } else if (strcmp(command, "clear") == 0) {
        clear_screen();
    } else if (strncmp(command, "echo", 4) == 0) {
        if (command[4] == ' ') {
            printf("%s\n", command + 5);
        } else {
            printf("\n");
        }
    } else if (strcmp(command, "exit") == 0) {
        log_info("Main", "Shutting down system...");
        clear_screen();
        printf("System has shut down");
        while (1) {
            __asm__ volatile("hlt");
        }
    }
}

void keyboard_handler(Registers* regs)
{
    uint8_t scancode = inb(0x60);
    log_debug("Keyboard", "IRQ1 fired, Scancode: 0x%x", scancode);
    outb(0x20, 0x20); // acknowledge PIC

    const char scancode_to_ascii[128] = {
        0,  0,  '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t', 'q','w','e','r','t','y','u','i','o','p','[',']', '\n',
        0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
        0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
        '*', 0,  ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
    };

    if (scancode < 128 && scancode_to_ascii[scancode] != 0) {
        char c = scancode_to_ascii[scancode];
        if (c == '\b' && input_index > 0) {
            // backspace
            input_index--;
            put_char(c);
        } else if (c == '\n') { // enter
            input_buffer[input_index] = '\0';  // null terminate the string
            process_command(input_buffer);
            input_index = 0;
            put_char('\n');  // print newline after command
        } else if (c != 0) {
            // store character in buffer and print it
            if (input_index < MAX_INPUT_LENGTH - 1) {
                input_buffer[input_index++] = c;
                put_char(c);
            }
        }
    }
}

// 
// Startup
//

void start(BootParams* bootParams)
{   
    _init();
    HAL_Initialize();
    remap_pic();
    unmask_irq1();
    __asm__ volatile("sti");

    log_debug("Main", "Boot device: %x", bootParams->BootDevice);
    log_debug("Main", "Memory region count: %d", bootParams->Memory.RegionCount);
    for (int i = 0; i < bootParams->Memory.RegionCount; i++) 
    {
        log_debug("Main", "MEM: start=0x%llx length=0x%llx type=%x", 
            bootParams->Memory.Regions[i].Begin,
            bootParams->Memory.Regions[i].Length,
            bootParams->Memory.Regions[i].Type);
    }

    log_info("Main", "Test info msg!");
    log_warn("Main", "Test warning msg!");
    log_err("Main", "Test error msg!");
    log_crit("Main", "Test critical msg!");
    
    printf("PauyOS v1.0.0\n");
    printf("This is a test msg.\n");

    set_cursor_position(0, 3); // place cursor one line after welcome msg
    log_info("Main", "Set cursor position!");

    i686_IRQ_RegisterHandler(1, keyboard_handler);

    for (;;);
}