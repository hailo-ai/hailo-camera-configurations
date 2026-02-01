#!/bin/bash


# Configuration variables
BUILD_TARGET="${BUILD_TARGET:-arm-yocto}"  # Options: x86, arm-yocto
BUILD_DIR="${BUILD_DIR:-builddir}"
LIB_NAME="${LIB_NAME:-librule_checker.so.1.0.0}"
YOCTO_SDK_PATH="${YOCTO_SDK_PATH:-/opt/poky/4.0.23}"
SYSROOT_BASE="${SYSROOT_BASE:-${YOCTO_SDK_PATH}/sysroots/armv8a-poky-linux}"
YOCTO_ENV_SCRIPT="${YOCTO_ENV_SCRIPT:-${YOCTO_SDK_PATH}/environment-setup-armv8a-poky-linux}"
TARGET_IP="${TARGET_IP:-10.0.0.1}"
TARGET_USER="root"
SCRIPT_PATH="$(readlink -f "$0")"           # Get absolute script path
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"      # Get script directory
WORKSPACE_DIR="${WORKSPACE_DIR:-$(dirname "$(dirname "$SCRIPT_DIR")")}" # Go up TWO levels from script location
INCLUDE_DIR="${INCLUDE_DIR:-${WORKSPACE_DIR}/validation/rule_checker_lib/include}"

# Parse command line arguments
COPY_TO_SYSROOT=true
COPY_TO_TARGET=true
CLEAN_BUILD=false

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  -t, --target TARGET      Build target (x86 or arm-yocto, default: arm-yocto)"
    echo "  -c, --clean              Clean build directory before building"
    echo "  -s, --skip-sysroot       Skip copying to sysroot"
    echo "  -d, --skip-deploy        Skip deploying to target device"
    echo "  -i, --ip IP              Target device IP (default: 10.0.0.1)"
    echo "  -p, --sdk-path PATH      Yocto SDK path (default: /opt/poky/4.0.23)"
    echo "  -h, --help               Show this help message"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--target)
            BUILD_TARGET="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -s|--skip-sysroot)
            COPY_TO_SYSROOT=false
            shift
            ;;
        -d|--skip-deploy)
            COPY_TO_TARGET=false
            shift
            ;;
        -i|--ip)
            TARGET_IP="$2"
            shift 2
            ;;
        -p|--sdk-path)
            YOCTO_SDK_PATH="$2"
            SYSROOT_BASE="${YOCTO_SDK_PATH}/sysroots/armv8a-poky-linux"
            YOCTO_ENV_SCRIPT="${YOCTO_SDK_PATH}/environment-setup-armv8a-poky-linux"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Validate build target
if [[ "$BUILD_TARGET" != "x86" && "$BUILD_TARGET" != "arm-yocto" ]]; then
    echo "Error: Invalid build target '$BUILD_TARGET'. Must be 'x86' or 'arm-yocto'"
    exit 1
fi

# Automatically disable sysroot and deploy options for x86 target
if [ "$BUILD_TARGET" = "x86" ]; then
    if [ "$COPY_TO_SYSROOT" = true ]; then
        echo "Note: Sysroot copying is not applicable for x86 target. Disabling it."
        COPY_TO_SYSROOT=false
    fi
    if [ "$COPY_TO_TARGET" = true ]; then
        echo "Note: Deploying to target device is not applicable for x86 target. Disabling it."
        COPY_TO_TARGET=false
    fi
fi

echo "=== Build Configuration ==="
echo "Build Target: $BUILD_TARGET"
echo "Build Directory: $BUILD_DIR"
echo "Clean Build: $CLEAN_BUILD"
echo "Copy to Sysroot: $COPY_TO_SYSROOT"
echo "Copy to Target: $COPY_TO_TARGET"
echo "Target Device: ${TARGET_USER}@${TARGET_IP}"
echo "Yocto SDK Path: $YOCTO_SDK_PATH"
echo "=========================="

# Source Yocto environment for cross-compilation
if [ "$BUILD_TARGET" = "arm-yocto" ]; then
    echo ""
    echo "=== Setting up Yocto Cross-Compilation Environment ==="
    if [ -f "$YOCTO_ENV_SCRIPT" ]; then
        echo "Sourcing: $YOCTO_ENV_SCRIPT"
        source "$YOCTO_ENV_SCRIPT"
        
        # Verify environment was loaded successfully
        if [ -z "$CC" ] || [ -z "$SDKTARGETSYSROOT" ]; then
            echo ""
            echo "✗ Error: Yocto environment failed to load properly"
            echo "  CC (compiler) is not set"
            echo "  SDKTARGETSYSROOT is not set"
            echo ""
            echo "  This is likely due to LD_LIBRARY_PATH being set."
            echo "  Run: unset LD_LIBRARY_PATH"
            echo "  Then try again."
            exit 1
        fi
        
        echo "✓ Yocto environment loaded successfully"
        echo "  Toolchain: $CC"
        echo "  Sysroot: $SDKTARGETSYSROOT"
    else
        echo "✗ Error: Yocto environment script not found at $YOCTO_ENV_SCRIPT"
        echo "Please set YOCTO_ENV_SCRIPT environment variable or use -p flag"
        exit 1
    fi
    echo "======================================================="
    echo ""
fi

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Setup meson build
echo "Setting up build for $BUILD_TARGET..."
if ! meson setup "$BUILD_DIR" --native-file "build-presets/${BUILD_TARGET}.ini"; then
    echo "Error: Meson setup failed"
    exit 1
fi

# Build with ninja
echo "Building..."
if ! ninja -C "$BUILD_DIR"; then
    echo "Error: Build failed"
    exit 1
fi

# Check if library was built
if [ ! -f "${BUILD_DIR}/${LIB_NAME}" ]; then
    echo "Error: Library ${LIB_NAME} not found in ${BUILD_DIR}"
    exit 1
fi

echo "Build successful!"

# Copy to sysroot (only for cross-compilation)
if [ "$COPY_TO_SYSROOT" = true ] && [ "$BUILD_TARGET" = "arm-yocto" ]; then
    echo "Copying to sysroot..."
    
    if [ ! -d "$SYSROOT_BASE" ]; then
        echo "Warning: Sysroot directory $SYSROOT_BASE does not exist"
    else
        # Copy library
        echo "  Copying library to ${SYSROOT_BASE}/usr/lib/"
        cp "${BUILD_DIR}/${LIB_NAME}" "${SYSROOT_BASE}/usr/lib/" || echo "Warning: Failed to copy library to sysroot"
        
        # Copy headers
        if [ -d "$INCLUDE_DIR" ]; then
            echo "  Copying headers to ${SYSROOT_BASE}/usr/include/rule_checker/"
            mkdir -p "${SYSROOT_BASE}/usr/include/rule_checker/"
            cp "${INCLUDE_DIR}"/* "${SYSROOT_BASE}/usr/include/rule_checker/" || echo "Warning: Failed to copy headers to sysroot"
        fi
    fi
fi

# Deploy to target device
if [ "$COPY_TO_TARGET" = true ] && [ "$BUILD_TARGET" = "arm-yocto" ]; then
    echo "Deploying to target device ${TARGET_USER}@${TARGET_IP}..."
    
    if ! scp "${BUILD_DIR}/${LIB_NAME}" "${TARGET_USER}@${TARGET_IP}:/usr/lib/"; then
        echo "Warning: Failed to copy library to target device"
    else
        echo "Successfully deployed to target device"
    fi
fi

echo "=== Done ==="