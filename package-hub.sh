#!/bin/bash
# package-hub.sh — Build and package glue-hub releases
#
# Produces:
#   glue-hub-linux-x86_64.AppImage   (Linux)
#   glue-hub-windows-x86_64.zip      (Windows, cross-compiled with llvm-mingw + Qt6)
#
# Usage: bash package-hub.sh [linux|windows|all]
#   default: all

set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$SCRIPT_DIR/release"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[hub-pkg]${NC} $*"; }
warn()  { echo -e "${YELLOW}[hub-pkg]${NC} $*"; }
error() { echo -e "${RED}[hub-pkg]${NC} $*" >&2; exit 1; }

TARGET="${1:-all}"
mkdir -p "$OUT_DIR"

# ══════════════════════════════════════════════════════════════════════════════
# LINUX AppImage
# ══════════════════════════════════════════════════════════════════════════════
build_linux() {
    info "Building Linux release…"

    local BUILD_DIR="$SCRIPT_DIR/build-release"

    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++

    cmake --build "$BUILD_DIR" -j "$(nproc)"

    [ -f "$BUILD_DIR/glue-hub" ] || error "Build failed — glue-hub not found"

    # Strip debug symbols
    local STRIPPED="$BUILD_DIR/glue-hub-stripped"
    strip -o "$STRIPPED" "$BUILD_DIR/glue-hub"

    info "Building AppImage…"

    # Download appimagetool if not present (reuse from reNut if available)
    local APPIMAGETOOL="$SCRIPT_DIR/appimagetool-x86_64.AppImage"
    if [ ! -x "$APPIMAGETOOL" ]; then
        local RENUT_TOOL="$(dirname "$SCRIPT_DIR")/reNut/appimagetool-x86_64.AppImage"
        if [ -x "$RENUT_TOOL" ]; then
            APPIMAGETOOL="$RENUT_TOOL"
        else
            info "Downloading appimagetool…"
            curl -sSL "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" \
                -o "$APPIMAGETOOL"
            chmod +x "$APPIMAGETOOL"
        fi
    fi

    # Build AppDir
    local APPDIR="$BUILD_DIR/AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR/usr/bin"
    mkdir -p "$APPDIR/usr/lib"
    mkdir -p "$APPDIR/usr/lib/qt6/plugins"
    mkdir -p "$APPDIR/usr/share/applications"
    mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

    # Binary
    cp "$STRIPPED" "$APPDIR/usr/bin/glue-hub"
    chmod +x "$APPDIR/usr/bin/glue-hub"

    # .desktop
    cat > "$APPDIR/usr/share/applications/glue-hub.desktop" <<'DESKTOP'
[Desktop Entry]
Name=Glue Hub
Exec=glue-hub
Icon=glue-hub
Type=Application
Categories=Game;Utility;
DESKTOP
    ln -sf usr/share/applications/glue-hub.desktop "$APPDIR/glue-hub.desktop"

    # Icon — use a gamerpic as placeholder
    local ICON_SRC
    ICON_SRC=$(find "$SCRIPT_DIR/gamerpics" -name "*.png" 2>/dev/null | sort | head -1)
    if [ -n "$ICON_SRC" ]; then
        if command -v magick &>/dev/null; then
            magick "$ICON_SRC" -resize 256x256 "$APPDIR/usr/share/icons/hicolor/256x256/apps/glue-hub.png"
        elif command -v convert &>/dev/null; then
            convert "$ICON_SRC" -resize 256x256 "$APPDIR/usr/share/icons/hicolor/256x256/apps/glue-hub.png"
        else
            cp "$ICON_SRC" "$APPDIR/usr/share/icons/hicolor/256x256/apps/glue-hub.png"
        fi
    else
        printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x01\x00\x00\x00\x01\x00\x08\x02\x00\x00\x00\xd3\x10?1\x00\x00\x00\x12IDATx\x9cc\xf8\x0f\x00\x00\x01\x01\x00\x05\x18\xd8N\x00\x00\x00\x00IEND\xaeB`\x82' \
            > "$APPDIR/usr/share/icons/hicolor/256x256/apps/glue-hub.png"
    fi
    ln -sf usr/share/icons/hicolor/256x256/apps/glue-hub.png "$APPDIR/glue-hub.png"
    ln -sf glue-hub.png "$APPDIR/.DirIcon"

    # Bundle shared libs (exclude system/GPU libs that must come from host)
    info "Bundling shared libraries…"
    local EXCLUDE_PATTERNS=(
        'linux-vdso'
        'ld-linux'
        'libc\.so'
        'libm\.so'
        'libdl\.so'
        'librt\.so'
        'libpthread\.so'
        'libresolv\.so'
        'libnss_'
        'libGLX\.so'
        'libGL\.so'
        'libEGL\.so'
        'libOpenGL\.so'
        'libGLdispatch\.so'
        'libGLESv2\.so'
        'libvulkan\.so'
        'libnvidia'
        'libdrm\.so'
        'libasound\.so'
        'libpulse'
        'libpipewire'
    )
    local EXCLUDE_RE
    EXCLUDE_RE=$(printf '%s|' "${EXCLUDE_PATTERNS[@]}")
    EXCLUDE_RE="${EXCLUDE_RE%|}"

    ldd "$BUILD_DIR/glue-hub" 2>/dev/null \
        | grep '=> /' \
        | grep -vE "$EXCLUDE_RE" \
        | awk '{print $3}' \
        | sort -u \
        | while read -r lib; do
            [ -f "$lib" ] && cp -Ln "$lib" "$APPDIR/usr/lib/" 2>/dev/null || true
        done

    # Qt6 platform plugins (not in ldd output but needed at runtime)
    local QT6_LIBDIR
    QT6_LIBDIR=$(ldd "$BUILD_DIR/glue-hub" | awk '/libQt6Core/{print $3}' | xargs dirname 2>/dev/null || true)
    local QT6_PLUGDIR=""
    for candidate in \
        "${QT6_LIBDIR}/qt6/plugins" \
        "${QT6_LIBDIR}/../lib/qt6/plugins" \
        "/usr/lib/qt6/plugins" \
        "/usr/lib/x86_64-linux-gnu/qt6/plugins"; do
        if [ -d "$candidate/platforms" ]; then
            QT6_PLUGDIR="$candidate"
            break
        fi
    done

    if [ -n "$QT6_PLUGDIR" ]; then
        mkdir -p "$APPDIR/usr/lib/qt6/plugins/platforms"
        for plug in libqxcb.so libqwayland-egl.so libqwayland-generic.so libqoffscreen.so; do
            [ -f "$QT6_PLUGDIR/platforms/$plug" ] && \
                cp -L "$QT6_PLUGDIR/platforms/$plug" "$APPDIR/usr/lib/qt6/plugins/platforms/" || true
        done
        if [ -d "$QT6_PLUGDIR/xcbglintegrations" ]; then
            mkdir -p "$APPDIR/usr/lib/qt6/plugins/xcbglintegrations"
            cp -L "$QT6_PLUGDIR/xcbglintegrations/"*.so "$APPDIR/usr/lib/qt6/plugins/xcbglintegrations/" 2>/dev/null || true
        fi
        if [ -d "$QT6_PLUGDIR/wayland-shell-integration" ]; then
            mkdir -p "$APPDIR/usr/lib/qt6/plugins/wayland-shell-integration"
            cp -L "$QT6_PLUGDIR/wayland-shell-integration/"*.so "$APPDIR/usr/lib/qt6/plugins/wayland-shell-integration/" 2>/dev/null || true
        fi
        info "  Qt6 plugins from $QT6_PLUGDIR"
    else
        warn "Qt6 plugins directory not found — xcb/wayland support may be missing"
    fi

    info "  Bundled $(ls "$APPDIR/usr/lib/" | wc -l) libraries"

    # AppRun
    cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/lib/qt6/plugins"
exec "${HERE}/usr/bin/glue-hub" "$@"
APPRUN
    chmod +x "$APPDIR/AppRun"

    local APPIMAGE="$OUT_DIR/glue-hub-linux-x86_64.AppImage"
    ARCH=x86_64 "$APPIMAGETOOL" --no-appstream "$APPDIR" "$APPIMAGE"

    info "Linux AppImage: $APPIMAGE"
    ls -lh "$APPIMAGE"
}

# ══════════════════════════════════════════════════════════════════════════════
# WINDOWS ZIP (cross-compiled: llvm-mingw + Qt6 for Windows)
# ══════════════════════════════════════════════════════════════════════════════
build_windows() {
    info "Building Windows release (cross-compile)…"

    local LLVM_MINGW="/opt/llvm-mingw"
    [ -d "$LLVM_MINGW" ] || error "llvm-mingw not found at $LLVM_MINGW"

    # Find Qt6 for Windows (downloaded via aqt install-qt windows desktop 6.10.3 win64_llvm_mingw)
    local QT6_WIN=""
    for candidate in \
        "$HOME/Qt/6.10.3/llvm-mingw_64" \
        "$HOME/Qt/6.7.3/llvm-mingw_64" \
        "$HOME/Qt/6.10.3/win64_llvm_mingw" \
        "$HOME/Qt/6.7.3/win64_llvm_mingw"; do
        if [ -f "$candidate/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            QT6_WIN="$candidate"
            break
        fi
    done
    [ -n "$QT6_WIN" ] || error "Qt6 for Windows not found. Run: aqt install-qt windows desktop 6.10.3 win64_llvm_mingw -O ~/Qt"

    # Find matching Qt6 host tools (must be same version as QT6_WIN)
    local QT6_WIN_VER
    QT6_WIN_VER=$(basename "$(dirname "$QT6_WIN")")
    local QT6_HOST=""
    for candidate in \
        "$HOME/Qt/${QT6_WIN_VER}/gcc_64" \
        "$HOME/Qt/${QT6_WIN_VER}/linux_gcc_64"; do
        if [ -f "$candidate/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            QT6_HOST="$candidate"
            break
        fi
    done
    [ -n "$QT6_HOST" ] || error "Qt6 Linux host tools for v${QT6_WIN_VER} not found. Run: aqt install-qt linux desktop ${QT6_WIN_VER} linux_gcc_64 -O ~/Qt"

    info "  Qt6 Windows: $QT6_WIN"
    info "  Qt6 host:    $QT6_HOST"

    local BUILD_DIR="$SCRIPT_DIR/build-win-release"
    mkdir -p "$BUILD_DIR"

    # Write toolchain file
    local TOOLCHAIN="$BUILD_DIR/toolchain-llvm-mingw.cmake"
    cat > "$TOOLCHAIN" <<TOOLCHAIN_EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   ${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER  ${LLVM_MINGW}/bin/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
TOOLCHAIN_EOF

    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DCMAKE_PREFIX_PATH="$QT6_WIN" \
        -DCMAKE_FIND_ROOT_PATH="$QT6_WIN;${LLVM_MINGW}/x86_64-w64-mingw32" \
        -DQT_HOST_PATH="$QT6_HOST" \
        -DQT_HOST_PATH_CMAKE_DIR="$QT6_HOST/lib/cmake"

    cmake --build "$BUILD_DIR" -j "$(nproc)"

    [ -f "$BUILD_DIR/glue-hub.exe" ] || error "Windows build failed"

    info "Packaging Windows zip…"

    local STAGING="$BUILD_DIR/glue-hub-windows-x86_64"
    rm -rf "$STAGING"
    mkdir -p "$STAGING"

    cp "$BUILD_DIR/glue-hub.exe" "$STAGING/"

    # extract-xiso Windows binary (for ISO extraction on Windows)
    if [ -f "$SCRIPT_DIR/extract-xiso.exe" ]; then
        cp "$SCRIPT_DIR/extract-xiso.exe" "$STAGING/"
    else
        warn "extract-xiso.exe not found — ISO extraction will not work on Windows"
        warn "Cross-compile from https://github.com/XboxDev/extract-xiso and place at $SCRIPT_DIR/extract-xiso.exe"
    fi

    # Qt6 DLLs
    for dll in Qt6Core Qt6Gui Qt6Widgets Qt6Network; do
        [ -f "$QT6_WIN/bin/${dll}.dll" ] && cp "$QT6_WIN/bin/${dll}.dll" "$STAGING/" || true
    done

    # Platform plugin
    mkdir -p "$STAGING/platforms"
    [ -f "$QT6_WIN/plugins/platforms/qwindows.dll" ] && \
        cp "$QT6_WIN/plugins/platforms/qwindows.dll" "$STAGING/platforms/" || true

    # Image format plugins (jpeg, png, svg, gif)
    mkdir -p "$STAGING/imageformats"
    for fmt in qjpeg qgif qsvg qico; do
        [ -f "$QT6_WIN/plugins/imageformats/${fmt}.dll" ] && \
            cp "$QT6_WIN/plugins/imageformats/${fmt}.dll" "$STAGING/imageformats/" || true
    done

    # TLS plugin — use Windows SChannel (no OpenSSL needed)
    mkdir -p "$STAGING/tls"
    [ -f "$QT6_WIN/plugins/tls/qschannelbackend.dll" ] && \
        cp "$QT6_WIN/plugins/tls/qschannelbackend.dll" "$STAGING/tls/" || true
    [ -f "$QT6_WIN/plugins/tls/qcertonlybackend.dll" ] && \
        cp "$QT6_WIN/plugins/tls/qcertonlybackend.dll" "$STAGING/tls/" || true

    # llvm-mingw runtime DLLs
    local MINGW_BIN="$LLVM_MINGW/x86_64-w64-mingw32/bin"
    for dll in libc++.dll libunwind.dll; do
        [ -f "$MINGW_BIN/$dll" ] && cp "$MINGW_BIN/$dll" "$STAGING/" || true
    done

    # qt.conf so Qt finds plugins in subdirs next to the exe
    cat > "$STAGING/qt.conf" <<'QTCONF'
[Paths]
Plugins = .
QTCONF

    # Any DLLs built alongside the exe
    find "$BUILD_DIR" -maxdepth 3 -name "*.dll" \
        ! -path "$STAGING/*" \
        -exec cp {} "$STAGING/" \; 2>/dev/null || true

    local ZIPFILE="$OUT_DIR/glue-hub-windows-x86_64.zip"
    rm -f "$ZIPFILE"
    (cd "$(dirname "$STAGING")" && zip -r "$ZIPFILE" "$(basename "$STAGING")")

    info "Windows zip: $ZIPFILE"
    ls -lh "$ZIPFILE"
}

# ══════════════════════════════════════════════════════════════════════════════
# Main
# ══════════════════════════════════════════════════════════════════════════════
case "$TARGET" in
    linux)   build_linux ;;
    windows) build_windows ;;
    all)     build_linux; build_windows ;;
    *)       error "Usage: $0 [linux|windows|all]" ;;
esac

info "Release packages in: $OUT_DIR/"
ls -lh "$OUT_DIR/"
