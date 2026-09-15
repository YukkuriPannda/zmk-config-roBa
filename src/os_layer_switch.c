/*
 * USB接続先として検出したホストOSに応じて、MACベースレイヤー(ID 1)を
 * locking付きで有効/無効にする。SETTINGSレイヤーの `&to 0` / `&to 1` は
 * 同じくlocking付きで状態を更新するため、この自動切り替えの後から手動操作
 * すれば必ずそちらが優先される。
 *
 * zmk-feature-os-detection はUSB/BLE両対応・レイヤー自動切り替え内蔵だが、
 * このリポジトリではUSBのみ・独自レイヤー番号(MAC=1)で完結させたいため、
 * それらは無効化した上でイベントだけを購読する側に回っている。
 */

#include <cormoran/os-detection/os_detection.h>

#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAC_LAYER_ID 1

static int os_layer_switch_listener(const zmk_event_t *eh) {
    const struct zmk_os_changed *ev = as_zmk_os_changed(eh);

    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    int ret;

    switch (ev->os) {
    case ZMK_OS_WINDOWS:
    case ZMK_OS_LINUX:
        ret = zmk_keymap_layer_deactivate(MAC_LAYER_ID, true);
        if (ret < 0) {
            LOG_WRN("Failed to deactivate MAC layer for OS %d: %d", ev->os, ret);
        } else {
            LOG_INF("Detected OS %d, deactivated MAC layer", ev->os);
        }
        break;
    case ZMK_OS_MACOS:
    case ZMK_OS_IOS:
        ret = zmk_keymap_layer_activate(MAC_LAYER_ID, true);
        if (ret < 0) {
            LOG_WRN("Failed to activate MAC layer for OS %d: %d", ev->os, ret);
        } else {
            LOG_INF("Detected OS %d, activated MAC layer", ev->os);
        }
        break;
    case ZMK_OS_UNKNOWN:
    case ZMK_OS_ANDROID:
    default:
        /* Androidは仕様上USBでは到達しない。Unknown(切断含む)は現在の
         * レイヤー選択を維持し、何もしない。 */
        break;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roba_os_layer_switch, os_layer_switch_listener);
ZMK_SUBSCRIPTION(roba_os_layer_switch, zmk_os_changed);
