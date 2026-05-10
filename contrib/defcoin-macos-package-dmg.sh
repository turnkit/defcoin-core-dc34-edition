#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

VERSION_LABEL="${1:-v0.21.5.4-defcoin.20260429}"
APP_SOURCE="${APP_SOURCE:-Defcoin-Qt.app}"
APP_NAME="Defcoin-Qt.app"
APP="dist/${APP_NAME}"
ARCH_LABEL="${ARCH_LABEL:-arm64}"
DMG_NAME="${DMG_NAME:-Defcoin-Core-${VERSION_LABEL}-${ARCH_LABEL}.dmg}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
QT_PREFIX="${QT_PREFIX:-$(/opt/homebrew/bin/brew --prefix qt@5 2>/dev/null || true)}"
QT_PREFIX="${QT_PREFIX:-/opt/homebrew/opt/qt@5}"
DEFCOIN_PACKAGE_THEMES="${DEFCOIN_PACKAGE_THEMES:-${ENABLE_DEFCOIN_FUN_UI:-1}}"
DEFCOIN_PACKAGE_DIRECT="${DEFCOIN_PACKAGE_DIRECT:-0}"
DEFCOIN_DMG_BG_WIDTH="${DEFCOIN_DMG_BG_WIDTH:-520}"
DEFCOIN_DMG_BG_HEIGHT="${DEFCOIN_DMG_BG_HEIGHT:-380}"
DEFCOIN_DMG_ICON_SIZE="${DEFCOIN_DMG_ICON_SIZE:-96}"
: "${DEFCOIN_DMG_APP_ICON_X:=150}"
: "${DEFCOIN_DMG_FOLDER_ICON_X:=370}"
: "${DEFCOIN_DMG_ICON_Y:=238}"
DEFCOIN_DMG_LOGO_SOURCE="${DEFCOIN_DMG_LOGO_SOURCE:-$ROOT_DIR/../defcoin-core-local-materials/local-only/defcoin_custom_graphics/themes/defcoin_logo_2k_nobackground_adj8b.png}"

QT_TRANSLATIONS="ar,bg,ca,cs,da,de,es,fa,fi,fr,gd,gl,he,hu,it,ja,ko,lt,lv,pl,pt_BR,ru,sk,sl,sv,uk,zh_CN,zh_TW"

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required tool: $1" >&2
    exit 1
  fi
}

require_tool hdiutil
require_tool install_name_tool
require_tool codesign
require_tool ditto
require_tool file
require_tool otool
require_tool sips
require_tool "$PYTHON_BIN"

if [ ! -d "$APP_SOURCE" ]; then
  echo "Missing ${APP_SOURCE}. Run 'make appbundle' first." >&2
  exit 1
fi

echo "+ Deploying Qt runtime into dist/"
rm -rf dist
STATIC_QT_PACKAGE=0
if [ "$DEFCOIN_PACKAGE_DIRECT" = "1" ]; then
  echo "  direct package mode: copying already-prepared app bundle"
  mkdir -p dist
  ditto "$APP_SOURCE" "$APP"
  STATIC_QT_PACKAGE=1
elif ! "$PYTHON_BIN" ./contrib/macdeploy/macdeployqtplus \
  "$APP_SOURCE" \
  -add-qt-tr "$QT_TRANSLATIONS" \
  -volname "Defcoin Core" \
  -verbose 1; then
  if ! otool -L "$APP_SOURCE/Contents/MacOS/Defcoin-Qt" | grep -Eq 'Qt.*framework|libQt5'; then
    echo "  static Qt app detected; using direct app bundle copy"
    mkdir -p dist
    ditto "$APP_SOURCE" "$APP"
    STATIC_QT_PACKAGE=1
  else
    echo "Qt deployment failed for a dynamically linked app bundle." >&2
    exit 1
  fi
fi

