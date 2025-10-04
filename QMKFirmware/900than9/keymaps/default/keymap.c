// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "my_config.h"
#include "my_effect.h"
#include "os_detection.h"
#include "my_keycode.h"

typedef bool (*key_handler_t)(bool pressed, os_variant_t host);

// 효과 비트 매크로는 my_effect.h에서 제공

// LED 핀 정의
#define LED_PIN_A6 A6
#define LED_PIN_A7 A7
#define LED_PIN_B0 B0

#define IDX_A6 0
#define IDX_A7 1
#define IDX_B0 2

#define PIN_COUNT 3
static const pin_t kPins[PIN_COUNT] = { LED_PIN_A6, LED_PIN_A7, LED_PIN_B0 };

// 인디케이터 소스: myfi 설정 사용 (0:none,1:scroll,2:caps)
typedef enum {
    IND_NONE = 0,
    IND_SCROLL = 1,
    IND_CAPS = 2,
} indicator_t;

static inline uint8_t get_pin_mode(uint8_t idx)
{
    return my_config_get_led_flags(idx);
}
static inline indicator_t get_indicator_src(uint8_t idx)
{
    return (indicator_t)my_config_get_indicator(idx);
}

static inline bool require_typing_state_update(void)
{
    const uint8_t m0 = get_pin_mode(IDX_A6);
    const uint8_t m1 = get_pin_mode(IDX_A7);
    const uint8_t m2 = get_pin_mode(IDX_B0);
    return (((m0 | m1 | m2) & EFFECT_NEEDS_STATE) != 0);
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    // clang-format off
    [0] = LAYOUT(
        KC_ESC,           KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,            KC_F9,   KC_F10,  KC_F11,  KC_F12,     KC_PSCR, KC_SCRL, KC_PAUS,
        KC_GRV,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSLS, KC_BSPC,     KC_INS,  KC_HOME, KC_PGUP,
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,             KC_DEL,  KC_END,  KC_PGDN,
        KC_CAPS,          KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_BSLS, KC_ENT,
        MO(1),   KC_LSFT, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,          KC_RSFT, MO(1),               KC_UP,
        KC_LCTL, KC_LGUI, KC_LGUI, KC_LALT, KC_RCTL,  KC_SPC,           KC_SPC,           KC_SPC,           KC_RALT,  KC_RGUI, KC_RGUI, KC_RCTL,    KC_LEFT, KC_DOWN, KC_RGHT
    ),
    [1] = LAYOUT(
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,             XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
        MO(1),   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, MO(1),               XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX
    ),
    [2] = LAYOUT(
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,             XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
        MO(1),   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, MO(1),               XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX
    ),
    [3] = LAYOUT(
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,             XXXXXXX, XXXXXXX, XXXXXXX,
        XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
        MO(1),   XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX, MO(1),               XXXXXXX,
        XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,          XXXXXXX,          XXXXXXX,          XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,    XXXXXXX, XXXXXXX, XXXXXXX
    ),
    // clang-format on
};


void keyboard_post_init_user(void)
{
    // A6, A7, B0 핀 출력 설정
    setPinOutput(LED_PIN_A6);
    setPinOutput(LED_PIN_A7);
    setPinOutput(LED_PIN_B0);
    my_effect_init();
}

void matrix_scan_user(void)
{
    led_t leds = host_keyboard_led_state();
    for (uint8_t i = 0; i < PIN_COUNT; i++)
    {
        const pin_t pin = kPins[i];
        const indicator_t ind = get_indicator_src(i);
        const uint8_t mode = get_pin_mode(i);

        bool ind_on = false;
        if (ind == IND_SCROLL) ind_on = leds.scroll_lock;
        else if (ind == IND_CAPS) ind_on = leds.caps_lock;

        if (ind_on) { writePinHigh(pin); }
        else { my_effect_apply_pin_effect(pin, mode); }
    }
}

void housekeeping_task_user(void)
{
    led_t leds = host_keyboard_led_state();
    for (uint8_t i = 0; i < PIN_COUNT; i++)
    {
        const pin_t pin = kPins[i];
        const indicator_t ind = get_indicator_src(i);
        const uint8_t mode = get_pin_mode(i);

        // 인디케이터 ON이면 이펙트 무시
        bool ind_on = false;
        if (ind == IND_SCROLL) ind_on = leds.scroll_lock;
        else if (ind == IND_CAPS) ind_on = leds.caps_lock;
        my_effect_update_effects_for_pin(pin, mode, ind_on);
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t* record)
{
    if (require_typing_state_update())
    {
        my_effect_update_typing_state_from_key_event(record->event.pressed);
    }

    os_variant_t host = detected_host_os();
    if (!process_my_custom_keycodes(keycode, record->event.pressed, host)) return false;

    return true;
}
