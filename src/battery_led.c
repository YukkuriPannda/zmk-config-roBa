/*
 * 起動後に一度だけ、XIAO BLE のオンボード LED でバッテリー残量を示す。
 *
 * ZMK には残量を LED で表現する機能が無いため、zmk_battery_state_changed を
 * 購読して自前で GPIO を叩いている。ZMK の battery.c は初回サンプリングを
 * K_NO_WAIT で仕掛けるので、このイベントは起動直後に届く。
 *
 * 左右それぞれのファームに組み込まれ、各自が自分側の電池残量を表示する
 * (セントラルがスプリット経由で受け取る相手側の残量は対象外)。
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if !DT_NODE_EXISTS(DT_ALIAS(led0)) || !DT_NODE_EXISTS(DT_ALIAS(led2))
#error "ROBA_BATTERY_LED needs the led0 (red) and led2 (green) aliases from the board"
#endif

/* XIAO BLE: led0=赤(P0.26) / led1=青(P0.30) / led2=緑(P0.06)。いずれも ACTIVE_LOW。
 * 赤と緑の同時点灯で黄色になるため、青は使わず3段階を表現する。 */
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static bool shown;

static void battery_led_off(struct k_work *work) {
    ARG_UNUSED(work);

    gpio_pin_set_dt(&led_red, 0);
    gpio_pin_set_dt(&led_green, 0);
}

static K_WORK_DELAYABLE_DEFINE(battery_led_off_work, battery_led_off);

static int battery_led_listener(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    /* 起動後の最初の1回のみ。以降の定期報告やアイドル復帰では点灯させない。 */
    if (ev == NULL || shown) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    shown = true;

    const uint8_t soc = ev->state_of_charge;
    const int red = (soc < CONFIG_ROBA_BATTERY_LED_MID_PERCENT) ? 1 : 0;
    const int green = (soc >= CONFIG_ROBA_BATTERY_LED_LOW_PERCENT) ? 1 : 0;

    LOG_INF("Battery at %u%%, lighting LED (red=%d green=%d)", soc, red, green);

    gpio_pin_set_dt(&led_red, red);
    gpio_pin_set_dt(&led_green, green);
    k_work_schedule(&battery_led_off_work, K_MSEC(CONFIG_ROBA_BATTERY_LED_DURATION_MS));

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roba_battery_led, battery_led_listener);
ZMK_SUBSCRIPTION(roba_battery_led, zmk_battery_state_changed);

static int battery_led_init(void) {
    if (!gpio_is_ready_dt(&led_red) || !gpio_is_ready_dt(&led_green)) {
        LOG_ERR("Battery LED GPIOs are not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure the red LED: %d", ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure the green LED: %d", ret);
        return ret;
    }

    return 0;
}

SYS_INIT(battery_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
