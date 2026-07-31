static inline unsigned int get_ccount(void) {
    unsigned int ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=a"(ccount));
    return ccount;
}

// --- 32-Bit Global Uptime Engine ---
#define CYCLES_PER_US 40 
#define CYCLES_PER_SEC (CYCLES_PER_US * 1000000)

unsigned int global_uptime_sec = 0;
unsigned int cycle_accumulator = 0;
unsigned int last_ccount = 0;

void update_uptime() {
    unsigned int current = get_ccount();
    unsigned int delta;
    
    if (current < last_ccount) {
        delta = (0xFFFFFFFF - last_ccount) + current + 1;
    } else {
        delta = current - last_ccount;
    }
    last_ccount = current;
    
    cycle_accumulator += delta;
    
    // Convert directly to seconds to save CPU (No MS rendering)
    while (cycle_accumulator >= CYCLES_PER_SEC) {
        cycle_accumulator -= CYCLES_PER_SEC;
        global_uptime_sec++;
    }
}


void delay_us(unsigned int us) {
    unsigned int start = get_ccount();
    unsigned int cycles = us * CYCLES_PER_US; 
    while (get_ccount() - start < cycles) {
        update_uptime(); 
    }
}

#define UART0_BASE       0x3FF40000UL
#define UART0_FIFO_REG   ((volatile unsigned int *)(UART0_BASE + 0x00))
#define UART0_STATUS_REG ((volatile unsigned int *)(UART0_BASE + 0x1C))

char uart_getchar() {
    while ((*UART0_STATUS_REG & 0xFF) == 0) {
        update_uptime(); // Keep OS clock running while waiting for input
    }
    return (char)(*UART0_FIFO_REG);
}

int uart_has_char() {
    return ((*UART0_STATUS_REG & 0xFF) > 0);
}

void custom_itoa(unsigned int val, char* buf) {
    if (val == 0) {
        buf[0] = '0'; buf[1] = '\0';
        return;
    }
    int i = 0;
    unsigned int temp = val;
    while (temp > 0) {
        buf[i++] = (temp % 10) + '0';
        temp /= 10;
    }
    buf[i] = '\0';
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char t = buf[start];
        buf[start] = buf[end];
        buf[end] = t;
        start++;
        end--;
    }
}

#define GPIO_ENABLE_W1TS_REG (*(volatile unsigned int *)0x3FF44024)
#define GPIO_OUT_W1TS_REG    (*(volatile unsigned int *)0x3FF44008)
#define GPIO_OUT_W1TC_REG    (*(volatile unsigned int *)0x3FF4400C)

#define PIN_DC   2
#define PIN_RST  4
#define PIN_CS   5
#define PIN_CLK  18
#define PIN_MOSI 23

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410

void spi_init() {
    GPIO_ENABLE_W1TS_REG = (1 << PIN_CLK) | (1 << PIN_MOSI) | (1 << PIN_DC) | (1 << PIN_RST) | (1 << PIN_CS);
    GPIO_OUT_W1TS_REG = (1 << PIN_CS);
    GPIO_OUT_W1TC_REG = (1 << PIN_CLK);
}

void spi_send_byte(unsigned char data) {
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) {
            GPIO_OUT_W1TS_REG = (1 << PIN_MOSI);
        } else {
            GPIO_OUT_W1TC_REG = (1 << PIN_MOSI);
        }
        data <<= 1;
        GPIO_OUT_W1TS_REG = (1 << PIN_CLK);
        GPIO_OUT_W1TC_REG = (1 << PIN_CLK);
    }
}

void st7789_send_command(unsigned char cmd) {
    GPIO_OUT_W1TC_REG = (1 << PIN_CS);
    GPIO_OUT_W1TC_REG = (1 << PIN_DC);
    spi_send_byte(cmd);
    GPIO_OUT_W1TS_REG = (1 << PIN_CS);
}

