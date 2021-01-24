# ######################
# # Appveyor side:
#
# # Setup System
# # Core update (in case any core packages are outdated)
# C:\msys64\usr\bin\bash -lc "pacman --noconfirm -Syuu"
# # Normal update
# C:\msys64\usr\bin\bash -lc "pacman --noconfirm -Syuu"
#
# # Setup Repo
# $env:CHERE_INVOKING='yes'  # Preserve the current working directory
# $env:MSYSTEM='MINGW64' # Start a 64 bit Mingw environment
# mkdir build
# cd build
#
# # Build
# C:\msys64\usr\bin\bash -lc "../deploy/appveyor.sh 2>&1"
#
# ######################

set -xe
pacman --noconfirm -S \
    git zip unzip \
    mingw-w64-x86_64-toolchain \
    mingw-w64-x86_64-qt5 \
    mingw-w64-x86_64-zlib \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-python \
    mingw-w64-x86_64-potrace \
    mingw-w64-x86_64-ffmpeg \
    mingw-w64-x86_64-libimagequant

git submodule update --init --recursive
(cd ../data/icons/breeze-icons/ && git config core.symlinks true && git reset --hard)
# find data/icons/breeze-icons/ -name '*@?x' -exec rm -rf {} \;

env

if [ -d /c/Python39-x64 ]
then
    mv /c/Python39-x64 /c/gtfo9
fi

if [ -d /c/Python38-x64 ]
then
    mv /c/Python38-x64 /c/gtfo8
fi

# Build
cmake.exe .. \
    -DQt5_DIR=/mingw64/lib/cmake/Qt5 \
    -DZLIB_LIBRARY=/mingw64/lib/libz.a \
    -DCMAKE_PREFIX_PATH='/mingw64/lib/' \
    -DZLIB_INCLUDE_DIR=/mingw64/include \
    -DPython3_PREFIX=/mingw64/ \
    -DPython3_LIBRARIES=/mingw64/bin/libpython3.8.dll \
    -DPython3_EXECUTABLE=/mingw64/bin/python3 \
    -G 'MSYS Makefiles' \
    -DCMAKE_INSTALL_PREFIX=''
mingw32-make.exe -j2

# Setup package
mingw32-make.exe translations
mingw32-make.exe install DESTDIR=glaxnimate
windeployqt.exe glaxnimate/bin/glaxnimate.exe
cp /mingw64/bin/*.dll glaxnimate/bin
cp ./external/Qt-Color-Widgets/libQtColorWidgets.dll glaxnimate/bin
cp ../deploy/glaxnimate.vbs glaxnimate

# PyPI
if [ "$APPVEYOR_REPO_TAG" = true ]
then
    pacman --noconfirm -S mingw-w64-x86_64-python-pip
    pip.exe install wheel
    cmake.exe .. -DVERSION_SUFFIX=""
    mingw32-make.exe glaxnimate_python_depends_install
    mingw32-make.exe glaxnimate_python
    (cd py_module && ./setup.py build --compiler=unix bdist_wheel && cd ..)
    mingw32-make.exe glaxnimate_python_upload
fi

# Create Artifacts
zip -r glaxnimate-x86_64.zip glaxnimate
sha1sum glaxnimate-x86_64.zip >checksum.txt

if [ \( "$APPVEYOR_REPO_BRANCH" = master -o "$APPVEYOR_REPO_BRANCH" = pre-release -o "$APPVEYOR_REPO_BRANCH" = release -o "$APPVEYOR_REPO_TAG" = true \) -a "$APPVEYOR_PULL_REQUEST_NUMBER" = "" ]
then
    path="$APPVEYOR_REPO_BRANCH"
    if [ "$APPVEYOR_REPO_TAG" = true ]
    then
        path="$APPVEYOR_REPO_TAG_NAME"
    fi
    ../deploy/gitlab_upload.py glaxnimate-x86_64.zip "$path/Win/glaxnimate-x86_64.zip"
    ../deploy/gitlab_upload.py checksum.txt "$path/Win/checksum.txt"
fi
