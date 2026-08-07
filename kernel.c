
// Classic Windows RGB565 Palette
#define WIN_DESKTOP  0x0410 // Deep Teal
#define WIN_FACE     0xD69A // Classic Light Gray
#define WIN_TITLE    0x0935 // Dark Blue Title Bar
#define WIN_SHADOW   0x8410 // Dark Gray (Outer Bevel)
#define WIN_HILITE   0xFFFF // White (Inner Bevel)
#define WIN_TEXT     0x0000 // Black
#define WIN_TEXT_SEL 0xFFFF // White Text on Blue Background

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410
#define COLOR_ORANGE 0xFD20

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
#define UART0_FIFO (*(volatile unsigned int *)(0x3FF40000))

#define GPIO_IN_REG              (*(volatile unsigned int *)0x3FF4403C)
#define GPIO_ENABLE_W1TC_REG     (*(volatile unsigned int *)0x3FF44028)

// --- JOYSTICK ADC & GPIO REGISTERS ---
#define SENS_BASE                 0x3FF48800UL
#define SENS_SAR_READ_CTRL_REG    (*(volatile unsigned int *)(SENS_BASE + 0x00))
#define SENS_SAR_MEAS_WAIT2_REG   (*(volatile unsigned int *)(SENS_BASE + 0x0C))
#define SENS_SAR_ATTEN1_REG       (*(volatile unsigned int *)(SENS_BASE + 0x34)) // CORRECTED OFFSET
#define SENS_SAR_MEAS_START1_REG  (*(volatile unsigned int *)(SENS_BASE + 0x54)) // CORRECTED OFFSET

// --- CORRECTED RTC IO REGISTERS ---
#define RTC_IO_BASE               0x3FF48400UL
#define RTC_IO_TOUCH_PAD9_REG     (*(volatile unsigned int *)(RTC_IO_BASE + 0xA8)) // GPIO 32 (was 0x9C)
#define RTC_IO_TOUCH_PAD8_REG     (*(volatile unsigned int *)(RTC_IO_BASE + 0xA4)) // GPIO 33 (was 0x98)

// Sends a single character
void uart_putc(char c) {
    UART0_FIFO = c;
}

// Sends an entire string (loops through putc until it hits the null terminator)
void uart_puts(const char *str) {
    while (*str) {
        uart_putc(*str++);
    }
}

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

