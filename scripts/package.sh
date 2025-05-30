#!/bin/bash

# Exit on error
set -e

# Configuration
VERSION=$(git describe --tags --always)
BUILD_DIR="build"
PACKAGE_DIR="package"
PLATFORM=$(uname -s)

# Create package directory
mkdir -p "${PACKAGE_DIR}"

# Function to create Windows package
create_windows_package() {
    echo "Creating Windows package..."
    
    # Create NSIS script
    cat > "${PACKAGE_DIR}/installer.nsi" << EOF
!include "MUI2.nsh"

Name "3D Game of Life Vulkan Edition"
OutFile "3DGameOfLife-Vulkan-${VERSION}-win64.exe"
InstallDir "\$PROGRAMFILES64\\3DGameOfLife-Vulkan"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetOutPath "\$INSTDIR"
    File /r "${BUILD_DIR}/bin/Release/*.*"
    File /r "${BUILD_DIR}/bin/Release/shaders"
    WriteUninstaller "\$INSTDIR\\uninstall.exe"
    
    CreateDirectory "\$SMPROGRAMS\\3DGameOfLife-Vulkan"
    CreateShortCut "\$SMPROGRAMS\\3DGameOfLife-Vulkan\\3D Game of Life.lnk" "\$INSTDIR\\3DGameOfLife.exe"
    CreateShortCut "\$SMPROGRAMS\\3DGameOfLife-Vulkan\\Uninstall.lnk" "\$INSTDIR\\uninstall.exe"
SectionEnd

Section "Uninstall"
    Delete "\$INSTDIR\\uninstall.exe"
    RMDir /r "\$INSTDIR"
    
    Delete "\$SMPROGRAMS\\3DGameOfLife-Vulkan\\3D Game of Life.lnk"
    Delete "\$SMPROGRAMS\\3DGameOfLife-Vulkan\\Uninstall.lnk"
    RMDir "\$SMPROGRAMS\\3DGameOfLife-Vulkan"
SectionEnd
EOF

    # Build installer using NSIS
    makensis "${PACKAGE_DIR}/installer.nsi"
}

# Function to create Linux package
create_linux_package() {
    echo "Creating Linux package..."
    
    # Create AppImage
    cat > "${PACKAGE_DIR}/AppRun" << EOF
#!/bin/bash
cd "\$(dirname "\$0")"
./3DGameOfLife "\$@"
EOF
    chmod +x "${PACKAGE_DIR}/AppRun"
    
    # Copy files
    cp -r "${BUILD_DIR}/bin/3DGameOfLife" "${PACKAGE_DIR}/"
    cp -r "${BUILD_DIR}/bin/shaders" "${PACKAGE_DIR}/"
    
    # Create AppImage
    appimagetool "${PACKAGE_DIR}" "3DGameOfLife-Vulkan-${VERSION}-x86_64.AppImage"
}

# Main packaging logic
case "${PLATFORM}" in
    "MINGW"*|"MSYS"*|"CYGWIN"*)
        create_windows_package
        ;;
    "Linux")
        create_linux_package
        ;;
    *)
        echo "Unsupported platform: ${PLATFORM}"
        exit 1
        ;;
esac

echo "Packaging complete!" 