echo "+ Pruning unused Qt plugin families"
if [ -d "$APP/Contents/PlugIns" ] || [ -d "$APP/Contents/Frameworks" ]; then
  rm -rf "$APP/Contents/PlugIns/assetimporters" \
       "$APP/Contents/PlugIns/renderers" \
       "$APP/Contents/PlugIns/platformthemes" \
       "$APP/Contents/PlugIns/generic" \
       "$APP/Contents/PlugIns/platforms/libqminimal.dylib" \
       "$APP/Contents/PlugIns/platforms/libqoffscreen.dylib" \
       "$APP/Contents/PlugIns/platforms/libqwebgl.dylib" \
       "$APP/Contents/Frameworks/Qt3DCore.framework" \
       "$APP/Contents/Frameworks/Qt3DRender.framework" \
       "$APP/Contents/Frameworks/QtConcurrent.framework" \
       "$APP/Contents/Frameworks/QtQml.framework" \
       "$APP/Contents/Frameworks/QtQmlModels.framework" \
       "$APP/Contents/Frameworks/QtQuick.framework" \
       "$APP/Contents/Frameworks/QtQuick3DAssetImport.framework" \
       "$APP/Contents/Frameworks/QtQuick3DRender.framework" \
       "$APP/Contents/Frameworks/QtQuick3DUtils.framework" \
       "$APP/Contents/Frameworks/QtWebSockets.framework"
fi

if [ "$STATIC_QT_PACKAGE" != "1" ] && [ ! -d "$QT_PREFIX/lib/QtSvg.framework" ]; then
  echo "Missing QtSvg.framework under ${QT_PREFIX}. Install Homebrew qt@5 or set QT_PREFIX." >&2
  exit 1
fi
if [ "$STATIC_QT_PACKAGE" != "1" ] && \
   { [ ! -f "$QT_PREFIX/plugins/iconengines/libqsvgicon.dylib" ] || \
     [ ! -f "$QT_PREFIX/plugins/imageformats/libqsvg.dylib" ]; }; then
  echo "Missing Qt SVG plugins under ${QT_PREFIX}/plugins. Install Homebrew qt@5 or set QT_PREFIX." >&2
  exit 1
fi

if [ "$STATIC_QT_PACKAGE" != "1" ]; then
  echo "+ Adding Qt SVG runtime support"
  rm -rf "$APP/Contents/Frameworks/QtSvg.framework"
  ditto "$QT_PREFIX/lib/QtSvg.framework" "$APP/Contents/Frameworks/QtSvg.framework"
  rm -rf "$APP/Contents/Frameworks/QtSvg.framework/Headers" \
       "$APP/Contents/Frameworks/QtSvg.framework/Versions/5/Headers" \
       "$APP/Contents/Frameworks/QtSvg.framework/_CodeSignature" \
       "$APP/Contents/Frameworks/QtSvg.framework/Versions/5/_CodeSignature"
  mkdir -p "$APP/Contents/PlugIns/iconengines" "$APP/Contents/PlugIns/imageformats"
  cp -p "$QT_PREFIX/plugins/iconengines/libqsvgicon.dylib" \
      "$APP/Contents/PlugIns/iconengines/libqsvgicon.dylib"
  cp -p "$QT_PREFIX/plugins/imageformats/libqsvg.dylib" \
      "$APP/Contents/PlugIns/imageformats/libqsvg.dylib"
else
  echo "+ Static Qt package: skipping dynamic Qt SVG runtime support"
fi

echo "+ Adding Defcoin theme resources"
rm -rf "$APP/Contents/Resources/themes" "$APP/Contents/Resources/custom_themes"
if [ "$DEFCOIN_PACKAGE_THEMES" = "0" ]; then
  echo "  standard package: theme resources omitted"
  PRODUCT_NAME="Defcoin Core"
  SHORT_VERSION="2026.1"
  GET_INFO_STRING="Defcoin Core 2026.1"
  VOLUME_NAME="Defcoin Core"
else
  ditto "src/qt/res/themes" "$APP/Contents/Resources/themes"
  PRODUCT_NAME="Defcoin Core DC34 Edition"
  SHORT_VERSION="2026.1 DC34 Edition"
  GET_INFO_STRING="Defcoin Core DC34 Edition 2026.1, Codename: Token Jester"
  VOLUME_NAME="Defcoin Core DC34 Edition"
fi

echo "+ Normalizing Defcoin bundle metadata"
/usr/libexec/PlistBuddy \
  -c "Set :CFBundleName ${PRODUCT_NAME}" \
  -c "Set :CFBundleDisplayName ${PRODUCT_NAME}" \
  -c "Set :CFBundleShortVersionString ${SHORT_VERSION}" \
  -c "Set :CFBundleGetInfoString ${GET_INFO_STRING}" \
  -c "Set :NSHumanReadableCopyright Copyright (C) 2014-2026 The Defcoin Core developers" \
  "$APP/Contents/Info.plist"
