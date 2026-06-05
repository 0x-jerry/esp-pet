#include "buttons.h"

#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdatomic.h>

#define DEBOUNCE_MS 200

static atomic_bool action_flagged;
static atomic_bool talk_flagged;

static int64_t action_last = 0;
static int64_t talk_last   = 0;

static void IRAM_ATTR btn_action_isr(void *arg) {
    int64_t now = esp_timer_get_time() / 1000;
    if (now - action_last < DEBOUNCE_MS) return;
    action_last = now;
    action_flagged = true;
}

static void IRAM_ATTR btn_talk_isr(void *arg) {
    int64_t now = esp_timer_get_time() / 1000;
    if (now - talk_last < DEBOUNCE_MS) return;
    talk_last = now;
    talk_flagged = true;
}

void buttons_init(void) {
    gpio_config_t cfg = {
        .intr_type    = GPIO_INTR_NEGEDGE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_ACTION) | (1ULL << BTN_TALK),
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&cfg);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BTN_ACTION, btn_action_isr, NULL);
    gpio_isr_handler_add(BTN_TALK, btn_talk_isr, NULL);

    action_flagged = false;
    talk_flagged   = false;
}

bool button_action_pressed(void) {
    bool val = action_flagged;
    action_flagged = false;
    return val;
}

bool button_talk_pressed(void) {
    bool val = talk_flagged;
    talk_flagged = false;
    return val;
}
