// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "myfi.h"

#include "os_detection.h"

// 타이핑 LED 제어 변수
static bool typing_led_on = false;     // 현재 점등 여부

// 브리딩 LED 제어 변수
static uint16_t breathing_cycle = 0;    // 0-1023 사이클
static uint32_t last_typing_time = 0;   // 마지막 타이핑 시간
static bool breathing_mode = false;     // 공용 브리딩 모드 여부
static uint8_t pwm_counter_common = 0;  // 공용 브리딩 PWM 카운터
static bool any_key_held = false;       // 현재 하나라도 눌림 유지 중인지
// 키 입력 순간 점멸 지속 시간(ms)
static const uint16_t TYPING_FLASH_MS = 33;

// LED 동작 모드 플래그 (가독성 향상)
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

// 별칭(읽기 쉬운 이름)
#define EFFECT_NONE          LED_MODE_NONE
#define EFFECT_TYPING_HOLD   LED_MODE_TYPING_HOLD
#define EFFECT_BREATHING     LED_MODE_BREATHING
#define EFFECT_TYPING_EDGE   LED_MODE_TYPING_EDGE
#define EFFECT_INVERT        LED_MODE_INVERT
#define EFFECT_FORCE_ON      LED_MODE_FORCE_ON

// 헬퍼
#define EFFECT_HAS(mode, flag) (((mode) & (flag)) != 0)

// (중복 정의 제거됨)

// LED 핀 정의
#define LED_PIN_A6 A6
#define LED_PIN_A7 A7
#define LED_PIN_B0 B0

// 핀 인덱스
#define IDX_A6 0
#define IDX_A7 1
#define IDX_B0 2

// 핀 배열
static const pin_t kPins[3] = { LED_PIN_A6, LED_PIN_A7, LED_PIN_B0 };

// 인디케이터 소스: myfi 설정 사용 (0:none,1:scroll,2:caps)
typedef enum {
    IND_NONE = 0,
    IND_SCROLL = 1,
    IND_CAPS = 2,
} indicator_t;

// VIA 저장소(myfi)의 플래그와 연동: getter/setter로 접근
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

// 공통 유틸
static inline bool in_typing_pulse_window(void)
{
    return (timer_elapsed32(last_typing_time) < TYPING_FLASH_MS);
}

static inline void stop_breathing_on_typing(void)
{
    breathing_mode = false;
    breathing_cycle = 0;
    pwm_counter_common = 0;
}

static inline void begin_breathing_if_idle(void)
{
    if (timer_elapsed32(last_typing_time) > 1000)
    {
        if (!breathing_mode)
        {
            breathing_mode = true;
            breathing_cycle = 0;
            pwm_counter_common = 0;
        }
    }
}

// 11가지 효과별 전용 함수
static void update_led_effect_none(pin_t pin)
{
    breathing_mode = false;
    writePinLow(pin);
}

static void update_led_effect_force_on(pin_t pin)
{
    breathing_mode = false;
    writePinHigh(pin);
}

static void update_led_effect_breathing(pin_t pin)
{
    // 브리딩 모드는 조건 없이 즉시 활성화
    breathing_mode = true;
    // 파형 적용은 housekeeping_task_user에서 수행
}

static void update_led_effect_typing_hold(pin_t pin, bool invert)
{
    if (any_key_held)
    {
        stop_breathing_on_typing();
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        breathing_mode = false;
        if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
    }
}

static void update_led_effect_typing_edge(pin_t pin, bool invert)
{
    if (in_typing_pulse_window())
    {
        stop_breathing_on_typing();
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        breathing_mode = false;
        if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
    }
}

static void update_led_effect_hold_breathing(pin_t pin, bool invert)
{
    if (any_key_held)
    {
        stop_breathing_on_typing();
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        // 조건부 브리딩: 아이들 1초 이후에만 브리딩, 그 전에는 기본 꺼짐 유지
        begin_breathing_if_idle();
        if (!breathing_mode)
        {
            writePinLow(pin);
        }
    }
}

