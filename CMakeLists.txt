# このリポジトリは zephyr/module.yml により Zephyr モジュールとして扱われる。
# board_root によるシールド探索に加えて、ここで独自の C コードをビルドに含める。

if(CONFIG_ROBA_BATTERY_LED)
  zephyr_library_named(roba_battery_led)
  zephyr_library_sources(src/battery_led.c)
endif()

if(CONFIG_ROBA_OS_LAYER_SWITCH)
  zephyr_library_named(roba_os_layer_switch)
  zephyr_library_sources(src/os_layer_switch.c)
endif()
