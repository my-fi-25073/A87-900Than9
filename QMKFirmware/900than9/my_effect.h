#pragma once

#include "quantum.h"

#ifndef BIT
#define BIT(n) (1u << (n))
#endif

// LED 모드 플래그 (비트 OR 조합)
#define LED_MODE_NONE        0u
#define LED_MODE_TYPING_HOLD BIT(0)
#define LED_MODE_BREATHING   BIT(1)
#define LED_MODE_TYPING_EDGE BIT(2)
#define LED_MODE_INVERT      BIT(3)
#define LED_MODE_FORCE_ON    BIT(4)

// Convenience macros for combinations and checks
#define EFFECT_TYPING       (LED_MODE_TYPING_HOLD | LED_MODE_TYPING_EDGE)
#define EFFECT_NEEDS_STATE  (EFFECT_TYPING)
#define EFFECT_HAS(mode, flag) (((mode) & (flag)) != 0)

// --- Effect timing/shape constants ---
#define EFFECT_TYPING_PULSE_MS     33u
#define EFFECT_IDLE_MS             1000u
#define EFFECT_PWM_STEP            24u
#define EFFECT_BREATH_PERIOD       16000u
#define EFFECT_BREATH_HALF_PERIOD  8000u
#define EFFECT_BREATH_BASE         25u
#define EFFECT_BREATH_RANGE        230u

// --- Module state ---
typedef struct {
    bool     any_key_held;
    uint32_t last_typing_time;
    uint16_t breathing_cycle;
    uint8_t  pwm_counter;
} my_effect_state_t;

// Initialize/reset module state
void my_effect_init(void);
void my_effect_reset(void);

// 키 이벤트로 타이핑 상태 업데이트가 필요한지 여부
bool my_effect_requires_state_update(void);

// 키 입력 이벤트로 타이핑 상태를 갱신
void my_effect_update_typing_state_from_key_event(bool pressed);

// 모드에 따른 이펙트를 즉시 적용 (인디케이터 OFF일 때 호출)
void my_effect_apply_pin_effect(pin_t pin, uint8_t mode);

// 브리딩/PWM 등 주기 처리: 인디케이터 ON 여부를 함께 전달
void my_effect_update_effects_for_pin(pin_t pin, uint8_t mode, bool indicator_on);
