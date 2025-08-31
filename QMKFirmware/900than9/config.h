#pragma once

#define DYNAMIC_KEYMAP_LAYER_COUNT 6
#define USB_POLLING_INTERVAL_MS 1
#define FORCE_NKRO

// LED Pins: A6, A7, B0
// A6 = ESC, A7 = SCROLL, B0 = CAPS
// TIM3 타이머 사용: A6 = TIM3_CH1, A7 = TIM3_CH2, B0 = TIM3_CH3

// // LED Indicators 설정
// #define LED_KANA_PIN A6
// #define LED_SCROLL_LOCK_PIN A7
// #define LED_CAPS_LOCK_PIN B0

// // LED가 켜질 때의 핀 상태 (1: HIGH, 0: LOW)
// #define LED_PIN_ON_STATE 1

/* 백라이트 설정 - 현재 비활성화 */
// #define BACKLIGHT_PIN A6
// #define BACKLIGHT_LEVELS 1
