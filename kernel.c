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

    st7789_fill_screen(0x0000);

    while(1) {
        
        // ==========================================
        // STATE 0: THE FILE LIST
        // ==========================================
        if (browser_state == 0) {
            if (previous_cursor != cursor) {
                st7789_draw_string(0, 0, "-- RAM DISK FILES --", 0x07E0, 0x0000);
                
                if (ram_file_count == 0) {
                    st7789_draw_string(0, 24, "DISK EMPTY", 0xF800, 0x0000);
                } else {
                    for (int i = 0; i < ram_file_count; i++) {
                        int y_pos = 24 + (i * 16); 
                        unsigned int color = (i == cursor) ? 0xFFE0 : 0xFFFF; // Yellow / White
                        
                        if (i == cursor) {
                            st7789_draw_string(0, y_pos, "> FILE  ", color, 0x0000);
                            st7789_draw_char(64, y_pos, '1' + i, color, 0x0000);
                        } else {
                            st7789_draw_string(0, y_pos, "  FILE  ", color, 0x0000);
                            st7789_draw_char(64, y_pos, '1' + i, color, 0x0000);
                        }
                    }
                }
                previous_cursor = cursor;
            }
            
            char in = uart_getchar();
            
            if (in == 'w' && cursor > 0) cursor--;
            if (in == 's' && cursor < ram_file_count - 1) cursor++;
            if (in == 'e' && ram_file_count > 0) {
                browser_state = 1; // Jump to Action Menu
                previous_action = -1;
                action_cursor = 0;
                st7789_fill_screen(0x0000);
            }
            if (in == 'q') break; // Exit Explorer entirely
        } 
        
        // ==========================================
        // STATE 1: THE ACTION MENU
        // ==========================================
        else if (browser_state == 1) {
            if (previous_action != action_cursor) {
                st7789_draw_string(0, 0, "-- FILE OPTIONS --", 0x07E0, 0x0000);
                const char* actions[] = {"READ", "EDIT", "DELETE"};
                
                for (int i = 0; i < 3; i++) {
                    int y_pos = 24 + (i * 16);
                    unsigned int color = (i == action_cursor) ? 0xFFE0 : 0xFFFF;
                    
                    if (i == action_cursor) {
                        st7789_draw_string(0, y_pos, "> ", color, 0x0000);
                    } else {
                        st7789_draw_string(0, y_pos, "  ", color, 0x0000);
                    }
                    st7789_draw_string(16, y_pos, actions[i], color, 0x0000);
                }
                previous_action = action_cursor;
            }
            
            char in = uart_getchar();
            
            if (in == 'w' && action_cursor > 0) action_cursor--;
            if (in == 's' && action_cursor < 2) action_cursor++;
            if (in == 'q') {
                browser_state = 0; // Back to File List
                previous_cursor = -1;
                st7789_fill_screen(0x0000);
            }
            if (in == 'e') {
                if (action_cursor == 0) {
                    browser_state = 2; // Enter Read Mode
                    st7789_fill_screen(0x0000);
                } 
                else if (action_cursor == 1) {
                    browser_state = 3; // Enter Edit Mode
                    st7789_fill_screen(0x0000);
                } 
                else if (action_cursor == 2) {
                    // EXECUTING DELETE
                    custom_free(ram_disk[cursor].content); // Free the memory!
                    
                    // Shift the remaining files up to fill the gap
                    for (int i = cursor; i < ram_file_count - 1; i++) {
                        ram_disk[i] = ram_disk[i + 1];
                        ram_disk[i].id = i + 1;
                    }
                    ram_file_count--;
                    
                    // Keep cursor in bounds
                    if (cursor >= ram_file_count && cursor > 0) cursor--;
                    
                    // Return to File List
                    browser_state = 0;
                    previous_cursor = -1;
                    st7789_fill_screen(0x0000);
                }
            }
        }
        
        // ==========================================
        // STATE 2: READ MODE
        // ==========================================
        else if (browser_state == 2) {
            st7789_draw_string(0, 0, "-- VIEWING FILE --", 0x07E0, 0x0000);
            int len = custom_strlen(ram_disk[cursor].content);
            int start_index = 0;
            if (len > 1110) start_index = len - 1110;
            
            int r = 0;
            int c = 0;
            for (int i = start_index; i < len; i++) {
                st7789_draw_char(c * 8, (r * 8) + 24, ram_disk[cursor].content[i], 0xFFFF, 0x0000);
                c++;
                if (c >= 30) {
                    c = 0;
                    r++;
                }
            }
            
            char in = uart_getchar();
            if (in == 'q') {
                browser_state = 1; // Go back to Action Menu
                previous_action = -1;
                st7789_fill_screen(0x0000);
            }
        }
        
        // ==========================================
        // STATE 3: EDIT MODE (Append)
        // ==========================================
        else if (browser_state == 3) {
            st7789_draw_string(0, 0, "-- EDITING FILE --", 0x07E0, 0x0000);
            
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

            int r = 0;
            int c = 0;
            for (unsigned int i = 0; i < current_length; i++) {
                st7789_draw_char(c * 8, (r * 8) + 24, text_buffer[i], 0xFFFF, 0x0000);
                c++;
                if (c >= 30) { c = 0; r++; }
            }

            unsigned int last_blink = get_ccount();
            int cursor_state = 0;

            while (1) {
                if (get_ccount() - last_blink > 20000000) {
                    cursor_state = !cursor_state;
                    last_blink = get_ccount();
                    
                    r = current_length / 30;
                    c = current_length % 30;
                    if (current_length >= 1110) { r = 36; c = 29; }
                    
                    char draw_char = cursor_state ? '_' : ' ';
                    st7789_draw_char(c * 8, (r * 8) + 24, draw_char, 0xFFFF, 0x0000);
                }

                if ((*UART0_STATUS_REG & 0xFF) > 0) {
                    char in = uart_getchar();
                    
                    r = current_length / 30;
                    c = current_length % 30;
                    if (current_length >= 1110) { r = 36; c = 29; }
                    st7789_draw_char(c * 8, (r * 8) + 24, ' ', 0x0000, 0x0000);

                    if (in == 'q') {
                        browser_state = 1;
                        previous_action = -1;
                        st7789_fill_screen(0x0000);
                        break;
                    }

                    if (in == 127 || in == 8) {
                        if (current_length > 0) {
                            current_length--;
                            text_buffer[current_length] = '\0';
                            
                            r = current_length / 30;
                            c = current_length % 30;
                            st7789_draw_char(c * 8, (r * 8) + 24, ' ', 0x0000, 0x0000); 
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
                        
                        r = current_length / 30;
                        c = current_length % 30;
                        if (current_length < 1110) { 
                            st7789_draw_char(c * 8, (r * 8) + 24, in, 0xFFFF, 0x0000);
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
    st7789_fill_screen(COLOR_BLACK);
    st7789_draw_rect(0, 0, 240, 20, COLOR_GRAY);
    st7789_draw_string(5, 6, "SNAKE ['q'=Quit]", COLOR_YELLOW, COLOR_GRAY);
    
    st7789_draw_string(150, 6, "Score: 0", COLOR_WHITE, COLOR_GRAY);

    SnakeNode_t *head = (SnakeNode_t *)custom_malloc(sizeof(SnakeNode_t));
    if (!head) return;
    head->x = 12; 
    head->y = 16;
    head->next = (void*)0;

    int dx = 1, dy = 0;
    int apple_x = 0, apple_y = 0;
    int score = 0;
    int game_over = 0;
    char buf[8];

    prng_state = get_ccount();
    apple_x = (custom_rand() % 22) + 1; 
    apple_y = (custom_rand() % 28) + 3; 
    st7789_draw_rect(apple_x * 10, apple_y * 10, 10, 10, COLOR_RED);

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
                return;
            }
            if (!game_over) {
                if (in == 'w' && dy == 0) { dx = 0; dy = -1; }
                else if (in == 's' && dy == 0) { dx = 0; dy = 1; }
                else if (in == 'a' && dx == 0) { dx = -1; dy = 0; }
                else if (in == 'd' && dx == 0) { dx = 1; dy = 0; }
            }
        }

        if (game_over) {
            st7789_draw_string(80, 160, "GAME OVER", COLOR_RED, COLOR_BLACK);
            delay_us(100000);
            continue;
        }

        int next_x = head->x + dx;
        int next_y = head->y + dy;

        if (next_x < 0 || next_x >= 24 || next_y < 2 || next_y >= 32) {
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

        st7789_draw_rect(head->x * 10, head->y * 10, 10, 10, COLOR_GREEN);

        if (next_x == apple_x && next_y == apple_y) {
            score++;
            custom_itoa(score, buf);
            st7789_draw_string(150, 6, "Score:    ", COLOR_WHITE, COLOR_GRAY);
            st7789_draw_string(206, 6, buf, COLOR_YELLOW, COLOR_GRAY);

            int valid_apple = 0;
            while (!valid_apple) {
                apple_x = (custom_rand() % 22) + 1;
                apple_y = (custom_rand() % 28) + 3;
                valid_apple = 1;
                SnakeNode_t *c = head;
                while (c != (void*)0) {
                    if (c->x == apple_x && c->y == apple_y) valid_apple = 0;
                    c = c->next;
                }
            }
            st7789_draw_rect(apple_x * 10, apple_y * 10, 10, 10, COLOR_RED);
        } else {
            SnakeNode_t *curr = head;
            while (curr->next != (void*)0 && curr->next->next != (void*)0) {
                curr = curr->next;
            }
            st7789_draw_rect(curr->next->x * 10, curr->next->y * 10, 10, 10, COLOR_BLACK);
            custom_free(curr->next);
            curr->next = (void*)0;
        }

        delay_us(80000); 
    }
}
void st7789_run_block_breaker() {
    st7789_fill_screen(COLOR_BLACK);
    
    // Draw the Top UI Bar
    st7789_draw_rect(0, 0, 240, 20, COLOR_GRAY);
    st7789_draw_string(5, 6, "['q'=Quit]", COLOR_YELLOW, COLOR_GRAY);
    st7789_draw_string(95, 6, "Blk: 40", COLOR_WHITE, COLOR_GRAY);
    st7789_draw_string(165, 6, "Pts: 0", COLOR_WHITE, COLOR_GRAY);

    unsigned char blocks[5][8];
    unsigned short colors[5] = {COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_MAGENTA};
    
    int blocks_left = 40; 
    int game_state = 0;   
    int score = 0;
    char ui_buf[10];

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 8; c++) {
            blocks[r][c] = 1;
            int bx = c * 30 + 1;
            int by = r * 15 + 30;
            st7789_draw_rect(bx, by, 28, 13, colors[r]);
        }
    }

    int ball_x = 120, ball_y = 200;
    int ball_dx = 5, ball_dy = -5; 
    
    int paddle_w = 50, paddle_h = 6;
    int paddle_x = 95, paddle_y = 300;

    st7789_draw_rect(paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLUE);

    while (1) {
        int old_paddle_x = paddle_x;

        if (uart_has_char()) {
            char in = uart_getchar();
            if (in == 'q') return; 
            
            if (game_state == 0) {
                if (in == 'a') {
                    paddle_x -= 20;
                    if (paddle_x < 0) paddle_x = 0; 
                }
                if (in == 'd') {
                    paddle_x += 20;
                    if (paddle_x > 240 - paddle_w) paddle_x = 240 - paddle_w; 
                }
            }
        }

        if (game_state != 0) {
            if (game_state == 1) {
                st7789_draw_string(75, 160, "GAME OVER", COLOR_RED, COLOR_BLACK);
            } else if (game_state == 2) {
                st7789_draw_string(80, 160, "YOU WIN!", COLOR_GREEN, COLOR_BLACK);
            }
            delay_us(100000); 
            continue; 
        }

        st7789_draw_rect(ball_x, ball_y, 4, 4, COLOR_BLACK);

        if (old_paddle_x != paddle_x) {
            st7789_draw_rect(old_paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLACK);
        }

        ball_x += ball_dx;
        ball_y += ball_dy;

        if (ball_x <= 0) { ball_x = 0; ball_dx = -ball_dx; }
        if (ball_x >= 236) { ball_x = 236; ball_dx = -ball_dx; }
        if (ball_y <= 20) { ball_y = 20; ball_dy = -ball_dy; } // Bounces safely below the UI bar

        if (ball_y >= 316) {
            game_state = 1; 
            continue;
        }

        if (ball_y + 4 >= paddle_y && ball_y <= paddle_y + paddle_h && 
            ball_x + 4 >= paddle_x && ball_x <= paddle_x + paddle_w) {
            ball_dy = -ball_dy;
            ball_y = paddle_y - 4; 
        }

        int hit = 0;
        for (int r = 0; r < 5 && !hit; r++) {
            for (int c = 0; c < 8 && !hit; c++) {
                if (blocks[r][c]) {
                    int bx = c * 30 + 1;
                    int by = r * 15 + 30;
                    
                    if (ball_x + 4 >= bx && ball_x <= bx + 28 && 
                        ball_y + 4 >= by && ball_y <= by + 13) {
                        
                        blocks[r][c] = 0; 
                        st7789_draw_rect(bx, by, 28, 13, COLOR_BLACK); 
                        ball_dy = -ball_dy; 
                        hit = 1;
                        
                        // --- UI UPDATE LOGIC ---
                        blocks_left--;
                        score += 100;
                        
                        // Update Blocks Left Counter (starts at x=135)
                        custom_itoa(blocks_left, ui_buf);
                        st7789_draw_string(135, 6, "  ", COLOR_WHITE, COLOR_GRAY); // Erase old numbers
                        st7789_draw_string(135, 6, ui_buf, COLOR_WHITE, COLOR_GRAY);
                        
                        // Update Score Counter (starts at x=205)
                        custom_itoa(score, ui_buf);
                        st7789_draw_string(205, 6, "    ", COLOR_WHITE, COLOR_GRAY); // Erase old numbers
                        st7789_draw_string(205, 6, ui_buf, COLOR_WHITE, COLOR_GRAY);
                        
                        if (blocks_left == 0) game_state = 2;
                    }
                }
            }
        }

        st7789_draw_rect(ball_x, ball_y, 4, 4, COLOR_WHITE);
        st7789_draw_rect(paddle_x, paddle_y, paddle_w, paddle_h, COLOR_BLUE);

        delay_us(33000); 
    }
}


void st7789_run_sys_monitor() {
    st7789_fill_screen(COLOR_BLACK);
    st7789_draw_rect(0, 0, 240, 20, COLOR_BLUE);
    st7789_draw_string(5, 6, "SYS MONITOR ['q' = Exit]", COLOR_WHITE, COLOR_BLUE);

    st7789_draw_string(10, 30, "Clock: 40MHz XTAL", COLOR_CYAN, COLOR_BLACK);
    st7789_draw_string(10, 45, "SRAM:  32768 Bytes Total", COLOR_CYAN, COLOR_BLACK);

    st7789_draw_string(10, 110, "CPU Load %", COLOR_GRAY, COLOR_BLACK);
    st7789_draw_rect(10, 124, 202, 52, COLOR_WHITE);
    st7789_draw_rect(11, 125, 200, 50, COLOR_BLACK);

    st7789_draw_string(10, 190, "SRAM Usage", COLOR_GRAY, COLOR_BLACK);
    st7789_draw_rect(10, 204, 202, 52, COLOR_WHITE);
    st7789_draw_rect(11, 205, 200, 50, COLOR_BLACK);

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

        st7789_draw_string(10, 65, "Used: ", COLOR_YELLOW, COLOR_BLACK);
        custom_itoa(used_m, buf);
        st7789_draw_string(60, 65, "      ", COLOR_BLACK, COLOR_BLACK);
        st7789_draw_string(60, 65, buf, COLOR_WHITE, COLOR_BLACK);

        update_uptime();
        
        st7789_draw_string(120, 65, "Up: ", COLOR_MAGENTA, COLOR_BLACK);
        custom_itoa(global_uptime_sec, buf);
        st7789_draw_string(150, 65, "      ", COLOR_BLACK, COLOR_BLACK);
        st7789_draw_string(150, 65, buf, COLOR_WHITE, COLOR_BLACK);

        if (uart_has_char()) {
            if (uart_getchar() == 'q') return;
        }

        unsigned int end = get_ccount();
        unsigned int act = end - start;
        unsigned int idle = 100000 * CYCLES_PER_US;
        unsigned int tot = act + idle;
        unsigned int cpu = (act * 100) / tot;

        st7789_draw_string(10, 80, "CPU:  ", COLOR_RED, COLOR_BLACK);
        custom_itoa(cpu, buf);
        st7789_draw_string(60, 80, "    ", COLOR_BLACK, COLOR_BLACK);
        st7789_draw_string(60, 80, buf, COLOR_WHITE, COLOR_BLACK);
        st7789_draw_string(85, 80, "%", COLOR_WHITE, COLOR_BLACK);

        for (int i = 0; i < 199; i++) {
            cpu_hist[i] = cpu_hist[i + 1];
            sram_hist[i] = sram_hist[i + 1];
        }
        cpu_hist[199] = (cpu > 100) ? 100 : cpu;
        sram_hist[199] = used_m;

        for (int i = 0; i < 200; i++) {
            unsigned int c_h = (cpu_hist[i] * 50) / 100;
            unsigned int s_h = (sram_hist[i] * 50) / HEAP_SIZE;

            st7789_draw_vline(11 + i, 125, 50, COLOR_BLACK);
            if (c_h > 0) st7789_draw_vline(11 + i, 125 + (50 - c_h), c_h, COLOR_RED);

            st7789_draw_vline(11 + i, 205, 50, COLOR_BLACK);
            if (s_h > 0) st7789_draw_vline(11 + i, 205 + (50 - s_h), s_h, COLOR_GREEN);
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
                int bx = px + c;
                int by = py + r;
                if (by >= 0) { // Only draw if inside the board
                    st7789_draw_rect(70 + bx * 10, 60 + by * 10, 9, 9, color);
                }
            }
        }
    }
}

