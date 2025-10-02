// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "myfi.h"

#include "os_detection.h"

static bool typing_led_on = false;
static bool any_key_held = false;
static const uint16_t TYPING_FLASH_MS = 33;

static uint16_t breathing_cycle = 0;
static uint32_t last_typing_time = 0;
static uint8_t pwm_counter_common = 0;


#ifndef BIT
#define BIT(n) (1u << (n))
#endif

typedef enum {
    LED_MODE_NONE             = 0,
    LED_MODE_TYPING_HOLD      = BIT(0), // 타이핑 반응 적용 (홀드)
    LED_MODE_BREATHING        = BIT(1), // 브리딩 적용
    LED_MODE_TYPING_EDGE      = BIT(2), // 눌림 에지에서만 반응(홀드 무시)
    LED_MODE_INVERT           = BIT(3), // 타이핑 시 반전(끄기)
    LED_MODE_FORCE_ON         = BIT(4)  // 강제 켬(모드 무시)
} led_mode_flag_t;

typedef bool (*key_handler_t)(bool pressed, os_variant_t host);

#define EFFECT_NONE          LED_MODE_NONE
#define EFFECT_TYPING_HOLD   LED_MODE_TYPING_HOLD
#define EFFECT_BREATHING     LED_MODE_BREATHING
#define EFFECT_TYPING_EDGE   LED_MODE_TYPING_EDGE
#define EFFECT_INVERT        LED_MODE_INVERT
#define EFFECT_FORCE_ON      LED_MODE_FORCE_ON

// 타이핑 기반 효과 비트 마스크
#define EFFECT_TYPING  (LED_MODE_TYPING_HOLD | LED_MODE_TYPING_EDGE)
#define EFFECT_NEEDS_STATE  (EFFECT_TYPING | EFFECT_BREATHING)
#define EFFECT_HAS(mode, flag) (((mode) & (flag)) != 0)

// LED 핀 정의
#define LED_PIN_A6 A6
#define LED_PIN_A7 A7
#define LED_PIN_B0 B0

#define IDX_A6 0
#define IDX_A7 1
#define IDX_B0 2

static const pin_t kPins[3] = { LED_PIN_A6, LED_PIN_A7, LED_PIN_B0 };

// 인디케이터 소스: myfi 설정 사용 (0:none,1:scroll,2:caps)
typedef enum {
    IND_NONE = 0,
    IND_SCROLL = 1,
    IND_CAPS = 2,
} indicator_t;

static inline uint8_t get_pin_mode(uint8_t idx)
{
    return myfi_get_led_flags(idx);
}
static inline void set_pin_mode(uint8_t idx, uint8_t flags)
{
    myfi_set_led_flags(idx, flags);
}
static inline indicator_t get_indicator_src(uint8_t idx)
{
    return (indicator_t)myfi_get_indicator(idx);
}

static inline bool require_typing_state_update(void)
{
    const uint8_t m0 = get_pin_mode(IDX_A6);
    const uint8_t m1 = get_pin_mode(IDX_A7);
    const uint8_t m2 = get_pin_mode(IDX_B0);
    return (((m0 | m1 | m2) & EFFECT_NEEDS_STATE) != 0);
}

static inline bool in_typing_pulse_window(void)
{
    return (timer_elapsed32(last_typing_time) < TYPING_FLASH_MS);
}

static void update_led_effect_none(pin_t pin)
{
    writePinLow(pin);
}

static void update_led_effect_force_on(pin_t pin)
{
    writePinHigh(pin);
}

static void update_led_effect_breathing(pin_t pin)
{
    // 파형 적용은 housekeeping_task_user에서 수행
}

static void update_led_effect_typing_hold(pin_t pin, bool invert)
{
    if (any_key_held)
    {
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
    }
}

static void update_led_effect_typing_edge(pin_t pin, bool invert)
{
    if (in_typing_pulse_window())
    {
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
    }
}

static void update_led_effect_hold_breathing(pin_t pin, bool invert)
{
    if (any_key_held)
    {
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        bool idle_1s = (timer_elapsed32(last_typing_time) > 1000);
        if (!idle_1s)
        {
            if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
        }
    }
}

