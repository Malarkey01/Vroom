# Project Dependencies

## Compile and Run:

``` bash
gcc -o VroomSystem \
    main.c MainWindow.c SettingsWindow.c VehicleInfoWindow.c \
    AudioManager.c BacklightManager.c Popup.c RotaryEncoder.c \
    `pkg-config --cflags --libs gtk+-3.0 json-glib-1.0` \
    -lwiringPi && ./VroomSystem
```

## OBD Library:

Python OBD Library: https://github.com/brendan-w/python-OBD

## Intallation steps:
``` bash
sudo apt update && sudo apt upgrade -y
sudo apt-get install libgtk-3-dev
```

## Rebuild OpenAuto:

``` bash
cd ~/OpenAuto/build
sudo make install
sudo ldconfig
```

## OpenAuto installation guide:
### Update the Raspberry Pi & Basic Build Tools

Start by ensuring you have a fully updated system with development essentials:

``` bash
sudo apt-get update
sudo apt-get upgrade -y
sudo apt-get install -y git build-essential cmake pkg-config
```
### Install Dependencies for OpenAuto & AASDK

* OpenAuto and AASDK rely heavily on Boost, Protobuf, libusb, OpenSSL, and Qt5:
``` bash
sudo apt-get install -y \
    libboost-all-dev \
    libusb-1.0-0-dev \
    libssl-dev \
    libprotobuf-dev \
    protobuf-compiler \
    qtbase5-dev \
    qtmultimedia5-dev \
    qtdeclarative5-dev \
    qtdeclarative5-dev-tools \
    qml-module-qtquick-controls2 \
    qttools5-dev \
    qttools5-dev-tools \
    libsdl2-dev \
    libtag1-dev \
    libqt5websockets5-dev \
    libqt5serialport5-dev \
    libqt5serialbus5-dev \
    libkf5bluezqt-dev
```
### Install AASDK (Patched to Remove Deprecated OpenSSL Calls)

Originally, the classic AASDK code references old OpenSSL calls like FIPS_mode_set(0), which fails on OpenSSL 3.0 (the default in Debian 12/Bookworm). Also, it references private boost::asio::detail classes that can cause compilation errors on newer Boost. We ended up using a version of AASDK that we patched so that it no longer calls FIPS_mode_set(0), or we removed those lines in src/Transport/SSLWrapper.cpp.

### Clone AASDK

``` bash
cd ~
git clone https://github.com/f1xpl/aasdk.git
cd aasdk
```

### Patch out FIPS_mode_set(0) and ERR_load_BIO_strings()

OpenSSL 3.0 removed them. In src/Transport/SSLWrapper.cpp, look for lines like:

``` c
ERR_load_BIO_strings();
FIPS_mode_set(0);
```
Comment out or remove those lines.

### Build and Install AASDK

``` bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 ..
make -j4
sudo make install
sudo ldconfig
```

## Install H264bitstream

OpenAuto tries to link with -lh264bitstream, but Debian 12/Bookworm doesn’t provide a libh264bitstream-dev package. So we built it from source:

``` bash
cd ~
git clone https://github.com/aizvorski/h264bitstream.git
cd h264bitstream
mkdir build
cd build
cmake ..
make -j4
sudo make install
sudo ldconfig
```

## Build & Install OpenAuto (Patched if Needed)

### Clone OpenAuto

``` bash
cd ~
git clone https://github.com/f1xpl/OpenAuto.git
cd OpenAuto
```

### Patch OpenAuto to Actually Exit When “Exit” Is Pressed (If Desired)

In original OpenAuto, pressing Android Auto’s built-in “Exit” on the phone might just disconnect from the phone but not kill autoapp. If we want it to truly exit, we can modify AndroidAutoEntity::stop() or AndroidAutoEntity::destroy() so it calls std::exit(0) (or QCoreApplication::exit(0)).

For example, a typical patch might be in AndroidAutoEntity.cpp:

``` cpp
void AndroidAutoEntity::stop(){

    // ... original teardown ...

    std::exit(0); // force the entire process to end

}
```
### Check for a GStreamer or Qt Multimedia backend

OpenAuto typically uses Qt’s QMediaPlayer or GStreamer for video. We installed Qt Multimedia packages and GStreamer plugin sets:

``` bash
sudo apt-get install -y \
  qtmultimedia5-dev \
  qml-module-qtmultimedia \
  libqt5multimedia5-plugins \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-libav
```

(We did this so that we don’t see “no service found for org.qt-project.qt.mediaplayer” and end up with a white screen.)

### Build OpenAuto

Inside ~/OpenAuto:

``` bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 ..
make -j4
```

### (Optional) sudo make install

``` bash
sudo make install
sudo ldconfig
```