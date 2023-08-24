#include <klibc/string.h>
#include <lunaix/spike.h>
#include <lunaix/tty/console.h>
#include <lunaix/tty/tty.h>
#include <stdint.h>

#include <sys/port_io.h>

vga_attribute* tty_vga_buffer;

vga_attribute tty_theme_color = VGA_COLOR_BLACK;

inline void
tty_clear()
{
    asm volatile("rep stosw" ::"D"(tty_vga_buffer),
                 "c"(TTY_HEIGHT * TTY_WIDTH),
                 "a"(tty_theme_color)
                 : "memory");
}

void
tty_init(void* vga_buf)
{
    tty_vga_buffer = (vga_attribute*)vga_buf;

    tty_clear();

    port_wrbyte(0x3D4, 0x0A);
    port_wrbyte(0x3D5, (port_rdbyte(0x3D5) & 0xC0) | 13);

    port_wrbyte(0x3D4, 0x0B);
    port_wrbyte(0x3D5, (port_rdbyte(0x3D5) & 0xE0) | 15);
}

void
tty_set_cursor(u8_t x, u8_t y)
{
    if (x >= TTY_WIDTH || y >= TTY_HEIGHT) {
        x = y = 0;
    }
    u32_t pos = y * TTY_WIDTH + x;
    port_wrbyte(0x3D4, 14);
    port_wrbyte(0x3D5, pos / 256);
    port_wrbyte(0x3D4, 15);
    port_wrbyte(0x3D5, pos % 256);
}

void
tty_set_theme(vga_attribute fg, vga_attribute bg)
{
    tty_theme_color = (bg << 4 | fg) << 8;
}

void
tty_flush_buffer(struct fifo_buf* buf)
{
    int x = 0, y = 0;

    // Clear screen
    tty_clear();

    char chr;
    int state = 0;
    int g[2] = { 0, 0 };
    vga_attribute current_theme = tty_theme_color;
    while (fifo_readone_async(buf, (u8_t*)&chr)) {
        if (state == 0 && chr == '\033') {
            state = 1;
        } else if (state == 1 && chr == '[') {
            state = 2;
        } else if (state > 1) {
            if ('0' <= chr && chr <= '9') {
                g[state - 2] = (chr - '0') + g[state - 2] * 10;
            } else if (chr == ';' && state == 2) {
                state = 3;
            } else {
                if (g[0] == 39 && g[1] == 49) {
                    current_theme = tty_theme_color;
                } else {
                    current_theme = (g[1] << 4 | g[0]) << 8;
                }
                g[0] = 0;
                g[1] = 0;
                state = 0;
            }
        } else {
            state = 0;
            switch (chr) {
                case '\t':
                    x += 4;
                    break;
                case '\n':
                    y++;
                    // fall through
                case '\r':
                    x = 0;
                    break;
                // case '\x08':
                //     *(tty_vga_buffer + x + y * TTY_WIDTH) =
                //       (current_theme | 0x20);
                //     break;
                default:
                    *(tty_vga_buffer + x + y * TTY_WIDTH) =
                      (current_theme | chr);
                    (x)++;
                    break;
            }

            if (x >= TTY_WIDTH) {
                x = 0;
                y++;
            }
            if (y >= TTY_HEIGHT) {
                y--;
                break;
            }
        }
    }
    tty_set_cursor(x, y);
}

void
tty_clear_line(int line_num)
{
    asm volatile("rep stosw" ::"D"(tty_vga_buffer + line_num * TTY_WIDTH),
                 "c"(TTY_WIDTH),
                 "a"(tty_theme_color)
                 : "memory");
}

void
tty_put_str_at(char* str, int x, int y)
{
    char c;
    while ((c = (*str)) && y < TTY_HEIGHT) {
        *(tty_vga_buffer + x + y * TTY_WIDTH) = c | tty_theme_color;
        x++;
        if (x >= TTY_WIDTH) {
            y++;
            x = 0;
        }
        str++;
    }
}