static void update_led_effect_edge_breathing(pin_t pin, bool invert)
{
    if (in_typing_pulse_window())
    {
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        bool idle_1s = (timer_elapsed32(last_typing_time) > 1000);
        if (!idle_1s)
        {
            if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
        }
    }
}

// 핀별 효과 적용 래퍼: 11개 옵션을 전용 함수로 라우팅
static void apply_pin_effect(pin_t pin, uint8_t mode)
{
    switch (mode)
    {
        case 0: // None
            update_led_effect_none(pin);
            break;
        case 16: // Force On
            update_led_effect_force_on(pin);
            break;
        case 1: // Typing Hold
            update_led_effect_typing_hold(pin, false);
            break;
        case 4: // Typing Edge
            update_led_effect_typing_edge(pin, false);
            break;
        case 2: // Breathing only
            update_led_effect_breathing(pin);
            break;
        case 9: // Hold + Invert (1+8)
            update_led_effect_typing_hold(pin, true);
            break;
        case 12: // Edge + Invert (4+8)
            update_led_effect_typing_edge(pin, true);
            break;
        case 6: // Edge + Breathing (4+2)
            update_led_effect_edge_breathing(pin, false);
            break;
        case 3: // Hold + Breathing (1+2)
            update_led_effect_hold_breathing(pin, false);
            break;
        case 11: // Hold + Breathing + Invert (1+2+8)
            update_led_effect_hold_breathing(pin, true);
            break;
        case 14: // Edge + Breathing + Invert (4+2+8)
            update_led_effect_edge_breathing(pin, true);
            break;
        default:
            writePinLow(pin);
            break;
    }
}

// led*_effect_enabled 매크로 제거됨: 타이핑 관련 효과가 있을 때만 입력 상태를 추적

enum my_keymap_layers
{
    LAYER_BASE = 0,
    LAYER_LOWER,
    LAYER_RAISE,
    LAYER_POINTER,
    LAYER_EXTRA,
    LAYER_EXTRA2,
};

#define LOWER MO(LAYER_LOWER)
#define RAISE MO(LAYER_RAISE)
#define POINT MO(LAYER_POINTER)
#define EXTRA1 MO(LAYER_EXTRA)
#define EXTRA2 MO(LAYER_EXTRA2)

#define _LO(x) LT(LAYER_LOWER, x)
#define _RA(x) LT(LAYER_RAISE, x)
#define _PT(x) LT(LAYER_POINTER, x)

#define _C(x) CTL_T(x)
#define _A(x) ALT_T(x)
#define _G(x) GUI_T(x)
#define _LS(x) LSFT_T(x)
#define _RS(x) RSFT_T(x)

#define PT_Z LT(LAYER_POINTER, KC_Z)
#define PT_SLSH LT(LAYER_POINTER, KC_SLSH)

#define LO_SPC _LO(KC_SPC)
#define LO_COMM _LO(KC_COMM)
#define LO_DOT _LO(KC_DOT)

#define RA_COMM _RA(KC_COMM)
#define RA_DOT _RA(KC_DOT)

#define TO_PT TO(LAYER_POINTER)
#define TO_BA TO(LAYER_BASE)

#define SH____1 LSFT(KC_1)
#define SH____2 LSFT(KC_2)
#define SH____3 LSFT(KC_3)
#define SH____4 LSFT(KC_4)
#define SH____5 LSFT(KC_5)
#define SH____6 LSFT(KC_6)
#define SH____7 LSFT(KC_7)
#define SH____8 LSFT(KC_8)
#define SH____9 LSFT(KC_9)
#define SH____0 LSFT(KC_0)

#define SH__GRV LSFT(KC_GRV)
#define SH_MINS LSFT(KC_MINS)
#define SH__EQL LSFT(KC_EQL)
#define SH_LBRC LSFT(KC_LBRC)
#define SH_RBRC LSFT(KC_RBRC)
#define SH_BSLS LSFT(KC_BSLS)
#define SH_QUOT LSFT(KC_QUOT)
#define SH_SCLN LSFT(KC_SCLN)
#define SH_COMM LSFT(KC_COMM)
#define SH_DOT LSFT(KC_DOT)
#define SH_SLSH LSFT(KC_SLSH)

