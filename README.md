# Flappy Bird AVR

A complete Flappy Bird game implementation on AVR ATmega1284 microcontroller featuring custom peripheral drivers, sophisticated state machine architecture, and real-time concurrent task scheduling.


## Demo
https://github.com/user-attachments/assets/501d6ca0-d0bd-45a4-968c-676462b8d0d5



## Overview

This project implements the classic Flappy Bird game on bare-metal AVR hardware with:
- **ST7735 TFT Display** (132x132) for smooth gameplay graphics
- **16x2 LCD** for real-time score tracking
- **EEPROM** persistent high score storage
- **Button Controls** for menu navigation and gameplay
- **Buzzer** audio feedback system
- **Custom Hardware Drivers** written from scratch

## Features

- 🎮 Smooth 60 FPS gameplay with real-time collision detection
- 📊 Live score display with persistent high score storage
- 🔄 Infinite procedurally generated level with wrap-around system
- ⏸️ Pause/Resume functionality with visual feedback
- 🎯 Precise pixel-perfect collision detection
- 🏗️ Modular state machine architecture
- ⚡ Efficient concurrent task scheduling system

---

## State Machine & Task Architecture

### Task Scheduling System

The project implements a **cooperative multitasking scheduler** based on Greatest Common Divisor (GCD) period calculation. Each task runs at its own configurable period, allowing for precise timing control across multiple concurrent operations.

#### Task Structure
```c
typedef struct _task{
    unsigned int state;          // Current state
    unsigned long period;        // Task period in ms
    unsigned long elapsedTime;   // Time since last tick
    int (*TickFct)(int);        // State machine function pointer
} task;
```

#### Scheduler Implementation
The `TimerISR()` function iterates through all tasks, checking if each has reached its period. When ready, it executes the task's tick function and resets the elapsed time:

```c
void TimerISR() {
    for (unsigned int i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].elapsedTime == tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}
```

### State Machines

The game features **6 concurrent state machines** that communicate through shared variables:

#### 1. **TickButtons** - Input Handler
- **States**: `IDLE`, `SET_CONTROL`, `SET_JUMP`
- **Purpose**: Debounces button input and sets control flags
- **Outputs**: `control` (pause/resume), `jump` (player jump)

#### 2. **TickMenu** - Game State Controller
- **States**: `PAUSED`, `HOLDING_PLAY_RESET`, `PLAYING`, `RESETTING`, `HOLDING_PAUSED`
- **Purpose**: Manages game flow and reset logic
- **Key Feature**: Hold-to-reset mechanism (30 ticks) prevents accidental resets
- **Outputs**: `game_state` (PAUSE/PLAY/RESET)

#### 3. **TickPosition** - Player Physics
- **States**: `FALLING`, `JUMPING`, `FREEZE`, `RESTART`
- **Purpose**: Handles player vertical movement with gravity and jump mechanics
- **Physics**: Acceleration-based falling with configurable hang time on jumps
- **Outputs**: `height` (player Y position)

#### 4. **TickLevel** - Level Progression
- **States**: `STOP`, `GO`
- **Purpose**: Advances level frame-by-frame and manages procedural generation
- **Key Feature**: Triggers pipe regeneration and score increments
- **Outputs**: `frame`, `curr_column`, `score`

#### 5. **TickDeath** - Collision Detection
- **States**: `CHECK` (continuous monitoring)
- **Purpose**: Detects collisions with pipes, ceiling, and floor
- **Algorithm**: Checks player bounding box against current column and wrap-around adjacent columns
- **Outputs**: `dead` flag

#### 6. **TickDraw** - Graphics Renderer
- **States**: `SETUP`, `DRAW`
- **Purpose**: Renders player, pipes, and handles screen inversion on pause
- **Optimization**: Only redraws changed regions to minimize SPI overhead

### Task Diagram

```
┌─────────────┐  control/jump   ┌──────────────┐
│TickButtons  │─────────────────▶│  TickMenu    │
│  (Input)    │                  │(Game State)  │
└─────────────┘                  └──────┬───────┘
                                        │ game_state
                    ┌───────────────────┼────────────────┐
                    ▼                   ▼                ▼
              ┌──────────┐        ┌──────────┐    ┌──────────┐
              │TickLevel │        │TickPos   │    │TickDraw  │
              │  (Frame) │        │(Physics) │    │(Graphics)│
              └────┬─────┘        └────┬─────┘    └──────────┘
                   │curr_column        │height
                   │score              │
                   └───────┬───────────┘
                           ▼
                    ┌──────────────┐
                    │  TickDeath   │
                    │ (Collision)  │
                    └──────────────┘
```

