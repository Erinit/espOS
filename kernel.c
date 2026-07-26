/* kernel.c */

/* ESP32 UART0 Hardware Register Definitions */
#define UART0_BASE       0x3FF40000UL
#define UART0_FIFO_REG   ((volatile unsigned int *)(UART0_BASE + 0x00))
#define UART0_STATUS_REG ((volatile unsigned int *)(UART0_BASE + 0x1C))

/* Custom lightweight string length function (zero memory bloat) */
unsigned int custom_strlen(const char *str) {
    unsigned int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/* Transmit a single character through UART0 */
void uart_putc(char c) {
    /* 
     * UART0_STATUS_REG bits [23:16] track TXFIFO_CNT (bytes currently in the queue).
     * The hardware FIFO holds up to 128 bytes.
     * We wait in a tight loop if the transmission buffer is full.
     */
    while (((*UART0_STATUS_REG >> 16) & 0xFF) >= 127) {
        /* Wait for hardware FIFO space to clear */
    }

    /* Push byte directly into hardware transmit queue */
    *UART0_FIFO_REG = (unsigned int)c;
}

/* Wait for and read a single character from UART0 */
char uart_getchar() {
    /* * UART0_STATUS_REG bits [7:0] track RXFIFO_CNT (bytes received).
     * We loop until at least one byte is waiting for us.
     */
    while ((*UART0_STATUS_REG & 0xFF) == 0) {
        // Wait for user input (Mock Thumbstick)
    }

    /* Pop the byte directly out of the hardware receive queue */
    return (char)(*UART0_FIFO_REG);
}

/* Transmit a string to the serial console */
void uart_print(const char *str) {
    unsigned int len = custom_strlen(str);
    for (unsigned int i = 0; i < len; i++) {
        uart_putc(str[i]);
    }
}

// CPU cycle counter for microsecond delays
static inline unsigned int get_ccount(void) {
    unsigned int ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=a"(ccount));
    return ccount;
}

void delay_us(unsigned int us) {
    unsigned int start = get_ccount();
    unsigned int cycles = us * 80; 
    while (get_ccount() - start < cycles);
}

// --- MISSING PIN DEFINITIONS ADDED HERE ---
#define SDA_PIN 21
#define SCL_PIN 22

// New Registers for Open-Drain simulation
#define GPIO_OUT_W1TC_REG    (*(volatile unsigned int *)0x3FF4400C) // Clear output bit
#define GPIO_ENABLE_W1TS_REG (*(volatile unsigned int *)0x3FF44024) // Enable output (Drive)
#define GPIO_ENABLE_W1TC_REG (*(volatile unsigned int *)0x3FF44028) // Disable output (Float)

// Simulate Open-Drain:
// To pull LOW: Enable output (the output data register is kept at 0)
// To release HIGH: Disable output (let internal pull-up resistor pull it high)
#define SDA_LOW()  do { GPIO_ENABLE_W1TS_REG = (1 << SDA_PIN); } while(0)
#define SDA_HIGH() do { GPIO_ENABLE_W1TC_REG = (1 << SDA_PIN); } while(0)
#define SCL_LOW()  do { GPIO_ENABLE_W1TS_REG = (1 << SCL_PIN); } while(0)
#define SCL_HIGH() do { GPIO_ENABLE_W1TC_REG = (1 << SCL_PIN); } while(0)

void i2c_init() {
    // 1. Configure IO_MUX for GPIO 21 and 22 (Function 2 is standard GPIO)
    *(volatile unsigned int *)0x3FF4907C = (2 << 12) | (1 << 8); 
    *(volatile unsigned int *)0x3FF49080 = (2 << 12) | (1 << 8); 
    
    // 2. Force the actual output data value to 0 (LOW) permanently for these pins
    GPIO_OUT_W1TC_REG = (1 << SDA_PIN) | (1 << SCL_PIN);
    
    // 3. Start in the idle state (Lines released to HIGH)
    SDA_HIGH();
    SCL_HIGH();
}

void i2c_start() {
    SDA_HIGH();
    SCL_HIGH();
    delay_us(40);
    SDA_LOW();  
    delay_us(40);
    SCL_LOW();
}

void i2c_stop() {
    SDA_LOW();
    delay_us(40);
    SCL_HIGH();
    delay_us(40);
    SDA_HIGH(); 
    delay_us(40);
}

void i2c_write_byte(unsigned char data) {
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) {
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        data <<= 1;
        delay_us(20);
        SCL_HIGH();
        delay_us(40);
        SCL_LOW();
        delay_us(20);
    }
    // Clock pulse for ACK bit 
    SDA_HIGH(); 
    delay_us(20);
    SCL_HIGH();
    delay_us(40);
    SCL_LOW();
}