mkdir -p "$APP/Contents/Resources/Base.lproj"
printf '{\tCFBundleDisplayName = "%s"; CFBundleName = "%s"; }\n' \
  "$PRODUCT_NAME" "$PRODUCT_NAME" > "$APP/Contents/Resources/Base.lproj/InfoPlist.strings"

relink_qt_framework_refs() {
  local binary="$1"
  local fw dep
  for fw in QtCore QtGui QtWidgets QtNetwork QtDBus QtPrintSupport QtSvg; do
    while IFS= read -r dep; do
      [ -n "$dep" ] || continue
      install_name_tool -change "$dep" \
        "@executable_path/../Frameworks/${fw}.framework/Versions/5/${fw}" \
        "$binary" 2>/dev/null || true
    done < <(otool -L "$binary" | awk -v fw="${fw}.framework" '$1 ~ fw { print $1 }')
  done
}

bundle_homebrew_dylib() {
  local binary="$1"
  local pattern="$2"
  local dep base dest install_name
  dep="$(otool -L "$binary" | awk -v pat="$pattern" '$1 ~ pat { print $1; exit }')"
  [ -n "$dep" ] || return 0
  [ -f "$dep" ] || return 0
  base="$(basename "$dep")"
  dest="$APP/Contents/Frameworks/$base"
  install_name="@executable_path/../Frameworks/$base"
  mkdir -p "$APP/Contents/Frameworks"
  cp -p "$dep" "$dest"
  chmod u+rw "$dest"
  install_name_tool -id "$install_name" "$dest" 2>/dev/null || true
  install_name_tool -change "$dep" "$install_name" "$binary" 2>/dev/null || true
}

echo "+ Normalizing Qt plugin install IDs"
[ -f "$APP/Contents/PlugIns/platforms/libqcocoa.dylib" ] && \
  install_name_tool -id "@executable_path/../PlugIns/platforms/libqcocoa.dylib" \
    "$APP/Contents/PlugIns/platforms/libqcocoa.dylib"
[ -f "$APP/Contents/PlugIns/styles/libqmacstyle.dylib" ] && \
  install_name_tool -id "@executable_path/../PlugIns/styles/libqmacstyle.dylib" \
    "$APP/Contents/PlugIns/styles/libqmacstyle.dylib"
[ -f "$APP/Contents/PlugIns/bearer/libqgenericbearer.dylib" ] && \
  install_name_tool -id "@executable_path/../PlugIns/bearer/libqgenericbearer.dylib" \
    "$APP/Contents/PlugIns/bearer/libqgenericbearer.dylib"
[ -f "$APP/Contents/PlugIns/iconengines/libqsvgicon.dylib" ] && \
  install_name_tool -id "@executable_path/../PlugIns/iconengines/libqsvgicon.dylib" \
    "$APP/Contents/PlugIns/iconengines/libqsvgicon.dylib"
[ -f "$APP/Contents/PlugIns/imageformats/libqsvg.dylib" ] && \
  install_name_tool -id "@executable_path/../PlugIns/imageformats/libqsvg.dylib" \
    "$APP/Contents/PlugIns/imageformats/libqsvg.dylib"

echo "+ Relinking Qt SVG runtime dependencies"
[ -f "$APP/Contents/Frameworks/QtSvg.framework/Versions/5/QtSvg" ] && \
  install_name_tool -id "@executable_path/../Frameworks/QtSvg.framework/Versions/5/QtSvg" \
    "$APP/Contents/Frameworks/QtSvg.framework/Versions/5/QtSvg"
[ -f "$APP/Contents/Frameworks/QtSvg.framework/Versions/5/QtSvg" ] && \
  relink_qt_framework_refs "$APP/Contents/Frameworks/QtSvg.framework/Versions/5/QtSvg"
[ -f "$APP/Contents/PlugIns/iconengines/libqsvgicon.dylib" ] && \
  relink_qt_framework_refs "$APP/Contents/PlugIns/iconengines/libqsvgicon.dylib"
[ -f "$APP/Contents/PlugIns/imageformats/libqsvg.dylib" ] && \
  relink_qt_framework_refs "$APP/Contents/PlugIns/imageformats/libqsvg.dylib"

