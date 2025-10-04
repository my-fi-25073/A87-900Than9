// myfi: VIA 제어용 LED 플래그 저장/로드/제공
#include "my_config.h"
#include "quantum.h"
#include "eeprom.h"

#ifndef BIT
#define BIT(n) (1u << (n))
#endif

my_config_t g_my_config;

// --- Bit packing layout (LSB-first) ---
#define MYFI_A6_SHIFT      0u
#define MYFI_A7_SHIFT      5u
#define MYFI_B0_SHIFT      10u
#define MYFI_IND_A6_SHIFT  15u
#define MYFI_IND_A7_SHIFT  17u
#define MYFI_IND_B0_SHIFT  19u

#define MYFI_FLAGS_MASK    0x1Fu
#define MYFI_IND_MASK      0x03u

static inline uint32_t my_config_pack_field(uint32_t raw, uint32_t value, uint32_t shift, uint32_t mask)
{
    raw &= ~(mask << shift);
    raw |= ((value & mask) << shift);
    return raw;
}

static inline uint32_t my_config_unpack_field(uint32_t raw, uint32_t shift, uint32_t mask)
{
    return (raw >> shift) & mask;
}

static void read_my_config_from_eeprom(my_config_t* config)
{
    config->raw = eeconfig_read_kb();
}

static void write_my_config_to_eeprom(const my_config_t* config)
{
    eeconfig_update_kb(config->raw);
}

static void my_config_apply_defaults(my_config_t* config)
{
    uint32_t raw = 0u;
    // keymap.c 기본 효과: A6: Edge+Invert(12), A7: Edge(4), B0: Hold+Invert(9)
    raw = my_config_pack_field(raw, (uint32_t)(BIT(2) | BIT(3)), MYFI_A6_SHIFT, MYFI_FLAGS_MASK);
    raw = my_config_pack_field(raw, (uint32_t)(BIT(2)),          MYFI_A7_SHIFT, MYFI_FLAGS_MASK);
    raw = my_config_pack_field(raw, (uint32_t)(BIT(0) | BIT(3)), MYFI_B0_SHIFT, MYFI_FLAGS_MASK);
    // 인디케이터 기본: A6:none(0), A7:scroll(1), B0:caps(2)
    raw = my_config_pack_field(raw, 0u, MYFI_IND_A6_SHIFT, MYFI_IND_MASK);
    raw = my_config_pack_field(raw, 1u, MYFI_IND_A7_SHIFT, MYFI_IND_MASK);
    raw = my_config_pack_field(raw, 2u, MYFI_IND_B0_SHIFT, MYFI_IND_MASK);
    // 버전 필드는 더 이상 사용하지 않음
    config->raw = raw;
}

void eeconfig_init_kb(void)
{
    my_config_apply_defaults(&g_my_config);
    write_my_config_to_eeprom(&g_my_config);
    eeconfig_init_user();
}

void matrix_init_kb(void)
{
    read_my_config_from_eeprom(&g_my_config);
    // 기본값 미설정 시 기본값 적용 후 저장
    if (g_my_config.raw == 0u || g_my_config.raw == 0xFFFFFFFFu)
    {
        my_config_apply_defaults(&g_my_config);
        write_my_config_to_eeprom(&g_my_config);
    }
    matrix_init_user();
}

uint8_t my_config_get_led_flags(uint8_t idx)
{
    static const uint8_t shifts[3] = { MYFI_A6_SHIFT, MYFI_A7_SHIFT, MYFI_B0_SHIFT };
    return (uint8_t)my_config_unpack_field(g_my_config.raw, shifts[(idx > 2) ? 2 : idx], MYFI_FLAGS_MASK);
}

void my_config_set_led_flags(uint8_t idx, uint8_t flags)
{
    static const uint8_t shifts[3] = { MYFI_A6_SHIFT, MYFI_A7_SHIFT, MYFI_B0_SHIFT };
    g_my_config.raw = my_config_pack_field(g_my_config.raw, flags, shifts[(idx > 2) ? 2 : idx], MYFI_FLAGS_MASK);
}