#define OLED_ADDRESS 0x78 

void ssd1306_send_command(unsigned char cmd) {
    i2c_start();
    i2c_write_byte(OLED_ADDRESS); 
    i2c_write_byte(0x00); 
    i2c_write_byte(cmd);  
    i2c_stop();
}

// Send pixel data to the VRAM (Notice the control byte is 0x40 instead of 0x00)
void ssd1306_send_data(unsigned char data) {
    i2c_start();
    i2c_write_byte(OLED_ADDRESS);
    i2c_write_byte(0x40); // 0x40 means "the following byte is Data"
    i2c_write_byte(data);
    i2c_stop();
}

// Move the invisible drawing cursor to a specific Page (0-7) and Column (0-127)
void ssd1306_set_cursor(unsigned char page, unsigned char col) {
    ssd1306_send_command(0xB0 + page);                // Set Page Start Address
    ssd1306_send_command(0x00 + (col & 0x0F));        // Set Lower Column Address
    ssd1306_send_command(0x10 + ((col >> 4) & 0x0F)); // Set Higher Column Address
}

// Overwrite the entire VRAM with zeros to clear the random startup garbage
void ssd1306_clear() {
    for (int page = 0; page < 8; page++) {
        ssd1306_set_cursor(page, 0);
        
        // We can send a continuous stream of data bytes to fill the whole page rapidly
        i2c_start();
        i2c_write_byte(OLED_ADDRESS);
        i2c_write_byte(0x40); // Data stream mode
        
        for (int col = 0; col < 128; col++) {
            i2c_write_byte(0x00); // 0x00 = all 8 vertical pixels OFF
        }
        i2c_stop();
    }
}

// --- UPDATED TO THE ROBUST WAKE-UP SEQUENCE ---
void ssd1306_init() {
    ssd1306_send_command(0xAE); // Display OFF (Sleep)
    ssd1306_send_command(0xD5); // Set Display Clock Divide Ratio
    ssd1306_send_command(0x80); // Default ratio
    ssd1306_send_command(0xA8); // Set Multiplex Ratio
    ssd1306_send_command(0x3F); // 64 lines (for 128x64 display)
    ssd1306_send_command(0x8D); // Charge Pump Setting
    ssd1306_send_command(0x14); // Enable Charge Pump
    ssd1306_send_command(0x20); // Memory Addressing Mode
    ssd1306_send_command(0x00); // Horizontal Addressing Mode
    ssd1306_send_command(0xAF); // Display ON
    ssd1306_send_command(0xA5); // Force Entire Display ON
    ssd1306_send_command(0xAF); // Display ON
    ssd1306_send_command(0xA4);
}