### Cross-Task Communication

Tasks communicate through **shared global variables** with clear ownership:

| Variable | Set By | Used By | Purpose |
|----------|--------|---------|---------|
| `control` | TickButtons | TickMenu | Pause/resume button flag |
| `jump` | TickButtons | TickPosition | Jump button flag |
| `game_state` | TickMenu | TickPosition, TickLevel, TickDraw | Game flow control |
| `height` | TickPosition | TickDeath | Player Y position |
| `dead` | TickDeath | TickMenu | Collision detection flag |
| `curr_column` | TickLevel | TickDeath | Current pipe for collision |
| `score` | TickLevel | TickMenu | Current game score |

---

## Level Generation & Wrap-Around System

### Procedural Generation

The level consists of **128 unique frames** that loop infinitely. Pipes are spaced every 32 frames with randomized gap positions:

```c
#define LEVEL_SIZE 128
#define PIPE_SPACING 32
#define GAP 32

void create_level() {
    int max = 86, min = 10;
    int range = max - min + 1;
    
    for (int i = 0; i < LEVEL_SIZE; i++) {
        if (i % PIPE_SPACING == 0 && i != 0) {
            columns[i].has_pipe = true;
            columns[i].bottom = rand() % range + min;
        }
        else {
            columns[i].has_pipe = false;
            columns[i].bottom = -1;
        }
    }
}
```

### Wrap-Around Mechanism

When the level reaches frame 127, it seamlessly wraps back to frame 0. **Key innovation**: Just before a pipe exits the screen, its values are regenerated for the next cycle:

```c
// In TickLevel: GO state
if (i % PIPE_SPACING == PIPE_SPACING - 1 && i != 0) { 
    refresh_pipe(i);  // Regenerate pipe off-screen
}

i = (i < LEVEL_SIZE - 1) ? i + 1 : 0;  // Wrap around
```

### Collision Detection with Wrap-Around

The collision system accounts for the player's width spanning across the wrap boundary:

```c
for(int i = frame - PLAYER_SIZE/2; i < frame + PLAYER_SIZE/2 + 1; i++) {
    // Handle wrap-around indices
    int wrapped_i = (i < 0) ? i + LEVEL_SIZE : 
                    (i >= LEVEL_SIZE) ? i - LEVEL_SIZE : i;
    
    column check_column = columns[wrapped_i];
    if (check_column.has_pipe && 
        (height - PLAYER_SIZE/4 + 1 < check_column.bottom || 
         (check_column.bottom + check_column.gap) < height + PLAYER_SIZE/4)) {
        dead = true;
    }
}
```

This ensures accurate collision detection even when the player straddles frame 0 and frame 127.

---

## Custom Peripheral Drivers

All hardware interfaces are implemented **from scratch** without Arduino libraries, directly manipulating AVR registers for maximum performance and control.

### 1. SPI Driver (`spiAVR.h`)

#### ST7735 TFT Display Controller

**Initialization Sequence**:
- Hardware reset via GPIO toggle
- Software reset command
- Sleep mode exit
- Color mode configuration (RGB565)
- Display rotation setup (MADCTL)
- Display power-on

**Key Functions**:
```c
void SPI_INIT()           // Configure SPI hardware in master mode
void SPI_SEND(char data)  // Blocking 8-bit SPI transmission
void SendCommand(uint8_t) // Send command byte to display
void SetWriteWindow()     // Define drawable pixel region
void FillWindow()         // Fast bulk pixel writing
```

**Optimization**: The `FillWindow` function minimizes overhead by:
1. Setting the pixel window once
2. Holding CS low during the entire transfer
3. Streaming high/low color bytes in a tight loop