enum keycodes
{
    GO_LEFT = QK_KB_0, // ctrl + left, left desktop, win ctrl <
    GO_RGHT,              // ctrl + right, right desktop, win ctrl >
    GO_UP,                // ctrl + up, mission control, win tab

    WO_LEFT, // next word
    WO_RGHT, // prev word
    OS_LANG, // change language, ctrl + space at mac, right alt at window
    OS_PSCR,

    MC_LCMD, // work lcmd on mac, lctl on win
    MC_LCTL, // work lctl on mac, lgui on win

    VS_BRCK, // visual studio put break point
    VC_FLDA, // visual studio code fold all
    VC_UFDA, // visual studio code unfold all
    VC_FLDR, // visual studio code fold recursive
    VC_UFDR, // visual studio code unfold recursive
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    // clang-format off
    [0] = LAYOUT(
        KC_ESC,           KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,            KC_F9,   KC_F10,  KC_F11,  KC_F12,     KC_PSCR, KC_SCRL, KC_PAUS,
        KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSLS, KC_BSPC,     KC_INS,  KC_HOME, KC_PGUP,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,             KC_DEL,  KC_END,  KC_PGDN,
        KC_CAPS,          KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_BSLS, KC_ENT,
        MO(1),   KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,          KC_RSFT, MO(1),               KC_UP,
        KC_LCTL, KC_LGUI, KC_LGUI, KC_LALT,           KC_SPC,           KC_SPC,           KC_SPC,           KC_RALT,  KC_RGUI, KC_RGUI, KC_RCTL,    KC_LEFT, KC_DOWN, KC_RGHT
    ),
    [1] = LAYOUT(
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,             XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
        MO(1),   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, MO(1),               XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX
    ),
    [2] = LAYOUT(
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,             XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
        MO(1),   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, MO(1),               XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX
    ),
    [3] = LAYOUT(
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,             XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
        MO(1),   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, MO(1),               XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX
    ),
    // clang-format on
};


void keyboard_post_init_user(void)
{
    // A6, A7, B0 핀 출력 설정
    setPinOutput(LED_PIN_A6);
    setPinOutput(LED_PIN_A7);
    setPinOutput(LED_PIN_B0);
}



void matrix_scan_user(void)
{
    led_t leds = host_keyboard_led_state();
    for (uint8_t i = 0; i < 3; i++)
    {
        const pin_t pin = kPins[i];
        const indicator_t ind = get_indicator_src(i);
        const uint8_t mode = get_pin_mode(i);

        bool ind_on = false;
        if (ind == IND_SCROLL) ind_on = leds.scroll_lock;
        else if (ind == IND_CAPS) ind_on = leds.caps_lock;

        if (ind_on)
        {
            writePinHigh(pin);
        }
        else
        {
            apply_pin_effect(pin, mode);
        }
    }
}

void housekeeping_task_user(void)
{
    breathing_cycle = (breathing_cycle + 1) % 8000; // 4초 주기 가정
    uint8_t brightness;
    uint16_t phase = breathing_cycle % 8000;
    uint16_t half_cycle = 4000;
    if (phase < half_cycle)
    {
        brightness = 45 + (phase * 210) / half_cycle;
    }
    else
    {
        uint16_t progress = 8000 - phase;
        brightness = 45 + (progress * 210) / half_cycle;
    }

    pwm_counter_common += 12;
    bool new_led_state = (pwm_counter_common < brightness);
    const bool in_pulse = in_typing_pulse_window();
    const bool idle_1s = (timer_elapsed32(last_typing_time) > 1000);
    led_t leds = host_keyboard_led_state();
    for (uint8_t i = 0; i < 3; i++)
    {
        const pin_t pin = kPins[i];
        const indicator_t ind = get_indicator_src(i);
        const uint8_t mode = get_pin_mode(i);

        // 인디케이터 ON이면 이펙트 무시
        bool ind_on = false;
        if (ind == IND_SCROLL) ind_on = leds.scroll_lock;
        else if (ind == IND_CAPS) ind_on = leds.caps_lock;
        if (ind_on)
        {
            writePinHigh(pin);
            continue;
        }

        if (EFFECT_HAS(mode, EFFECT_BREATHING))
        {
            bool want_hold = EFFECT_HAS(mode, EFFECT_TYPING_HOLD);
            bool want_edge = EFFECT_HAS(mode, EFFECT_TYPING_EDGE);
            bool allow_breath;
            if (!want_hold && !want_edge)
            {
                allow_breath = idle_1s;
            }
            else if (want_hold)
            {
                allow_breath = (!any_key_held) && idle_1s;
            }
            else /* want_edge */
            {
                allow_breath = (!in_pulse) && idle_1s;
            }

            if (allow_breath)
            {
                if (new_led_state) { writePinHigh(pin); } else { writePinLow(pin); }
            }
        }
    }
}