// Standard 8x8 ASCII Font (Maps ASCII 32 to 126)
const unsigned char font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 Space
    {0x00,0x00,0x5F,0x00,0x00,0x00,0x00,0x00}, // 33 !
    {0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00}, // 34 "
    {0x14,0x7F,0x14,0x7F,0x14,0x00,0x00,0x00}, // 35 #
    {0x24,0x2A,0x7F,0x2A,0x12,0x00,0x00,0x00}, // 36 $
    {0x23,0x13,0x08,0x64,0x62,0x00,0x00,0x00}, // 37 %
    {0x36,0x49,0x55,0x22,0x50,0x00,0x00,0x00}, // 38 &
    {0x00,0x05,0x03,0x00,0x00,0x00,0x00,0x00}, // 39 '
    {0x00,0x1C,0x22,0x41,0x00,0x00,0x00,0x00}, // 40 (
    {0x00,0x41,0x22,0x1C,0x00,0x00,0x00,0x00}, // 41 )
    {0x14,0x08,0x3E,0x08,0x14,0x00,0x00,0x00}, // 42 *
    {0x08,0x08,0x3E,0x08,0x08,0x00,0x00,0x00}, // 43 +
    {0x00,0x50,0x30,0x00,0x00,0x00,0x00,0x00}, // 44 ,
    {0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00}, // 45 -
    {0x00,0x60,0x60,0x00,0x00,0x00,0x00,0x00}, // 46 .
    {0x20,0x10,0x08,0x04,0x02,0x00,0x00,0x00}, // 47 /
    {0x3E,0x51,0x49,0x45,0x3E,0x00,0x00,0x00}, // 48 0
    {0x00,0x42,0x7F,0x40,0x00,0x00,0x00,0x00}, // 49 1
    {0x42,0x61,0x51,0x49,0x46,0x00,0x00,0x00}, // 50 2
    {0x21,0x41,0x45,0x4B,0x31,0x00,0x00,0x00}, // 51 3
    {0x18,0x14,0x12,0x7F,0x10,0x00,0x00,0x00}, // 52 4
    {0x27,0x45,0x45,0x45,0x39,0x00,0x00,0x00}, // 53 5
    {0x3C,0x4A,0x49,0x49,0x30,0x00,0x00,0x00}, // 54 6
    {0x01,0x71,0x09,0x05,0x03,0x00,0x00,0x00}, // 55 7
    {0x36,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, // 56 8
    {0x06,0x49,0x49,0x29,0x1E,0x00,0x00,0x00}, // 57 9
    {0x00,0x36,0x36,0x00,0x00,0x00,0x00,0x00}, // 58 :
    {0x00,0x56,0x36,0x00,0x00,0x00,0x00,0x00}, // 59 ;
    {0x08,0x14,0x22,0x41,0x00,0x00,0x00,0x00}, // 60 <
    {0x14,0x14,0x14,0x14,0x14,0x00,0x00,0x00}, // 61 =
    {0x00,0x41,0x22,0x14,0x08,0x00,0x00,0x00}, // 62 >
    {0x02,0x01,0x51,0x09,0x06,0x00,0x00,0x00}, // 63 ?
    {0x32,0x49,0x79,0x41,0x3E,0x00,0x00,0x00}, // 64 @
    {0x7E,0x11,0x11,0x11,0x7E,0x00,0x00,0x00}, // 65 A
    {0x7F,0x49,0x49,0x49,0x36,0x00,0x00,0x00}, // 66 B
    {0x3E,0x41,0x41,0x41,0x22,0x00,0x00,0x00}, // 67 C
    {0x7F,0x41,0x41,0x22,0x1C,0x00,0x00,0x00}, // 68 D
    {0x7F,0x49,0x49,0x49,0x41,0x00,0x00,0x00}, // 69 E
    {0x7F,0x09,0x09,0x09,0x01,0x00,0x00,0x00}, // 70 F
    {0x3E,0x41,0x49,0x49,0x7A,0x00,0x00,0x00}, // 71 G
    {0x7F,0x08,0x08,0x08,0x7F,0x00,0x00,0x00}, // 72 H
    {0x00,0x41,0x7F,0x41,0x00,0x00,0x00,0x00}, // 73 I
    {0x20,0x40,0x41,0x3F,0x01,0x00,0x00,0x00}, // 74 J
    {0x7F,0x08,0x14,0x22,0x41,0x00,0x00,0x00}, // 75 K
    {0x7F,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, // 76 L
    {0x7F,0x02,0x0C,0x02,0x7F,0x00,0x00,0x00}, // 77 M
    {0x7F,0x04,0x08,0x10,0x7F,0x00,0x00,0x00}, // 78 N
    {0x3E,0x41,0x41,0x41,0x3E,0x00,0x00,0x00}, // 79 O
    {0x7F,0x09,0x09,0x09,0x06,0x00,0x00,0x00}, // 80 P
    {0x3E,0x41,0x51,0x21,0x5E,0x00,0x00,0x00}, // 81 Q
    {0x7F,0x09,0x19,0x29,0x46,0x00,0x00,0x00}, // 82 R
    {0x46,0x49,0x49,0x49,0x31,0x00,0x00,0x00}, // 83 S
    {0x01,0x01,0x7F,0x01,0x01,0x00,0x00,0x00}, // 84 T
    {0x3F,0x40,0x40,0x40,0x3F,0x00,0x00,0x00}, // 85 U
    {0x1F,0x20,0x40,0x20,0x1F,0x00,0x00,0x00}, // 86 V
    {0x3F,0x40,0x38,0x40,0x3F,0x00,0x00,0x00}, // 87 W
    {0x63,0x14,0x08,0x14,0x63,0x00,0x00,0x00}, // 88 X
    {0x07,0x08,0x70,0x08,0x07,0x00,0x00,0x00}, // 89 Y
    {0x61,0x51,0x49,0x45,0x43,0x00,0x00,0x00}, // 90 Z
    {0x00,0x7F,0x41,0x41,0x00,0x00,0x00,0x00}, // 91 [
    {0x02,0x04,0x08,0x10,0x20,0x00,0x00,0x00}, // 92 
    {0x00,0x41,0x41,0x7F,0x00,0x00,0x00,0x00}, // 93 ]
    {0x04,0x02,0x01,0x02,0x04,0x00,0x00,0x00}, // 94 ^
    {0x40,0x40,0x40,0x40,0x40,0x00,0x00,0x00}, // 95 _
    {0x00,0x01,0x02,0x04,0x00,0x00,0x00,0x00}, // 96 `
    {0x20,0x54,0x54,0x54,0x78,0x00,0x00,0x00}, // 97 a
    {0x7F,0x48,0x44,0x44,0x38,0x00,0x00,0x00}, // 98 b
    {0x38,0x44,0x44,0x44,0x20,0x00,0x00,0x00}, // 99 c
    {0x38,0x44,0x44,0x48,0x7F,0x00,0x00,0x00}, // 100 d
    {0x38,0x54,0x54,0x54,0x18,0x00,0x00,0x00}, // 101 e
    {0x08,0x7E,0x09,0x01,0x02,0x00,0x00,0x00}, // 102 f
    {0x0C,0x52,0x52,0x52,0x3E,0x00,0x00,0x00}, // 103 g
    {0x7F,0x08,0x04,0x04,0x78,0x00,0x00,0x00}, // 104 h
    {0x00,0x44,0x7D,0x40,0x00,0x00,0x00,0x00}, // 105 i
    {0x20,0x40,0x44,0x3D,0x00,0x00,0x00,0x00}, // 106 j
    {0x7F,0x10,0x28,0x44,0x00,0x00,0x00,0x00}, // 107 k
    {0x00,0x41,0x7F,0x40,0x00,0x00,0x00,0x00}, // 108 l
    {0x7C,0x04,0x18,0x04,0x78,0x00,0x00,0x00}, // 109 m
    {0x7C,0x08,0x04,0x04,0x78,0x00,0x00,0x00}, // 110 n
    {0x38,0x44,0x44,0x44,0x38,0x00,0x00,0x00}, // 111 o
    {0x7C,0x14,0x14,0x14,0x08,0x00,0x00,0x00}, // 112 p
    {0x08,0x14,0x14,0x18,0x7C,0x00,0x00,0x00}, // 113 q
    {0x7C,0x08,0x04,0x04,0x08,0x00,0x00,0x00}, // 114 r
    {0x48,0x54,0x54,0x54,0x20,0x00,0x00,0x00}, // 115 s
    {0x04,0x3F,0x44,0x40,0x20,0x00,0x00,0x00}, // 116 t
    {0x3C,0x40,0x40,0x20,0x7C,0x00,0x00,0x00}, // 117 u
    {0x1C,0x20,0x40,0x20,0x1C,0x00,0x00,0x00}, // 118 v
    {0x3C,0x40,0x30,0x40,0x3C,0x00,0x00,0x00}, // 119 w
    {0x44,0x28,0x10,0x28,0x44,0x00,0x00,0x00}, // 120 x
    {0x0C,0x50,0x50,0x50,0x3C,0x00,0x00,0x00}, // 121 y
    {0x44,0x64,0x54,0x4C,0x44,0x00,0x00,0x00}, // 122 z
    {0x00,0x08,0x36,0x41,0x00,0x00,0x00,0x00}, // 123 {
    {0x00,0x00,0x7F,0x00,0x00,0x00,0x00,0x00}, // 124 |
    {0x00,0x41,0x36,0x08,0x00,0x00,0x00,0x00}, // 125 }
    {0x10,0x08,0x18,0x10,0x08,0x00,0x00,0x00}  // 126 ~
};

// Lookup a character in our font array and send its 8 bytes to the VRAM
void ssd1306_print_char(char c) {
    // Ignore characters outside our ASCII font array boundaries
    if (c < 32 || c > 126) return;
    
    // Calculate the array index (Space is ASCII 32, which is index 0)
    int index = c - 32;
    
    i2c_start();
    i2c_write_byte(OLED_ADDRESS);
    i2c_write_byte(0x40); // 0x40 means "Incoming Data Stream"
    
    // Blast the 8 vertical columns that make up the character
    for (int i = 0; i < 8; i++) {
        i2c_write_byte(font8x8[index][i]);
    }
    
    i2c_stop();
}

// Print a full string until it hits the null terminator
void ssd1306_print_string(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        ssd1306_print_char(str[i]);
        i++;
    }
}