void st7789_send_data(unsigned char data) {
    GPIO_OUT_W1TC_REG = (1 << PIN_CS);
    GPIO_OUT_W1TS_REG = (1 << PIN_DC);
    spi_send_byte(data);
    GPIO_OUT_W1TS_REG = (1 << PIN_CS);
}

void st7789_init() {
    GPIO_OUT_W1TC_REG = (1 << PIN_RST); 
    delay_us(50000); 
    GPIO_OUT_W1TS_REG = (1 << PIN_RST); 
    delay_us(150000); 
    st7789_send_command(0x01);
    delay_us(150000);
    st7789_send_command(0x11);
    delay_us(500000);          
    st7789_send_command(0x3A); 
    st7789_send_data(0x55);    
    st7789_send_command(0x36); 
    st7789_send_data(0x00);    
    st7789_send_command(0x20); 
    st7789_send_command(0x29); 
    delay_us(100000);
}

void st7789_set_window(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1) {
    st7789_send_command(0x2A); 
    st7789_send_data(x0 >> 8); st7789_send_data(x0 & 0xFF); 
    st7789_send_data(x1 >> 8); st7789_send_data(x1 & 0xFF); 
    st7789_send_command(0x2B); 
    st7789_send_data(y0 >> 8); st7789_send_data(y0 & 0xFF); 
    st7789_send_data(y1 >> 8); st7789_send_data(y1 & 0xFF); 
    st7789_send_command(0x2C);
}

void st7789_fill_screen(unsigned short color) {
    st7789_set_window(0, 0, 239, 319);
    GPIO_OUT_W1TC_REG = (1 << PIN_CS);
    GPIO_OUT_W1TS_REG = (1 << PIN_DC);
    unsigned char ch = color >> 8;
    unsigned char cl = color & 0xFF;
    for (unsigned int i = 0; i < (240 * 320); i++) {
        spi_send_byte(ch);
        spi_send_byte(cl);
    }
    GPIO_OUT_W1TS_REG = (1 << PIN_CS);
}

void st7789_draw_rect(unsigned short x, unsigned short y, unsigned short w, unsigned short h, unsigned short color) {
    st7789_set_window(x, y, x + w - 1, y + h - 1);
    GPIO_OUT_W1TC_REG = (1 << PIN_CS); 
    GPIO_OUT_W1TS_REG = (1 << PIN_DC); 
    unsigned char ch = color >> 8;
    unsigned char cl = color & 0xFF;
    for (unsigned int i = 0; i < (w * h); i++) {
        spi_send_byte(ch);
        spi_send_byte(cl);
    }
    GPIO_OUT_W1TS_REG = (1 << PIN_CS); 
}