unsigned int custom_strlen(const char *str) {
    unsigned int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Custom memory copy (Required because GCC optimizes array copies into memcpy)
void *memcpy(void *dest, const void *src, unsigned int n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (unsigned int i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

// Custom memory set (Required because GCC optimizes array zeroing into memset)
void *memset(void *s, int c, unsigned int n) {
    unsigned char *p = (unsigned char *)s;
    for (unsigned int i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

#define GPIO_ENABLE_W1TS_REG (*(volatile unsigned int *)0x3FF44024)
#define GPIO_OUT_W1TS_REG    (*(volatile unsigned int *)0x3FF44008)
#define GPIO_OUT_W1TC_REG    (*(volatile unsigned int *)0x3FF4400C)

#define PIN_DC   2
#define PIN_RST  4
#define PIN_CS   5
#define PIN_CLK  18
#define PIN_MOSI 23


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

void st7789_draw_vline(unsigned short x, unsigned short y, unsigned short h, unsigned short color) {
    if (h == 0) return;
    st7789_draw_rect(x, y, 1, h, color);
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

typedef struct {
    int id;
    char* content;
} ram_file_t;

#define MAX_RAM_FILES 5
ram_file_t ram_disk[MAX_RAM_FILES];
int ram_file_count = 0;

void save_to_ram_disk(char* buffer) {
    if (ram_file_count < MAX_RAM_FILES) {
        ram_disk[ram_file_count].id = ram_file_count + 1;
        ram_disk[ram_file_count].content = buffer;
        ram_file_count++;
    } else {
        custom_free(buffer);
    }
}

void draw_win2k_window(int x, int y, int w, int h, const char* title) {
    // 1. Fill the main window gray body
    st7789_draw_rect(x, y, w, h, WIN_FACE);
    
    // 2. Draw 3D Raised Bevels
    // Top and Left Highlights (White)
    st7789_draw_rect(x, y, w, 1, WIN_HILITE);
    st7789_draw_rect(x, y, 1, h, WIN_HILITE);
    
    // Bottom and Right Shadows (Black outer, Dark Gray inner)
    st7789_draw_rect(x, y + h - 1, w, 1, 0x0000);
    st7789_draw_rect(x + w - 1, y, 1, h, 0x0000);
    st7789_draw_rect(x + 1, y + h - 2, w - 2, 1, WIN_SHADOW);
    st7789_draw_rect(x + w - 2, y + 1, 1, h - 2, WIN_SHADOW);

    // 3. Draw Title Bar (Blue)
    st7789_draw_rect(x + 3, y + 3, w - 6, 18, WIN_TITLE);
    
    // 4. Draw Title Text (White text, Blue background)
    st7789_draw_string(x + 6, y + 8, title, WIN_HILITE, WIN_TITLE);
}

void run_ram_file_browser() {
    int cursor = 0;
    int action_cursor = 0;
    
    // 0 = List, 1 = Action Menu, 2 = Read, 3 = Edit
    int browser_state = 0; 
    
    int previous_cursor = -1;
    int previous_action = -1;

    while(1) {
        
        // ==========================================
        // STATE 0: EXPLORER.EXE (File List)
        // ==========================================
        if (browser_state == 0) {
            // Draw window frame only when first entering the state
            if (previous_cursor == -1) {
                st7789_fill_screen(WIN_DESKTOP);
                draw_win2k_window(10, 10, 220, 300, "Explorer.exe");
                st7789_draw_string(20, 290, "e:Options  q:Exit", WIN_TEXT, WIN_FACE);

                // Sunken white canvas for file list
                st7789_draw_rect(18, 38, 204, 244, WIN_SHADOW);
                st7789_draw_rect(19, 39, 202, 242, WIN_HILITE);
            }

            if (previous_cursor != cursor || previous_cursor == -1) {
                if (ram_file_count == 0) {
                    st7789_draw_string(25, 45, "Folder is Empty", WIN_TEXT, WIN_HILITE);
                } else {
                    for (int i = 0; i < ram_file_count; i++) {
                        int y_pos = 45 + (i * 16); 
                        
                        // Differential Highlighting
                        if (i == cursor) {
                            st7789_draw_rect(20, y_pos - 2, 198, 16, WIN_TITLE);
                            st7789_draw_string(25, y_pos, "FILE", WIN_TEXT_SEL, WIN_TITLE);
                            st7789_draw_char(65, y_pos, '1' + i, WIN_TEXT_SEL, WIN_TITLE);
                        } else {
                            st7789_draw_rect(20, y_pos - 2, 198, 16, WIN_HILITE); // Erase with white
                            st7789_draw_string(25, y_pos, "FILE", WIN_TEXT, WIN_HILITE);
                            st7789_draw_char(65, y_pos, '1' + i, WIN_TEXT, WIN_HILITE);
                        }
                    }
                }
                previous_cursor = cursor;
            }
            
            char in = uart_getchar();
            
            if (in == 'w' && cursor > 0) cursor--;
            if (in == 's' && cursor < ram_file_count - 1) cursor++;
            if (in == 'e' && ram_file_count > 0) {
                browser_state = 1; // Jump to Action Menu pop-up
                previous_action = -1;
                action_cursor = 0;
            }
            if (in == 'q') {
                // Return to main OS menu
                st7789_fill_screen(COLOR_BLACK); 
                break; 
            }
        } 
        
        // ==========================================
        // STATE 1: FLOATING ACTION MENU POP-UP
        // ==========================================
        else if (browser_state == 1) {
            if (previous_action == -1) {
                // Draw a smaller window layered perfectly over Explorer
                draw_win2k_window(45, 100, 150, 100, "Options");
            }

            if (previous_action != action_cursor || previous_action == -1) {
                const char* actions[] = {"Read", "Edit", "Delete"};
                
                for (int i = 0; i < 3; i++) {
                    int y_pos = 130 + (i * 20);
                    
                    if (i == action_cursor) {
                        st7789_draw_rect(55, y_pos - 2, 130, 16, WIN_TITLE);
                        st7789_draw_string(65, y_pos, actions[i], WIN_TEXT_SEL, WIN_TITLE);
                    } else {
                        st7789_draw_rect(55, y_pos - 2, 130, 16, WIN_FACE);
                        st7789_draw_string(65, y_pos, actions[i], WIN_TEXT, WIN_FACE);
                    }
                }
                previous_action = action_cursor;
            }
            
            char in = uart_getchar();
            
            if (in == 'w' && action_cursor > 0) action_cursor--;
            if (in == 's' && action_cursor < 2) action_cursor++;
            if (in == 'q') {
                browser_state = 0; // Back to File List
                previous_cursor = -1; // Triggers full Explorer redraw
            }
            if (in == 'e') {
                if (action_cursor == 0) {
                    browser_state = 2; // Read Mode
                    // Draw Notepad Window Frame
                    st7789_fill_screen(WIN_DESKTOP);
                    draw_win2k_window(5, 5, 230, 310, "Notepad (Read-Only)");
                    st7789_draw_rect(10, 28, 220, 280, WIN_SHADOW);
                    st7789_draw_rect(11, 29, 218, 278, WIN_HILITE);
                } 
                else if (action_cursor == 1) {
                    browser_state = 3; // Edit Mode
                    // Draw Notepad Window Frame
                    st7789_fill_screen(WIN_DESKTOP);
                    draw_win2k_window(5, 5, 230, 310, "Notepad.exe");
                    st7789_draw_rect(10, 28, 220, 280, WIN_SHADOW);
                    st7789_draw_rect(11, 29, 218, 278, WIN_HILITE);
                } 
                else if (action_cursor == 2) {
                    // EXECUTING DELETE
                    custom_free(ram_disk[cursor].content); 
                    
                    for (int i = cursor; i < ram_file_count - 1; i++) {
                        ram_disk[i] = ram_disk[i + 1];
                        ram_disk[i].id = i + 1;
                    }
                    ram_file_count--;
                    
                    if (cursor >= ram_file_count && cursor > 0) cursor--;
                    
                    browser_state = 0;
                    previous_cursor = -1; // Triggers full Explorer redraw
                }
            }
        }
        
        // ==========================================
        // STATE 2: READ MODE (Notepad Layout)
        // ==========================================
        else if (browser_state == 2) {
            int len = custom_strlen(ram_disk[cursor].content);
            int start_index = 0;
            // Notepad Canvas is 27 columns x 34 rows = 918 chars
            if (len > 918) start_index = len - 918;
            
            int r = 0;
            int c = 0;
            for (int i = start_index; i < len; i++) {
                int tx = (c * 8) + 12;
                int ty = (r * 8) + 30;
                st7789_draw_char(tx, ty, ram_disk[cursor].content[i], WIN_TEXT, WIN_HILITE);
                c++;
                if (c >= 27) { c = 0; r++; }
            }
            
            // Wait for exit
            while(1) {
                if (uart_has_char() && uart_getchar() == 'q') {
                    browser_state = 0; // Go all the way back to Explorer
                    previous_cursor = -1;
                    break;
                }
            }
        }
        
        // ==========================================
        // STATE 3: EDIT MODE (Notepad Layout)
        // ==========================================
        else if (browser_state == 3) {
            char* text_buffer = ram_disk[cursor].content;
            unsigned int current_length = custom_strlen(text_buffer);
            unsigned int buffer_capacity = current_length + 32;
            
            char* new_buffer = (char*)custom_malloc(buffer_capacity);
            for (unsigned int i = 0; i <= current_length; i++) {
                new_buffer[i] = text_buffer[i];
            }
            custom_free(text_buffer);
            text_buffer = new_buffer;
            ram_disk[cursor].content = text_buffer;

            // Render existing text into the 27x34 canvas
            int start_index = 0;
            if (current_length > 918) start_index = current_length - 918;

            int r = 0;
            int c = 0;
            for (unsigned int i = start_index; i < current_length; i++) {
                int tx = (c * 8) + 12;
                int ty = (r * 8) + 30;
                st7789_draw_char(tx, ty, text_buffer[i], WIN_TEXT, WIN_HILITE);
                c++;
                if (c >= 27) { c = 0; r++; }
            }

            unsigned int last_blink = get_ccount();
            int cursor_state = 0;

            while (1) {
                r = current_length / 27;
                c = current_length % 27;
                if (current_length >= 918) { r = 33; c = 26; }
                int tx = (c * 8) + 12;
                int ty = (r * 8) + 30;

                // Blinking Black Cursor on White
                if (get_ccount() - last_blink > 20000000) {
                    cursor_state = !cursor_state;
                    last_blink = get_ccount();
                    char draw_char = cursor_state ? '_' : ' ';
                    st7789_draw_char(tx, ty, draw_char, WIN_TEXT, WIN_HILITE);
                }

                if ((*UART0_STATUS_REG & 0xFF) > 0) {
                    char in = uart_getchar();
                    
                    st7789_draw_char(tx, ty, ' ', WIN_TEXT, WIN_HILITE); // Erase cursor

                    if (in == 'q') {
                        browser_state = 0; // Save and jump back to Explorer
                        previous_cursor = -1;
                        break;
                    }

                    if (in == 127 || in == 8) { // Backspace
                        if (current_length > 0) {
                            current_length--;
                            text_buffer[current_length] = '\0';
                            
                            r = current_length / 27;
                            c = current_length % 27;
                            tx = (c * 8) + 12;
                            ty = (r * 8) + 30;
                            st7789_draw_char(tx, ty, ' ', WIN_TEXT, WIN_HILITE); 
                        }
                    } 
                    else if (in >= 32 && in <= 126) {
                        if (current_length + 1 >= buffer_capacity) {
                            unsigned int new_cap = buffer_capacity * 2;
                            char* grow_buffer = (char*)custom_malloc(new_cap);
                            for (unsigned int i = 0; i <= current_length; i++) {
                                grow_buffer[i] = text_buffer[i];
                            }
                            custom_free(text_buffer);
                            text_buffer = grow_buffer;
                            ram_disk[cursor].content = text_buffer;
                            buffer_capacity = new_cap;
                        }

                        text_buffer[current_length] = in;
                        
                        if (current_length < 918) { 
                            st7789_draw_char(tx, ty, in, WIN_TEXT, WIN_HILITE);
                        } else {
                            // Hardware wipe scrolling
                            st7789_draw_rect(11, 29, 218, 278, WIN_HILITE);
                            unsigned int s_idx = current_length - 917;
                            int temp_r = 0, temp_c = 0;
                            for (unsigned int i = s_idx; i <= current_length; i++) {
                                int ttx = (temp_c * 8) + 12;
                                int tty = (temp_r * 8) + 30;
                                st7789_draw_char(ttx, tty, text_buffer[i], WIN_TEXT, WIN_HILITE);
                                temp_c++;
                                if (temp_c >= 27) { temp_c = 0; temp_r++; }
                            }
                        }
                        
                        current_length++;
                        text_buffer[current_length] = '\0';
                    }
                }
            }
        }
    }
}

void run_dynamic_text_editor() {
    unsigned int buffer_capacity = 32;
    unsigned int current_length = 0;
    char* text_buffer = (char*)custom_malloc(buffer_capacity);
    
    if (text_buffer == (void*)0) return;
    text_buffer[0] = '\0';

    // 1. Draw the Desktop and the main Notepad Window
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(5, 5, 230, 310, "Notepad");
    
    // 2. Draw the sunken white text canvas
    st7789_draw_rect(10, 28, 220, 280, WIN_SHADOW); // Dark outer bevel
    st7789_draw_rect(11, 29, 218, 278, WIN_HILITE); // Pure white typing area

    unsigned int last_blink = get_ccount();
    int cursor_state = 0;

    while (1) {
        // Calculate grid position for 27 columns and 34 rows (918 chars)
        unsigned int r = current_length / 27;
        unsigned int c = current_length % 27;
        if (current_length >= 918) { 
            r = 33; 
            c = 26; 
        }
        
        // Pixel offsets to perfectly align text inside the white canvas
        int x_pos = (c * 8) + 12;
        int y_pos = (r * 8) + 30;

        // Non-Blocking Blinking Cursor (Black text on White background)
        if (get_ccount() - last_blink > 20000000) {
            cursor_state = !cursor_state;
            last_blink = get_ccount();
            char draw_char = cursor_state ? '_' : ' ';
            st7789_draw_char(x_pos, y_pos, draw_char, WIN_TEXT, WIN_HILITE);
        }

        if ((*UART0_STATUS_REG & 0xFF) > 0) {
            char in = uart_getchar();

            // Erase the cursor instantly before moving
            st7789_draw_char(x_pos, y_pos, ' ', WIN_TEXT, WIN_HILITE);

            if (in == 'q') {
                save_to_ram_disk(text_buffer);
                break;
            }

            if (in == 127 || in == 8) { // Backspace
                if (current_length > 0) {
                    current_length--;
                    text_buffer[current_length] = '\0';
                    
                    r = current_length / 27;
                    c = current_length % 27;
                    x_pos = (c * 8) + 12;
                    y_pos = (r * 8) + 30;
                    st7789_draw_char(x_pos, y_pos, ' ', WIN_TEXT, WIN_HILITE); 
                }
            } 
            else if (in >= 32 && in <= 126) {
                
                // Dynamic RAM Allocation
                if (current_length + 1 >= buffer_capacity) {
                    unsigned int new_capacity = buffer_capacity * 2;
                    char* new_buffer = (char*)custom_malloc(new_capacity);
                    if (new_buffer != (void*)0) {
                        for (unsigned int i = 0; i < current_length; i++) {
                            new_buffer[i] = text_buffer[i];
                        }
                        new_buffer[current_length] = '\0';
                        custom_free(text_buffer);
                        text_buffer = new_buffer;
                        buffer_capacity = new_capacity;
                    }
                }

                text_buffer[current_length] = in;
                
                if (current_length < 918) { 
                    // Draw normal character (Black on White)
                    st7789_draw_char(x_pos, y_pos, in, WIN_TEXT, WIN_HILITE);
                } else {
                    // Scrolling Redraw (Uses the hardware rect fill to instantly wipe the canvas)
                    st7789_draw_rect(11, 29, 218, 278, WIN_HILITE);
                    
                    unsigned int start_index = current_length - 917;
                    unsigned int temp_r = 0;
                    unsigned int temp_c = 0;
                    for (unsigned int i = start_index; i <= current_length; i++) {
                        int tx = (temp_c * 8) + 12;
                        int ty = (temp_r * 8) + 30;
                        st7789_draw_char(tx, ty, text_buffer[i], WIN_TEXT, WIN_HILITE);
                        temp_c++;
                        if (temp_c >= 27) {
                            temp_c = 0;
                            temp_r++;
                        }
                    }
                }
                
                current_length++;
                text_buffer[current_length] = '\0';
            }
        }
    }
}

// --- CUSTOM 8x8 UI ICONS ---
const unsigned char ICON_SNAKE[8] = { 0x00, 0x78, 0x40, 0x7C, 0x04, 0x3C, 0x00, 0x00 };
const unsigned char ICON_TETRIS[8] = { 0x00, 0x60, 0x60, 0x78, 0x78, 0x00, 0x00, 0x00 };
const unsigned char ICON_BRICK[8] = { 0x7E, 0x7E, 0x00, 0x18, 0x18, 0x00, 0x3C, 0x3C };
const unsigned char ICON_BACK[8] = { 0x10, 0x30, 0x7E, 0xFE, 0x7E, 0x30, 0x10, 0x00 };

// Icon Renderer: Draws the 8x8 mask using 1x1 rectangles
void st7789_draw_icon(int x, int y, const unsigned char* icon, unsigned short fg, unsigned short bg) {
    for (int r = 0; r < 8; r++) {
        unsigned char row = icon[r];
        for (int c = 0; c < 8; c++) {
            if (row & (1 << (7 - c))) {
                st7789_draw_rect(x + c, y + r, 1, 1, fg); // Draw icon pixel
            } else {
                st7789_draw_rect(x + c, y + r, 1, 1, bg); // Draw background pixel
            }
        }
    }
}

typedef struct SnakeNode {
    int x;
    int y;
    struct SnakeNode *next;
} SnakeNode_t;

unsigned int prng_state = 0;

int custom_rand() {
    if (prng_state == 0) prng_state = get_ccount();
    prng_state = (prng_state * 1103515245 + 12345) & 0x7FFFFFFF;
    return prng_state;
}

void st7789_run_snake() {
    // 1. Draw the Desktop and the main Game Window
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(5, 5, 230, 310, "Snake.exe");
    
    // 2. Draw the Status Bar at the bottom
    st7789_draw_string(10, 295, "Score: 0", WIN_TEXT, WIN_FACE);
    st7789_draw_string(140, 295, "['q'=Quit]", WIN_TEXT, WIN_FACE);

    // 3. Draw the sunken black game canvas (220x260 pixels)
    st7789_draw_rect(9, 29, 222, 262, WIN_SHADOW); 
    st7789_draw_rect(10, 30, 220, 260, COLOR_BLACK); 

    SnakeNode_t *head = (SnakeNode_t *)custom_malloc(sizeof(SnakeNode_t));
    if (!head) return;
    head->x = 11; // Center of new 22-wide grid
    head->y = 13; // Center of new 26-tall grid
    head->next = (void*)0;

    int dx = 1, dy = 0;
    int apple_x = 0, apple_y = 0;
    int score = 0;
    int game_over = 0;
    int show_game_over = 0;
    char buf[8];

    prng_state = get_ccount();
    apple_x = custom_rand() % 22; 
    apple_y = custom_rand() % 26; 
    
    // Grid Offset: X starts at 10, Y starts at 30
    st7789_draw_rect((apple_x * 10) + 10, (apple_y * 10) + 30, 10, 10, COLOR_RED);

    while (1) {
        if (uart_has_char()) {
            char in = uart_getchar();
            if (in == 'q') {
                SnakeNode_t *curr = head;
                while (curr != (void*)0) {
                    SnakeNode_t *next = curr->next;
                    custom_free(curr);
                    curr = next;
                }
                return; // Exits back to the OS router
            }
            if (!game_over) {
                if (in == 'w' && dy == 0) { dx = 0; dy = -1; }
                else if (in == 's' && dy == 0) { dx = 0; dy = 1; }
                else if (in == 'a' && dx == 0) { dx = -1; dy = 0; }
                else if (in == 'd' && dx == 0) { dx = 1; dy = 0; }
            }
        }

        if (game_over) {
            // Draw the error pop-up only once to prevent flickering
            if (show_game_over == 0) {
                draw_win2k_window(50, 120, 140, 60, "Info");
                st7789_draw_string(80, 145, "GAME OVER!", WIN_TEXT, WIN_FACE);
                show_game_over = 1;
            }
            delay_us(100000);
            continue;
        }

        int next_x = head->x + dx;
        int next_y = head->y + dy;

        // Updated boundaries for the 22x26 grid
        if (next_x < 0 || next_x >= 22 || next_y < 0 || next_y >= 26) {
            game_over = 1;
            continue;
        }

        SnakeNode_t *check = head;
        int collision = 0;
        while (check != (void*)0) {
            if (check->x == next_x && check->y == next_y) collision = 1;
            check = check->next;
        }
        if (collision) {
            game_over = 1;
            continue;
        }

        SnakeNode_t *new_head = (SnakeNode_t *)custom_malloc(sizeof(SnakeNode_t));
        new_head->x = next_x;
        new_head->y = next_y;
        new_head->next = head;
        head = new_head;

        // Draw new head with canvas offsets
        st7789_draw_rect((head->x * 10) + 10, (head->y * 10) + 30, 10, 10, COLOR_GREEN);

        if (next_x == apple_x && next_y == apple_y) {
            score++;
            custom_itoa(score, buf);
            
            // Update Status Bar Score
            st7789_draw_string(10, 295, "Score:      ", WIN_TEXT, WIN_FACE);
            st7789_draw_string(66, 295, buf, WIN_TEXT, WIN_FACE);

            int valid_apple = 0;
            while (!valid_apple) {
                apple_x = custom_rand() % 22;
                apple_y = custom_rand() % 26;
                valid_apple = 1;
                SnakeNode_t *c = head;
                while (c != (void*)0) {
                    if (c->x == apple_x && c->y == apple_y) valid_apple = 0;
                    c = c->next;
                }
            }
            st7789_draw_rect((apple_x * 10) + 10, (apple_y * 10) + 30, 10, 10, COLOR_RED);
        } else {
            SnakeNode_t *curr = head;
            while (curr->next != (void*)0 && curr->next->next != (void*)0) {
                curr = curr->next;
            }
            // Erase tail with canvas offsets
            st7789_draw_rect((curr->next->x * 10) + 10, (curr->next->y * 10) + 30, 10, 10, COLOR_BLACK);
            custom_free(curr->next);
            curr->next = (void*)0;
        }

        delay_us(80000); 
    }
}

void st7789_run_custom_block_breaker(unsigned char custom_layout[5][8]) {
    // 1. Draw the Desktop and the main App Window
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(5, 5, 230, 310, "Brick.exe");

    // Draw Sunken Black Game Canvas
    st7789_draw_rect(13, 28, 214, 236, WIN_SHADOW); 
    st7789_draw_rect(14, 29, 212, 234, COLOR_BLACK); 

    unsigned char blocks[5][8];
    unsigned short colors[5] = {COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_MAGENTA};
    
    int blocks_left = 0; 
    int game_state = 0;   
    int score = 0;
    int show_pop_up = 0;
    char ui_buf[10];

    // Load custom layout with BIG blocks
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 8; c++) {
            blocks[r][c] = custom_layout[r][c];
            if (blocks[r][c] == 1) {
                blocks_left++;
                int bx = c * 25 + 20; // Expanded X spacing
                int by = r * 20 + 40; // Expanded Y spacing
                st7789_draw_rect(bx, by, 23, 17, colors[r]); // Chunky 23x17 blocks
            }
        }
    }

    // --- PERFECTLY SPACED STATUS BAR INITIALIZATION ---
    st7789_draw_string(15, 290, "Blk:", WIN_TEXT, WIN_FACE);
    custom_itoa(blocks_left, ui_buf);
    st7789_draw_string(50, 290, ui_buf, WIN_TEXT, WIN_FACE);
    
    st7789_draw_string(85, 290, "Pts:", WIN_TEXT, WIN_FACE); 
    st7789_draw_string(120, 290, "0", WIN_TEXT, WIN_FACE); // Zero is safe!

    int ball_x = 120, ball_y = 150;
    int ball_dx = 4, ball_dy = -4; 
    
    int paddle_w = 46, paddle_h = 6;
    int paddle_x = 97, paddle_y = 245; 

    st7789_draw_rect(paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLUE);

    while (1) {
        int old_paddle_x = paddle_x;

        if (uart_has_char()) {
            char in = uart_getchar();
            if (in == 'q') return; 
            if (game_state == 0) {
                if (in == 'a') { paddle_x -= 15; if (paddle_x < 15) paddle_x = 15; }
                if (in == 'd') { paddle_x += 15; if (paddle_x > 224 - paddle_w) paddle_x = 224 - paddle_w; }
            }
        }

        if (game_state != 0) {
            if (show_pop_up == 0) {
                draw_win2k_window(45, 115, 150, 70, "Info");
                if (game_state == 1) st7789_draw_string(75, 142, "GAME OVER!", WIN_TEXT, WIN_FACE);
                else if (game_state == 2) st7789_draw_string(85, 142, "YOU WIN!", WIN_TEXT, WIN_FACE);
                show_pop_up = 1;
            }
            delay_us(100000); 
            continue; 
        }

        st7789_draw_rect(ball_x, ball_y, 4, 4, COLOR_BLACK);
        if (old_paddle_x != paddle_x) st7789_draw_rect(old_paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLACK);

        ball_x += ball_dx; ball_y += ball_dy;

        if (ball_x <= 14) { ball_x = 14; ball_dx = -ball_dx; }
        if (ball_x >= 222) { ball_x = 222; ball_dx = -ball_dx; }
        if (ball_y <= 29) { ball_y = 29; ball_dy = -ball_dy; } 
        if (ball_y >= 260) { game_state = 1; continue; }

        if (ball_y + 4 >= paddle_y && ball_y <= paddle_y + paddle_h && ball_x + 4 >= paddle_x && ball_x <= paddle_x + paddle_w) {
            ball_dy = -ball_dy; ball_y = paddle_y - 4; 
        }

        int hit = 0;
        for (int r = 0; r < 5 && !hit; r++) {
            for (int c = 0; c < 8 && !hit; c++) {
                if (blocks[r][c]) {
                    // Match the bigger box collision math
                    int bx = c * 25 + 20; 
                    int by = r * 20 + 40;
                    
                    if (ball_x + 4 >= bx && ball_x <= bx + 23 && ball_y + 4 >= by && ball_y <= by + 17) {
                        blocks[r][c] = 0; 
                        st7789_draw_rect(bx, by, 23, 17, COLOR_BLACK); 
                        ball_dy = -ball_dy; hit = 1;
                        
                        blocks_left--; score += 100;
                        
                        // --- PERFECTLY SPACED STATUS BAR UPDATES ---
                        custom_itoa(blocks_left, ui_buf);
                        st7789_draw_string(50, 290, "   ", WIN_TEXT, WIN_FACE); 
                        st7789_draw_string(50, 290, ui_buf, WIN_TEXT, WIN_FACE);
                        
                        custom_itoa(score, ui_buf);
                        st7789_draw_string(120, 290, "      ", WIN_TEXT, WIN_FACE); 
                        st7789_draw_string(120, 290, ui_buf, WIN_TEXT, WIN_FACE);
                        
                        if (blocks_left == 0) game_state = 2;
                    }
                }
            }
        }
        st7789_draw_rect(ball_x, ball_y, 4, 4, COLOR_WHITE);
        st7789_draw_rect(paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLUE);
        delay_us(30000); 
    }
}
void st7789_run_block_studio() {
    // 1. Draw Win2K Level Editor UI
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(5, 5, 230, 310, "BrickStudio.exe");
    
    // Status Bar Commands
    st7789_draw_string(15, 290, "e:Draw p:Play q:Quit", WIN_TEXT, WIN_FACE);

    // Draw Sunken Black Canvas
    st7789_draw_rect(13, 28, 214, 236, WIN_SHADOW); 
    st7789_draw_rect(14, 29, 212, 234, COLOR_BLACK); 

    unsigned char custom_blocks[5][8]; 
    // Safely zero-out the grid
    for(int r = 0; r < 5; r++) {
        for(int c = 0; c < 8; c++) {
            custom_blocks[r][c] = 0;
        }
    }
    
    unsigned short colors[5] = {COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_MAGENTA};

    int cursor_r = 0;
    int cursor_c = 0;
    int cursor_state = 0;
    unsigned int last_blink = get_ccount();

    while(1) {
        // --- HOLLOW CURSOR MATH FOR 23x17 BLOCKS ---
        if (get_ccount() - last_blink > 15000000) {
            cursor_state = !cursor_state;
            last_blink = get_ccount();
            
            int bx = cursor_c * 25 + 20; 
            int by = cursor_r * 20 + 40;
            
            if (cursor_state) {
                st7789_draw_rect(bx - 1, by - 1, 25, 1, COLOR_WHITE);  
                st7789_draw_rect(bx - 1, by + 17, 25, 1, COLOR_WHITE); 
                st7789_draw_rect(bx - 1, by, 1, 17, COLOR_WHITE);      
                st7789_draw_rect(bx + 23, by, 1, 17, COLOR_WHITE);     
            } else {
                st7789_draw_rect(bx - 1, by - 1, 25, 1, COLOR_BLACK);  
                st7789_draw_rect(bx - 1, by + 17, 25, 1, COLOR_BLACK); 
                st7789_draw_rect(bx - 1, by, 1, 17, COLOR_BLACK);      
                st7789_draw_rect(bx + 23, by, 1, 17, COLOR_BLACK);     
            }
        }

        if (uart_has_char()) {
            char in = uart_getchar();
            
            // Erase old cursor frame before moving
            int bx = cursor_c * 25 + 20; 
            int by = cursor_r * 20 + 40;
            st7789_draw_rect(bx - 1, by - 1, 25, 1, COLOR_BLACK); 
            st7789_draw_rect(bx - 1, by + 17, 25, 1, COLOR_BLACK);
            st7789_draw_rect(bx - 1, by, 1, 17, COLOR_BLACK);
            st7789_draw_rect(bx + 23, by, 1, 17, COLOR_BLACK);
            
            if (in == 'q') return; // Exit back to Games Menu

            // Movement Bounds
            if (in == 'w' && cursor_r > 0) cursor_r--;
            if (in == 's' && cursor_r < 4) cursor_r++;
            if (in == 'a' && cursor_c > 0) cursor_c--;
            if (in == 'd' && cursor_c < 7) cursor_c++;

            // Toggle Block (Draw / Erase)
            if (in == 'e') {
                custom_blocks[cursor_r][cursor_c] = !custom_blocks[cursor_r][cursor_c];
                
                bx = cursor_c * 25 + 20; 
                by = cursor_r * 20 + 40;
                
                if (custom_blocks[cursor_r][cursor_c]) {
                    st7789_draw_rect(bx, by, 23, 17, colors[cursor_r]); 
                } else {
                    st7789_draw_rect(bx, by, 23, 17, COLOR_BLACK); 
                }
            }

            // --- THE NEW EMPTY-LEVEL EXCEPTION LOGIC ---
            if (in == 'p') {
                int total_blocks = 0;
                
                // Count how many blocks the user has drawn
                for (int r = 0; r < 5; r++) {
                    for (int c = 0; c < 8; c++) {
                        if (custom_blocks[r][c]) total_blocks++;
                    }
                }

                if (total_blocks == 0) {
                    // Trigger Win2K Error Pop-up
                    draw_win2k_window(45, 115, 150, 70, "Warning");
                    st7789_draw_string(75, 142, "ADD BLOCKS!", WIN_TEXT, WIN_FACE);
                    
                    // Wait safely for the user to press ANY key to dismiss the pop-up
                    while (1) {
                        if (uart_has_char()) {
                            uart_getchar(); // Consume the keypress
                            break;
                        }
                    }
                } else {
                    // Valid Level -> Pass array directly into the game!
                    st7789_run_custom_block_breaker(custom_blocks);
                }
                
                // Whether we returned from playing a game, or just dismissed the warning popup:
                // Redraw the entire Studio UI to wipe the screen clean!
                st7789_fill_screen(WIN_DESKTOP);
                draw_win2k_window(5, 5, 230, 310, "BrickStudio.exe");
                st7789_draw_string(15, 290, "e:Draw p:Play q:Quit", WIN_TEXT, WIN_FACE);
                st7789_draw_rect(13, 28, 214, 236, WIN_SHADOW); 
                st7789_draw_rect(14, 29, 212, 234, COLOR_BLACK); 
                
                // Redraw our custom blocks exactly where we left them
                for (int r = 0; r < 5; r++) {
                    for (int c = 0; c < 8; c++) {
                        if (custom_blocks[r][c]) {
                            st7789_draw_rect(c * 25 + 20, r * 20 + 40, 23, 17, colors[r]);
                        }
                    }
                }
            }
        }
    }
}

void st7789_run_block_breaker() {
    // 1. Draw the Desktop and the main App Window
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(5, 5, 230, 310, "Brick.exe");

    // 2. Status Bar at the bottom for Score and Info
    st7789_draw_string(15, 290, "Blk:40", WIN_TEXT, WIN_FACE);
    st7789_draw_string(85, 290, "Pts:0", WIN_TEXT, WIN_FACE);
    st7789_draw_string(160, 290, "['q'=Quit]", WIN_TEXT, WIN_FACE);

    // 3. Draw Sunken Black Game Canvas (212 wide x 234 tall, starting at X: 14, Y: 29)
    st7789_draw_rect(13, 28, 214, 236, WIN_SHADOW); 
    st7789_draw_rect(14, 29, 212, 234, COLOR_BLACK); 

    unsigned char blocks[5][8];
    unsigned short colors[5] = {COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_MAGENTA};
    
    int blocks_left = 40; 
    int game_state = 0;   
    int score = 0;
    int show_pop_up = 0;
    char ui_buf[10];

    // Initialize and draw blocks inside the new canvas coordinates (Offset X by 18, Y by 35)
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 8; c++) {
            blocks[r][c] = 1;
            int bx = c * 24 + 18; 
            int by = r * 12 + 35;
            st7789_draw_rect(bx, by, 22, 10, colors[r]);
        }
    }

    int ball_x = 120, ball_y = 150;
    int ball_dx = 4, ball_dy = -4; 
    
    int paddle_w = 46, paddle_h = 6;
    int paddle_x = 97, paddle_y = 245; // Positioned safely inside the 234px tall canvas

    st7789_draw_rect(paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLUE);

    while (1) {
        int old_paddle_x = paddle_x;

        if (uart_has_char()) {
            char in = uart_getchar();
            if (in == 'q') return; 
            
            if (game_state == 0) {
                if (in == 'a') {
                    paddle_x -= 15;
                    if (paddle_x < 15) paddle_x = 15; // Bound to left canvas wall
                }
                if (in == 'd') {
                    paddle_x += 15;
                    if (paddle_x > 224 - paddle_w) paddle_x = 224 - paddle_w; // Bound to right canvas wall
                }
            }
        }

        if (game_state != 0) {
            // Draw Win2K pop-up dialog box once upon end state
            if (show_pop_up == 0) {
                draw_win2k_window(45, 115, 150, 70, "Info");
                if (game_state == 1) {
                    st7789_draw_string(75, 142, "GAME OVER!", WIN_TEXT, WIN_FACE);
                } else if (game_state == 2) {
                    st7789_draw_string(85, 142, "YOU WIN!", WIN_TEXT, WIN_FACE);
                }
                show_pop_up = 1;
            }
            delay_us(100000); 
            continue; 
        }

        // Erase old ball
        st7789_draw_rect(ball_x, ball_y, 4, 4, COLOR_BLACK);

        if (old_paddle_x != paddle_x) {
            // Erase old paddle
            st7789_draw_rect(old_paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLACK);
        }

        ball_x += ball_dx;
        ball_y += ball_dy;

        // Canvas Wall Collisions (Left: 14, Right: 226, Top: 29)
        if (ball_x <= 14) { ball_x = 14; ball_dx = -ball_dx; }
        if (ball_x >= 222) { ball_x = 222; ball_dx = -ball_dx; }
        if (ball_y <= 29) { ball_y = 29; ball_dy = -ball_dy; } 

        // Bottom canvas boundary = Game Over
        if (ball_y >= 260) {
            game_state = 1; 
            continue;
        }

        // Paddle Collision
        if (ball_y + 4 >= paddle_y && ball_y <= paddle_y + paddle_h && 
            ball_x + 4 >= paddle_x && ball_x <= paddle_x + paddle_w) {
            ball_dy = -ball_dy;
            ball_y = paddle_y - 4; 
        }

        // Block Collision Loop
        int hit = 0;
        for (int r = 0; r < 5 && !hit; r++) {
            for (int c = 0; c < 8 && !hit; c++) {
                if (blocks[r][c]) {
                    int bx = c * 24 + 18;
                    int by = r * 12 + 35;
                    
                    if (ball_x + 4 >= bx && ball_x <= bx + 22 && 
                        ball_y + 4 >= by && ball_y <= by + 10) {
                        
                        blocks[r][c] = 0; 
                        st7789_draw_rect(bx, by, 22, 10, COLOR_BLACK); 
                        ball_dy = -ball_dy; 
                        hit = 1;
                        
                        // --- UI STATUS BAR UPDATE ---
                        blocks_left--;
                        score += 100;
                        
                        // Update Blocks Left Counter (at X: 55)
                        custom_itoa(blocks_left, ui_buf);
                        st7789_draw_string(55, 290, "   ", WIN_TEXT, WIN_FACE); 
                        st7789_draw_string(55, 290, ui_buf, WIN_TEXT, WIN_FACE);
                        
                        // Update Score Counter (at X: 120)
                        custom_itoa(score, ui_buf);
                        st7789_draw_string(120, 290, "     ", WIN_TEXT, WIN_FACE); 
                        st7789_draw_string(120, 290, ui_buf, WIN_TEXT, WIN_FACE);
                        
                        if (blocks_left == 0) game_state = 2;
                    }
                }
            }
        }

        // Draw new ball and paddle positions
        st7789_draw_rect(ball_x, ball_y, 4, 4, COLOR_WHITE);
        st7789_draw_rect(paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLUE);

        delay_us(30000); 
    }
}

// Bare-metal time formatter: Converts raw seconds into an "HH:MM:SS" string
void custom_format_time(unsigned int total_seconds, char* out_buf) {
    unsigned int h = total_seconds / 3600;
    unsigned int m = (total_seconds % 3600) / 60;
    unsigned int s = total_seconds % 60;

    // Cap at 99 hours to prevent buffer overflow
    if (h > 99) h = 99; 

    // Manually build the ASCII string with leading zeros
    out_buf[0] = '0' + (h / 10);
    out_buf[1] = '0' + (h % 10);
    out_buf[2] = ':';
    out_buf[3] = '0' + (m / 10);
    out_buf[4] = '0' + (m % 10);
    out_buf[5] = ':';
    out_buf[6] = '0' + (s / 10);
    out_buf[7] = '0' + (s % 10);
    out_buf[8] = '\0'; // Null terminator
}



void st7789_run_sys_monitor() {
    // 1. Draw the Desktop and the main App Window
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(5, 5, 230, 310, "Taskmgr.exe");

    // 2. Static System Information Text
    st7789_draw_string(15, 30, "Clock: 40MHz XTAL", WIN_TEXT, WIN_FACE);
    st7789_draw_string(15, 45, "SRAM:  32768 Bytes", WIN_TEXT, WIN_FACE);

    st7789_draw_string(15, 65, "Used:", WIN_TEXT, WIN_FACE);
    st7789_draw_string(115, 65, "Up:", WIN_TEXT, WIN_FACE);
    st7789_draw_string(15, 80, "CPU Load:", WIN_TEXT, WIN_FACE);

    // 3. CPU Graph UI (Sunken Canvas)
    st7789_draw_string(15, 100, "CPU Usage History", WIN_TEXT, WIN_FACE);
    st7789_draw_rect(14, 114, 202, 52, WIN_SHADOW);   
    st7789_draw_rect(15, 115, 200, 50, COLOR_BLACK);  

    // 4. Memory Graph UI (Sunken Canvas)
    st7789_draw_string(15, 180, "Memory Usage History", WIN_TEXT, WIN_FACE);
    st7789_draw_rect(14, 194, 202, 52, WIN_SHADOW);   
    st7789_draw_rect(15, 195, 200, 50, COLOR_BLACK);  

    // 5. Status Bar
    st7789_draw_string(15, 290, "['q' = Exit Taskmgr]", WIN_TEXT, WIN_FACE);

    unsigned char cpu_hist[200];
    unsigned short sram_hist[200];
    for(int i = 0; i < 200; i++) { 
        cpu_hist[i] = 0; 
        sram_hist[i] = 0; 
    }

    char buf[16];

    while(1) {
        unsigned int start = get_ccount();
        unsigned int free_m = get_free_heap();
        unsigned int used_m = HEAP_SIZE - free_m;

        // Dynamic Text: Used RAM
        custom_itoa(used_m, buf);
        st7789_draw_string(55, 65, "      ", WIN_TEXT, WIN_FACE); 
        st7789_draw_string(55, 65, buf, WIN_TEXT, WIN_FACE);

        // Dynamic Text: Uptime
        // Dynamic Text: Uptime (Formatted as HH:MM:SS)
        update_uptime();
        custom_format_time(global_uptime_sec, buf); // Use our new formatter!
        
        // Use 8 spaces to cleanly erase the 8-character "HH:MM:SS" string
        st7789_draw_string(140, 65, "        ", WIN_TEXT, WIN_FACE); 
        st7789_draw_string(140, 65, buf, WIN_TEXT, WIN_FACE);

        if (uart_has_char()) {
            if (uart_getchar() == 'q') return;
        }

        unsigned int end = get_ccount();
        unsigned int act = end - start;
        unsigned int idle = 100000 * CYCLES_PER_US;
        unsigned int tot = act + idle;
        unsigned int cpu = (act * 100) / tot;

        // Dynamic Text: CPU %
        custom_itoa(cpu, buf);
        
        // Dynamically append the '%' sign directly to the end of the number string
        int i = 0;
        while(buf[i] != '\0') {
            i++;
        }
        buf[i] = '%';
        buf[i+1] = '\0';

        st7789_draw_string(85, 80, "    ", WIN_TEXT, WIN_FACE); // Erase old value cleanly
        st7789_draw_string(85, 80, buf, WIN_TEXT, WIN_FACE);    // Draw "7%" or "100%" natively
        // Shift History Buffers
        for (int i = 0; i < 199; i++) {
            cpu_hist[i] = cpu_hist[i + 1];
            sram_hist[i] = sram_hist[i + 1];
        }
        cpu_hist[199] = (cpu > 100) ? 100 : cpu;
        sram_hist[199] = used_m;

        // --- NEW LINE GRAPH RENDERING ---
        for (int i = 0; i < 200; i++) {
            unsigned int c_h = (cpu_hist[i] * 50) / 100;
            unsigned int s_h = (sram_hist[i] * 50) / HEAP_SIZE;

            // Bounds check so the pixels don't draw outside the canvas
            if (c_h >= 50) c_h = 49;
            if (s_h >= 50) s_h = 49;

            // CPU Line Graph (Base Y: 115)
            st7789_draw_vline(15 + i, 115, 50, COLOR_BLACK); // Wipe column
            // Draw a 2-pixel tall dot exactly at the peak height
            st7789_draw_vline(15 + i, 115 + (48 - c_h), 2, COLOR_GREEN); 

            // Memory Line Graph (Base Y: 195)
            st7789_draw_vline(15 + i, 195, 50, COLOR_BLACK); // Wipe column
            // Draw a 2-pixel tall dot exactly at the peak height
            st7789_draw_vline(15 + i, 195 + (48 - s_h), 2, COLOR_YELLOW);
        }
        delay_us(100000); 
    }
}

const unsigned short tetris_pieces[7][4] = {
    { 0x0F00, 0x2222, 0x0F00, 0x2222 }, // I
    { 0x44C0, 0x8E00, 0x6440, 0x0E20 }, // J
    { 0x4460, 0x0E80, 0xC440, 0x2E00 }, // L
    { 0xCC00, 0xCC00, 0xCC00, 0xCC00 }, // O
    { 0x06C0, 0x8C40, 0x06C0, 0x8C40 }, // S
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 }, // T
    { 0x0C60, 0x4C80, 0x0C60, 0x4C80 }  // Z
};

const unsigned short tetris_colors[7] = {
    COLOR_CYAN, COLOR_BLUE, COLOR_ORANGE, COLOR_YELLOW, 
    COLOR_GREEN, COLOR_MAGENTA, COLOR_RED
};

// Helper: Checks if the 4x4 bitmask hits a wall or locked block
int tetris_check_collision(unsigned short board[20][10], int piece, int rot, int px, int py) {
    unsigned short mask = tetris_pieces[piece][rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (mask & (1 << (15 - (r * 4 + c)))) {
                int bx = px + c;
                int by = py + r;
                if (bx < 0 || bx >= 10 || by >= 20) return 1; 
                if (by >= 0 && board[by][bx]) return 1; 
            }
        }
    }
    return 0;
}