echo "+ Bundling optional runtime libraries"
bundle_homebrew_dylib "$APP/Contents/MacOS/Defcoin-Qt" "libminiupnpc"

echo "+ Normalizing Qt framework layout for macOS codesign"
for fw in "$APP"/Contents/Frameworks/Qt*.framework; do
  [ -d "$fw" ] || continue
  name="$(basename "$fw" .framework)"
  mkdir -p "$fw/Versions/5"
  if [ -d "$fw/Resources" ] && [ ! -L "$fw/Resources" ]; then
    rm -rf "$fw/Versions/5/Resources"
    mv "$fw/Resources" "$fw/Versions/5/Resources"
  fi
  rm -f "$fw/Resources" "$fw/$name" "$fw/Headers"
  ln -s "Versions/Current/Resources" "$fw/Resources"
  ln -s "Versions/Current/$name" "$fw/$name"
done
[ -d "$APP/Contents/Frameworks" ] && \
  find "$APP/Contents/Frameworks" -path '*/Headers' -type d -prune -exec rm -rf {} +

chmod -R u+rwX "$APP"
xattr -rc "$APP" 2>/dev/null || true

echo "+ Ad-hoc signing app bundle"
find "$APP" -type f -print0 | while IFS= read -r -d '' f; do
  if file -b "$f" | grep -q 'Mach-O'; then
    chmod u+x "$f"
  fi
done

for fw in "$APP"/Contents/Frameworks/Qt*.framework; do
  [ -d "$fw" ] && codesign --force --sign - --timestamp=none "$fw" >/dev/null
done

if [ -d "$APP/Contents/Frameworks" ] || [ -d "$APP/Contents/PlugIns" ]; then
  find "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" -type f -name '*.dylib' -print0 2>/dev/null | while IFS= read -r -d '' f; do
    codesign --force --sign - --timestamp=none "$f" >/dev/null
  done
fi

codesign --force --sign - --timestamp=none "$APP/Contents/MacOS/Defcoin-Qt" >/dev/null
codesign --force --sign - --timestamp=none "$APP" >/dev/null
codesign --verify --deep --strict --verbose=2 "$APP"

echo "+ Checking bundled dependency paths"
"$PYTHON_BIN" - "$APP" <<'PY'
import os
import subprocess
import sys

root = sys.argv[1]
refs = []
for dirpath, _dirnames, filenames in os.walk(root):
    for name in filenames:
        path = os.path.join(dirpath, name)
        try:
            kind = subprocess.check_output(["file", "-b", path], text=True, stderr=subprocess.DEVNULL)
        except Exception:
            continue
        if "Mach-O" not in kind:
            continue
        try:
            libs = subprocess.check_output(["otool", "-L", path], text=True, stderr=subprocess.DEVNULL)
        except subprocess.CalledProcessError:
            continue
        for line in libs.splitlines()[1:]:
            dep = line.strip().split(" (compatibility", 1)[0]
            if dep.startswith("/opt/homebrew"):
                refs.append((path, dep))

if refs:
    for path, dep in refs:
        print(f"{path}: {dep}")
    sys.exit(1)

print("No /opt/homebrew dependency references found.")
PY

echo "+ Testing version output"
"$APP/Contents/MacOS/Defcoin-Qt" --version >/dev/null

echo "+ Creating DMG"
STAGE_DIR="$(mktemp -d /tmp/defcoin-dmg-stage.XXXXXX)"
cleanup() {
  rm -rf "$STAGE_DIR"
}
trap cleanup EXIT

ditto "$APP" "$STAGE_DIR/$APP_NAME"
ln -s /Applications "$STAGE_DIR/Applications"
rm -f "$DMG_NAME"

echo "+ Adding DMG background"
mkdir -p "$STAGE_DIR/.background"
"$PYTHON_BIN" - "$STAGE_DIR/.background/background.png" "$DEFCOIN_PACKAGE_THEMES" "$DEFCOIN_DMG_BG_WIDTH" "$DEFCOIN_DMG_BG_HEIGHT" "$DEFCOIN_DMG_LOGO_SOURCE" "$DEFCOIN_DMG_APP_ICON_X" "$DEFCOIN_DMG_FOLDER_ICON_X" "$DEFCOIN_DMG_ICON_Y" <<'PY'
import math
import os
import random
import sys

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageFont

