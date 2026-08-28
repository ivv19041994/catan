#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
default_apk="$project_dir/Builds/Android/Catan-arm64.apk"
[[ -f "$default_apk" ]] || default_apk="$project_dir/Builds/AndroidLatest/Catan-arm64.apk"
apk="${1:-$default_apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
log_dir="$(mktemp -d /tmp/catan-android-smoke.XXXXXX)"
log_file="$log_dir/logcat.txt"
screenshot="$log_dir/main-menu.png"
started_emulator=0

fail() {
  if command -v adb >/dev/null 2>&1 && [[ -n "$(adb devices 2>/dev/null | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
    adb exec-out screencap -p >"$log_dir/failure-screen.png" 2>/dev/null || true
    adb logcat -d >"$log_file" 2>/dev/null || true
  fi
  print -u2 "FAIL: $1"
  print -u2 "Artifacts: $log_dir"
  exit 1
}

if [[ ! -f "$apk" ]]; then
  fail "APK not found: $apk"
fi
if ! command -v adb >/dev/null 2>&1; then
  fail "adb is not available"
fi

if [[ -z "$(adb devices | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
  print "Starting $avd with host Vulkan renderer..."
  "$emulator" -avd "$avd" -gpu host -feature Vulkan -no-snapshot-save \
    -netdelay none -netspeed full >"$log_dir/emulator.log" 2>&1 &
  started_emulator=1
fi

adb wait-for-device
for attempt in {1..120}; do
  [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] && break
  sleep 1
done
[[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] \
  || fail "emulator did not finish booting"

renderer="$(adb shell dumpsys SurfaceFlinger 2>/dev/null | rg -m1 'GLES:' || true)"
print "Renderer: ${renderer:-unknown}"
adb install -r "$apk" >/dev/null || fail "APK installation failed"
fatal_pattern='No Vulkan driver found|Unable to run on this device|Assertion failed|Ensure condition failed|Handled ensure|Fatal error|FATAL EXCEPTION|Fatal signal|Lock at Offset|RequestExit\(1'

capture_app_log() {
  local app_pid="$(adb shell pidof -s "$package_name" 2>/dev/null | tr -d '\r')"
  if [[ -n "$app_pid" ]]; then adb logcat -d --pid="$app_pid" >"$log_file"
  else : >"$log_file"
  fi
}

assert_modal_hides_actions() {
  capture_app_log
  rg -q 'CATAN_ACTION_PANEL modal=1 visible=0' "$log_file" \
    || fail "modal did not hide the lower action panel"
}

assert_running_without_fatal() {
  local context="$1"
  capture_app_log
  if rg -q "$fatal_pattern" "$log_file"; then
    rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
    fail "$context"
  fi
  local top_activity="$(adb shell dumpsys activity activities | rg -m1 'topResumedActivity' || true)"
  [[ "$top_activity" == *"$package_name/com.epicgames.unreal.GameActivity"* ]] \
    || fail "Catan GameActivity is no longer in the foreground: $top_activity"
}

test_combo_preview() {
  local mode="$1"
  local tap_x="$2"
  local tap_y="$3"
  local output="$log_dir/${mode:l}-dropdown.png"
  print "Testing $mode dropdown contrast and post-GC stability..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline "-CatanUIPreview=$mode" >/dev/null \
    || fail "$mode preview launch failed"
  local preview_ready=0
  local style_ready=0
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "$mode preview crashed during startup"
    fi
    rg -q "CATAN_UI_PREVIEW ready mode=$mode" "$log_file" && preview_ready=1
    rg -q 'CATAN_COMBO_STYLE ready widgets=23 popupText=white rowHeight=82 font=30' "$log_file" && style_ready=1
    (( preview_ready && style_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "$mode preview marker was not observed"
  (( style_ready )) || fail "large readable combo style was not applied to all 23 dropdowns"
  assert_modal_hides_actions
  if [[ "$mode" == "PlayerTrade" ]]; then
    rg -q 'CATAN_PLAYER_TRADE_LIMITS max=1,2,3,4,7 receive=5' "$log_file" \
      || fail "Other Player give limits do not match the local hand"
    rg -q 'CATAN_RESOURCE_LABELS .*semanticLeak=0' "$log_file" \
      || fail "an internal semantic resource key leaked into the visible UI"
    rg -q 'CATAN_PLAYER_TRADE_LAYOUT modalHeight=900 rows=5 actions=2 scrollFallback=1' "$log_file" \
      || fail "PlayerTrade does not reserve room for all rows and action buttons"
  fi
  if [[ "$mode" == "Discard" ]]; then
    rg -q 'CATAN_DISCARD_LIMITS max=8,8,8,8,8 integerDropdowns=1' "$log_file" \
      || fail "Discard dropdown limits do not match the local hand"
  fi
  if [[ "$mode" == "DevelopmentPlenty" ]]; then
    rg -q 'CATAN_DEVELOPMENT_MENU mode=plenty totalLimit=2' "$log_file" \
      || fail "Year of Plenty total selection limit was not applied"
  fi
  if [[ "$mode" == "PlayerTrade" ]]; then
    adb exec-out screencap -p >"$log_dir/playertrade-closed.png"
    [[ -s "$log_dir/playertrade-closed.png" ]] || fail "closed PlayerTrade screenshot is empty"
  fi
  local combo_open=0
  # The preview marker can arrive one frame before Android starts routing touch to Slate.
  sleep 2
  for attempt in {1..3}; do
    adb shell input tap "$tap_x" "$tap_y"
    sleep 1
    capture_app_log
    rg -q 'CATAN_COMBO_OPEN' "$log_file" && combo_open=1
    (( combo_open )) && break
  done
  (( combo_open )) || fail "$mode dropdown did not open at $tap_x,$tap_y"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "$mode dropdown screenshot is empty"
  if [[ "$mode" == "PlayerTrade" ]]; then
    local compact_selection=0
    # A successful attempt must produce the native lifecycle marker proving that
    # selected content was regenerated with the compact layout while the popup
    # was still reporting itself as open.
    for attempt in {1..3}; do
      # Android's immersive landscape viewport starts below the physical input
      # origin; +160 reaches the second visible row (Player 2), not the already
      # selected first row.
      adb shell input tap "$tap_x" "$((tap_y + 160))"
      sleep 1
      capture_app_log
      rg -q 'CATAN_COMBO_SELECTION compact=1 popupRowApplied=0' "$log_file" \
        && compact_selection=1
      (( compact_selection )) && break
    done
    (( compact_selection )) \
      || fail "selected PlayerTrade value inherited popup row height"
    assert_running_without_fatal "PlayerTrade layout failed after selecting a popup row"
    adb exec-out screencap -p >"$log_dir/playertrade-selected.png"
    [[ -s "$log_dir/playertrade-selected.png" ]] \
      || fail "selected PlayerTrade screenshot is empty"
  fi
  adb shell am broadcast -a android.intent.action.RUN -e cmd 'obj gc' >/dev/null
  sleep 3
  assert_running_without_fatal "$mode dropdown failed after garbage collection"
}

test_bank_preview() {
  local output="$log_dir/bank-rates.png"
  print "Testing per-resource bank rate labels..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline '-CatanUIPreview=Bank' >/dev/null \
    || fail "bank preview launch failed"
  local preview_ready=0
  local labels_ready=0
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "bank preview crashed during startup"
    fi
    rg -q 'CATAN_UI_PREVIEW ready mode=Bank' "$log_file" && preview_ready=1
    rg -q 'CATAN_BANK_LABELS rates=4,3,4,2,4' "$log_file" && labels_ready=1
    (( preview_ready && labels_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "bank preview marker was not observed"
  (( labels_ready )) || fail "per-resource bank rate labels were not applied"
  assert_modal_hides_actions
  assert_running_without_fatal "bank rate labels failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "bank rate screenshot is empty"
}

test_resource_bank_visual() {
  local output="$log_dir/resource-bank-outline.png"
  print "Testing resource-bank stack number outline in a real game..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline '-CatanAutoBots=1' >/dev/null \
    || fail "resource-bank game launch failed"
  local board_ready=0
  local outline_ready=0
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      fail "resource-bank game crashed during startup"
    fi
    rg -q 'CATAN_SMOKE client board ready' "$log_file" && board_ready=1
    rg -q 'CATAN_BANK_NUMBER_STYLE foreground=white outline=black' "$log_file" \
      && outline_ready=1
    (( board_ready && outline_ready )) && break
    sleep 1
  done
  (( board_ready )) || fail "resource-bank board was not built"
  (( outline_ready )) || fail "bank stack numbers do not have a black outline"
  assert_running_without_fatal "resource-bank number outline failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "resource-bank outline screenshot is empty"
}

test_onboarding_preview() {
  local language="$1"
  local page="$2"
  local output="$log_dir/onboarding-$language-$page.png"
  print "Testing $language onboarding page $page and localization..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanUIPreview=Onboarding -CatanLanguage=$language -CatanOnboardingPage=$page'" >/dev/null \
    || fail "$language onboarding preview launch failed"
  local preview_ready=0
  local onboarding_ready=0
  local localization_ready=0
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      fail "$language onboarding preview crashed"
    fi
    rg -q 'CATAN_UI_PREVIEW ready mode=Onboarding' "$log_file" && preview_ready=1
    if [[ "$page" == "welcome" ]]; then
      rg -q "CATAN_ONBOARDING page=welcome step=1/3 language=$language touchTargets=56" \
        "$log_file" && onboarding_ready=1
    elif [[ "$page" == "controls" ]]; then
      rg -q 'CATAN_ONBOARDING page=controls step=2/3' "$log_file" && onboarding_ready=1
    else
      rg -q 'CATAN_ONBOARDING page=turn step=3/3' "$log_file" && onboarding_ready=1
    fi
    rg -q "CATAN_LOCALIZATION_AUDIT language=$language keys=17 missing=0" \
      "$log_file" && localization_ready=1
    (( preview_ready && onboarding_ready && localization_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "$language onboarding preview marker was not observed"
  (( onboarding_ready )) || fail "$language onboarding page was not mobile-ready"
  (( localization_ready )) || fail "$language onboarding has missing translations"
  assert_modal_hides_actions
  # The preview markers are emitted on frame zero, before Android has necessarily
  # presented the first Slate/RHI frame. Never accept a splash or magenta clear
  # buffer as visual evidence.
  sleep 3
  assert_running_without_fatal "$language onboarding failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "$language onboarding screenshot is empty"
}

test_event_history_preview() {
  local output="$log_dir/event-history-ru.png"
  print "Testing localized Russian game history..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanUIPreview=History -CatanLanguage=ru'" >/dev/null \
    || fail "Russian event-history preview launch failed"
  local preview_ready=0
  local history_ready=0
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      fail "Russian event-history preview crashed"
    fi
    rg -q 'CATAN_UI_PREVIEW ready mode=History' "$log_file" && preview_ready=1
    rg -q 'CATAN_EVENT_HISTORY language=ru translated=4 fallback=0' "$log_file" \
      && history_ready=1
    (( preview_ready && history_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "Russian event-history preview marker was not observed"
  (( history_ready )) || fail "Russian game history contains untranslated known events"
  sleep 3
  assert_running_without_fatal "Russian event-history preview failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "Russian event-history screenshot is empty"
}

test_russian_action_label_preview() {
  local output="$log_dir/action-label-city-ru.png"
  print "Testing collision-free Russian city action label..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanUIPreview=ActionLabels -CatanLanguage=ru'" >/dev/null \
    || fail "Russian action-label preview launch failed"
  for attempt in {1..60}; do
    capture_app_log
    rg -q 'CATAN_ACTION_LABEL city=ЗАМОК collisionFree=1' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_ACTION_LABEL city=ЗАМОК collisionFree=1' "$log_file" \
    || fail "city action label is not the nominative Russian ЗАМОК"
  sleep 3
  assert_running_without_fatal "Russian action-label preview failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "Russian action-label screenshot is empty"
}

test_development_monopoly_preview() {
  local output="$log_dir/development-monopoly.png"
  print "Testing Monopoly resource submenu..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline '-CatanUIPreview=DevelopmentMonopoly' >/dev/null \
    || fail "Monopoly preview launch failed"
  local preview_ready=0
  local selection_ready=0
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "Monopoly preview crashed during startup"
    fi
    rg -q 'CATAN_UI_PREVIEW ready mode=DevelopmentMonopoly' "$log_file" && preview_ready=1
    rg -q 'CATAN_DEVELOPMENT_MENU mode=monopoly singleSelection=1' "$log_file" && selection_ready=1
    (( preview_ready && selection_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "Monopoly preview marker was not observed"
  (( selection_ready )) || fail "Monopoly single-resource selection was not applied"
  assert_modal_hides_actions
  assert_running_without_fatal "Monopoly resource submenu failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "Monopoly resource submenu screenshot is empty"
}

test_online_page_preview() {
  local mode="$1"
  local output="$log_dir/${mode:l}-page.png"
  print "Testing $mode online page..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline "-CatanUIPreview=$mode" >/dev/null \
    || fail "$mode page launch failed"
  local preview_ready=0
  local split_ready=0
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "$mode page crashed during startup"
    fi
    rg -q "CATAN_UI_PREVIEW ready mode=$mode" "$log_file" && preview_ready=1
    rg -q 'CATAN_ONLINE_MENU_SPLIT ready pages=chooser,local,dedicated dedicatedScroll=0' "$log_file" \
      && split_ready=1
    (( preview_ready && split_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "$mode page preview marker was not observed"
  (( split_ready )) || fail "split online menu marker was not observed"
  assert_modal_hides_actions
  assert_running_without_fatal "$mode online page failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "$mode page screenshot is empty"
}

test_local_network_page() {
  local output="$log_dir/localnetwork-page.png"
  print "Testing scrollable Local Network page and empty save catalog..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline '-CatanUIPreview=LocalNetwork' >/dev/null \
    || fail "Local Network page launch failed"
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then fail "Local Network page crashed"; fi
    rg -q 'CATAN_UI_PREVIEW ready mode=LocalNetwork' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_UI_PREVIEW ready mode=LocalNetwork' "$log_file" \
    || fail "Local Network page did not become ready"
  adb shell input swipe 1200 680 1200 300 600
  sleep 1
  assert_running_without_fatal "Local Network page swipe failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "Local Network page screenshot is empty"
}

test_settings_preview() {
  local output="$log_dir/settings-russian-page.png"
  print "Testing persistent settings page and Cyrillic rendering..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanUIPreview=Settings -CatanLanguage=ru -CatanColorVision=high-contrast'" >/dev/null \
    || fail "settings preview launch failed"
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "settings preview crashed during startup"
    fi
    rg -q 'CATAN_SETTINGS_PREVIEW name=.* language=ru' "$log_file" \
      && rg -q 'CATAN_POLISH_SETTINGS effects=5 music=5 haptics=2 palettes=5 touch=72 scroll=1' "$log_file" \
      && rg -q 'CATAN_ACCESSIBILITY palette=high-contrast .*labels=always' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_SETTINGS_PREVIEW name=.* language=ru' "$log_file" \
    || fail "settings persistence marker was not observed"
  rg -q 'CATAN_POLISH_SETTINGS effects=5 music=5 haptics=2 palettes=5 touch=72 scroll=1' "$log_file" \
    || fail "polish settings controls or mobile touch targets are incomplete"
  rg -q 'CATAN_ACCESSIBILITY palette=high-contrast .*labels=always' "$log_file" \
    || fail "high-contrast resource palette was not applied"
  adb shell input swipe 1200 680 1200 300 600
  sleep 1
  assert_modal_hides_actions
  assert_running_without_fatal "settings page failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "settings page screenshot is empty"
}

test_final_dashboard_preview() {
  local output="$log_dir/final-dashboard.png"
  print "Testing scrollable four-player final dashboard..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanUIPreview=FinalDashboard -CatanLanguage=ru'" >/dev/null \
    || fail "final dashboard preview launch failed"
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then fail "final dashboard crashed"; fi
    rg -q 'CATAN_FINAL_DASHBOARD rows=4 winner=Player vpCardsRevealed=4 scroll=1 touch=72' "$log_file" \
      && break
    sleep 1
  done
  rg -q 'CATAN_FINAL_DASHBOARD rows=4 winner=Player vpCardsRevealed=4 scroll=1 touch=72' "$log_file" \
    || fail "final dashboard did not reveal and rank all four players"
  assert_modal_hides_actions
  adb shell input swipe 1200 650 1200 320 500
  sleep 1
  assert_running_without_fatal "final dashboard swipe failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "final dashboard screenshot is empty"
}

test_online_navigation() {
  print "Testing touch navigation between split online pages..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline '-CatanUIPreview=Online' >/dev/null \
    || fail "online navigation preview launch failed"
  for attempt in {1..60}; do
    capture_app_log
    rg -q 'CATAN_UI_PREVIEW ready mode=Online' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_UI_PREVIEW ready mode=Online' "$log_file" \
    || fail "online chooser was not ready for navigation"
  sleep 2
  adb shell input tap 1200 495
  sleep 1
  capture_app_log
  rg -q 'CATAN_ONLINE_NAV page=dedicated' "$log_file" \
    || fail "Dedicated Server button did not open its page"
  adb shell input tap 1200 615
  sleep 1
  adb shell input tap 1200 420
  sleep 1
  capture_app_log
  rg -q 'CATAN_ONLINE_NAV page=chooser' "$log_file" \
    || fail "Dedicated Server Back button did not return to the chooser"
  rg -q 'CATAN_ONLINE_NAV page=local' "$log_file" \
    || fail "Local Network button did not open its page"
  assert_running_without_fatal "split online touch navigation failed"
}

test_hud_graph() {
  local output="$log_dir/hud-graph.png"
  print "Testing complete HUD state graph..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline '-CatanHUDGraphSmoke' >/dev/null \
    || fail "HUD graph launch failed"
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      fail "HUD graph crashed"
    fi
    rg -q 'CATAN_HUD_GRAPH PASS edges=43 failures=0' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_HUD_GRAPH PASS edges=43 failures=0' "$log_file" \
    || fail "complete HUD graph did not pass"
  assert_running_without_fatal "HUD graph failed after traversal"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "HUD graph screenshot is empty"
}

test_four_player_status_panel() {
  local output="$log_dir/player-status-4p.png"
  print "Testing compact four-player status panel and touch scroll..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanAutoBots=3 -CatanUIPreview=Players4'" >/dev/null \
    || fail "four-player status preview launch failed"
  for attempt in {1..60}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      fail "four-player status panel crashed"
    fi
    rg -q 'CATAN_PLAYER_STATUS rows=4 scroll=1 compact=1 viewport=230' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_PLAYER_STATUS rows=4 scroll=1 compact=1 viewport=230' "$log_file" \
    || fail "four-player status panel did not expose all rows in a scroll container"
  adb shell input swipe 1880 510 1880 270 500
  sleep 1
  assert_running_without_fatal "four-player player-list swipe failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "four-player status screenshot is empty"
}

test_failed_connections() {
  print "Testing failed LAN join returns to main menu..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanAutoManualJoin=10.0.2.2:1 -CatanAutoName=InvalidAndroidJoin'" >/dev/null \
    || fail "invalid LAN join launch failed"
  for attempt in {1..50}; do
    capture_app_log
    rg -q 'CATAN_HUD_GRAPH returned-main status=Connection failed:' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_HUD_GRAPH returned-main status=Connection failed:' "$log_file" \
    || fail "failed LAN join did not return to main menu"
  ! rg -q 'CATAN_HUD_GRAPH leave-scheduled' "$log_file" \
    || fail "failed LAN join incorrectly entered a lobby"

  print "Testing unavailable dedicated server stays outside lobby..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanDedicatedAddress=10.0.2.2:1 -CatanDedicatedJoin=INVALID -CatanAutoName=InvalidDedicated'" >/dev/null \
    || fail "unavailable dedicated launch failed"
  for attempt in {1..30}; do
    capture_app_log
    rg -q 'CATAN_HUD_GRAPH request-failure dedicatedActive=0 leaving=0' "$log_file" && break
    sleep 1
  done
  rg -q 'CATAN_HUD_GRAPH request-failure dedicatedActive=0 leaving=0' "$log_file" \
    || fail "unavailable dedicated server failure was not reported"
  ! rg -q 'CATAN_HUD_GRAPH leave-scheduled' "$log_file" \
    || fail "unavailable dedicated server incorrectly entered a lobby"
}

adb logcat -c
adb shell am force-stop "$package_name"
adb shell am start -n "$activity" >/dev/null || fail "activity launch failed"

initialized=0
menu_ready=0
for attempt in {1..60}; do
  capture_app_log
  if rg -q "$fatal_pattern" "$log_file"; then
    rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
    fail "fatal Android/RHI condition detected"
  fi
  rg -q 'Passed GEngineLoop.Init\(\)' "$log_file" && initialized=1
  rg -q 'CATAN_MENU backdrop ready' "$log_file" && menu_ready=1
  (( initialized && menu_ready )) && break
  sleep 1
done
(( initialized )) || fail "UE engine initialization marker was not observed"
(( menu_ready )) || fail "Catan main-menu marker was not observed"

# Some renderer failures happen several frames after the menu is created.
sleep 10
assert_running_without_fatal "late Android/RHI failure detected"

adb exec-out screencap -p >"$screenshot"
[[ -s "$screenshot" ]] || fail "Android screenshot is empty"

test_combo_preview PlayerTrade 1200 350
test_combo_preview Discard 1600 265
test_combo_preview DevelopmentPlenty 1550 390
test_development_monopoly_preview
test_online_navigation
test_online_page_preview Online
test_online_page_preview DedicatedServer
test_local_network_page
test_combo_preview Bots 1200 390
test_bank_preview
test_resource_bank_visual
test_event_history_preview
test_russian_action_label_preview
test_onboarding_preview en welcome
test_onboarding_preview ru welcome
test_onboarding_preview ru controls
test_onboarding_preview ru turn
test_settings_preview
test_final_dashboard_preview
test_hud_graph
test_four_player_status_panel
test_failed_connections

print "PASS: Android startup, four-player status panel, full HUD graph, failed joins, settings, development menus, trade and dropdown families"
print "APK: $apk"
print "Artifacts: $log_dir"
if (( started_emulator )); then
  print "Emulator was started by this script and left running for inspection."
fi