// Draw the static text of the menu ONCE on boot
void init_ui_menu(const char *menu_items[], int num_items, int starting_item) {
    ssd1306_clear(); 
    
    // Draw Title Banner
    ssd1306_set_cursor(0, 16);
    ssd1306_print_string("-- OS MENU --");

    // Draw the static menu text
    for (int i = 0; i < num_items; i++) {
        ssd1306_set_cursor((i * 2) + 2, 0); 
        
        // Draw the initial cursor position
        if (i == starting_item) {
            ssd1306_print_string("> "); 
        } else {
            ssd1306_print_string("  "); 
        }
        
        ssd1306_print_string(menu_items[i]);
    }
}

// Only redraw the specific cursor locations to save I2C bandwidth
void update_ui_cursor(int old_item, int new_item) {
    if (old_item == new_item) return; // Do nothing if it didn't change

    // 1. Erase the old cursor by printing two blank spaces over it
    ssd1306_set_cursor((old_item * 2) + 2, 0);
    ssd1306_print_string("  ");

    // 2. Draw the new cursor
    ssd1306_set_cursor((new_item * 2) + 2, 0);
    ssd1306_print_string("> ");
}

// Define the major states of the Operating System
typedef enum {
    STATE_MENU,
    STATE_APP_TETRIS,
    STATE_APP_MONITOR,
    STATE_APP_SETTINGS
} os_state_t;