// Helper: Draws the "Next" piece in the UI area on the right
void tetris_draw_next_piece(int piece, unsigned short color) {
    unsigned short mask = tetris_pieces[piece][0]; // Always show default rotation
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (mask & (1 << (15 - (r * 4 + c)))) {
                st7789_draw_rect(180 + c * 10, 75 + r * 10, 9, 9, color);
            }
        }
    }
}

void st7789_run_tetris() {
    st7789_fill_screen(COLOR_BLACK);
    st7789_draw_rect(0, 0, 240, 20, COLOR_GRAY);
    st7789_draw_string(5, 6, "TETRIS ['q'=Quit]", COLOR_YELLOW, COLOR_GRAY);

    // Draw Board Borders (10x20 grid, 10px per block -> 100x200 total size)
    st7789_draw_rect(68, 58, 104, 204, COLOR_WHITE); // Outer border
    st7789_draw_rect(70, 60, 100, 200, COLOR_BLACK); // Inner playfield

    // Draw Right Side UI (spaced out safely to avoid bounding box collisions)
    st7789_draw_string(180, 60, "NEXT:", COLOR_WHITE, COLOR_BLACK);
    st7789_draw_string(175, 145, "SCORE:", COLOR_WHITE, COLOR_BLACK);
    st7789_draw_string(175, 160, "0", COLOR_YELLOW, COLOR_BLACK);
    
    unsigned short board[20][10];
    for (int r = 0; r < 20; r++)
        for (int c = 0; c < 10; c++)
            board[r][c] = 0;

    if (prng_state == 0) prng_state = get_ccount();

    int cur_p = custom_rand() % 7;
    int next_p = custom_rand() % 7; // Initialize next piece preview
    int cur_r = 0;
    int cur_x = 3, cur_y = -3; // Spawn slightly above the board
    
    int gravity_timer = 0;
    int game_over = 0;
    
    // UI Variables
    int score = 0;
    char score_buf[10];

    // Draw the very first next piece preview
    tetris_draw_next_piece(next_p, tetris_colors[next_p]);

    while (1) {
        if (uart_has_char()) {
            char in = uart_getchar();
            if (in == 'q') return;

            if (!game_over) {
                tetris_draw_piece(cur_p, cur_r, cur_x, cur_y, COLOR_BLACK); // Erase old

                if (in == 'a' && !tetris_check_collision(board, cur_p, cur_r, cur_x - 1, cur_y)) cur_x--;
                if (in == 'd' && !tetris_check_collision(board, cur_p, cur_r, cur_x + 1, cur_y)) cur_x++;
                if (in == 's' && !tetris_check_collision(board, cur_p, cur_r, cur_x, cur_y + 1)) cur_y++; // Soft drop
                if (in == 'w') { // Rotate
                    int new_r = (cur_r + 1) % 4;
                    if (!tetris_check_collision(board, cur_p, new_r, cur_x, cur_y)) {
                        cur_r = new_r;
                    }
                }
            }
        }

        if (game_over) {
            st7789_draw_string(85, 150, "GAME OVER", COLOR_RED, COLOR_BLACK);
            delay_us(100000);
            continue;
        }

        // Apply Gravity every ~500ms
        gravity_timer++;
        if (gravity_timer >= 15) { 
            gravity_timer = 0;
            tetris_draw_piece(cur_p, cur_r, cur_x, cur_y, COLOR_BLACK); // Erase old

            if (!tetris_check_collision(board, cur_p, cur_r, cur_x, cur_y + 1)) {
                cur_y++;
            } else {
                // LOCK PIECE INTO BOARD
                unsigned short mask = tetris_pieces[cur_p][cur_r];
                for (int r = 0; r < 4; r++) {
                    for (int c = 0; c < 4; c++) {
                        if (mask & (1 << (15 - (r * 4 + c)))) {
                            int bx = cur_x + c;
                            int by = cur_y + r;
                            if (by < 0) game_over = 1; // Locked above screen = death
                            else {
                                board[by][bx] = tetris_colors[cur_p];
                                st7789_draw_rect(70 + bx * 10, 60 + by * 10, 9, 9, tetris_colors[cur_p]);
                            }
                        }
                    }
                }

                // 2. LINE CLEAR CHECK
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
                        
                        // Shift all rows down above this row
                        for (int sr = r; sr > 0; sr--) {
                            for (int c = 0; c < 10; c++) {
                                board[sr][c] = board[sr - 1][c];
                            }
                        }
                        // Clear the absolute top row
                        for (int c = 0; c < 10; c++) {
                            board[0][c] = 0;
                        }
                        r++; // Re-check the same row index since everything dropped down
                    }
                }

                if (lines_cleared_this_turn > 0) {
                    redraw_board = 1;
                    score += (lines_cleared_this_turn * 100);
                    
                    // Convert score and cleanly draw it using differential spacing
                    custom_itoa(score, score_buf);
                    st7789_draw_string(175, 160, "      ", COLOR_BLACK, COLOR_BLACK); // Clear old score area
                    st7789_draw_string(175, 160, score_buf, COLOR_YELLOW, COLOR_BLACK); // Draw new score
                }

                if (redraw_board) {
                    st7789_draw_rect(70, 60, 100, 200, COLOR_BLACK);
                    for (int r = 0; r < 20; r++) {
                        for (int c = 0; c < 10; c++) {
                            if (board[r][c]) {
                                st7789_draw_rect(70 + c * 10, 60 + r * 10, 9, 9, board[r][c]);
                            }
                        }
                    }
                }

                // SPAWN NEW PIECE & UPDATE NEXT PREVIEW
                tetris_draw_next_piece(next_p, COLOR_BLACK); // Erase old preview block
                
                cur_p = next_p;
                next_p = custom_rand() % 7;
                cur_r = 0;
                cur_x = 3; cur_y = -3;
                
                tetris_draw_next_piece(next_p, tetris_colors[next_p]); // Draw new preview block

                if (tetris_check_collision(board, cur_p, cur_r, cur_x, cur_y)) game_over = 1;
            }
        }

        // Draw current moving piece
        if (!game_over) {
            tetris_draw_piece(cur_p, cur_r, cur_x, cur_y, tetris_colors[cur_p]);
        }

        delay_us(33000); // 30 FPS core loop
    }
}

