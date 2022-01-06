# Ipcgull
Ipcgull is a C++ IPC library that takes advantage of modern C++17 features to
provide a simple interface for developers to handle IPC.

Currently, Ipcgull only supports a D-Bus backend (via GDBus), but this is
abstracted by the library and can theoretically be replaced. However, that is
out of scope for this project.

## Documentation
This project was originally designed solely for use in
[LogiOps](https://github.com/PixlOne/logiops), and as such, it is not currently
documented. This is planned for the future though.

## Building

To build this project, run:

```
mkdir build
cd build
cmake ..
make
```

To install, run `sudo make install`. Although, it is recommended that ipcgull
is installed through a package manager if possible.
