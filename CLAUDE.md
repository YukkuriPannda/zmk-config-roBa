# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

（このリポジトリでの応答・コミットメッセージは日本語で書くこと）

## 概要

分割キーボード **roBa**（43キー / seeeduino_xiao_ble ×2 / 右手側にPMW3610トラックボール、左手側にEC11ロータリーエンコーダ）の ZMK ユーザー設定リポジトリ。
本体ファームウェアのソースは含まず、`config/west.yml` が指す ZMK 本体・モジュールを GitHub Actions が取得してビルドする。

## ビルドと確認

ローカルに west ワークスペース（`.west/`）は無く、**ビルドはすべて GitHub Actions 上で行う**。ローカルでの lint / テストは存在しない。

```bash
# push でビルドが走る（トリガーは config/**, boards/**, build.yaml,
# src/**, CMakeLists.txt, Kconfig, zephyr/**, build.yml 自身）
# README.md や draw.yml だけの変更ではビルドされない
git push

# 手動実行
gh workflow run build.yml

# 結果確認（このリポジトリでの唯一の検証手段）
gh run list --limit 5
gh run watch
gh run view <run-id> --log-failed   # ビルドエラー（devicetree / Kconfig）の確認はこれ

# uf2 の取得
gh run download <run-id>
```

キーマップ画像（`keymap-drawer/roBa.svg`、README に表示）は `draw.yml` が `config/roBa.keymap` などへの push で自動再描画し、結果を `[Draw]` コミットとしてリポジトリに書き戻す。手動実行は `gh workflow run draw.yml`。書き戻し先は `keymap-drawer/**` で、これは `build.yml` / `draw.yml` どちらのパスフィルタにも含まれないため再帰的な発火は起きない。なお `draw.yml` が参照する `keymap_drawer.config.yaml` はリポジトリに存在せず、無視される（キーの表示名をカスタムしたくなったらここに作る）。

## 構成の全体像

ビルド対象は `build.yaml` の3つ:

| board | shield | 備考 |
|---|---|---|
| seeeduino_xiao_ble | `roBa_R` | セントラル。`studio-rpc-usb-uart` snippet 付き（ZMK Studio 用） |
| seeeduino_xiao_ble | `roBa_L` | ペリフェラル |
| seeeduino_xiao_ble | `settings_reset` | ZMK 標準のペアリング情報リセット用 |

### ZMK 本体は upstream ではなく cormoran fork

`config/west.yml` は **cormoran/zmk の `v0.3-branch+dya`** と DYA Studio 系モジュール
（`zmk-module-ble-management` / `battery-history` / `settings-rpc` / `runtime-input-processor`、および `zmk-pmw3610-driver`）を取得する。
`.github/workflows/build.yml` が参照する `zmkfirmware/zmk@v0.3` は**再利用ワークフローの取得元にすぎず、ビルドされる ZMK 本体ではない**。

つまり `CONFIG_ZMK_RUNTIME_INPUT_PROCESSOR` や `CONFIG_ZMK_SETTINGS_RPC`、`&mouse_runtime_input_processor` などは
**upstream ZMK には存在しない fork 側の機能**。これらの挙動やオプション名を調べるときは upstream のドキュメントではなく cormoran のリポジトリを見ること。

### シールド定義（`boards/shields/roBa/`）

`zephyr/module.yml` の `board_root: .` によってこのディレクトリがシールド探索対象になる。

- **`roBa.dtsi`** — 左右共通。物理レイアウト（43キー）、`default_transform`（11列×4行）、`kscan0`（col2row、row は `xiao_d 1/2/3/6`）、エンコーダ、トラックボール関連ノードを `status = "disabled"` の雛形として定義。
- **`roBa_L.overlay`** — 列を6本（`xiao_d 10/9/8/7`, `gpio0 10`, `gpio0 9`）定義し、`left_encoder` を有効化。
- **`roBa_R.overlay`** — 列は5本、`&default_transform { col-offset = <6>; }` で右手側のキーを列6〜10にマップ。SPI0 + PMW3610 を有効化し、`trackball_listener` を構成。
- **`.conf`** — `roBa_R.conf` がセントラル側（BLE、ZMK Studio、DYA Studio 系 RPC を全部盛り）、`roBa_L.conf` がペリフェラル側。`Kconfig.defconfig` の `ZMK_KEYBOARD_NAME` 既定値（`roBa_R` / `roBa_L`）は `roBa_R.conf` の `CONFIG_ZMK_KEYBOARD_NAME="roBa"` で上書きされる。

**列の割り当てを触るときの注意**: 左右で列数が違い（左6 / 右5）、右は `col-offset = 6` に依存している。`roBa.dtsi` の `default_transform` の `map` は左右を合わせた 11 列前提なので、片側だけ変更すると対応が崩れる。

### トラックボールの入力処理（`roBa_R.overlay`）

```
input-processors = <&zip_temp_layer 5 500 &mouse_runtime_input_processor>;
```