static void update_led_effect_edge_breathing(pin_t pin, bool invert)
{
    if (in_typing_pulse_window())
    {
        stop_breathing_on_typing();
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        // 조건부 브리딩: 아이들 1초 이후에만 브리딩, 그 전에는 기본 꺼짐 유지
        begin_breathing_if_idle();
        if (!breathing_mode)
        {
            writePinLow(pin);
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
            // 알 수 없는 조합은 안전하게 끄기
            breathing_mode = false;
            writePinLow(pin);
            break;
    }
}

// 가독성을 위한 효과 활성화 매크로
#define led0_effect_enabled (get_pin_mode(IDX_A6) != 0)
#define led1_effect_enabled (get_pin_mode(IDX_A7) != 0)
#define led2_effect_enabled (get_pin_mode(IDX_B0) != 0)

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


// --- 일반화된 LED 유틸리티 ---
// (미사용 헬퍼 제거)

bool process_record_user(uint16_t keycode, keyrecord_t* record)
{
    // 과거 로직 복원: 효과가 하나라도 켜져 있으면 타이핑 상태 관리
    if (led0_effect_enabled || led1_effect_enabled || led2_effect_enabled)
    {
        if (record->event.pressed)
        {
            // 눌림 순간만 감지 (홀드 동안은 추가 갱신 없음)
            typing_led_on = true;
            last_typing_time = timer_read32();
            breathing_mode = false;
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
    switch (keycode)
    {
        case OS_LANG:
            if (record->event.pressed)
            {
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
            }
            return false;
        case OS_PSCR:
            if (host == OS_WINDOWS)
            {
                if (record->event.pressed)
                {
                    register_code(KC_LGUI);
                    register_code(KC_LSFT);
                    tap_code(KC_S);
                    unregister_code(KC_LGUI);
                    unregister_code(KC_LSFT);
                }
            }
            else
            {
                if (record->event.pressed)
                {
                    register_code(KC_LGUI);
                    register_code(KC_LSFT);
                    tap_code(KC_4);
                    unregister_code(KC_LGUI);
                    unregister_code(KC_LSFT);
                }
            }
            return false;

        case WO_LEFT:
            if (record->event.pressed)
            {
                if (host == OS_WINDOWS)
                {
                    register_code(KC_LCTL);
                    register_code(KC_LEFT);
                    // unregister_code(KC_LCTL);
                }
                else
                {
                    register_code(KC_LALT);
                    register_code(KC_LEFT);
                    // unregister_code(KC_LALT);
                }
            }
            else
            {
                if (host == OS_WINDOWS)
                {
                    // register_code(KC_LCTL);
                    unregister_code(KC_LEFT);
                    unregister_code(KC_LCTL);
                }
                else
                {
                    // register_code(KC_LALT);
                    unregister_code(KC_LEFT);
                    unregister_code(KC_LALT);
                }
            }
            return false;

        case WO_RGHT:
            if (record->event.pressed)
            {
                if (host == OS_WINDOWS)
                {
                    register_code(KC_LCTL);
                    register_code(KC_RIGHT);
                    // unregister_code(KC_LCTL);
                }
                else
                {
                    register_code(KC_LALT);
                    register_code(KC_RIGHT);
                    // unregister_code(KC_LALT);
                }
            }
            else
            {
                if (host == OS_WINDOWS)
                {
                    // register_code(KC_LCTL);
                    unregister_code(KC_RIGHT);
                    unregister_code(KC_LCTL);
                }
                else
                {
                    // register_code(KC_LALT);
                    unregister_code(KC_RIGHT);
                    unregister_code(KC_LALT);
                }
            }

            return false;

        case MC_LCMD:
            if (host == OS_WINDOWS)
            {
                if (record->event.pressed)
                    register_code(KC_LCTL);
                else
                    unregister_code(KC_LCTL);
            }
            else
            {
                if (record->event.pressed)
                    register_code(KC_LGUI);
                else
                    unregister_code(KC_LGUI);
            }

            return false;

        case MC_LCTL:
            if (host == OS_WINDOWS)
            {
                if (record->event.pressed)
                    register_code(KC_LGUI);
                else
                    unregister_code(KC_LGUI);
            }
            else
            {
                if (record->event.pressed)
                    register_code(KC_LCTL);
                else
                    unregister_code(KC_LCTL);
            }

            return false;

        case GO_LEFT:
            if (host == OS_WINDOWS)
            {
                if (record->event.pressed)
                {
                    register_code(KC_LGUI);
                    register_code(KC_LCTL);
                    tap_code(KC_LEFT);
                    unregister_code(KC_LCTL);
                    unregister_code(KC_LGUI);
                }
            }
            else
            {
                if (record->event.pressed)
                {
                    register_code(KC_LCTL);
                    tap_code(KC_LEFT);
                    unregister_code(KC_LCTL);
                }
            }

            return false;

        case GO_RGHT:
            if (host == OS_WINDOWS)
            {
                if (record->event.pressed)
                {
                    register_code(KC_LGUI);
                    register_code(KC_LCTL);
                    tap_code(KC_RIGHT);
                    unregister_code(KC_LCTL);
                    unregister_code(KC_LGUI);
                }
            }
            else
            {
                if (record->event.pressed)
                {
                    register_code(KC_LCTL);
                    tap_code(KC_RIGHT);
                    unregister_code(KC_LCTL);
                }
            }

            return false;

        case GO_UP:
            if (host == OS_WINDOWS)
            {
                if (record->event.pressed)
                {
                    register_code(KC_LGUI);
                    tap_code(KC_TAB);
                    unregister_code(KC_LGUI);
                }
            }
            else
            {
                if (record->event.pressed)
                {
                    register_code(KC_LCTL);
                    tap_code(KC_UP);
                    unregister_code(KC_LCTL);
                }
            }
            return false;

        case VS_BRCK:
            if (record->event.pressed)
            {
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
            }
            return false;

        case VC_FLDA:
            if (record->event.pressed)
            {
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
            }
            return false;

        case VC_UFDA:
            if (record->event.pressed)
            {
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
            }
            return false;

        case VC_FLDR:
            if (record->event.pressed)
            {
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
            }
            return false;

        case VC_UFDR:
            if (record->event.pressed)
            {
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
            }
            return false;
        // TG_LED* 제거됨: VIA에서 제어
    }

    return true;
}

void keyboard_post_init_user(void)
{
    // A6, A7, B0 핀 출력 설정
    setPinOutput(LED_PIN_A6);
    setPinOutput(LED_PIN_A7);
    setPinOutput(LED_PIN_B0);
}

// 제거됨: 효과별 전용 함수로 대체

void matrix_scan_user(void)
{
    // 핀별 루프: 인디케이터 우선 -> 이펙트
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
    // 브리딩 활성화 시 공용 파형 계산
    if (breathing_mode)
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

            // 이 핀이 브리딩 모드면 파형 적용
            if (mode & LED_MODE_BREATHING)
            {
                if (new_led_state) { writePinHigh(pin); } else { writePinLow(pin); }
            }
        }
    }
}

bool led_update_user(led_t led_state)
{
    // 인디케이터/이펙트 처리는 matrix_scan/housekeeping에서 수행
    return true;
}
