# 💬 2srt2ass++

Merge two SRT subtitle files into an ASS file for dual language subtitles.

## ⭐ Features

 - 🔄 Character encoding conversion (see `--t-enc`, `--b-enc`, and `--o-enc`).
 - ⏱️ Manual time shifting.
 - 🦺 Manual synchronization based on two given subtitle indices (e.g., 'synchronize Dutch subtitle number 5 with English subtitle number 7').
 - 🪄 Automatic time shifting, by letting 2srt2ass++ guess the correct alignment of the top SRT file to match up with the bottom SRT file.

## 🔨 Build
Depends on GNU `libiconv` to do character conversion.
Then run `make` to compile this program.

## ❓ Synopsis

```
Usage: ./2srt2ass++ [--help] [--version] --bottom VAR [--bottom-enc VAR] [--bottom-tshift VAR] --top VAR [--top-enc VAR] [--top-tshift VAR] [--sync-top-to-bottom VAR...] [--auto-sync-top-to-bottom] --output VAR [--o-enc VAR]

Optional arguments:
  -h, --help                                 shows help message and exits 
  -v, --version                              prints version information and exits 
  -b, --bottom                               SRT file for the bottom subtitles file. [required]
  --b-enc, --bottom-enc                      Encoding of the bottom SRT file. [nargs=0..1] [default: "UTF-8"]
  --b-shift, --bottom-tshift                 Time shift the bottom subtitles 
  -t, --top                                  SRT file for the top subtitles file. [required]
  --t-enc, --top-enc                         Encoding of the top SRT file. [nargs=0..1] [default: "UTF-8"]
  --t-shift, --top-tshift                    Time shift the top subtitles 
  --sync-tb, --sync-top-to-bottom            Time synchronize the [arg-0]th subtitle entry of the top SRT file to the [arg-1]th subtitle entry of the bottom SRT file. [nargs: 2] 
  --auto-sync-tb, --auto-sync-top-to-bottom  Automatically time synchronize the top SRT file to the bottom SRT file. 
  -o, --output                               The output ASS filename. [required]
  --o-enc                                    Output encoding [nargs=0..1] [default: "UTF-8"]
```

To get a list of supported character encodings, use:

```sh
iconv -l
```

To guess the character encoding, use [`enca`](https://linux.die.net/man/1/enca).

## ⚖️ License

MIT-License