void st7789_run_games_menu() {
    int game_selection = 0;
    int redraw = 1;
    
    while(1) {
        if (redraw) {
            st7789_fill_screen(COLOR_BLACK);
            st7789_draw_rect(0, 0, 240, 30, COLOR_BLUE);
            st7789_draw_string(10, 10, "GAMES MENU ['q'=Back]", COLOR_WHITE, COLOR_BLUE);

            st7789_draw_string(40, 70, "1. Snake", 
                game_selection == 0 ? COLOR_BLACK : COLOR_WHITE, 
                game_selection == 0 ? COLOR_GREEN : COLOR_BLACK);
                
            st7789_draw_string(40, 100, "2. Block Breaker", 
                game_selection == 1 ? COLOR_BLACK : COLOR_WHITE, 
                game_selection == 1 ? COLOR_GREEN : COLOR_BLACK);
                
            st7789_draw_string(40, 130, "3. Tetris", 
                game_selection == 2 ? COLOR_BLACK : COLOR_WHITE, 
                game_selection == 2 ? COLOR_GREEN : COLOR_BLACK);
                
            redraw = 0;
        }

        if (uart_has_char()) {
            char in = uart_getchar();
            if (in == 'q') return; // Exit to main OS
            if (in == 'w' && game_selection > 0) { game_selection--; redraw = 1; }
            if (in == 's' && game_selection < 2) { game_selection++; redraw = 1; }
            if (in == 'e') {
                if (game_selection == 0) st7789_run_snake();
                if (game_selection == 1) st7789_run_block_breaker();
                if (game_selection == 2) st7789_run_tetris();
                
                redraw = 1; // Repaint menu when returning from game
            }
        }
        delay_us(50000); 
    }
}


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