- ボールを動かすと **レイヤー5（MOUSE）に 500ms 一時遷移**する。`&zip_temp_layer` の `require-prior-idle-ms = <100>` は誤爆抑制用で、直近のタイピング後の意図しないレイヤー切替を防ぐ調整点。
- `excluded-positions` は「ここに挙げた位置**以外**を押すとレイヤーを即解除する」という**反転した意味**を持つ。マウスボタン（9 15 18 19 20 21）だけを挙げてあり、他のキー（言語選択キー含む）を押すとマウスレイヤーを抜けてベースレイヤーに戻る。**このプロパティが空だと ZMK は `zmk_position_state_changed` の購読自体を行わず、キー入力での解除が一切効かなくなる**（タイムアウト待ちのみになる）ので、消してはいけない。
- `scroller` ノードは **レイヤー6（SCROLL）** で XY をスクロールに変換（Y反転）。
- `mouse_runtime_input_processor` / `scroll_runtime_input_processor` は ZMK Studio から実行時に感度等を変更するための fork 側機能。

### 電池残量LED（`src/battery_led.c`）

このリポジトリ自身が Zephyr モジュールとして C コードを持つ（`zephyr/module.yml` の `cmake` / `kconfig`）。起動後の最初の `zmk_battery_state_changed` で XIAO のオンボード LED を3秒だけ点灯させる。ZMK にも DYA モジュールにも LED を触るコードは無い（`DT_ALIAS led` の検索結果0件）ので、**初期化しない LED はブートローダーが残した状態のまま点きっぱなしになる**。3つとも必ず消灯状態に固定すること。

**ピン留めしている Zephyr 3.5 のボード定義は緑と青のラベルが逆。** `seeeduino_xiao_ble.dts` は led1(P0.30)="Blue" / led2(P0.06)="Green" としているが実機と逆で、upstream Zephyr の `boards/seeed/xiao_ble/` では led1="Green" / led2="Blue" に修正済み。**DTS の `label` を信じてはいけない**（緑のつもりで青が点き、赤と混ざって紫になる）。

## キーマップ（`config/roBa.keymap`）を編集するとき

レイヤー番号は定義順に 0〜8：

| # | 名前 | 用途 |
|---|---|---|
| 0 | `default_layer` | Windows ベース |
| 1 | `MAC` | Mac ベース（最下段の修飾キーだけが差分） |
| 2 | `FUNCTION` | `&lt 2 LANG2` |
| 3 | `NUM` | `&lt 3 LANG1` |
| 4 | `ARROW` | **現在どこからも遷移できない**（到達手段なし） |
| 5 | `MOUSE` | トラックボールが `zip_temp_layer` で自動遷移 |
| 6 | `SCROLL` | `scroller` 用。**キーからは到達できない** |
| 7 | `SETTINGS` | BT / bootloader / studio_unlock / OS切替 |
| 8 | `MAC_FUNCTION` | conditional-layers `<1 2>` で自動有効化 |

`&lt 2 LANG2` / `&lt 3 LANG1` / `&lt 7 TAB` や overlay 側の `zip_temp_layer 5` / `scroller layers = <6>` がこの番号に依存しているため、**レイヤーの並び替えは overlay も含めて影響する**。

**代替ベースレイヤーは必ずオーバーレイ層より若い番号に置くこと。** ZMKは有効なレイヤーのうち最大番号から解決するため、`MAC` を末尾（例: 8）に置くと `FUNCTION`/`NUM`/`MOUSE`/`SETTINGS` が全て `MAC` に負けて機能しなくなる。特にトラックボールの `zip_temp_layer` が無効化されるので気付きにくい。`MAC` が 1 にあるのはこの制約による。

### OS の切り替え

`SETTINGS` レイヤーの **W = `&to 0`（Windows）/ M = `&to 1`（Mac）**。トグルではなく明示指定なので現在状態を知らなくても復帰できる（roBaには表示手段が無いため意図的にこの設計）。

Windows・Mac ともUSB有線接続のため、`zmk_endpoint_changed` は発火せず**接続先による自動切り替えはできない**。ZMKにOS検出機能（QMKの`os_detection`相当）も無い。

`muhennkann` combo（A+S）は `layers` で Windows 側 / Mac 側に出し分けている（`to_layer_0` + 無変換 / `to_layer_1` + 英数）。ただし combo の判定は「最上位の有効レイヤー」で行われるため、**MAC ベース中に NUM や MOUSE が有効な状態で A+S を押すと Windows 側の combo が発火して layer 0 に飛ぶ**。共有レイヤーからはどちらのベースにいたか判別できないための既知の制約。

**全レイヤーとも 43 バインディングちょうどで書くこと。** ZMK は余剰分を黙って捨てるためビルドは通ってしまうが、keymap-drawer は `config/roBa.json` の43キーに対して描画するので数が合わないと壊れる。実際 `default_layer` / `FUNCTION` / `NUM` には同一4行ブロックの重複（86個）が入り込んでいた時期がある。

`combos` の `key-positions` は `default_transform` の map 上のインデックス（0始まり・43キー）を指す。

## 参考

- roBa 本家（シールド定義の出自）: https://github.com/kumamuk-git/zmk-config-roBa
- `config/roBa.json` は keymap-drawer 用の物理レイアウト定義（`draw.yml` の `json_path: "config"`）。
