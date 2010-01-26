#!/bin/sh

BACKGROUND_FILE=background_1024x600.png
if [ -e /tmp/use_ugly_x_cursor ]; then
  BACKGROUND_FILE=background_1024x600_retro.png
fi

WM=/usr/bin/chromeos-wm
IMAGES=/usr/share/chromeos-assets/images
BACKGROUND="${IMAGES}/${BACKGROUND_FILE}"
LOG_DIR="${HOME}/log"
XTERM_COMMAND="/usr/bin/xterm -bg black -fg grey"

# TODO(cmasone): figure out where the ibus stuff should go.
# Start IBus daemon so users can input CJK characters.
# TODO(yusukes): should not launch ibus-daemon when no IME is configured.
# export GTK_IM_MODULE=ibus
# (sleep 5; /usr/bin/ibus-daemon) &
/usr/bin/xscreensaver -no-splash &
/usr/sbin/omaha_tracker.sh &

cd "${HOME}"
exec "${WM}"                                                              \
    --hotkey_overlay_image_dir="${IMAGES}"                                \
    --lm_new_overview_mode=true                                           \
    --lm_overview_gradient_image="${IMAGES}/window_overview_gradient.png" \
    --log_dir="${LOG_DIR}"                                                \
    --panel_anchor_image="${IMAGES}/panel_anchor.png"                     \
    --panel_bar_image="${IMAGES}/panel_bar_bg.png"                        \
    --shadow_image_dir="${IMAGES}"                                        \
    --wm_background_image="${BACKGROUND}"                                 \
    --wm_xterm_command="${XTERM_COMMAND}"
