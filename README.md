# vm_stat2

A simple macOS command-line tool to display virtual memory statistics in a human-readable format. An enhanced alternative to the built-in `vm_stat` command.

## Features

- Human-readable memory statistics with percentage breakdowns
- Customizable unit display (auto, bytes, KB, MB, GB)
- Polling mode for continuous monitoring
- Verbose mode for detailed information

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

```bash
vm_stat2 -c 5 2
```

Display in megabytes with all details:

```bash
vm_stat2 -m -a
```

## Requirements

- macOS
- Clang compiler

Tested on macOS 15 and 26.

## License

[MIT](./LICENSE)