// Define the metadata header for our memory blocks
typedef struct block_meta {
    unsigned int size;           // How many bytes is this block?
    int free;                    // 1 if free, 0 if in use
    struct block_meta *next;     // Pointer to the next block in the chain
} block_meta_t;

// The start of our linked list of memory
// The actual memory pool we will use for the heap (Let's start with a 32 KB pool)
#define HEAP_SIZE 32768
static unsigned char custom_heap[HEAP_SIZE];
static block_meta_t *heap_head = (void*)0; 

// Initialize the heap on first use
void heap_init() {
    // If the heap hasn't been set up yet, format it as one massive free block
    if (heap_head == (void*)0) {
        heap_head = (block_meta_t *)custom_heap;
        heap_head->size = HEAP_SIZE - sizeof(block_meta_t);
        heap_head->free = 1;
        heap_head->next = (void*)0;
    }
}

// Our custom bare-metal memory allocator
void* custom_malloc(unsigned int size) {
    if (size == 0) return (void*)0;
    
    // Ensure the heap is initialized before we start
    if (heap_head == (void*)0) {
        heap_init();
    }
    
    block_meta_t *current = heap_head;
    
    // Traverse the linked list to find a free block that is big enough
    while (current != (void*)0) {
        if (current->free && current->size >= size) {
            
            // The Split: Can we slice this block into two? 
            // (Only if there is enough leftover space for a new header + at least 1 byte of memory)
            if (current->size > size + sizeof(block_meta_t)) {
                
                // Do pointer math to place a new header right after the chunk we are handing out
                block_meta_t *new_block = (block_meta_t *)((unsigned char*)current + sizeof(block_meta_t) + size);
                new_block->size = current->size - size - sizeof(block_meta_t);
                new_block->free = 1;
                new_block->next = current->next;
                
                // Shrink the current block and link it to the newly created free block
                current->size = size;
                current->next = new_block;
            }
            
            current->free = 0; // Mark as officially IN USE
            
            // Return the memory address directly AFTER the metadata header
            return (void*)(current + 1); 
        }
        current = current->next;
    }
    
    // If we reach here, we are completely Out Of Memory (OOM)!
    return (void*)0; 
}



