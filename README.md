# dakara_check

## Usage

```sh
dakara_check <FILE|URL>
```

return code 0 means that the file passed all the tests.

## Compiling

To compile dakara_check you will need `meson` and `ffmpeg` (you must install the
libraries).

Then in the cloned repository:
```sh
meson setup build
meson compile -C build
```

dakara_check will be built in `build/dakara_check`
