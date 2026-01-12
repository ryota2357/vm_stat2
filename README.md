# vm_stat2

A simple macOS command-line tool to display virtual memory statistics in a human-readable format. An enhanced alternative to the built-in `vm_stat` command.

## Installation

### Build from source

Build the project using Make:

```bash
make
```

The binary will be created at `build/vm_stat2`. You can move it to a directory in your `PATH` (e.g., `/usr/local/bin/`) to use it from anywhere.

### Using Nix

Install with Nix:

```bash
nix profile add github:ryota2357/vm_stat2
```

Or run directly without installing:

```bash
nix run github:ryota2357/vm_stat2
```

## Usage

```
vm_stat2 [-b|-k|-m|-g] [-a] [[-c count] interval]
```

### Options

- `-b` - Display values in bytes
- `-k` - Display values in kilobytes
- `-m` - Display values in megabytes
- `-g` - Display values in gigabytes
- `-a` - Show all details (verbose mode)
- `-c count` - Number of times to poll
- `interval` - Polling interval in seconds

### Examples

Display current memory statistics:

```console
$ vm_stat2
Mach Virtual Memory Statistics 2: (page size: 16.00 KB)
Total Memory:    32.00 GB
Used Memory:     19.70 GB  (61.6%)
  App Memory:    13.69 GB  (42.8%)
  Wired Memory:   3.48 GB  (10.9%)
  Compressed:     2.53 GB  ( 7.9%)
Cached Files:     9.08 GB  (28.4%)
Swap Used:         0.00 B
```

Poll every 2 seconds, 5 times:

```console
$ vm_stat2 -c 5 2
Mach Virtual Memory Statistics 2: (page size: 16.00 KB)
      Free        App      Wired   Cmprssed      Cache       Swap   PageIn/s  PageOut/s
   1.55 GB   14.13 GB    3.69 GB    1.48 GB   10.35 GB     0.00 B          -          -
   1.58 GB   14.12 GB    3.68 GB    1.48 GB   10.35 GB     0.00 B    2.12 MB     0.00 B
   1.60 GB   14.14 GB    3.64 GB    1.48 GB   10.35 GB     0.00 B    3.17 MB     0.00 B
   1.58 GB   14.29 GB    3.50 GB    1.48 GB   10.35 GB     0.00 B    3.34 MB     0.00 B
   1.41 GB   13.97 GB    4.04 GB    1.48 GB   10.30 GB     0.00 B    5.46 MB     0.00 B
```

Display in megabytes with all details:

```console
$ vm_stat2 -m -a
Mach Virtual Memory Statistics 2: (page size: 0.02 MB)
Total Memory:    32768.00 MB
Used Memory:     19607.44 MB  (59.8%)
  App Memory:    14303.62 MB  (43.7%)
  Wired Memory:   3792.52 MB  (11.6%)
  Compressed:     1511.30 MB  ( 4.6%)
Cached Files:    10656.05 MB  (32.5%)
Swap Used:           0.00 MB
Pages free:                      1685.89 MB
Pages active:                   12482.38 MB
Pages inactive:                 11928.97 MB
Pages speculative:                548.33 MB
Pages throttled:                    0.00 MB
Pages wired down:                3792.52 MB
Pages purgeable:                  255.44 MB
"Translation faults":            8465182096
Pages copy-on-write:             2020057252
Pages zero filled:               2196463069
Pages reactivated:              52917.44 MB
Pages purged:                  153772.64 MB
File-backed pages:              10400.61 MB
Anonymous pages:                14559.06 MB
Pages stored in compressor:      4433.67 MB
Pages occupied by compressor:    1511.30 MB
Decompressions:                 21651.30 MB
Compressions:                   38998.59 MB
Pageins:                       645292.69 MB
Pageouts:                         426.52 MB
Swapins:                            0.00 MB
Swapouts:                           0.00 MB
```

## Requirements

- macOS
- Clang compiler

Tested on macOS 15 and 26.

## License

[MIT](./LICENSE)