uint8_t my_config_get_indicator(uint8_t idx)
{
    static const uint8_t shifts[3] = { MYFI_IND_A6_SHIFT, MYFI_IND_A7_SHIFT, MYFI_IND_B0_SHIFT };
    return (uint8_t)my_config_unpack_field(g_my_config.raw, shifts[(idx > 2) ? 2 : idx], MYFI_IND_MASK);
}

void my_config_set_indicator(uint8_t idx, uint8_t indicator)
{
    static const uint8_t shifts[3] = { MYFI_IND_A6_SHIFT, MYFI_IND_A7_SHIFT, MYFI_IND_B0_SHIFT };
    uint8_t v = (indicator > 2u) ? 0u : indicator;
    g_my_config.raw = my_config_pack_field(g_my_config.raw, v, shifts[(idx > 2) ? 2 : idx], MYFI_IND_MASK);
}

#ifdef VIA_ENABLE
// VIA 커스텀 get/set: 핀별 LED 플래그 3종
void custom_config_get_value(uint8_t *data)
{
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);

    switch (*value_id)
    {
        case id_custom_led_flags_a6: *value_data = my_config_get_led_flags(0); break;
        case id_custom_led_flags_a7: *value_data = my_config_get_led_flags(1); break;
        case id_custom_led_flags_b0: *value_data = my_config_get_led_flags(2); break;
        case id_custom_indicator_a6: *value_data = my_config_get_indicator(0); break;
        case id_custom_indicator_a7: *value_data = my_config_get_indicator(1); break;
        case id_custom_indicator_b0: *value_data = my_config_get_indicator(2); break;
    }
}

void my_config_save_if_changed(uint32_t before_raw)
{
    if (g_my_config.raw != before_raw)
    {
        write_my_config_to_eeprom(&g_my_config);
    }
}

void custom_config_set_value(uint8_t *data)
{
    uint8_t *value_id   = &(data[0]);
    uint8_t *value_data = &(data[1]);
    uint32_t before_raw = g_my_config.raw;

    switch (*value_id)
    {
        case id_custom_led_flags_a6: my_config_set_led_flags(0, *value_data); break;
        case id_custom_led_flags_a7: my_config_set_led_flags(1, *value_data); break;
        case id_custom_led_flags_b0: my_config_set_led_flags(2, *value_data); break;
        case id_custom_indicator_a6: my_config_set_indicator(0, *value_data); break;
        case id_custom_indicator_a7: my_config_set_indicator(1, *value_data); break;
        case id_custom_indicator_b0: my_config_set_indicator(2, *value_data); break;
    }

    my_config_save_if_changed(before_raw);
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length)
{
    uint8_t *command_id        = &(data[0]);
    uint8_t *channel_id        = &(data[1]);
    uint8_t *value_id_and_data = &(data[2]);

    // 채널 우선 라우팅: 컨트롤 분리 (10/11/12: LED flags A6/A7/B0, 20/21/22: indicator A6/A7/B0)
    uint8_t ch = *channel_id;
    if (ch >= 10 && ch <= 12)
    {
        uint8_t idx = (uint8_t)(ch - 10);
        // flags channel
        if (*command_id == id_custom_set_value)
        {
            uint8_t v = value_id_and_data[1];
            my_config_set_led_flags(idx, v);
            write_my_config_to_eeprom(&g_my_config);
        }
        else if (*command_id == id_custom_get_value)
        {
            value_id_and_data[1] = my_config_get_led_flags(idx);
        }
        else if (*command_id == id_custom_save)
        {
            write_my_config_to_eeprom(&g_my_config);
        }
        else
        {
            *command_id = id_unhandled;
        }
        return;
    }
    else if (ch >= 20 && ch <= 22)
    {
        uint8_t idx = (uint8_t)(ch - 20);
        // indicator channel
        if (*command_id == id_custom_set_value)
        {
            uint8_t v = value_id_and_data[1];
            my_config_set_indicator(idx, v);
            write_my_config_to_eeprom(&g_my_config);
        }
        else if (*command_id == id_custom_get_value)
        {
            value_id_and_data[1] = my_config_get_indicator(idx);
        }
        else if (*command_id == id_custom_save)
        {
            write_my_config_to_eeprom(&g_my_config);
        }
        else
        {
            *command_id = id_unhandled;
        }
        return;
    }

    *command_id = id_unhandled;
}
#endif
