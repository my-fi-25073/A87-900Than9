#pragma once

#include "quantum.h"

typedef union {
    uint32_t raw; // 모든 설정은 myfi.c에서 비트 pack/unpack으로 관리
} my_config_t;

extern my_config_t g_my_config;

#ifdef VIA_ENABLE
enum custom_value_id {
    // VIA 커스텀 값: 핀별 LED 플래그 (LED_MODE_* 비트 OR 값)
    id_custom_led_flags_a6 = 0,
    id_custom_led_flags_a7,
    id_custom_led_flags_b0,
    // VIA 커스텀 값: 핀별 인디케이터 선택(0:none,1:scroll,2:caps)
    id_custom_indicator_a6,
    id_custom_indicator_a7,
    id_custom_indicator_b0
};
#endif

// 핀 인덱스와 동일한 순서 사용: 0:A6, 1:A7, 2:B0
uint8_t myfi_get_led_flags(uint8_t idx);
void myfi_set_led_flags(uint8_t idx, uint8_t flags);

// 인디케이터 get/set (0:none, 1:scroll, 2:caps)
uint8_t myfi_get_indicator(uint8_t idx);
void myfi_set_indicator(uint8_t idx, uint8_t indicator);

#define MYFI_CFG_VERSION 1u
