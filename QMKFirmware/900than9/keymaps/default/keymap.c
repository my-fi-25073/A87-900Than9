// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#include "os_detection.h"

// 타이핑 LED 제어 변수
static bool esc_led_enabled = true;    // 기본값: 켜짐
static bool scroll_led_enabled = true; // 기본값: 켜짐
static bool typing_led_on = false;     // 현재 점등 여부

// 브리딩 LED 제어 변수
static uint16_t breathing_cycle = 0;    // 0-1023 사이클
static uint32_t last_typing_time = 0;   // 마지막 타이핑 시간
static bool breathing_mode = false;     // 공용 브리딩 모드 여부
static uint8_t pwm_counter_common = 0;  // 공용 브리딩 PWM 카운터

// LED 핀 정의
#define LED_PIN_A6 A6
#define LED_PIN_A7 A7
#define LED_PIN_B0 B0

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
    GO_LEFT = SAFE_RANGE, // ctrl + left, left desktop, win ctrl <
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

    TG_TLED, // Typing LED 토글 키
    TG_LESC, // ESC LED 토글 키
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
        TG_LESC,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, TG_TLED, XXXXXXX,
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
static void toggle_led_with_cleanup(bool* enabled_flag, pin_t pin, bool* typing_flag, bool* breathing_flag)
{
    *enabled_flag = !(*enabled_flag);
    if (!(*enabled_flag))
    {
        writePinLow(pin); // 끌 때 확실히 꺼주기
        if (typing_flag)
        {
            *typing_flag = false;
        }
        if (breathing_flag)
        {
            *breathing_flag = false;
        }
    }
}

// 브리딩만 적용: 브리딩 중이 아니면 기본 켜짐, 브리딩은 housekeeping에서 처리
static __attribute__((unused)) void update_led_breathing_only(bool enabled, pin_t pin)
{
    if (!enabled)
    {
        return;
    }
    if (!breathing_mode)
    {
        writePinHigh(pin);
    }
}

// 타이핑 반응만 적용: invert_on_typing=true면 타이핑 시 끄기(ESC 패턴)
static __attribute__((unused)) void update_led_typing_only(bool enabled, pin_t pin, bool invert_on_typing)
{
    if (!enabled)
    {
        return;
    }
    if (typing_led_on)
    {
        if (invert_on_typing)
        {
            writePinLow(pin);
        }
        else
        {
            writePinHigh(pin);
        }
    }
    else
    {
        if (invert_on_typing)
        {
            writePinHigh(pin);
        }
        else
        {
            writePinLow(pin);
        }
    }
}

// 타이핑 + 브리딩 공통 업데이트 (invert_on_typing: 타이핑 중일 때 끔/켬 반전 여부)
static void update_led_typing_breathing(bool enabled, pin_t pin, bool invert_on_typing)
{
    if (!enabled)
    {
        return;
    }

    if (typing_led_on)
    {
            breathing_mode = false; // 타이핑 감지 시 브리딩 해제
            breathing_cycle = 0;    // 다음 진입 시 동일 시작점
            pwm_counter_common = 0;
        if (invert_on_typing)
        {
            writePinLow(pin);
        }
        else
        {
            writePinHigh(pin);
        }
        return;
    }

    // 타이핑이 없으면 1초 후 브리딩 모드로 전환 (진입 시 주기/카운터 초기화)
    if (timer_elapsed32(last_typing_time) > 1000)
    {
        if (!breathing_mode)
        {
            breathing_mode = true;
            breathing_cycle = 0;
            pwm_counter_common = 0;
        }
    }

    // 브리딩은 housekeeping_task_user에서 처리. 브리딩이 아니면 기본 켜짐.
    if (!breathing_mode)
    {
        writePinHigh(pin);
    }
}




bool process_record_user(uint16_t keycode, keyrecord_t* record)
{
    if (scroll_led_enabled || esc_led_enabled )
    {
        if (record->event.pressed)
        {
            typing_led_on = true;
            last_typing_time = timer_read32(); // 타이핑 시간 업데이트
            breathing_mode = false;            // 타이핑 중이면 브리딩 모드 해제
        }
        else
        {
            // 눌린 키 없으면 꺼지게
            typing_led_on = false;
            for (uint8_t row = 0; row < MATRIX_ROWS; row++)
            {
                if (matrix_get_row(row))
                {
                    typing_led_on = true;
                    break;
                }
            }
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
        case TG_TLED:
            if (record->event.pressed)
            {
                toggle_led_with_cleanup(&scroll_led_enabled, LED_PIN_A7, &typing_led_on, &breathing_mode);
            }
            return false;

        case TG_LESC:
            if (record->event.pressed)
            {
                toggle_led_with_cleanup(&esc_led_enabled, LED_PIN_A6, &typing_led_on, &breathing_mode);
            }
            return false;
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

void matrix_scan_user(void)
{
    // --- ESC(A6), Scroll(A7), F13(B1) 공통 타이핑 + 브리딩 적용 ---
    update_led_typing_breathing(esc_led_enabled, LED_PIN_A6, false);

    // Scroll Lock 인디케이터가 켜지면 타이핑/브리딩 무시하고 항상 켬
    if (host_keyboard_led_state().scroll_lock)
    {
        writePinHigh(LED_PIN_A7);
    }
    else
    {
        update_led_typing_breathing(scroll_led_enabled, LED_PIN_A7, false);
    }

    // --- B0 CapsLock LED ---
    if (!host_keyboard_led_state().caps_lock)
    {
        // CapsLock이 꺼져있으면 LED 끄기
        writePinLow(LED_PIN_B0);
    }
}

void housekeeping_task_user(void)
{
    // --- 공용 브리딩 효과: 세 핀 동시 적용 ---
    if (breathing_mode && (esc_led_enabled || scroll_led_enabled))
    {
        // 브리딩 사이클 업데이트 (매 호출마다 - 4초 주기)
        breathing_cycle = (breathing_cycle + 1) % 8000; // 0-7999 사이클 (8000 호출 = 4초)

        // 자연스러운 브리딩 효과 (4초 주기)
        uint8_t brightness;
        uint16_t phase = breathing_cycle % 8000;
        uint16_t half_cycle = 4000;

        if (phase < half_cycle)
        {
            // 상승 구간: 선형 증가
            brightness = 45 + (phase * 210) / half_cycle;
        }
        else
        {
            // 하강 구간: 선형 감소
            uint16_t progress = 8000 - phase;
            brightness = 45 + (progress * 210) / half_cycle;
        }

        // 고주파 PWM 시뮬레이션: 더 빠른 깜빡임 (공용 카운터)
        pwm_counter_common += 12;
        bool new_led_state = (pwm_counter_common < brightness);

        // 핀 동시 적용 (활성화된 핀만)
        if (esc_led_enabled)
        {
            if (new_led_state) { writePinHigh(LED_PIN_A6); } else { writePinLow(LED_PIN_A6); }
        }
        // Scroll Lock 인디케이터가 켜져있으면 A7은 항상 켬
        if (host_keyboard_led_state().scroll_lock)
        {
            writePinHigh(LED_PIN_A7);
        }
        else if (scroll_led_enabled)
        {
            if (new_led_state) { writePinHigh(LED_PIN_A7); } else { writePinLow(LED_PIN_A7); }
        }
    }
}

bool led_update_user(led_t led_state)
{
    if (led_state.caps_lock)
    {
        writePinHigh(LED_PIN_B0); // CapsLock 켜지면 B0 LED 켜기
    }
    else
    {
        writePinLow(LED_PIN_B0);
    }
    return true;
}