// Free a dynamically allocated block of memory
void custom_free(void *ptr) {
    // If a null pointer is passed, do nothing
    if (ptr == (void*)0) {
        return; 
    }

    // Pointer math in reverse! 
    // Step backward by exactly the size of one block_meta_t struct 
    // to find our hidden metadata header.
    block_meta_t *block_ptr = (block_meta_t *)ptr - 1;

    // Mark the block as free so custom_malloc can claim it again
    block_ptr->free = 1;

    // --- The Coalesce (De-fragmentation) ---
    // If the next adjacent block in memory is ALSO free, merge them together 
    // into one massive free block. This prevents "Swiss-cheese" memory fragmentation.
    if (block_ptr->next != (void*)0 && block_ptr->next->free == 1) {
        block_ptr->size += sizeof(block_meta_t) + block_ptr->next->size;
        block_ptr->next = block_ptr->next->next;
    }
}



// A basic System Monitor view
void run_sys_monitor() {
    ssd1306_clear();
    ssd1306_set_cursor(0, 0);
    ssd1306_print_string("-- SYS MONITOR --");
    
    ssd1306_set_cursor(2, 0);
    ssd1306_print_string("CPU: OK (Bare-Metal)");
    
    ssd1306_set_cursor(4, 0);
    ssd1306_print_string("RAM: 520KB Total");
    
    // Instructions to exit
    ssd1306_set_cursor(7, 0);
    ssd1306_print_string("< 'a' to exit");
}

// A generic placeholder for unfinished apps
void run_placeholder_app(const char* title) {
    ssd1306_clear();
    ssd1306_set_cursor(0, 0);
    ssd1306_print_string(title);
    
    ssd1306_set_cursor(4, 16);
    ssd1306_print_string("Coming Soon...");
    
    ssd1306_set_cursor(7, 0);
    ssd1306_print_string("< 'a' to exit");
}

// Run a live dynamic typing buffer using our custom heap
void run_dynamic_text_editor() {
    unsigned int buffer_capacity = 16; // Start with 16 bytes of heap memory
    unsigned int current_length = 0;
    
    // Dynamically allocate our initial text buffer from the heap!
    char *text_buffer = (char *)custom_malloc(buffer_capacity);
    if (text_buffer == (void*)0) {
        return; // Out of memory safeguard
    }
    
    text_buffer[0] = '\0'; // Null-terminate empty string

    // Clear screen and show UI instructions
    ssd1306_clear();
    ssd1306_set_cursor(0, 0);
    ssd1306_print_string("-- DYNAMIC EDITOR --");
    ssd1306_set_cursor(6, 0);
    ssd1306_print_string("Type on serial terminal");
    ssd1306_set_cursor(7, 0);
    ssd1306_print_string("Press ESC to exit");

    while (1) {
        char c = uart_getchar(); // Wait for keyboard input over serial

        // Check for ESC key (ASCII 27) to exit the editor
        if (c == 27) {
            custom_free(text_buffer); // Clean up our heap memory before exiting!
            break;
        }

        // Handle Backspace (ASCII 8 or 127)
        else if (c == 8 || c == 127) {
            if (current_length > 0) {
                current_length--;
                text_buffer[current_length] = '\0'; // Remove character
                
                // Echo backspace effect over serial terminal
                uart_putc('\b');
                uart_putc(' ');
                uart_putc('\b');
            }
        } 
        
        // Handle Normal Printable Characters
        else if (c >= 32 && c <= 126) {
            // Check if we need to expand our buffer capacity (Dynamic Reallocation simulation)
            if (current_length + 1 >= buffer_capacity) {
                buffer_capacity += 16; // Grow capacity
                char *new_buffer = (char *)custom_malloc(buffer_capacity);
                
                if (new_buffer != (void*)0) {
                    // Copy old data to new larger buffer
                    for (unsigned int i = 0; i < current_length; i++) {
                        new_buffer[i] = text_buffer[i];
                    }
                    new_buffer[current_length] = '\0';
                    
                    // Free the old smaller memory block back to the heap!
                    custom_free(text_buffer);
                    text_buffer = new_buffer;
                }
            }

            // Append the character
            text_buffer[current_length] = c;
            current_length++;
            text_buffer[current_length] = '\0';

            // Echo character back to the serial terminal
            uart_putc(c);
        }

        // Optional: Render the live dynamic string onto the OLED display on Page 3
        ssd1306_set_cursor(3, 0);
        // Clear line space first by printing blanks, then print current buffer
        ssd1306_print_string("                "); 
        ssd1306_set_cursor(3, 0);
        ssd1306_print_string(text_buffer);
    }
}