// Key handlers (return false to stop further processing)
static bool h_os_lang(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        tap_code(KC_RALT);
    }
    else
    {
        register_code(KC_LCTL);
        tap_code(KC_SPACE);
        unregister_code(KC_LCTL);
    }
    return false;
}

static bool h_os_pscr(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LGUI);
        register_code(KC_LSFT);
        tap_code(KC_S);
        unregister_code(KC_LGUI);
        unregister_code(KC_LSFT);
    }
    else
    {
        register_code(KC_LGUI);
        register_code(KC_LSFT);
        tap_code(KC_4);
        unregister_code(KC_LGUI);
        unregister_code(KC_LSFT);
    }
    return false;
}

static bool h_word_left(bool pressed, os_variant_t host)
{
    if (pressed)
    {
        if (host == OS_WINDOWS)
        {
            register_code(KC_LCTL);
            register_code(KC_LEFT);
        }
        else
        {
            register_code(KC_LALT);
            register_code(KC_LEFT);
        }
    }
    else
    {
        if (host == OS_WINDOWS)
        {
            unregister_code(KC_LEFT);
            unregister_code(KC_LCTL);
        }
        else
        {
            unregister_code(KC_LEFT);
            unregister_code(KC_LALT);
        }
    }
    return false;
}

static bool h_word_right(bool pressed, os_variant_t host)
{
    if (pressed)
    {
        if (host == OS_WINDOWS)
        {
            register_code(KC_LCTL);
            register_code(KC_RIGHT);
        }
        else
        {
            register_code(KC_LALT);
            register_code(KC_RIGHT);
        }
    }
    else
    {
        if (host == OS_WINDOWS)
        {
            unregister_code(KC_RIGHT);
            unregister_code(KC_LCTL);
        }
        else
        {
            unregister_code(KC_RIGHT);
            unregister_code(KC_LALT);
        }
    }
    return false;
}

static bool h_mc_lcmd(bool pressed, os_variant_t host)
{
    if (host == OS_WINDOWS)
    {
        if (pressed) register_code(KC_LCTL); else unregister_code(KC_LCTL);
    }
    else
    {
        if (pressed) register_code(KC_LGUI); else unregister_code(KC_LGUI);
    }
    return false;
}

static bool h_mc_lctl(bool pressed, os_variant_t host)
{
    if (host == OS_WINDOWS)
    {
        if (pressed) register_code(KC_LGUI); else unregister_code(KC_LGUI);
    }
    else
    {
        if (pressed) register_code(KC_LCTL); else unregister_code(KC_LCTL);
    }
    return false;
}

static bool h_go_left(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        // reg: LGUI then LCTL, unreg: LCTL then LGUI
        register_code(KC_LGUI);
        register_code(KC_LCTL);
        tap_code(KC_LEFT);
        unregister_code(KC_LCTL);
        unregister_code(KC_LGUI);
    }
    else
    {
        register_code(KC_LCTL);
        tap_code(KC_LEFT);
        unregister_code(KC_LCTL);
    }
    return false;
}

static bool h_go_right(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LGUI);
        register_code(KC_LCTL);
        tap_code(KC_RIGHT);
        unregister_code(KC_LCTL);
        unregister_code(KC_LGUI);
    }
    else
    {
        register_code(KC_LCTL);
        tap_code(KC_RIGHT);
        unregister_code(KC_LCTL);
    }
    return false;
}

