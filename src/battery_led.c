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

#if !DT_NODE_EXISTS(DT_ALIAS(led0)) || !DT_NODE_EXISTS(DT_ALIAS(led1)) ||                           \
    !DT_NODE_EXISTS(DT_ALIAS(led2))
#error "ROBA_BATTERY_LED needs the led0/led1/led2 aliases from the board"
#endif

/* XIAO BLE: led0=赤(P0.26) / led1=緑(P0.30) / led2=青(P0.06)。いずれも ACTIVE_LOW。
 *
 * 注意: ZMK がピン留めしている Zephyr 3.5 (zmkfirmware/zephyr v3.5.0+zmk-fixes) の
 * boards/arm/seeeduino_xiao_ble/seeeduino_xiao_ble.dts は led1 を "Blue LED"、
 * led2 を "Green LED" とラベルしているが、これは実機と逆。upstream Zephyr の
 * boards/seeed/xiao_ble/xiao_ble_common.dtsi では led1="Green" / led2="Blue" に
 * 修正済み。DTS の label を信じると緑のつもりで青が点く。
 *
 * 残量表示に使うのは赤と緑のみ(同時点灯で黄色)。青は使わないが、ZMK も DYA
 * モジュールも LED を触らないため、初期化しないとブートローダーが残した状態の
 * まま点きっぱなしになる。3つとも必ず消灯状態に固定すること。 */
static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
};

#define LED_RED 0
#define LED_GREEN 1
#define LED_BLUE 2

static bool shown;

static void battery_led_off(struct k_work *work) {
    ARG_UNUSED(work);

    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        gpio_pin_set_dt(&leds[i], 0);
    }
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

    gpio_pin_set_dt(&leds[LED_RED], red);
    gpio_pin_set_dt(&leds[LED_GREEN], green);
    k_work_schedule(&battery_led_off_work, K_MSEC(CONFIG_ROBA_BATTERY_LED_DURATION_MS));

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roba_battery_led, battery_led_listener);
ZMK_SUBSCRIPTION(roba_battery_led, zmk_battery_state_changed);

static int battery_led_init(void) {
    /* 残量表示に使わない青も含めて全て消灯状態に固定する。 */
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            LOG_ERR("LED %u is not ready", (unsigned int)i);
            return -ENODEV;
        }

        int ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure LED %u: %d", (unsigned int)i, ret);
            return ret;
        }
    }

    return 0;
}

SYS_INIT(battery_led_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
