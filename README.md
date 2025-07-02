# reMarkable Tablet Driver
This userspace driver for Linux is able to simulate the real tablet input from your reMarkable Paper Tablet on your PC, with pressure sensitivity.

# Demo
[demo.webm](https://github.com/user-attachments/assets/274d3eca-30fb-4983-9589-b4b68bbfb49e)

# Building
You can build the project using CMake:

```sh
mkdir build
cd build
cmake ..
make
```

This will generate the `tabletDriver` binary in the `build` directory. Ensure you have `libssh` and its development headers installed, as well as `pkg-config`.

# Installation

To install the built binary, run:

```
sudo make install
```

This will copy the `tabletDriver` binary to `/usr/local/bin`.

If you want to change the install location, you can specify it when running cmake:

```
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local/bin ..
```

Then run `make install` as usual.

# Command Line Arguments
```
Usage: tabletDriver [OPTION...]
reMarkable Tablet Driver -- a driver for using your reMarkable tablet as pen
input

  -a, --address=ADDRESS      Address of reMarkable tablet. Default is
                             10.11.99.1.
  -k, --key=FILE             Private key used for authentication.
  -o, --orientation=ORIENTATION   Tablet orientation. Valid options are top,
                             left, right, bottom, corresponding to the position
                             of the tablet buttons. Case sensitive! Default is
                             right.
  -p, --port=PORT            Port for ssh connection. Default is 22.
  -t, --threshold=THRESHOLD  Pen pressure threshold. Default is 600.
  -v, --verbose              Produce verbose output.
  -?, --help                 Give this help list
      --usage                Give a short usage message

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to https://github.com/FreeCap23/reMarkable-tablet-driver/issues.
```

# Limitations
I only tested this driver in KDE Plasma 6.4 under Wayland. It might work on X11, but it hasn't been tested.

Also, I'm not sure if it works on the reMarkable 2 or the Pro. I only have the rm1 to test. If you have an rm2 or pro, you're welcome to try it and submit an issue if it doesn't work.

It is expected you will use plasma settings' (or an equivalent) drawing tablet configuration to get the orientation and the active region right for you. 

# How does this differ from LinusCDE's and Evidlo's drivers?
First of all, they're both good drivers. I've used both in the past. However, there are two main problems that this fork solves:
- LinusCDE's driver requires two files to be run. One on the tablet itself, and one on your computer. I didn't like having to run two files every time I wanted to use the tablet.
- Evidlo's driver doesn't work on Wayland and probably never will, since it uses Python's libevdev library. There is another uinput library on Python, but I tried using it and the virtual device ends up being ignored, maybe as a safety feature.


# Credits
Thanks to [LinusCDE](https://github.com/LinusCDE/rmTabletDriver) for the base of the driver, which this driver is forked from.

Thanks to [Evidlo](https://github.com/Evidlo/remarkable_mouse) for inspiration on how this driver should operate, and their well documented and very readable code.