void st7789_draw_full_menu(int current_selection) {
    for (int i = 0; i < 5; i++) {
        st7789_draw_single_item(i, (i == current_selection));
    }
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
    os_state_t current_state = STATE_MENU;

    st7789_draw_desktop();
    st7789_draw_full_menu(current_selection);
    while(1) {
        char input = uart_getchar(); 
        if (current_state == STATE_MENU) {
            int previous_selection = current_selection;

            if (input == 'w') { 
                current_selection--;
                if (current_selection < 0) current_selection = 4; 
                
                // ONLY update the two items that changed
                st7789_draw_single_item(previous_selection, 0); // Deselect old
                st7789_draw_single_item(current_selection, 1);  // Select new
            } 
            else if (input == 's') { 
                current_selection++;
                if (current_selection > 4) current_selection = 0; 
                
                // ONLY update the two items that changed
                st7789_draw_single_item(previous_selection, 0); // Deselect old
                st7789_draw_single_item(current_selection, 1);  // Select new
            } else if (input == 'e') { 
                if (current_selection == 0) {
                    current_state = STATE_APP_MONITOR;
                    st7789_run_sys_monitor();
                    current_state = STATE_MENU;
                    st7789_draw_desktop();
                    st7789_draw_full_menu(current_selection);
                } 
                else if (current_selection == 1) {
                    current_state = STATE_APP_EDITOR;
                    run_dynamic_text_editor();
                    current_state = STATE_MENU;
                    st7789_draw_desktop();
                    st7789_draw_full_menu(current_selection);
                } 
                else if (current_selection == 2) {
                    current_state = STATE_APP_GAMES;
                    st7789_run_games_menu();
                    current_state = STATE_MENU;
                    st7789_draw_desktop();
                    st7789_draw_full_menu(current_selection);
                } 
                else if (current_selection == 3) {
                    current_state = STATE_APP_EXPLORER; // 1. Set state
                    run_ram_file_browser();             // 2. Run app
                    current_state = STATE_MENU;         // 3. Restore state
                    st7789_draw_desktop();              // 4. Redraw Background
                    st7789_draw_full_menu(current_selection); // 5. Redraw Menu List
                } 
                else if (current_selection == 4) {
                    current_state = STATE_APP_ABOUT;
                    st7789_run_app_placeholder("ABOUT", "Bare-Metal OS");
                    current_state = STATE_MENU;
                    st7789_draw_desktop();
                    st7789_draw_full_menu(current_selection);
                }
            }
        } 
        else {
            if (input == 'q') { 
                current_state = STATE_MENU;
                st7789_draw_desktop();
                st7789_draw_full_menu(current_selection);
            }
        }
    }
}