```c
void FillWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color) {
    uint32_t total = (x1 - x0 + 1) * (y1 - y0 + 1);
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    PORTB &= ~PIN_SS;   // CS LOW (start stream)
    PORTB |= A0;        // DC HIGH (data mode)

    for(uint32_t i = 0; i < total; i++) {
        SPI_SEND(hi);
        SPI_SEND(lo);
    }

    PORTB |= PIN_SS;    // CS HIGH (end stream)
}
```

### 2. LCD Driver (`LCD.h`)

#### 16x2 Character Display (HD44780 Protocol)

**Interface**: 4-bit parallel mode for reduced pin usage

**Key Functions**:
```c
void lcd_init()                      // 4-bit mode initialization sequence
void lcd_send_command(uint8_t)       // Send control commands
void lcd_write_character(char)       // Write single character
void lcd_write_str(char*)            // Write string
void lcd_goto_xy(uint8_t, uint8_t)   // Position cursor
```

**Custom Feature**: Right-aligned score display
```c
void write_score(int write_score, int line) {
    char buf[12];
    itoa(write_score, buf, 10);
    
    int len = 0;
    while(buf[len] != '\0') len++;
    
    lcd_goto_xy(line, 16 - len);  // Right-align on 16-char display
    
    for (int i = 0; i < len; i++) {
        lcd_write_character(buf[i]);
    }
}
```

### 3. EEPROM Driver (`EEPROM.h`)

#### Non-Volatile High Score Storage

**Direct register manipulation** following ATmega1284 datasheet specifications:

```c
void EEPROM_write_score(unsigned int uiAddress, unsigned char ucData) {
    while(EECR & (1<<EEPE));        // Wait for previous write
    EEAR = uiAddress;               // Set address
    EEDR = ucData;                  // Set data
    EECR |= (1<<EEMPE);             // Enable master write
    EECR |= (1<<EEPE);              // Start EEPROM write
}

unsigned char EEPROM_read(unsigned int uiAddress) {
    while(EECR & (1<<EEPE));        // Wait for previous write
    EEAR = uiAddress;               // Set address
    EECR |= (1<<EERE);              // Start EEPROM read
    return EEDR;                     // Return data
}
```

**Storage Location**: `0x3FF` (last byte of EEPROM)

**High Score Logic**: Saved on game reset if current score exceeds stored value

### 4. Timer Driver (`timerISR.h`)

#### Precision Task Scheduler

**Configuration**: Timer2 in CTC mode with 250 compare value
- Clock: 16 MHz / 64 prescaler = 250 kHz
- Compare: 250 ticks = 1 ms period
- Result: 1 kHz interrupt rate for millisecond precision

```c
void TimerOn() {
    TCCR2A = 0x02;          // CTC mode
    TCCR2B = 0x04;          // Prescaler /64
    OCR2A  = 250;           // Compare value for 1ms
    TIMSK2 = 0x02;          // Enable compare match interrupt
    TCNT2  = 0;             // Reset counter
    SREG  |= 0x80;          // Enable global interrupts
}
```

The scheduler ISR fires every 1ms and manages all task timing.

### 5. PWM Buzzer Driver (`periph.h`)

#### Audio Feedback System

**Timer1 Configuration**: Fast PWM mode for sound generation
```c
void TIMER1_init() {
    TCCR1A |= (1 << WGM11) | (1 << COM1A1);
    TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11);
    ICR1 = 39999;  // 20ms PWM period
}
```

**Sound Effects**:
- `yip()`: High-pitched jump sound (64 prescaler)
- `derp()`: Low-pitched death sound (1024 prescaler)
- `kill()`: Silence (duty cycle 100%)

---

## Hardware Setup

### Pin Configuration

#### ST7735 TFT Display (SPI)
| Pin | AVR Port | Function |
|-----|----------|----------|
| SCK | PB5 (Pin 13) | SPI Clock |
| MOSI | PB3 (Pin 11) | SPI Data |
| CS | PB2 (Pin 10) | Chip Select |
| DC | PB1 (Pin 9) | Data/Command |
| RST | PB0 (Pin 8) | Reset |

#### 16x2 LCD Display (Parallel)
| Pin | AVR Port | Function |
|-----|----------|----------|
| D4-D7 | PD4-PD7 | Data Bus (4-bit) |
| EN | PD3 | Enable |
| RS | PD2 | Register Select |

