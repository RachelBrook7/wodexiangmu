#include <REG51.H>

/* =========================================
 * Traffic Light + Stepper + Keypad + 4-Digit 7-Seg
 * Target: AT89C51, 12 MHz
 * Compiler: Keil C51
 * ========================================= */

#define FOSC 12000000UL
/* 定时器初值 (12MHz, 1ms) -> 65536 - 1000 = 64536 (0xFC18) */
#define TMR_RELOAD_1MS (65536UL - (FOSC/12UL/1000UL))

volatile unsigned long g_millis = 0;

/* --- 7-Segment Display Definitions --- */
#define SEG_PORT P0
sbit DIG1 = P2^4;
sbit DIG2 = P2^5;
sbit DIG3 = P2^6;
sbit DIG4 = P2^7;

/* --- Traffic Lights --- */
sbit LED_R = P2^0;
sbit LED_Y = P2^1;
sbit LED_G = P2^2;

/* --- Stepper Motor --- */
sbit M1 = P1^0;
sbit M2 = P1^1;
sbit M3 = P1^2;
sbit M4 = P1^3;

/* --- Keypad --- */
sbit R0 = P3^0;
sbit R1 = P3^1;
sbit R2 = P3^2;
sbit R3 = P3^3;
sbit C0 = P3^4;
sbit C1 = P3^5;
sbit C2 = P3^6;
sbit C3 = P3^7;

#define BLANK 0x00
#define DASH  0x40