// A global pointer to test our custom heap allocator
// A global pointer to test our custom heap allocator
char *dynamic_char_ptr = (void*)0;

void kernel_main() {
    // 1. Disable Watchdogs
    *(volatile unsigned int *)0x3FF480A4 = 0x50D83AA1;
    *(volatile unsigned int *)0x3FF4808C = 0;
    *(volatile unsigned int *)0x3FF5F064 = 0x50D83AA1;
    *(volatile unsigned int *)0x3FF5F048 = 0;

    // 2. Initialize Hardware
    i2c_init();
    delay_us(100000); 
    ssd1306_init();
    
    // 3. Define the UI State (Added "Text Editor" as the 4th option)
    const char *apps[] = {
        "Play Tetris",
        "Sys Monitor",
        "Settings",
        "Text Editor"
    };
    int total_apps = 4;
    int current_selection = 0;
    int previous_selection = 0;
    
    // Start the OS in the Menu state
    os_state_t current_state = STATE_MENU;

    // 4. Initial Screen Draw 
    init_ui_menu(apps, total_apps, current_selection);

    // 5. The Multi-State Input Loop
    while (1) {
        char input = uart_getchar(); 

        // --- IF WE ARE IN THE MAIN MENU ---
        if (current_state == STATE_MENU) {
            previous_selection = current_selection; 

            if (input == 'w') { // UP
                current_selection--;
                if (current_selection < 0) current_selection = total_apps - 1; 
                update_ui_cursor(previous_selection, current_selection);
                
            } else if (input == 's') { // DOWN
                current_selection++;
                if (current_selection >= total_apps) current_selection = 0; 
                update_ui_cursor(previous_selection, current_selection);
                
            } else if (input == 'd') { // SELECT / ENTER APP
                if (current_selection == 0) {
                    current_state = STATE_APP_TETRIS;
                    run_placeholder_app("TETRIS");
                } else if (current_selection == 1) {
                    current_state = STATE_APP_MONITOR;
                    run_sys_monitor();
                } else if (current_selection == 2) {
                    current_state = STATE_APP_SETTINGS;
                    run_placeholder_app("SETTINGS");
                } else if (current_selection == 3) {
                    // LAUNCH OUR DYNAMIC TEXT EDITOR!
                    run_dynamic_text_editor();
                    
                    // Once ESC is pressed inside the editor, it returns here.
                    // We redraw the main menu to restore the UI.
                    current_state = STATE_MENU;
                    init_ui_menu(apps, total_apps, current_selection);
                }
            }
        } 
        
        // --- IF WE ARE INSIDE A PLACEHOLDER APP ---
        else {
            if (input == 'a') { // BACK / EXIT TO MENU
                current_state = STATE_MENU;
                init_ui_menu(apps, total_apps, current_selection); 
            }
        }
    }
}