static bool h_go_up(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LGUI);
        tap_code(KC_TAB);
        unregister_code(KC_LGUI);
    }
    else
    {
        register_code(KC_LCTL);
        tap_code(KC_UP);
        unregister_code(KC_LCTL);
    }
    return false;
}

static bool h_vs_brck(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LCTL);
        register_code(KC_LALT);
        tap_code(KC_PAUS);
        unregister_code(KC_LALT);
        unregister_code(KC_LCTL);
    }
    else
    {
        tap_code(KC_PAUS);
    }
    return false;
}

static bool h_vc_flda(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LCTL);
        tap_code(KC_K);
        tap_code(KC_0);
        unregister_code(KC_LCTL);
    }
    else
    {
        register_code(KC_LGUI);
        tap_code(KC_K);
        tap_code(KC_0);
        unregister_code(KC_LGUI);
    }
    return false;
}

static bool h_vc_ufda(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LCTL);
        tap_code(KC_K);
        tap_code(KC_J);
        unregister_code(KC_LCTL);
    }
    else
    {
        register_code(KC_LGUI);
        tap_code(KC_K);
        tap_code(KC_J);
        unregister_code(KC_LGUI);
    }
    return false;
}

static bool h_vc_fldr(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LCTL);
        tap_code(KC_K);
        tap_code(KC_LBRC);
        unregister_code(KC_LCTL);
    }
    else
    {
        register_code(KC_LGUI);
        tap_code(KC_K);
        tap_code(KC_LBRC);
        unregister_code(KC_LGUI);
    }
    return false;
}

static bool h_vc_ufdr(bool pressed, os_variant_t host)
{
    if (!pressed) return false;
    if (host == OS_WINDOWS)
    {
        register_code(KC_LCTL);
        tap_code(KC_K);
        tap_code(KC_RBRC);
        unregister_code(KC_LCTL);
    }
    else
    {
        register_code(KC_LGUI);
        tap_code(KC_K);
        tap_code(KC_RBRC);
        unregister_code(KC_LGUI);
    }
    return false;
}

typedef struct {
    uint16_t keycode;
    key_handler_t handler;
} key_handler_entry_t;

static const key_handler_entry_t s_key_handlers[] = {
    { OS_LANG, h_os_lang },
    { OS_PSCR, h_os_pscr },
    { WO_LEFT, h_word_left },
    { WO_RGHT, h_word_right },
    { MC_LCMD, h_mc_lcmd },
    { MC_LCTL, h_mc_lctl },
    { GO_LEFT, h_go_left },
    { GO_RGHT, h_go_right },
    { GO_UP,   h_go_up },
    { VS_BRCK, h_vs_brck },
    { VC_FLDA, h_vc_flda },
    { VC_UFDA, h_vc_ufda },
    { VC_FLDR, h_vc_fldr },
    { VC_UFDR, h_vc_ufdr },
};

static inline key_handler_t find_key_handler(uint16_t kc)
{
    for (size_t i = 0; i < sizeof(s_key_handlers) / sizeof(s_key_handlers[0]); i++)
    {
        if (s_key_handlers[i].keycode == kc) return s_key_handlers[i].handler;
    }
    return NULL;
}

bool process_record_user(uint16_t keycode, keyrecord_t* record)
{
    if (require_typing_state_update())
    {
        if (record->event.pressed)
        {
            // 눌림 순간만 감지 (홀드 동안은 추가 갱신 없음)
            typing_led_on = true;
            last_typing_time = timer_read32();
            any_key_held = true;
        }
        else
        {
            // 눌린 키 없으면 꺼지게: 매트릭스 스캔 기반
            bool still_held = false;
            for (uint8_t row = 0; row < MATRIX_ROWS; row++)
            {
                if (matrix_get_row(row)) { still_held = true; break; }
            }
            any_key_held = still_held;
            typing_led_on = still_held;
        }
    }

    os_variant_t host = detected_host_os();
    key_handler_t handler = find_key_handler(keycode);
    if (handler != NULL)
    {
        return handler(record->event.pressed, host) /* always false currently */;
    }

    return true;
}