/* 数码管段码 (存放在 code 区) */
unsigned char code DIGITS[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

/* 步进电机序列 (存放在 code 区) */
unsigned char code halfstep_seq[8] = { 0x01,0x03,0x02,0x06,0x04,0x0C,0x08,0x09 };

volatile unsigned char display_digits[4] = { BLANK, BLANK, BLANK, BLANK };

/* Keypad Enumerations */
typedef enum {
    KEY_NONE = 0,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_STAR, KEY_HASH
} KeyCode_t; /* 改名以避免潜在冲突 */

typedef struct {
    KeyCode_t key_id;     /* [修复] 原名 'code' 是关键字，改为 'key_id' */
    unsigned char pressed;
} KeyEvent;

/* Debounce State */
static unsigned char stable_code  = KEY_NONE;
static unsigned char last_code    = KEY_NONE;
static unsigned char debounce_cnt = 0;
#define DEBOUNCE_MS 15

/* User Input State */
static unsigned char input_mode   = 0;
static unsigned int  input_value  = 0;
static unsigned char input_digits = 0;

/* Traffic & Gate State */
typedef enum { TS_RED = 0, TS_YELLOW = 1, TS_GREEN = 2 } TrafficState;
typedef enum { GATE_UP = 0, GATE_DOWN = 1 } GateState;

volatile TrafficState t_state = TS_RED;
volatile GateState    g_state = GATE_DOWN;

volatile unsigned int  red_sec       = 10;
volatile unsigned int  yellow_sec    = 3;
volatile unsigned int  green_sec     = 10;
volatile unsigned int  countdown_sec = 0;
volatile unsigned char gate_cmd_up   = 0;
volatile unsigned char gate_cmd_down = 0;

/* Stepper Variables */
#define STEPPER_STEP_MS 3
static volatile int           remaining_steps = 0;
static volatile unsigned char seq_idx         = 0;
static volatile unsigned int  step_timer      = 0;

/* --- Functions --- */

unsigned long millis(void){
    unsigned long t;
    EA = 0;
    t = g_millis;
    EA = 1;
    return t;
}

void stepper_phase(unsigned char m){
    M1 = (m & 1) ? 1 : 0;
    M2 = (m & 2) ? 1 : 0;
    M3 = (m & 4) ? 1 : 0;
    M4 = (m & 8) ? 1 : 0;
}

void gate_raise(void){ remaining_steps = 500; }
void gate_lower(void){ remaining_steps = -500; }

void stepper_tick_1ms(void){
    if(remaining_steps == 0){
        stepper_phase(0x00);
        return;
    }
    step_timer++;
    if(step_timer >= STEPPER_STEP_MS){
        step_timer = 0;
        if(remaining_steps > 0){
            seq_idx = (unsigned char)((seq_idx + 1) & 7);
            remaining_steps--;
        }else{
            seq_idx = (seq_idx == 0) ? 7 : (unsigned char)(seq_idx - 1);
            remaining_steps++;
        }
        stepper_phase(halfstep_seq[seq_idx]);
    }
}

static KeyCode_t read_raw_key(void){
    unsigned char col;

    R0=0; R1=1; R2=1; R3=1;
    col = ((C0==0)<<0) | ((C1==0)<<1) | ((C2==0)<<2) | ((C3==0)<<3);
    if(col){ if(col&1) return KEY_1; if(col&2) return KEY_2; if(col&4) return KEY_3; if(col&8) return KEY_A; }

    R0=1; R1=0; R2=1; R3=1;
    col = ((C0==0)<<0) | ((C1==0)<<1) | ((C2==0)<<2) | ((C3==0)<<3);
    if(col){ if(col&1) return KEY_4; if(col&2) return KEY_5; if(col&4) return KEY_6; if(col&8) return KEY_B; }

    R0=1; R1=1; R2=0; R3=1;
    col = ((C0==0)<<0) | ((C1==0)<<1) | ((C2==0)<<2) | ((C3==0)<<3);
    if(col){ if(col&1) return KEY_7; if(col&2) return KEY_8; if(col&4) return KEY_9; if(col&8) return KEY_C; }

    R0=1; R1=1; R2=1; R3=0;
    col = ((C0==0)<<0) | ((C1==0)<<1) | ((C2==0)<<2) | ((C3==0)<<3);
    if(col){ if(col&1) return KEY_STAR; if(col&2) return KEY_0; if(col&4) return KEY_HASH; if(col&8) return KEY_D; }

    return KEY_NONE;
}

void keypad_tick_1ms(void){
    KeyCode_t rc = read_raw_key();
    if(rc == last_code){
        if(debounce_cnt < DEBOUNCE_MS) debounce_cnt++;
        if(debounce_cnt >= DEBOUNCE_MS) stable_code = rc;
    }else{
        last_code = rc;
        debounce_cnt = 0;
    }
}

bit keypad_poll(KeyEvent *ev){
    static unsigned char prev = KEY_NONE;
    if(stable_code != prev){
        if(stable_code != KEY_NONE){
            ev->key_id  = (KeyCode_t)stable_code; /* [修复] 变量名改为 key_id */
            ev->pressed = 1;
            prev        = stable_code;
            return 1;
        }else{
            prev = stable_code;
        }
    }
    return 0;
}

void apply_key_event(const KeyEvent *ev){
    unsigned char d;
    if(!ev->pressed) return;

    /* [修复] 使用新变量名 ev->key_id */
    switch(ev->key_id){
        case KEY_A: input_mode=1; input_value=0; input_digits=0; break;
        case KEY_B: input_mode=2; input_value=0; input_digits=0; break;
        case KEY_C: input_mode=3; input_value=0; input_digits=0; break;
        case KEY_D:
            if(input_mode==1 && input_digits) red_sec    = (input_value>9999 ? 9999 : input_value);
            if(input_mode==2 && input_digits) yellow_sec = (input_value>9999 ? 9999 : input_value);
            if(input_mode==3 && input_digits) green_sec  = (input_value>9999 ? 9999 : input_value);
            input_mode=0; input_value=0; input_digits=0; 
            break;
        case KEY_STAR: 
            input_value=0; input_digits=0; 
            break;
        case KEY_HASH: 
            red_sec=10; yellow_sec=3; green_sec=10; 
            input_mode=0; input_value=0; input_digits=0; 
            break;
        case KEY_0: case KEY_1: case KEY_2: case KEY_3: case KEY_4:
        case KEY_5: case KEY_6: case KEY_7: case KEY_8: case KEY_9:
            if(input_mode && input_digits<4){
                d = (unsigned char)(ev->key_id - KEY_0); /* [修复] 使用 key_id */
                input_value = (unsigned int)(input_value*10 + d);
                input_digits++;
            }
            break;
        default: break;
    }
}

void update_display_buffer(unsigned int sec){
    unsigned int t;
    unsigned char thou, hund, tens, ones;

    if(sec > 9999){
        display_digits[0] = DASH; display_digits[1] = DASH;
        display_digits[2] = DASH; display_digits[3] = DASH;
        return;
    }

    t = sec;
    thou = (unsigned char)(t / 1000); t %= 1000;
    hund = (unsigned char)(t / 100);  t %= 100;
    tens = (unsigned char)(t / 10);
    ones = (unsigned char)(t % 10);

    display_digits[0] = (thou ? DIGITS[thou] : BLANK);
    display_digits[1] = ((thou || hund) ? DIGITS[hund] : BLANK);
    display_digits[2] = ((thou || hund || tens) ? DIGITS[tens] : BLANK);
    display_digits[3] = DIGITS[ones];
}

void set_light(TrafficState s){
    LED_R = (s == TS_RED);
    LED_Y = (s == TS_YELLOW);
    LED_G = (s == TS_GREEN);
}

void traffic_next_state(void){
    switch(t_state){
        case TS_RED:
            t_state = TS_GREEN;
            countdown_sec = green_sec;
            set_light(t_state);
            gate_cmd_up = 1;
            break;
        case TS_GREEN:
            t_state = TS_YELLOW;
            countdown_sec = yellow_sec;
            set_light(t_state);
            break;
        default:
            t_state = TS_RED;
            countdown_sec = red_sec;
            set_light(t_state);
            gate_cmd_down = 1;
            break;
    }
}

void timer0_init_ms(void){
    unsigned int r = (unsigned int)TMR_RELOAD_1MS;
    TMOD &= 0xF0; TMOD |= 0x01;
    TH0 = (unsigned char)((r>>8)&0xFF);
    TL0 = (unsigned char)(r&0xFF);
    ET0 = 1; TR0 = 1;
}

void timer1_init_ms(void){
    unsigned int r = (unsigned int)TMR_RELOAD_1MS;
    TMOD &= 0x0F; TMOD |= 0x10;
    TH1 = (unsigned char)((r>>8)&0xFF);
    TL1 = (unsigned char)(r&0xFF);
    ET1 = 1; TR1 = 1;
}

void timer0_isr(void) interrupt 1 {
    static unsigned char scan_idx = 0;
    unsigned int r = (unsigned int)TMR_RELOAD_1MS;
    TH0 = (unsigned char)((r>>8)&0xFF);
    TL0 = (unsigned char)(r&0xFF);
    g_millis++;

    DIG1=1; DIG2=1; DIG3=1; DIG4=1;
    SEG_PORT = display_digits[scan_idx];
    if(scan_idx==0) DIG1=0; else if(scan_idx==1) DIG2=0; 
    else if(scan_idx==2) DIG3=0; else DIG4=0;
    
    scan_idx++;
    if(scan_idx > 3) scan_idx = 0;
}

void timer1_isr(void) interrupt 3 {
    unsigned int r = (unsigned int)TMR_RELOAD_1MS;
    TH1 = (unsigned char)((r>>8)&0xFF);
    TL1 = (unsigned char)(r&0xFF);
    keypad_tick_1ms();
    stepper_tick_1ms();
}

void io_init(void){
    P3 = 0xFF; /* 输入模式准备 */
    DIG1=1; DIG2=1; DIG3=1; DIG4=1;
    set_light(TS_RED);
}

void main(void){
    unsigned long last;
    unsigned long now;
    KeyEvent ev;

    EA = 0;
    io_init();
    timer0_init_ms();
    timer1_init_ms();
    EA = 1;

    t_state = TS_RED;
    countdown_sec = red_sec;
    update_display_buffer(countdown_sec);

    last = millis();
    
    while(1){
        if(keypad_poll(&ev)){
            apply_key_event(&ev);
            if(input_mode != 0) update_display_buffer(input_value);
            else update_display_buffer(countdown_sec);
        }

        if(gate_cmd_up){
            gate_cmd_up = 0; gate_raise(); g_state = GATE_UP;
        }
        if(gate_cmd_down){
            gate_cmd_down = 0; gate_lower(); g_state = GATE_DOWN;
        }

        now = millis();
        if((now - last) >= 1000UL){
            last += 1000UL;
            if(input_mode == 0) {
                if(countdown_sec > 0) countdown_sec--;
                else traffic_next_state();
                update_display_buffer(countdown_sec);
            }
        }
    }
}