#### Controls
| Component | AVR Port | Function |
|-----------|----------|----------|
| Button 1 | PC0 | Pause/Resume |
| Button 2 | PC1 | Jump |
| Buzzer | PB6 (OC1A) | PWM Audio |

### Schematic

```
                    ATmega1284
                  ┌─────────────┐
    [ST7735 SPI]  │             │  [16x2 LCD]
         SCK ◀────┤PB5      PD7 ├────▶ D7
        MOSI ◀────┤PB3      PD6 ├────▶ D6
          CS ◀────┤PB2      PD5 ├────▶ D5
          DC ◀────┤PB1      PD4 ├────▶ D4
         RST ◀────┤PB0      PD3 ├────▶ EN
                  │         PD2 ├────▶ RS
    [Controls]    │             │
     Pause ────▶  ┤PC0          │
      Jump ────▶  ┤PC1          │
                  │             │
    [Audio]       │             │
    Buzzer ◀──────┤PB6 (PWM)    │
                  └─────────────┘
```

---

## Building & Flashing

### Prerequisites
- AVR-GCC toolchain
- AVRDUDE (programmer software)
- USBtinyISP or compatible AVR programmer

### Compilation
```bash
avr-gcc -mmcu=atmega1284 -DF_CPU=16000000UL -Os \
    -o flappy_bird.elf dshaw013_FlappyBirdV3.cpp

avr-objcopy -O ihex flappy_bird.elf flappy_bird.hex
```

### Flash to Microcontroller
```bash
avrdude -c usbtiny -p atmega1284 -U flash:w:flappy_bird.hex:i
```

---

## Gameplay

### Controls
- **Left Button (PC0)**: 
  - In menu: Press to start game
  - In game: Tap to pause, hold 3 seconds to reset
- **Right Button (PC1)**: Jump (hold for continuous ascent)

### Scoring
- **+1 point** for each pipe successfully passed
- Score displayed in real-time on LCD top row
- High score saved to EEPROM and displayed on LCD bottom row

### Game Over
Collision occurs when:
- Player hits top or bottom screen boundary
- Player intersects with any pipe (top or bottom segment)

High score automatically updates if current score exceeds previous best.

---

## Code Structure

```
.
├── dshaw013_FlappyBirdV3.cpp  # Main game logic & state machines
├── spiAVR.h                   # SPI & ST7735 TFT driver
├── LCD.h                      # HD44780 LCD driver
├── EEPROM.h                   # Non-volatile storage driver
├── timerISR.h                 # Task scheduler timer
├── periph.h                   # PWM buzzer driver
├── helper.h                   # Utility functions (GCD, bit ops)
└── serialATmega.h             # UART debugging (optional)
```

---

## Technical Highlights

### Performance Optimizations
1. **Selective Rendering**: Only erases and redraws changed screen regions
2. **Column Structure**: Pipes stored as metadata, not pixel arrays
3. **Fixed-Point Math**: Integer arithmetic for all physics calculations
4. **SPI Bulk Transfers**: Minimizes per-pixel overhead with window-based drawing

### Memory Management
- **Total SRAM Usage**: ~1.5 KB
  - Level array: 128 × 3 bytes = 384 bytes
  - Task array: 6 × 16 bytes = 96 bytes
  - Stack and variables: ~1 KB
- **Program Flash**: ~8 KB (50% of ATmega1284 capacity)

### Real-Time Performance
- **Display Refresh**: ~10 FPS (limited by SPI bandwidth)
- **Physics Update**: 10 Hz (100ms task period)
- **Input Polling**: 10 Hz with debouncing
- **Collision Checks**: Every 100ms with multi-column lookahead

---

## Future Enhancements

- [ ] Dynamic difficulty scaling (faster scroll speed over time)
- [ ] Multiple level themes with different pipe patterns
- [ ] Sound effects tied to game events (jump, score, death)
- [ ] Animated player sprite with rotation
- [ ] Save/display top 3 high scores
- [ ] Touch sensor input support

---

## License

MIT License - Feel free to use this project for educational purposes.

---

## Acknowledgments

- State machine architecture inspired by UC Riverside CS120B coursework
- Game concept based on original Flappy Bird by Dong Nguyen


**⭐ If you found this project interesting, please star the repository!**