path = sys.argv[1]
is_fun = sys.argv[2] != "0"
width = int(sys.argv[3])
height = int(sys.argv[4])
logo_path = sys.argv[5]
app_x = int(sys.argv[6])
folder_x = int(sys.argv[7])
icon_y = int(sys.argv[8])

scale = 4
rw = width * scale
rh = height * scale
sx = lambda v: int(round(v * scale))

base = Image.new("RGBA", (rw, rh), (244, 247, 251, 255) if not is_fun else (5, 12, 25, 255))
draw = ImageDraw.Draw(base, "RGBA")

def load_font(size, bold=False):
    candidates = [
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "/Library/Fonts/Arial Bold.ttf" if bold else "/Library/Fonts/Arial.ttf",
    ]
    for candidate in candidates:
        try:
            if candidate and os.path.exists(candidate):
                return ImageFont.truetype(candidate, sx(size))
        except Exception:
            pass
    return ImageFont.load_default()

if not is_fun:
    # A quiet installer surface: the Defcoin coin is the visual signal.
    for y in range(rh):
        shade = int(247 - 10 * (y / max(rh - 1, 1)))
        draw.line([(0, y), (rw, y)], fill=(shade, min(shade + 2, 255), min(shade + 5, 255), 255))
else:
    # DC34 Edition: reserved cyberpunk-lite geometry using the DC34 palette.
    dc_blue = (45, 129, 180, 255)
    dc_teal = (139, 203, 193, 255)
    dc_yellow = (216, 201, 52, 255)
    dc_red = (214, 36, 86, 255)
    dc_navy = (5, 12, 36, 255)
    for y in range(rh):
        t = y / max(rh - 1, 1)
        r = int(dc_navy[0] + 7 * t)
        g = int(dc_navy[1] + 12 * t)
        b = int(dc_navy[2] + 20 * t)
        draw.line([(0, y), (rw, y)], fill=(r, g, b, 255))
    random.seed(3401)
    protected_title_bottom = sx(132)
    for _ in range(44):
        x = random.randrange(sx(10), rw - sx(10))
        y = random.randrange(protected_title_bottom, rh - sx(18))
        length = random.randrange(sx(28), sx(92))
        color = random.choice([
            (dc_blue[0], dc_blue[1], dc_blue[2], 72),
            (dc_teal[0], dc_teal[1], dc_teal[2], 62),
            (dc_yellow[0], dc_yellow[1], dc_yellow[2], 52),
            (dc_red[0], dc_red[1], dc_red[2], 48),
        ])
        draw.line([(x, y), (min(rw, x + length), y)], fill=color, width=random.choice([sx(1), sx(2)]))
    for base_y, phase, color, amplitude in [
        (height - 78, 2.4, (dc_blue[0], dc_blue[1], dc_blue[2], 82), 9),
        (height - 46, 0.9, (dc_teal[0], dc_teal[1], dc_teal[2], 70), 7),
    ]:
        points = []
        for x in range(-sx(20), rw + sx(25), sx(6)):
            y = int(sx(base_y) + sx(amplitude) * math.sin((x / scale) / 65.0 + phase))
            points.append((x, y))
        draw.line(points, fill=color, width=sx(2))