// Helper: Renders the bitmask to the screen
void tetris_draw_piece(int piece, int rot, int px, int py, unsigned short color) {
    unsigned short mask = tetris_pieces[piece][rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (mask & (1 << (15 - (r * 4 + c)))) {
                
                int bx = px + c; // Pure Grid X (0 to 9)
                int by = py + r; // Pure Grid Y (0 to 19)
                
                if (by >= 0) { // Only draw if inside the board
                    // Convert grid to pixels right as we draw!
                    // X starts at 20, Y starts at 60
                    st7789_draw_rect(20 + (bx * 10), 60 + (by * 10), 9, 9, color);
                }
            }
        }
    }
}

// Helper: Draws the "Next" piece centered in the UI preview box on the right
void tetris_draw_next_piece(int piece, unsigned short color) {
    unsigned short mask = tetris_pieces[piece][0]; // Always show default rotation
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (mask & (1 << (15 - (r * 4 + c)))) {
                // Added a +6 pixel shift to center the shape inside the 54x54 preview box
                st7789_draw_rect(150 + 6 + (c * 10), 85 + (r * 10), 9, 9, color);
            }
        }
    }
}

void st7789_run_tetris() {
    // 1. Draw the Desktop and the main App Window
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(10, 10, 220, 300, "Tetris.exe");

    // 2. Status Bar at the bottom
    st7789_draw_string(20, 285, "Score: 0", WIN_TEXT, WIN_FACE);
    st7789_draw_string(135, 285, "['q'=Quit]", WIN_TEXT, WIN_FACE);

    // 3. Draw Board 3D Borders (Shifted Left to X: 20)
    st7789_draw_rect(18, 58, 104, 204, WIN_SHADOW);  // Sunken outer bevel
    st7789_draw_rect(19, 59, 102, 202, WIN_HILITE);  // Sunken inner bevel
    st7789_draw_rect(20, 60, 100, 200, COLOR_BLACK); // Inner playfield

    // 4. Draw Right Side UI (Centered nicely in the new empty space)
    st7789_draw_string(158, 58, "NEXT", WIN_TEXT, WIN_FACE);
    st7789_draw_rect(148, 78, 54, 54, WIN_SHADOW); 
    st7789_draw_rect(149, 79, 52, 52, COLOR_BLACK);
    
    unsigned short board[20][10];
    for (int r = 0; r < 20; r++)
        for (int c = 0; c < 10; c++)
            board[r][c] = 0;

    if (prng_state == 0) prng_state = get_ccount();

    int cur_p = custom_rand() % 7;
    int next_p = custom_rand() % 7; 
    int cur_r = 0;
    int cur_x = 3, cur_y = -3; 
    
    int gravity_timer = 0;
    int game_over = 0;
    int show_game_over = 0; 
    
    int score = 0;
    char score_buf[10];

    tetris_draw_next_piece(next_p, tetris_colors[next_p]);

    while (1) {
        if (uart_has_char()) {
            char in = uart_getchar();
            if (in == 'q') return;

            if (!game_over) {
                tetris_draw_piece(cur_p, cur_r, cur_x, cur_y, COLOR_BLACK); 

                if (in == 'a' && !tetris_check_collision(board, cur_p, cur_r, cur_x - 1, cur_y)) cur_x--;
                if (in == 'd' && !tetris_check_collision(board, cur_p, cur_r, cur_x + 1, cur_y)) cur_x++;
                if (in == 's' && !tetris_check_collision(board, cur_p, cur_r, cur_x, cur_y + 1)) cur_y++; 
                if (in == 'w') { 
                    int new_r = (cur_r + 1) % 4;
                    if (!tetris_check_collision(board, cur_p, new_r, cur_x, cur_y)) {
                        cur_r = new_r;
                    }
                }
            }
        }

        if (game_over) {
            if (show_game_over == 0) {
                draw_win2k_window(45, 120, 150, 60, "Info");
                st7789_draw_string(80, 145, "GAME OVER!", WIN_TEXT, WIN_FACE);
                show_game_over = 1;
            }
            delay_us(100000);
            continue;
        }

        gravity_timer++;
        if (gravity_timer >= 15) { 
            gravity_timer = 0;
            tetris_draw_piece(cur_p, cur_r, cur_x, cur_y, COLOR_BLACK); 

            if (!tetris_check_collision(board, cur_p, cur_r, cur_x, cur_y + 1)) {
                cur_y++;
            } else {
                unsigned short mask = tetris_pieces[cur_p][cur_r];
                for (int r = 0; r < 4; r++) {
                    for (int c = 0; c < 4; c++) {
                        if (mask & (1 << (15 - (r * 4 + c)))) {
                            int bx = cur_x + c;
                            int by = cur_y + r;
                            if (by < 0) game_over = 1; 
                            else {
                                board[by][bx] = tetris_colors[cur_p];
                                // Shifted block drawing to X: 20
                                st7789_draw_rect(20 + bx * 10, 60 + by * 10, 9, 9, tetris_colors[cur_p]);
                            }
                        }
                    }
                }

                int redraw_board = 0;
                int lines_cleared_this_turn = 0;

                for (int r = 19; r >= 0; r--) {
                    int full = 1;
                    for (int c = 0; c < 10; c++) {
                        if (board[r][c] == 0) {
                            full = 0;
                            break;
                        }
                    }
                    
                    if (full) {
                        lines_cleared_this_turn++;
                        for (int sr = r; sr > 0; sr--) {
                            for (int c = 0; c < 10; c++) {
                                board[sr][c] = board[sr - 1][c];
                            }
                        }
                        for (int c = 0; c < 10; c++) {
                            board[0][c] = 0;
                        }
                        r++; 
                    }
                }

                if (lines_cleared_this_turn > 0) {
                    redraw_board = 1;
                    score += (lines_cleared_this_turn * 100);
                    
                    custom_itoa(score, score_buf);
                    st7789_draw_string(20, 285, "Score:       ", WIN_TEXT, WIN_FACE); 
                    st7789_draw_string(76, 285, score_buf, WIN_TEXT, WIN_FACE); 
                }

                if (redraw_board) {
                    // Shifted board wipe to X: 20
                    st7789_draw_rect(20, 60, 100, 200, COLOR_BLACK);
                    for (int r = 0; r < 20; r++) {
                        for (int c = 0; c < 10; c++) {
                            if (board[r][c]) {
                                // Shifted redraw to X: 20
                                st7789_draw_rect(20 + c * 10, 60 + r * 10, 9, 9, board[r][c]);
                            }
                        }
                    }
                }

                tetris_draw_next_piece(next_p, COLOR_BLACK); 
                
                cur_p = next_p;
                next_p = custom_rand() % 7;
                cur_r = 0;
                cur_x = 3; cur_y = -3;
                
                tetris_draw_next_piece(next_p, tetris_colors[next_p]); 

                if (tetris_check_collision(board, cur_p, cur_r, cur_x, cur_y)) game_over = 1;
            }
        }

        if (!game_over) {
            tetris_draw_piece(cur_p, cur_r, cur_x, cur_y, tetris_colors[cur_p]);
        }

        delay_us(33000); 
    }
}

