# Syndicate Black Ops 1.38

The source of Syndicate Black Ops 1.38, a Half-Life mod, reconstructed to match the released `client.dll`, `hl.dll`
and `hl_i386.so` byte for byte.

## Building

- Windows: Visual C++ 6.0 (12.00.8168), the client with `cl_dll/cl_dll.dsp` ("Win32 Debug"), the server with the
  `dlls/mp.dsp` "Win32 Profile" flags and `dlls/hl.def`
- Linux: egcs 1.1.2 with the `dlls/Makefile` flags

## License

The Half-Life 1 SDK License, see [LICENSE](LICENSE).

SBO 2.0 is on the [`main` branch](https://github.com/syndicate-black-ops/syndicate-black-ops/tree/main).