if os.path.exists(logo_path):
    logo = Image.open(logo_path).convert("RGBA")
    logo = ImageEnhance.Contrast(logo).enhance(1.04)
    pad = int(max(logo.size) * 0.30)
    padded_logo = Image.new("RGBA", (logo.width + pad * 2, logo.height + pad * 2), (0, 0, 0, 0))
    padded_logo.alpha_composite(logo, (pad, pad))
    logo = padded_logo
    target = int(rh * (0.58 if is_fun else 0.62))
    logo.thumbnail((target, target), Image.Resampling.LANCZOS)
    if is_fun:
        pos = (-int(logo.width * 0.48), -int(logo.height * 0.30))
        alpha = 224
    else:
        pos = (-int(logo.width * 0.46), -int(logo.height * 0.28))
        alpha = 230
    logo_alpha = logo.getchannel("A").point(lambda p: int(p * alpha / 255))
    logo.putalpha(logo_alpha)
    shadow_pad = sx(72)
    logo_layer = Image.new("RGBA", (logo.width + shadow_pad * 2, logo.height + shadow_pad * 2), (0, 0, 0, 0))
    logo_layer.alpha_composite(logo, (shadow_pad, shadow_pad))
    shadow_alpha = Image.new("L", logo_layer.size, 0)
    shadow_alpha.paste(logo_alpha, (shadow_pad, shadow_pad))
    shadow_alpha = shadow_alpha.filter(ImageFilter.GaussianBlur(sx(22)))
    fade_edge_x = sx(70)
    fade_edge_y = sx(62)
    fade_mask = Image.new("L", logo_layer.size, 255)
    fade_draw = ImageDraw.Draw(fade_mask)
    for x in range(fade_edge_x):
        a = int(255 * (x / max(fade_edge_x - 1, 1)))
        fade_draw.line([(x, 0), (x, logo_layer.size[1])], fill=a)
    for y in range(fade_edge_y):
        a = int(255 * (y / max(fade_edge_y - 1, 1)))
        fade_draw.line([(0, y), (logo_layer.size[0], y)], fill=a)
    shadow_alpha = ImageChops.multiply(shadow_alpha, fade_mask)
    shadow = Image.new("RGBA", logo_layer.size, (0, 0, 0, 0))
    shadow.putalpha(shadow_alpha.point(lambda p: int(p * 0.52)))
    layer_pos = (pos[0] - shadow_pad, pos[1] - shadow_pad)
    base.alpha_composite(shadow, (layer_pos[0] + sx(14), layer_pos[1] + sx(14)))
    base.alpha_composite(logo_layer, layer_pos)

title_font = load_font(36, bold=True)
subtitle_font = load_font(16, bold=False)
title_x = sx(170)
title_y = sx(50)
title_color = (98, 104, 113, 255) if not is_fun else (139, 203, 193, 255)
title_shadow = (0, 0, 0, 75) if not is_fun else (0, 0, 0, 210)
draw.text((title_x + sx(2), title_y + sx(2)), "Defcoin Core", font=title_font, fill=title_shadow)
draw.text((title_x, title_y), "Defcoin Core", font=title_font, fill=title_color)
if is_fun:
    draw.text((title_x + sx(4), title_y + sx(45)), "DC34 Edition", font=subtitle_font, fill=(0, 0, 0, 160))
    draw.text((title_x + sx(2), title_y + sx(43)), "DC34 Edition", font=subtitle_font, fill=(216, 201, 52, 240))

