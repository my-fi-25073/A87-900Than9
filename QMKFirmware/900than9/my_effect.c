#include "my_effect.h"

static my_effect_state_t s_state;

void my_effect_init(void)
{
    s_state.any_key_held = false;
    s_state.last_typing_time = 0;
    s_state.breathing_cycle = 0;
    s_state.pwm_counter = 0;
}

void my_effect_reset(void)
{
    s_state.any_key_held = false;
    s_state.last_typing_time = 0;
}

static inline bool in_typing_pulse_window(uint16_t flash_ms)
{
    return (timer_elapsed32(s_state.last_typing_time) < flash_ms);
}

static inline uint8_t compute_breath_brightness_ease(uint16_t phase)
{
    uint16_t half_cycle = EFFECT_BREATH_HALF_PERIOD;
    uint16_t mirrored = (phase < half_cycle) ? phase : (uint16_t)(EFFECT_BREATH_PERIOD - phase);
    // u in [0, 65535]
    uint32_t u = ((uint32_t)mirrored << 16) / half_cycle;
    // smoothstep: 3u^2 - 2u^3  (scaled to 16.16)
    uint32_t u2 = (u * u) >> 16;
    uint32_t smooth = (u2 * (((uint32_t)3 << 16) - (u << 1))) >> 16;
    uint32_t brightness = (uint32_t)EFFECT_BREATH_BASE + (((uint32_t)EFFECT_BREATH_RANGE * smooth) >> 16);
    return (uint8_t)brightness;
}

bool my_effect_requires_state_update(void)
{
    // 키 이벤트가 들어오는지에 따라만 업데이트 필요 여부 판단은 호출 측에서 모드 OR로 검출함
    return true;
}

void my_effect_update_typing_state_from_key_event(bool pressed)
{
    if (pressed)
    {
        s_state.last_typing_time = timer_read32();
        s_state.any_key_held = true;
    }
    else
    {
        bool still_held = false;
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
        {
            if (matrix_get_row(row)) { still_held = true; break; }
        }
        s_state.any_key_held = still_held;
    }
}

static void update_led_effect_none(pin_t pin)
{
    writePinLow(pin);
}

static void update_led_effect_force_on(pin_t pin)
{
    writePinHigh(pin);
}

static void update_led_effect_typing_hold(pin_t pin, bool invert)
{
    if (s_state.any_key_held)
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
    if (in_typing_pulse_window(EFFECT_TYPING_PULSE_MS))
    {
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
    }
}

static void update_led_effect_breathing_noinput(pin_t pin, bool invert)
{
    // 브리딩은 housekeeping에서 수행. 여기서는 자리만 유지.
    (void)pin; (void)invert;
}

static void update_led_effect_hold_breathing(pin_t pin, bool invert)
{
    if (s_state.any_key_held)
    {
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        bool idle_1s = (timer_elapsed32(s_state.last_typing_time) > EFFECT_IDLE_MS);
        if (!idle_1s)
        {
            if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
        }
    }
}

static void update_led_effect_edge_breathing(pin_t pin, bool invert)
{
    if (in_typing_pulse_window(EFFECT_TYPING_PULSE_MS))
    {
        if (invert) { writePinLow(pin); } else { writePinHigh(pin); }
    }
    else
    {
        bool idle_1s = (timer_elapsed32(s_state.last_typing_time) > EFFECT_IDLE_MS);
        if (!idle_1s)
        {
            if (invert) { writePinHigh(pin); } else { writePinLow(pin); }
        }
    }
}

void my_effect_apply_pin_effect(pin_t pin, uint8_t mode)
{
    if (mode == LED_MODE_FORCE_ON) { update_led_effect_force_on(pin); return; }
    if (mode == LED_MODE_NONE)     { update_led_effect_none(pin); return; }

    const bool invert = (mode & LED_MODE_INVERT) != 0;
    const bool hold   = (mode & LED_MODE_TYPING_HOLD) != 0;
    const bool edge   = (mode & LED_MODE_TYPING_EDGE) != 0;
    const bool breath = (mode & LED_MODE_BREATHING) != 0;

    if (breath && hold) { update_led_effect_hold_breathing(pin, invert); return; }
    if (breath && edge) { update_led_effect_edge_breathing(pin, invert); return; }
    if (breath)         { update_led_effect_breathing_noinput(pin, invert); return; }
    if (hold)           { update_led_effect_typing_hold(pin, invert); return; }
    if (edge)           { update_led_effect_typing_edge(pin, invert); return; }

    writePinLow(pin);
}

void my_effect_update_effects_for_pin(pin_t pin, uint8_t mode, bool indicator_on)
{
    // 인디케이터가 켜져 있으면 이펙트 무시
    if (indicator_on) { writePinHigh(pin); return; }

    s_state.breathing_cycle = (uint16_t)((s_state.breathing_cycle + 1) % EFFECT_BREATH_PERIOD);
    uint16_t phase = (uint16_t)(s_state.breathing_cycle % EFFECT_BREATH_PERIOD);
    uint8_t brightness = compute_breath_brightness_ease(phase);

    s_state.pwm_counter = (uint8_t)(s_state.pwm_counter + EFFECT_PWM_STEP);
    bool new_led_state = (s_state.pwm_counter < brightness);
    const bool in_pulse = in_typing_pulse_window(EFFECT_TYPING_PULSE_MS);
    const bool idle_1s = (timer_elapsed32(s_state.last_typing_time) > EFFECT_IDLE_MS);

    const bool breath = (mode & LED_MODE_BREATHING) != 0;
    const bool hold   = (mode & LED_MODE_TYPING_HOLD) != 0;
    const bool edge   = (mode & LED_MODE_TYPING_EDGE) != 0;

    if (!breath) return;

    bool allow_breath;
    if (!hold && !edge)
    {
        // 브리딩 단독일 때는 유휴 상태와 무관하게 항상 허용
        allow_breath = true;
    }
    else if (hold)
    {
        allow_breath = (!s_state.any_key_held) && idle_1s;
    }
    else /* edge */
    {
        allow_breath = (!in_pulse) && idle_1s;
    }

    if (allow_breath)
    {
        if (new_led_state) { writePinHigh(pin); } else { writePinLow(pin); }
    }
}