const unsigned char font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00,0x00,0x00,0x00}, 
    {0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00}, {0x14,0x7F,0x14,0x7F,0x14,0x00,0x00,0x00}, 
    {0x24,0x2A,0x7F,0x2A,0x12,0x00,0x00,0x00}, {0x23,0x13,0x08,0x64,0x62,0x00,0x00,0x00}, 
    {0x36,0x49,0x55,0x22,0x50,0x00,0x00,0x00}, {0x00,0x05,0x03,0x00,0x00,0x00,0x00,0x00}, 
    {0x00,0x1C,0x22,0x41,0x00,0x00,0x00,0x00}, {0x00,0x41,0x22,0x1C,0x00,0x00,0x00,0x00}, 
    {0x14,0x08,0x3E,0x08,0x14,0x00,0x00,0x00}, {0x08,0x08,0x3E,0x08,0x08,0x00,0x00,0x00}, 
    {0x00,0x50,0x30,0x00,0x00,0x00,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00}, 
    {0x00,0x60,0x60,0x00,0x00,0x00,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02,0x00,0x00,0x00}, 
    {0x3E,0x51,0x49,0x45,0x3E,0x00,0x00,0x00}, {0x00,0x42,0x7F,0x40,0x00,0x00,0x00,0x00}, 
    {0x42,0x61,0x51,0x49,0x46,0x00,0x00,0x00}, {0x21,0x41,0x45,0x4B,0x31,0x00,0x00,0x00}, 
    {0x18,0x14,0x12,0x7F,0x10,0x00,0x00,0x00}, {0x27,0x45,0x45,0x45,0x39,0x00,0x00,0x00}, 
    {0x3C,0x4A,0x49,0x49,0x30,0x00,0x00,0x00}, {0x01,0x71,0x09,0x05,0x03,0x00,0x00,0x00}, 
    {0x36,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, {0x06,0x49,0x49,0x29,0x1E,0x00,0x00,0x00}, 
    {0x00,0x36,0x36,0x00,0x00,0x00,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00,0x00,0x00,0x00}, 
    {0x08,0x14,0x22,0x41,0x00,0x00,0x00,0x00}, {0x14,0x14,0x14,0x14,0x14,0x00,0x00,0x00}, 
    {0x00,0x41,0x22,0x14,0x08,0x00,0x00,0x00}, {0x02,0x01,0x51,0x09,0x06,0x00,0x00,0x00}, 
    {0x32,0x49,0x79,0x41,0x3E,0x00,0x00,0x00}, {0x7E,0x11,0x11,0x11,0x7E,0x00,0x00,0x00}, 
    {0x7F,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, {0x3E,0x41,0x41,0x41,0x22,0x00,0x00,0x00}, 
    {0x7F,0x41,0x41,0x22,0x1C,0x00,0x00,0x00}, {0x7F,0x49,0x49,0x49,0x41,0x00,0x00,0x00}, 
    {0x7F,0x09,0x09,0x09,0x01,0x00,0x00,0x00}, {0x3E,0x41,0x49,0x49,0x7A,0x00,0x00,0x00}, 
    {0x7F,0x08,0x08,0x08,0x7F,0x00,0x00,0x00}, {0x00,0x41,0x7F,0x41,0x00,0x00,0x00,0x00}, 
    {0x20,0x40,0x41,0x3F,0x01,0x00,0x00,0x00}, {0x7F,0x08,0x14,0x22,0x41,0x00,0x00,0x00}, 
    {0x7F,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, {0x7F,0x02,0x0C,0x02,0x7F,0x00,0x00,0x00}, 
    {0x7F,0x04,0x08,0x10,0x7F,0x00,0x00,0x00}, {0x3E,0x41,0x41,0x41,0x3E,0x00,0x00,0x00}, 
    {0x7F,0x09,0x09,0x09,0x06,0x00,0x00,0x00}, {0x3E,0x41,0x51,0x21,0x5E,0x00,0x00,0x00}, 
    {0x7F,0x09,0x19,0x29,0x46,0x00,0x00,0x00}, {0x46,0x49,0x49,0x49,0x31,0x00,0x00,0x00}, 
    {0x01,0x01,0x7F,0x01,0x01,0x00,0x00,0x00}, {0x3F,0x40,0x40,0x40,0x3F,0x00,0x00,0x00}, 
    {0x1F,0x20,0x40,0x20,0x1F,0x00,0x00,0x00}, {0x3F,0x40,0x38,0x40,0x3F,0x00,0x00,0x00}, 
    {0x63,0x14,0x08,0x14,0x63,0x00,0x00,0x00}, {0x07,0x08,0x70,0x08,0x07,0x00,0x00,0x00}, 
    {0x61,0x51,0x49,0x45,0x43,0x00,0x00,0x00}, {0x00,0x7F,0x41,0x41,0x00,0x00,0x00,0x00}, 
    {0x02,0x04,0x08,0x10,0x20,0x00,0x00,0x00}, {0x00,0x41,0x41,0x7F,0x00,0x00,0x00,0x00}, 
    {0x04,0x02,0x01,0x02,0x04,0x00,0x00,0x00}, {0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, 
    {0x00,0x01,0x02,0x04,0x00,0x00,0x00,0x00}, {0x20,0x54,0x54,0x54,0x78,0x00,0x00,0x00}, 
    {0x7F,0x48,0x44,0x44,0x38,0x00,0x00,0x00}, {0x38,0x44,0x44,0x44,0x20,0x00,0x00,0x00}, 
    {0x38,0x44,0x44,0x48,0x7F,0x00,0x00,0x00}, {0x38,0x54,0x54,0x54,0x18,0x00,0x00,0x00}, 
    {0x08,0x7E,0x09,0x01,0x02,0x00,0x00,0x00}, {0x0C,0x52,0x52,0x52,0x3E,0x00,0x00,0x00}, 
    {0x7F,0x08,0x04,0x04,0x78,0x00,0x00,0x00}, {0x00,0x44,0x7D,0x40,0x00,0x00,0x00,0x00}, 
    {0x20,0x40,0x44,0x3D,0x00,0x00,0x00,0x00}, {0x7F,0x10,0x28,0x44,0x00,0x00,0x00,0x00}, 
    {0x00,0x41,0x7F,0x40,0x00,0x00,0x00,0x00}, {0x7C,0x04,0x18,0x04,0x78,0x00,0x00,0x00}, 
    {0x7C,0x08,0x04,0x04,0x78,0x00,0x00,0x00}, {0x38,0x44,0x44,0x44,0x38,0x00,0x00,0x00}, 
    {0x7C,0x14,0x14,0x14,0x08,0x00,0x00,0x00}, {0x08,0x14,0x14,0x18,0x7C,0x00,0x00,0x00}, 
    {0x7C,0x08,0x04,0x04,0x08,0x00,0x00,0x00}, {0x48,0x54,0x54,0x54,0x20,0x00,0x00,0x00}, 
    {0x04,0x3F,0x44,0x40,0x20,0x00,0x00,0x00}, {0x3C,0x40,0x40,0x20,0x7C,0x00,0x00,0x00}, 
    {0x1C,0x20,0x40,0x20,0x1C,0x00,0x00,0x00}, {0x3C,0x40,0x30,0x40,0x3C,0x00,0x00,0x00}, 
    {0x44,0x28,0x10,0x28,0x44,0x00,0x00,0x00}, {0x0C,0x50,0x50,0x50,0x3C,0x00,0x00,0x00}, 
    {0x44,0x64,0x54,0x4C,0x44,0x00,0x00,0x00}, {0x00,0x08,0x36,0x41,0x00,0x00,0x00,0x00}, 
    {0x00,0x00,0x7F,0x00,0x00,0x00,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00,0x00,0x00,0x00}, 
    {0x10,0x08,0x18,0x10,0x08,0x00,0x00,0x00}
};

void st7789_draw_char(unsigned short x, unsigned short y, char c, unsigned short fg_color, unsigned short bg_color) {
    if (c < 32 || c > 126) return; 
    int index = c - 32;
    st7789_set_window(x, y, x + 7, y + 7);
    GPIO_OUT_W1TC_REG = (1 << PIN_CS); 
    GPIO_OUT_W1TS_REG = (1 << PIN_DC); 
    unsigned char fg_h = fg_color >> 8, fg_l = fg_color & 0xFF;
    unsigned char bg_h = bg_color >> 8, bg_l = bg_color & 0xFF;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (font8x8[index][col] & (1 << row)) { 
                spi_send_byte(fg_h);
                spi_send_byte(fg_l);
            } else { 
                spi_send_byte(bg_h);
                spi_send_byte(bg_l);
            }
        }
    }
    GPIO_OUT_W1TS_REG = (1 << PIN_CS); 
}

void st7789_draw_string(unsigned short x, unsigned short y, const char *str, unsigned short fg_color, unsigned short bg_color) {
    int i = 0;
    while (str[i] != '\0') {
        st7789_draw_char(x + (i * 8), y, str[i], fg_color, bg_color);
        i++;
    }
}

typedef struct block_meta {
    unsigned int size;
    int free;
    struct block_meta *next;
} block_meta_t;

#define HEAP_SIZE 32768
static unsigned char custom_heap[HEAP_SIZE];
static block_meta_t *heap_head = (void*)0;

void heap_init() {
    if (heap_head == (void*)0) {
        heap_head = (block_meta_t *)custom_heap;
        heap_head->size = HEAP_SIZE - sizeof(block_meta_t);
        heap_head->free = 1;
        heap_head->next = (void*)0;
    }
}

void* custom_malloc(unsigned int size) {
    if (size == 0) return (void*)0;
    if (heap_head == (void*)0) heap_init();
    block_meta_t *current = heap_head;
    while (current != (void*)0) {
        if (current->free && current->size >= size) {
            if (current->size > size + sizeof(block_meta_t)) {
                block_meta_t *new_block = (block_meta_t *)((unsigned char*)current + sizeof(block_meta_t) + size);
                new_block->size = current->size - size - sizeof(block_meta_t);
                new_block->free = 1;
                new_block->next = current->next;
                current->size = size;
                current->next = new_block;
            }
            current->free = 0;
            return (void*)(current + 1); 
        }
        current = current->next;
    }
    return (void*)0; 
}

void custom_free(void *ptr) {
    if (ptr == (void*)0) return; 
    block_meta_t *block_ptr = (block_meta_t *)ptr - 1;
    block_ptr->free = 1;
    if (block_ptr->next != (void*)0 && block_ptr->next->free == 1) {
        block_ptr->size += sizeof(block_meta_t) + block_ptr->next->size;
        block_ptr->next = block_ptr->next->next;
    }
}

unsigned int get_free_heap() {
    if (heap_head == (void*)0) return HEAP_SIZE;
    unsigned int free_mem = 0;
    block_meta_t *current = heap_head;
    while (current != (void*)0) {
        if (current->free) free_mem += current->size;
        current = current->next;
    }
    return free_mem;
}

typedef enum {
    STATE_MENU,
    STATE_APP_MONITOR,
    STATE_APP_EDITOR,
    STATE_APP_GAMES,
    STATE_APP_EXPLORER,
    STATE_APP_ABOUT
} os_state_t;

void st7789_run_app_placeholder(const char* title, const char* desc) {
    st7789_fill_screen(COLOR_BLACK);
    st7789_draw_rect(0, 0, 240, 30, COLOR_BLUE);
    st7789_draw_string(10, 10, title, COLOR_WHITE, COLOR_BLUE);
    st7789_draw_string(10, 50, desc, COLOR_CYAN, COLOR_BLACK);
    st7789_draw_string(10, 300, "[ Press 'q' to Exit ]", COLOR_RED, COLOR_BLACK);
}

void st7789_run_text_editor() {
    st7789_fill_screen(COLOR_BLACK);
    st7789_draw_rect(0, 0, 240, 16, COLOR_BLUE);
    st7789_draw_string(2, 4, "TEXT EDITOR [Press 'q' to exit]", COLOR_WHITE, COLOR_BLUE);
    unsigned int buffer_capacity = 32;
    unsigned int current_length = 0;
    char *text_buffer = (char *)custom_malloc(buffer_capacity);
    if (text_buffer == (void*)0) return;
    text_buffer[0] = '\0';
    while (1) {
        char c = uart_getchar();
        if (c == 'q') {
            custom_free(text_buffer);
            return; 
        }
        else if (c == 8 || c == 127) {
            if (current_length > 0) {
                current_length--;
                text_buffer[current_length] = '\0';
                int col = current_length % 30;
                int row = current_length / 30;
                st7789_draw_char(col * 8, 20 + (row * 8), ' ', COLOR_WHITE, COLOR_BLACK);
            }
        } 
        else if (c >= 32 && c <= 126) {
            if (current_length + 1 >= buffer_capacity) {
                buffer_capacity += 32; 
                char *new_buffer = (char *)custom_malloc(buffer_capacity);
                if (new_buffer != (void*)0) {
                    for (unsigned int i = 0; i < current_length; i++) {
                        new_buffer[i] = text_buffer[i];
                    }
                    new_buffer[current_length] = '\0';
                    custom_free(text_buffer);
                    text_buffer = new_buffer;
                } else {
                    buffer_capacity -= 32;
                    continue;
                }
            }
            text_buffer[current_length] = c;
            int col = current_length % 30;
            int row = current_length / 30;
            if (row < 37) {
                st7789_draw_char(col * 8, 20 + (row * 8), c, COLOR_GREEN, COLOR_BLACK);
            }
            current_length++;
            text_buffer[current_length] = '\0';
        }
    }
}

void st7789_run_sys_monitor() {
    st7789_fill_screen(COLOR_BLACK);
    st7789_draw_rect(0, 0, 240, 30, COLOR_BLUE);
    st7789_draw_string(10, 10, "SYSTEM MONITOR ['q' to Exit]", COLOR_WHITE, COLOR_BLUE);
    st7789_draw_string(10, 50, "CPU Core:   Xtensa LX6", COLOR_CYAN, COLOR_BLACK);
    st7789_draw_string(10, 70, "Clock:      Calibrating...", COLOR_CYAN, COLOR_BLACK);
    st7789_draw_string(10, 90, "Display:    ST7789 (VSPI)", COLOR_CYAN, COLOR_BLACK);
    st7789_draw_string(10, 130, "Total SRAM: 32768 Bytes", COLOR_YELLOW, COLOR_BLACK);
    
    char num_buf[16];
    
    while (1) {
        unsigned int start_cycles = get_ccount();

        unsigned int free_mem = get_free_heap();
        unsigned int used_mem = HEAP_SIZE - free_mem;
        
        st7789_draw_string(10, 150, "Used SRAM:  ", COLOR_YELLOW, COLOR_BLACK);
        custom_itoa(used_mem, num_buf);
        st7789_draw_string(110, 150, "        ", COLOR_BLACK, COLOR_BLACK); 
        st7789_draw_string(110, 150, num_buf, COLOR_WHITE, COLOR_BLACK);
        
        st7789_draw_string(10, 170, "Free SRAM:  ", COLOR_YELLOW, COLOR_BLACK);
        custom_itoa(free_mem, num_buf);
        st7789_draw_string(110, 170, "        ", COLOR_BLACK, COLOR_BLACK); 
        st7789_draw_string(110, 170, num_buf, COLOR_WHITE, COLOR_BLACK);

        // Fetch absolute global hardware time
        update_uptime();
        unsigned int secs = global_uptime_sec;

        st7789_draw_string(10, 210, "Uptime (s): ", COLOR_MAGENTA, COLOR_BLACK);
        custom_itoa(secs, num_buf);
        st7789_draw_string(110, 210, "        ", COLOR_BLACK, COLOR_BLACK); 
        st7789_draw_string(110, 210, num_buf, COLOR_WHITE, COLOR_BLACK);

        if (uart_has_char()) {
            char c = uart_getchar();
            if (c == 'q') return;
        }

        unsigned int end_cycles = get_ccount();
        unsigned int active_cycles = end_cycles - start_cycles;
        unsigned int idle_cycles = 100000 * CYCLES_PER_US; 
        unsigned int total_cycles = active_cycles + idle_cycles;
        unsigned int cpu_load = (active_cycles * 100) / total_cycles;

        st7789_draw_string(10, 190, "CPU Load:   ", COLOR_RED, COLOR_BLACK);
        custom_itoa(cpu_load, num_buf);
        st7789_draw_string(110, 190, "    ", COLOR_BLACK, COLOR_BLACK); 
        st7789_draw_string(110, 190, num_buf, COLOR_WHITE, COLOR_BLACK);
        st7789_draw_string(140, 190, "%", COLOR_WHITE, COLOR_BLACK);
        
        delay_us(100000); 
    }
}

const char *menu_items[] = {
    "System Monitor",
    "Text Editor",
    "Retro Games",
    "File Explorer (Media)",
    "About the Creator"
};
const int TOTAL_MENU_ITEMS = 5;

void st7789_init_menu(int starting_item) {
    st7789_fill_screen(COLOR_BLACK);
    st7789_draw_rect(0, 0, 240, 40, COLOR_GRAY);
    st7789_draw_string(30, 16, "BARE-METAL OS V1.0", COLOR_YELLOW, COLOR_GRAY);
    for (int i = 0; i < TOTAL_MENU_ITEMS; i++) {
        int y_pos = 70 + (i * 30);
        if (i == starting_item) {
            st7789_draw_string(10, y_pos, ">", COLOR_YELLOW, COLOR_BLACK);
            st7789_draw_string(30, y_pos, menu_items[i], COLOR_YELLOW, COLOR_BLACK);
        } else {
            st7789_draw_string(10, y_pos, " ", COLOR_BLACK, COLOR_BLACK); 
            st7789_draw_string(30, y_pos, menu_items[i], COLOR_WHITE, COLOR_BLACK);
        }
    }
}

void st7789_update_menu_cursor(int old_item, int new_item) {
    if (old_item == new_item) return;
    int old_y = 70 + (old_item * 30);
    int new_y = 70 + (new_item * 30);
    st7789_draw_string(10, old_y, " ", COLOR_BLACK, COLOR_BLACK);
    st7789_draw_string(30, old_y, menu_items[old_item], COLOR_WHITE, COLOR_BLACK);
    st7789_draw_string(10, new_y, ">", COLOR_YELLOW, COLOR_BLACK);
    st7789_draw_string(30, new_y, menu_items[new_item], COLOR_YELLOW, COLOR_BLACK);
}

void kernel_main() {
    *(volatile unsigned int *)0x3FF480A4 = 0x50D83AA1;
    *(volatile unsigned int *)0x3FF4808C = 0;
    *(volatile unsigned int *)0x3FF5F064 = 0x50D83AA1;
    *(volatile unsigned int *)0x3FF5F048 = 0;
    last_ccount = get_ccount();
    spi_init();
    st7789_init();

    int current_selection = 0;
    int previous_selection = 0;
    os_state_t current_state = STATE_MENU;

    st7789_init_menu(current_selection);

    while(1) {
        char input = uart_getchar(); 
        if (current_state == STATE_MENU) {
            previous_selection = current_selection;
            if (input == 'w') { 
                current_selection--;
                if (current_selection < 0) current_selection = TOTAL_MENU_ITEMS - 1; 
                st7789_update_menu_cursor(previous_selection, current_selection);
            } else if (input == 's') { 
                current_selection++;
                if (current_selection >= TOTAL_MENU_ITEMS) current_selection = 0; 
                st7789_update_menu_cursor(previous_selection, current_selection);
            } else if (input == 'e') { 
                if (current_selection == 0) {
                    current_state = STATE_APP_MONITOR;
                    st7789_run_sys_monitor();
                    current_state = STATE_MENU;
                    st7789_init_menu(current_selection);
                } else if (current_selection == 1) {
                    current_state = STATE_APP_EDITOR;
                    st7789_run_text_editor();
                    current_state = STATE_MENU;
                    st7789_init_menu(current_selection);
                } else if (current_selection == 2) {
                    current_state = STATE_APP_GAMES;
                    st7789_run_app_placeholder("RETRO GAMES", "Tetris Engine");
                } else if (current_selection == 3) {
                    current_state = STATE_APP_EXPLORER;
                    st7789_run_app_placeholder("FILE EXPLORER", "Mounting MicroSD...");
                } else if (current_selection == 4) {
                    current_state = STATE_APP_ABOUT;
                    st7789_run_app_placeholder("ABOUT", "Bare-Metal OS");
                }
            }
        } 
        else {
            if (input == 'q') { 
                current_state = STATE_MENU;
                st7789_init_menu(current_selection); 
            }
        }
    }
}