const char *game_items[] = {
    "Snake",
    "Tetris",
    "Block Breaker"
};
#define TOTAL_GAMES 3


const char *menu_items[] = {
    "System Monitor",
    "Text Editor",
    "Games",
    "File Explorer",
    "About OS"
};

const int TOTAL_MENU_ITEMS = 5;

// 1. Draws the heavy background (Call this ONCE on boot, or when exiting an app)
void st7789_draw_desktop() {
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(10, 10, 220, 300, "Programs");
}

void st7789_draw_single_item(int index, int is_selected) {
    int start_y = 40;
    int start_x = 20;
    int y_pos = start_y + (index * 20);
    
    if (is_selected) {
        // Draw blue highlight 
        st7789_draw_rect(start_x, y_pos - 2, 180, 16, WIN_TITLE);
        st7789_draw_string(start_x + 4, y_pos, menu_items[index], WIN_TEXT_SEL, WIN_TITLE);
    } else {
        // Draw gray unselected
        st7789_draw_rect(start_x, y_pos - 2, 180, 16, WIN_FACE);
        st7789_draw_string(start_x + 4, y_pos, menu_items[index], WIN_TEXT, WIN_FACE);
    }
}


// ==========================================
// 1. SNAKE ICON LAYERS (Green Body, White Eye, Red Tongue)
// ==========================================
const unsigned short SNAKE_BODY[16] = {
    0x0000, 0x07E0, 0x0FF0, 0x1C38, 0x1818, 0x1818, 0x0C30, 0x07E0,
    0x03C0, 0x0038, 0x0018, 0x1818, 0x1C38, 0x0FE0, 0x07C0, 0x0000
};
const unsigned short SNAKE_EYE[16] = {
    0x0000, 0x0000, 0x0400, 0x0400, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short SNAKE_TONGUE[16] = {
    0x0000, 0x0000, 0x0000, 0x0004, 0x000E, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

// ==========================================
// 2. TETRIS ICON LAYERS (Cyan, Yellow, and Red Blocks)
// ==========================================
const unsigned short TETRIS_CYAN[16] = {
    0x0000, 0x0F00, 0x0F00, 0x0F00, 0x0F00, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short TETRIS_YELLOW[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00F0,
    0x00F0, 0x00F0, 0x00F0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short TETRIS_RED[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0xF000, 0xF000, 0x0000, 0x0000, 0x0000
};

// ==========================================
// 3. BRICK STUDIO LAYERS (Yellow/Red Bricks, White Ball, Blue Paddle)
// ==========================================
const unsigned short BRICK_YELLOW[16] = {
    0x0000, 0x7FFE, 0x7FFE, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
}; 
const unsigned short BRICK_RED[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x3FFC, 0x3FFC, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
}; 
const unsigned short BRICK_WHITE[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0180,
    0x03C0, 0x03C0, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
}; 
const unsigned short BRICK_BLUE[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FF8, 0x1FF8, 0x0000
}; 


// High-Speed Bitwise Renderer
void st7789_draw_hex_icon(int x, int y, const unsigned short* icon, unsigned short color) {
    for (int r = 0; r < 16; r++) {
        unsigned short row = icon[r];
        for (int c = 0; c < 16; c++) {
            // Check if the specific bit is a 1
            if (row & (1 << (15 - c))) {
                st7789_draw_rect(x + c, y + r, 1, 1, color); // Light up the pixel!
            }
            // 0s do nothing, making them completely transparent over the UI
        }
    }
}

// Draws ONLY one specific line of the games menu (Flicker-Free, Multi-Colored Icons!)
void st7789_draw_single_game(int index, int is_selected) {
    int start_y = 65; 
    int start_x = 35;
    int y_pos = start_y + (index * 20);
    
    // 1. Draw the background and text
    if (is_selected) {
        st7789_draw_rect(start_x, y_pos - 2, 170, 16, WIN_TITLE); // Blue Highlight
        st7789_draw_string(start_x + 24, y_pos, game_items[index], WIN_TEXT_SEL, WIN_TITLE);
    } else {
        st7789_draw_rect(start_x, y_pos - 2, 170, 16, WIN_FACE);  // Gray Standard
        st7789_draw_string(start_x + 24, y_pos, game_items[index], WIN_TEXT, WIN_FACE);
    }

    // 2. Stack the Colorful Hex Masks based on the game index
    int ix = start_x + 4; // X coordinate for the icon
    int iy = y_pos - 2;   // Y coordinate for the icon

    if (index == 0) { 
        // SNAKE: Green Body, White Eye, Red Tongue
        st7789_draw_hex_icon(ix, iy, SNAKE_BODY, COLOR_GREEN);
        st7789_draw_hex_icon(ix, iy, SNAKE_EYE, WIN_TEXT_SEL); 
        st7789_draw_hex_icon(ix, iy, SNAKE_TONGUE, COLOR_RED);
    }
    else if (index == 1) { 
        // TETRIS: Cyan, Yellow, and Red Blocks
        st7789_draw_hex_icon(ix, iy, TETRIS_CYAN, COLOR_CYAN);
        st7789_draw_hex_icon(ix, iy, TETRIS_YELLOW, COLOR_YELLOW);
        st7789_draw_hex_icon(ix, iy, TETRIS_RED, COLOR_RED);
    }
    else if (index == 2) { 
        // BRICK STUDIO: Yellow/Red Bricks, White Ball, Blue Paddle
        st7789_draw_hex_icon(ix, iy, BRICK_YELLOW, COLOR_YELLOW);
        st7789_draw_hex_icon(ix, iy, BRICK_RED, COLOR_RED);
        st7789_draw_hex_icon(ix, iy, BRICK_WHITE, WIN_TEXT_SEL);
        st7789_draw_hex_icon(ix, iy, BRICK_BLUE, COLOR_BLUE);
    }
}

void st7789_draw_full_games_list(int current_selection) {
    for (int i = 0; i < TOTAL_GAMES; i++) {
        st7789_draw_single_game(i, (i == current_selection));
    }
}

// The Main Games App Router
void st7789_run_games_menu() {
    int game_selection = 0;

    // 1. Draw the pop-up window UI over the desktop
    st7789_fill_screen(WIN_DESKTOP);
    draw_win2k_window(20, 30, 200, 200, "Entertainment"); 
    
    // 2. Draw the list (FIXED FUNCTION NAME)
    st7789_draw_full_games_list(game_selection);

    // 3. The non-blocking local input loop
    while (1) {
        if ((*UART0_STATUS_REG & 0xFF) > 0) {
            char in = uart_getchar();
            int prev_selection = game_selection;

            // Navigation
            if (in == 'w') {
                game_selection--;
                if (game_selection < 0) game_selection = TOTAL_GAMES - 1;
                st7789_draw_single_game(prev_selection, 0); 
                st7789_draw_single_game(game_selection, 1); 
            } 
            else if (in == 's') {
                game_selection++;
                if (game_selection >= TOTAL_GAMES) game_selection = 0;
                st7789_draw_single_game(prev_selection, 0); 
                st7789_draw_single_game(game_selection, 1); 
            } 
            // Execution
            else if (in == 'e') {
                if (game_selection == 0) {
                    st7789_run_snake();
                } 
                else if (game_selection == 1) {
                    // FIXED: Index 1 is Tetris
                    st7789_run_tetris();
                }
                else if (game_selection == 2) {
                    st7789_run_block_studio(); 
                    // FIXED: Index 2 is BrickStudio
                }

                // If we returned from a game, instantly redraw the Entertainment Window
                if (game_selection != 3) {
                    st7789_fill_screen(WIN_DESKTOP);
                    draw_win2k_window(20, 30, 200, 200, "Entertainment");
                    st7789_draw_full_games_list(game_selection);
                }
            }
            else if (in == 'q') {
                // Quick exit back to main OS
                break;
            }
        }
    }
}

// 1. PC MONITOR (Taskmgr.exe)
const unsigned short PC_FRAME[16] = {
    0x0000, 0x7FFE, 0x4002, 0x4002, 0x4002, 0x4002, 0x4002, 0x4002,
    0x4002, 0x4002, 0x7FFE, 0x0180, 0x0180, 0x0FF0, 0x0000, 0x0000
};
const unsigned short PC_SCREEN[16] = {
    0x0000, 0x0000, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC, 0x3FFC,
    0x3FFC, 0x3FFC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short PC_GRAPH[16] = {
    0x0000, 0x0000, 0x0000, 0x0060, 0x0190, 0x0608, 0x1804, 0x2000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

// 2. TEXT DOCUMENT (Notepad.exe)
const unsigned short FILE_PAPER[16] = {
    0x0000, 0x1FC0, 0x1FE0, 0x1FF0, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8,
    0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x1FF8, 0x0000
};
const unsigned short FILE_FOLD[16] = {
    0x0000, 0x0030, 0x0010, 0x0008, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short FILE_LINES[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0FE0, 0x0000, 0x07E0,
    0x0000, 0x0FE0, 0x0000, 0x03E0, 0x0000, 0x0FE0, 0x0000, 0x0000
};

// 3. FOLDER (Explorer.exe)
const unsigned short FOLDER_BACK[16] = {
    0x0000, 0x0000, 0x1F00, 0x3FF8, 0x3FF8, 0x3FF8, 0x3FF8, 0x3FF8,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short FOLDER_PAPER[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x1FF0, 0x1FF0, 0x1FF0, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short FOLDER_FRONT[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7FFE,
    0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x0000, 0x0000
};

// 4. ARCADE CABINET (Games)
const unsigned short ARCADE_BODY[16] = {
    0x0000, 0x0FF0, 0x1FF8, 0x1FF8, 0x1818, 0x1818, 0x1818, 0x1818,
    0x1FF8, 0x3FFC, 0x3FFC, 0x1FF8, 0x1FF8, 0x1FF8, 0x0FF0, 0x0000
};
const unsigned short ARCADE_SCREEN[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
const unsigned short ARCADE_CONTROLS[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x1248, 0x0000, 0x0000, 0x0A50, 0x0000, 0x0000, 0x0000
};

// 5. INFO / ABOUT (System Info)
const unsigned short INFO_BASE[16] = {
    0x0000, 0x07E0, 0x1FF8, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE,
    0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000, 0x0000
};
const unsigned short INFO_MARK[16] = {
    0x0000, 0x0000, 0x0000, 0x0180, 0x0180, 0x0000, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

// Draws ONLY one specific line of the Main OS menu with non-overlapping spacing
void st7789_draw_main_menu_item(int index, int is_selected, const char* label) {
    int start_y = 45; 
    int start_x = 25;
    int y_pos = start_y + (index * 22); // 22px spacing for the 16x16 icons
    
    // 1. Draw background block and text
    if (is_selected) {
        st7789_draw_rect(start_x, y_pos - 2, 170, 18, WIN_TITLE);
        st7789_draw_string(start_x + 24, y_pos + 1, label, WIN_TEXT_SEL, WIN_TITLE);
    } else {
        st7789_draw_rect(start_x, y_pos - 2, 170, 18, WIN_FACE);
        st7789_draw_string(start_x + 24, y_pos + 1, label, WIN_TEXT, WIN_FACE);
    }

    // 2. Stack the Colorful Hex Masks cleanly inside the row bounds
    int ix = start_x + 4; 
    int iy = y_pos - 1;   

    if (index == 0) { 
        // TASKMGR (PC Monitor)
        st7789_draw_hex_icon(ix, iy, PC_FRAME, COLOR_WHITE);
        st7789_draw_hex_icon(ix, iy, PC_SCREEN, COLOR_BLUE); 
        st7789_draw_hex_icon(ix, iy, PC_GRAPH, COLOR_GREEN);
    }
    else if (index == 1) { 
        // EDITOR (Text File)
        st7789_draw_hex_icon(ix, iy, FILE_PAPER, COLOR_WHITE);
        st7789_draw_hex_icon(ix, iy, FILE_FOLD, COLOR_GRAY);
        st7789_draw_hex_icon(ix, iy, FILE_LINES, COLOR_BLUE);
    }
    else if (index == 2) { 
        // GAMES (Arcade)
        st7789_draw_hex_icon(ix, iy, ARCADE_BODY, COLOR_RED);
        st7789_draw_hex_icon(ix, iy, ARCADE_SCREEN, COLOR_CYAN);
        st7789_draw_hex_icon(ix, iy, ARCADE_CONTROLS, COLOR_YELLOW);
    }
    else if (index == 3) { 
        // EXPLORER (Folder)
        st7789_draw_hex_icon(ix, iy, FOLDER_BACK, COLOR_YELLOW);
        st7789_draw_hex_icon(ix, iy, FOLDER_PAPER, COLOR_WHITE);
        // Orange looks best for the front flap, but RED or MAGENTA work if ORANGE isn't defined
        st7789_draw_hex_icon(ix, iy, FOLDER_FRONT, COLOR_RED); 
    }
    else if (index == 4) { 
        // ABOUT (Info Circle)
        unsigned short base_color = is_selected ? WIN_TEXT_SEL : WIN_TITLE;
        unsigned short mark_color = is_selected ? WIN_TITLE : WIN_TEXT_SEL;
        
        st7789_draw_hex_icon(ix, iy, INFO_BASE, base_color);
        st7789_draw_hex_icon(ix, iy, INFO_MARK, mark_color); // Draws the "i" hole inside the circle
    }
}

#define TOTAL_MAIN_ITEMS 5

// The loop that draws all 5 icons to the screen
void st7789_draw_full_menu(int current_selection) {
    // 1. Wipe the inside of the main window clean (prevents ghosting)
    st7789_draw_rect(20, 40, 200, 250, WIN_FACE);
    
    // 2. Loop through all 5 items and draw them using the NEW icon renderer
    for (int i = 0; i < TOTAL_MAIN_ITEMS; i++) {
        st7789_draw_main_menu_item(i, (i == current_selection), menu_items[i]);
    }
}

void kernel_main() {
    // 1. Hardware Watchdog Disable & Timer Init
    *(volatile unsigned int *)0x3FF480A4 = 0x50D83AA1;
    *(volatile unsigned int *)0x3FF4808C = 0;
    *(volatile unsigned int *)0x3FF5F064 = 0x50D83AA1;
    *(volatile unsigned int *)0x3FF5F048 = 0;
    last_ccount = get_ccount();

    // 2. Subsystem Initialization
    spi_init();
    st7789_init();
    // 3. OS State Tracker Setup
    int current_selection = 0;
    os_state_t current_state = STATE_MENU;

    // 4. Draw Initial OS Desktop
    st7789_draw_desktop();
    st7789_draw_full_menu(current_selection);

    // 5. The Core Kernel Loop
    while(1) {
        char input = uart_getchar();
        if (current_state == STATE_MENU) {            
            // --- NAVIGATION ---
            if (input == 'w') {
                int prev_selection = current_selection;
                current_selection--;
                if (current_selection < 0) current_selection = TOTAL_MAIN_ITEMS - 1;
                
                // Redraw ONLY the two lines that changed using the new icon function
                st7789_draw_main_menu_item(prev_selection, 0, menu_items[prev_selection]); 
                st7789_draw_main_menu_item(current_selection, 1, menu_items[current_selection]); 
            } 
            else if (input == 's') {
                int prev_selection = current_selection;
                current_selection++;
                if (current_selection >= TOTAL_MAIN_ITEMS) current_selection = 0;
                
                // Redraw ONLY the two lines that changed using the new icon function
                st7789_draw_main_menu_item(prev_selection, 0, menu_items[prev_selection]); 
                st7789_draw_main_menu_item(current_selection, 1, menu_items[current_selection]); 
            } 
            
            // --- APP EXECUTION ROUTER ---
            else if (input == 'e') { 
                
                // 1. Update state and branch to the specific app
                if (current_selection == 0) {
                    current_state = STATE_APP_MONITOR;
                    st7789_run_sys_monitor();
                } 
                else if (current_selection == 1) {
                    current_state = STATE_APP_EDITOR;
                    run_dynamic_text_editor();
                } 
                else if (current_selection == 2) {
                    current_state = STATE_APP_GAMES;
                    st7789_run_games_menu();
                } 
                else if (current_selection == 3) {
                    current_state = STATE_APP_EXPLORER; 
                    run_ram_file_browser();             
                } 
                else if (current_selection == 4) {
                    current_state = STATE_APP_ABOUT;
                    st7789_run_app_placeholder("ABOUT", "Bare-Metal OS");
                }
                
                // 2. The app has finished and returned control to the kernel.
                // Instantly restore the kernel state and redraw the desktop!
                current_state = STATE_MENU;         
                st7789_draw_desktop();              
                st7789_draw_full_menu(current_selection); 
            }
        } 
    }
}