arrow_center_y = sx(icon_y - 4)
arrow_mid_x = sx((app_x + folder_x) / 2.0)
arrow_len = sx(max(110, (folder_x - app_x) - 104))
shaft_half_h = sx(6)
arrow_half_h = sx(16)
arrow_head = sx(24)
arrow_shadow = (0, 0, 0, 24)
arrow_fill = (95, 174, 255, 230)
arrow_points = [
    (arrow_mid_x - arrow_len // 2, arrow_center_y - shaft_half_h),
    (arrow_mid_x + arrow_len // 2 - arrow_head, arrow_center_y - shaft_half_h),
    (arrow_mid_x + arrow_len // 2 - arrow_head, arrow_center_y - arrow_half_h),
    (arrow_mid_x + arrow_len // 2, arrow_center_y),
    (arrow_mid_x + arrow_len // 2 - arrow_head, arrow_center_y + arrow_half_h),
    (arrow_mid_x + arrow_len // 2 - arrow_head, arrow_center_y + shaft_half_h),
    (arrow_mid_x - arrow_len // 2, arrow_center_y + shaft_half_h),
]
shadow_points = [(x + sx(3), y + sx(3)) for x, y in arrow_points]
draw.polygon(shadow_points, fill=arrow_shadow)
draw.polygon(arrow_points, fill=arrow_fill)

base = base.resize((width, height), Image.Resampling.LANCZOS)
base.save(path)
PY

DMGBUILD_PYTHON_BIN=""
DMG_BUILDER_MODE="${DEFCOIN_DMG_BUILDER:-auto}"
if [ "$DMG_BUILDER_MODE" = "auto" ] || [ "$DMG_BUILDER_MODE" = "dmgbuild" ]; then
  for candidate in "$PYTHON_BIN" python3 /Library/Frameworks/Python.framework/Versions/3.13/bin/python3 /opt/homebrew/bin/python3; do
    if command -v "$candidate" >/dev/null 2>&1 && "$candidate" -c 'import dmgbuild' >/dev/null 2>&1; then
      DMGBUILD_PYTHON_BIN="$candidate"
      break
    fi
  done
fi

if [ -n "$DMGBUILD_PYTHON_BIN" ]; then
  echo "+ Creating DMG with direct Finder metadata"
  DMG_SETTINGS="$STAGE_DIR/dmgbuild_settings.py"
  cat > "$DMG_SETTINGS" <<PY
format = 'UDZO'
compression_level = 9
filesystem = 'HFS+'
files = [(r'$STAGE_DIR/$APP_NAME', '$APP_NAME')]
symlinks = {'Applications': '/Applications'}
background = r'$STAGE_DIR/.background/background.png'
window_rect = ((100, 100), ($DEFCOIN_DMG_BG_WIDTH, $DEFCOIN_DMG_BG_HEIGHT))
default_view = 'icon-view'
show_toolbar = False
show_status_bar = False
show_sidebar = False
icon_size = $DEFCOIN_DMG_ICON_SIZE
text_size = 13
arrange_by = None
icon_locations = {
    '$APP_NAME': ($DEFCOIN_DMG_APP_ICON_X, $DEFCOIN_DMG_ICON_Y),
    'Applications': ($DEFCOIN_DMG_FOLDER_ICON_X, $DEFCOIN_DMG_ICON_Y),
}
PY
  "$DMGBUILD_PYTHON_BIN" -m dmgbuild -s "$DMG_SETTINGS" "$VOLUME_NAME" "$DMG_NAME"
else
  RW_DMG="${DMG_NAME%.dmg}.rw.dmg"
  MOUNT_DIR="$(mktemp -d /tmp/defcoin-dmg-mount.XXXXXX)"
  rm -f "$RW_DMG"
  hdiutil create -fs HFS+ -volname "$VOLUME_NAME" -srcfolder "$STAGE_DIR" -format UDRW -ov "$RW_DMG"
  DEV="$(hdiutil attach -readwrite -noverify -noautoopen -mountpoint "$MOUNT_DIR" "$RW_DMG" | awk '/Apple_HFS|Apple_APFS/ { print $1; exit }')"
  if [ -n "$DEV" ]; then
    osascript >/dev/null <<APPLESCRIPT || echo "Warning: Finder DMG layout customization failed; continuing with packaged hidden .background image." >&2
with timeout of 120 seconds
  tell application "Finder"
    activate
    tell disk "$VOLUME_NAME"
      open
      delay 1
      set dmgWindow to container window
      set current view of dmgWindow to icon view
      set toolbar visible of dmgWindow to false
      set statusbar visible of dmgWindow to false
      set bounds of dmgWindow to {100, 100, 100 + $DEFCOIN_DMG_BG_WIDTH, 100 + $DEFCOIN_DMG_BG_HEIGHT}
      set viewOptions to the icon view options of dmgWindow
      set arrangement of viewOptions to not arranged
      set icon size of viewOptions to $DEFCOIN_DMG_ICON_SIZE
      set text size of viewOptions to 13
      set background picture of viewOptions to file ".background:background.png"
      set position of item "$APP_NAME" to {$DEFCOIN_DMG_APP_ICON_X, $DEFCOIN_DMG_ICON_Y}
      set position of item "Applications" to {$DEFCOIN_DMG_FOLDER_ICON_X, $DEFCOIN_DMG_ICON_Y}
      update without registering applications
      delay 2
      close
      open
      delay 1
      set dmgWindow to container window
      update without registering applications
      delay 2
      close
    end tell
  end tell
end timeout
APPLESCRIPT
    if command -v SetFile >/dev/null 2>&1; then
      SetFile -a V "$MOUNT_DIR/.background" >/dev/null 2>&1 || true
    fi
    sync
    hdiutil detach "$DEV" >/dev/null
  fi
  rm -rf "$MOUNT_DIR"
  hdiutil convert "$RW_DMG" -format UDZO -imagekey zlib-level=9 -o "$DMG_NAME"
  rm -f "$RW_DMG"
fi
hdiutil verify "$DMG_NAME"

echo "+ Done: $ROOT_DIR/$DMG_NAME"
