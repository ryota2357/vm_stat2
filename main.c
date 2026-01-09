#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach.h>
#include <sys/sysctl.h>

void print_usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s [-b|-k|-m|-g] [-a] [[-c count] interval]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -b        Display values in bytes\n");
    fprintf(stderr, "  -k        Display values in kilobytes\n");
    fprintf(stderr, "  -m        Display values in megabytes\n");
    fprintf(stderr, "  -g        Display values in gigabytes\n");
    fprintf(stderr, "  -a        Show all details (verbose)\n");
    fprintf(stderr, "  -c count  Number of times to poll\n");
    fprintf(stderr, "  interval  Polling interval in seconds (enables polling mode)\n");
}

typedef enum {
    UNIT_AUTO,
    UNIT_BYTE,
    UNIT_KB,
    UNIT_MB,
    UNIT_GB
} UnitMode;

typedef struct {
    UnitMode unit_mode;  // Unit mode (Auto, Byte, KB, MB, GB)
    int interval;        // Interval in seconds (0 for snapshot mode)
    int count;           // Number of times to poll
    bool show_all;       // Show all details
} Config;

Config parse_args(int argc, char* argv[]) {
    Config cfg = {
        .unit_mode = UNIT_AUTO,
        .interval = 0,
        .count = -1,
        .show_all = false
    };
    int opt;
    while ((opt = getopt(argc, argv, "bkmgac:")) != -1) {
        switch (opt) {
            case 'b':
                cfg.unit_mode = UNIT_BYTE;
                break;
            case 'k':
                cfg.unit_mode = UNIT_KB;
                break;
            case 'm':
                cfg.unit_mode = UNIT_MB;
                break;
            case 'g':
                cfg.unit_mode = UNIT_GB;
                break;
            case 'a':
                cfg.show_all = true;
                break;
            case 'c':
                cfg.count = atoi(optarg);
                if (cfg.count <= 0) {
                    fprintf(stderr, "Error: count must be positive\n");
                    print_usage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (optind < argc) {
        cfg.interval = atoi(argv[optind]);
        if (cfg.interval < 0) {
            fprintf(stderr, "Error: interval must be non-negative\n");
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        // Check for extra arguments after interval (e.g., "vm_stat2 1 -c 3" is invalid)
        if (optind + 1 < argc) {
            fprintf(stderr, "Error: unexpected argument after interval\n");
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    return cfg;
}

vm_size_t get_page_size(host_t host_port) {
    vm_size_t page_size;
    if (host_page_size(host_port, &page_size) != KERN_SUCCESS) {
        fprintf(stderr, "Failed to fetch page size\n");
        exit(EXIT_FAILURE);
    }
    return page_size;
}

vm_statistics64_data_t get_vm_statistics64(host_t host_port) {
    vm_statistics64_data_t vm_stat;
    auto count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stat, &count) != KERN_SUCCESS) {
        fprintf(stderr, "Failed to fetch VM statistics\n");
        exit(EXIT_FAILURE);
    }
    return vm_stat;
}

uint64_t get_total_memory() {
    uint64_t total_memory;
    auto len = sizeof(total_memory);
    if (sysctlbyname("hw.memsize", &total_memory, &len, NULL, 0) != 0) {
        fprintf(stderr, "Failed to fetch total memory\n");
        exit(EXIT_FAILURE);
    }
    return total_memory;
}

uint64_t get_swap_used() {
    struct xsw_usage vmusage;
    auto xsw_len = sizeof(vmusage);
    if (sysctlbyname("vm.swapusage", &vmusage, &xsw_len, NULL, 0) != 0) {
        fprintf(stderr, "Failed to fetch swap usage\n");
        exit(EXIT_FAILURE);
    }
    return vmusage.xsu_used;
}

typedef struct {
    uint64_t app_pages;
    uint64_t wired_pages;
    uint64_t compressed_pages;
    uint64_t cached_pages;
    int64_t swap_pages;
} MemoryData;

MemoryData calc_memory_data(vm_statistics64_data_t vm_stat) {
    // ref:
    //  - https://qiita.com/hann-solo/items/3ef57d21b004bb66aadd
    //  - https://songmu.jp/riji/entry/2015-05-08-mac-memory.html
    auto active = vm_stat.active_count;
    auto inactive = vm_stat.inactive_count;
    auto speculative = vm_stat.speculative_count;
    auto throttled = vm_stat.throttled_count;
    auto wired = vm_stat.wire_count;
    auto purgeable = vm_stat.purgeable_count;
    auto file_backed = vm_stat.external_page_count;
    auto compressor = vm_stat.compressor_page_count;
    auto swapins = vm_stat.swapins;
    auto swapouts = vm_stat.swapouts;
    return (MemoryData){
        .app_pages = active + inactive + speculative + throttled - purgeable - file_backed,
        .wired_pages = wired,
        .compressed_pages = compressor,
        .cached_pages = file_backed + purgeable,
        .swap_pages = (int64_t)swapouts - (int64_t)swapins,
    };
}

bool format_bytes(uint64_t bytes, UnitMode mode, char* buf, size_t bufsize) {
    int written;
    switch (mode) {
        case UNIT_AUTO: {
            const char* units[] = {"B", "KB", "MB", "GB", "TB"};
            int i = 0;
            double size = (double)bytes;
            while (size >= 1024.0 && i < 4) {
                size /= 1024.0;
                i += 1;
            }
            written = snprintf(buf, bufsize, "%.2f %s", size, units[i]);
            break;
        }
        case UNIT_BYTE:
            written = snprintf(buf, bufsize, "%llu B", (unsigned long long)bytes);
            break;
        case UNIT_KB:
            written = snprintf(buf, bufsize, "%.2f KB", (double)bytes / 1024.0);
            break;
        case UNIT_MB:
            written = snprintf(buf, bufsize, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
            break;
        case UNIT_GB:
            written = snprintf(buf, bufsize, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
            break;
        default:
            written = snprintf(buf, bufsize, "N/A");
            break;
    }
    return written > 0 && (size_t)written < bufsize;
}

void chmax(int* a, int b) {
    if (*a < b) {
        *a = b;
    }
}

void puts_memory_data_as_table(MemoryData mem_data, uint64_t total_memory, uint64_t swap_used, vm_size_t page_size, UnitMode unit_mode) {
    auto mem_app_bytes = mem_data.app_pages * page_size;
    auto mem_wired_bytes = mem_data.wired_pages * page_size;
    auto mem_compressed_bytes = mem_data.compressed_pages * page_size;
    auto mem_used_bytes = mem_app_bytes + mem_wired_bytes + mem_compressed_bytes;
    auto mem_cached_bytes = mem_data.cached_pages * page_size;

    char bytes_vals[7][32] = {0};
    uint64_t bytes_nums[7] = {total_memory, mem_used_bytes, mem_app_bytes, mem_wired_bytes, mem_compressed_bytes, mem_cached_bytes, swap_used};
    for (int i = 0; i < 7; i++) {
        format_bytes(bytes_nums[i], unit_mode, bytes_vals[i], sizeof(bytes_vals[i]));
    }

    int max_val_len = 0;
    for (int i = 0; i < 7; i++) {
        chmax(&max_val_len, (int)strlen(bytes_vals[i]));
    }

    printf("Total Memory:   %*s %s\n",             max_val_len - (int)strlen(bytes_vals[0]), "", bytes_vals[0]);
    printf("Used Memory:    %*s %s  (%4.1lf%%)\n", max_val_len - (int)strlen(bytes_vals[1]), "", bytes_vals[1], (mem_used_bytes * 100.0) / total_memory);
    printf("  App Memory:   %*s %s  (%4.1lf%%)\n", max_val_len - (int)strlen(bytes_vals[2]), "", bytes_vals[2], (mem_app_bytes * 100.0) / total_memory);
    printf("  Wired Memory: %*s %s  (%4.1lf%%)\n", max_val_len - (int)strlen(bytes_vals[3]), "", bytes_vals[3], (mem_wired_bytes * 100.0) / total_memory);
    printf("  Compressed:   %*s %s  (%4.1lf%%)\n", max_val_len - (int)strlen(bytes_vals[4]), "", bytes_vals[4], (mem_compressed_bytes * 100.0) / total_memory);
    printf("Cached Files:   %*s %s  (%4.1lf%%)\n", max_val_len - (int)strlen(bytes_vals[5]), "", bytes_vals[5], (mem_cached_bytes * 100.0) / total_memory);
    printf("Swap Used:      %*s %s\n",             max_val_len - (int)strlen(bytes_vals[6]), "", bytes_vals[6]);
}

void puts_vm_statistics64_as_table(vm_statistics64_data_t vm_stat, vm_size_t page_size, UnitMode unit_mode) {
    #define NUM_STATS 22
    uint64_t bytes_nums[NUM_STATS] = {
        (uint64_t)(vm_stat.free_count - vm_stat.speculative_count) * page_size,
        (uint64_t)vm_stat.active_count * page_size,
        (uint64_t)vm_stat.inactive_count * page_size,
        (uint64_t)vm_stat.speculative_count * page_size,
        (uint64_t)vm_stat.throttled_count * page_size,
        (uint64_t)vm_stat.wire_count * page_size,
        (uint64_t)vm_stat.purgeable_count * page_size,
        (uint64_t)vm_stat.faults,
        (uint64_t)vm_stat.cow_faults,
        (uint64_t)vm_stat.zero_fill_count,
        (uint64_t)vm_stat.reactivations * page_size,
        (uint64_t)vm_stat.purges * page_size,
        (uint64_t)vm_stat.external_page_count * page_size,
        (uint64_t)vm_stat.internal_page_count * page_size,
        (uint64_t)vm_stat.total_uncompressed_pages_in_compressor * page_size,
        (uint64_t)vm_stat.compressor_page_count * page_size,
        (uint64_t)vm_stat.decompressions * page_size,
        (uint64_t)vm_stat.compressions * page_size,
        (uint64_t)vm_stat.pageins * page_size,
        (uint64_t)vm_stat.pageouts * page_size,
        (uint64_t)vm_stat.swapins * page_size,
        (uint64_t)vm_stat.swapouts * page_size,
    };
    const char* labels[NUM_STATS] = {
        "Pages free:",
        "Pages active:",
        "Pages inactive:",
        "Pages speculative:",
        "Pages throttled:",
        "Pages wired down:",
        "Pages purgeable:",
        "\"Translation faults\":",
        "Pages copy-on-write:",
        "Pages zero filled:",
        "Pages reactivated:",
        "Pages purged:",
        "File-backed pages:",
        "Anonymous pages:",
        "Pages stored in compressor:",
        "Pages occupied by compressor:",
        "Decompressions:",
        "Compressions:",
        "Pageins:",
        "Pageouts:",
        "Swapins:",
        "Swapouts:",
    };

    char bytes_vals[NUM_STATS][32] = {0};
    for (int i = 0; i < NUM_STATS; i++) {
        if (i == 7 || i == 8 || i == 9) {
            snprintf(bytes_vals[i], sizeof(bytes_vals[i]), "%llu", (unsigned long long)bytes_nums[i]);
        } else {
            format_bytes(bytes_nums[i], unit_mode, bytes_vals[i], sizeof(bytes_vals[i]));
        }
    }

    int max_val_len = 0;
    int max_label_len = 0;
    for (int i = 0; i < NUM_STATS; i++) {
        int val_len = strlen(bytes_vals[i]);
        int label_len = strlen(labels[i]);
        chmax(&max_val_len, val_len);
        chmax(&max_label_len, label_len);
    }
    int width = max_label_len + max_val_len + 2;  // space for padding

    for (int i = 0; i < NUM_STATS; i++) {
        fputs(labels[i], stdout);
        int spaces = width - (int)strlen(labels[i]) - (int)strlen(bytes_vals[i]);
        for (int j = 0; j < spaces; j++) {
            putchar(' ');
        }
        fputs(bytes_vals[i], stdout);
        putchar('\n');
    }
    #undef NUM_STATS
}

void snapshot(const Config* cfg) {
    auto host_port = mach_host_self();

    auto page_size = get_page_size(host_port);
    auto vm_stat = get_vm_statistics64(host_port);
    auto total_memory = get_total_memory();
    auto swap_used = get_swap_used();

    auto mem_data = calc_memory_data(vm_stat);

    char buf[32];
    format_bytes(page_size, cfg->unit_mode, buf, sizeof(buf));
    printf("Mach Virtual Memory Statistics 2: (page size: %s)\n", buf);
    puts_memory_data_as_table(mem_data, total_memory, swap_used, page_size, cfg->unit_mode);
    if (cfg->show_all) {
        puts_vm_statistics64_as_table(vm_stat, page_size, cfg->unit_mode);
    }

    /*
    printf("Pages free:                   %lld\n", (uint64_t)vm_stat.free_count);
    printf("Pages active:                 %lld\n", (uint64_t)vm_stat.active_count);
    printf("Pages inactive:               %lld\n", (uint64_t)vm_stat.inactive_count);
    printf("Pages speculative:            %lld\n", (uint64_t)vm_stat.speculative_count);
    printf("Pages throttled:              %lld\n", (uint64_t)vm_stat.throttled_count);
    printf("Pages wired down:             %lld\n", (uint64_t)vm_stat.wire_count);
    printf("Pages purgeable:              %lld\n", (uint64_t)vm_stat.purgeable_count);
    printf("\"Translation faults\":         %lld\n", (uint64_t)vm_stat.faults);
    printf("Pages copy-on-write:          %lld\n", (uint64_t)vm_stat.cow_faults);
    printf("Pages zero filled:            %lld\n", (uint64_t)vm_stat.zero_fill_count);
    printf("Pages reactivated:            %lld\n", (uint64_t)vm_stat.reactivations);
    printf("Pages purged:                 %lld\n", (uint64_t)vm_stat.purges);
    printf("File-backed pages:            %lld\n", (uint64_t)vm_stat.external_page_count);
    printf("Anonymous pages:              %lld\n", (uint64_t)vm_stat.internal_page_count);
    printf("Pages stored in compressor:   %lld\n", (uint64_t)vm_stat.total_uncompressed_pages_in_compressor);
    printf("Pages occupied by compressor: %lld\n", (uint64_t)vm_stat.compressor_page_count);
    printf("Decompressions:               %lld\n", (uint64_t)vm_stat.decompressions);
    printf("Compressions:                 %lld\n", (uint64_t)vm_stat.compressions);
    printf("Pageins:                      %lld\n", (uint64_t)vm_stat.pageins);
    printf("Pageouts:                     %lld\n", (uint64_t)vm_stat.pageouts);
    printf("Swapins:                      %lld\n", (uint64_t)vm_stat.swapins);
    printf("Swapouts:                     %lld\n", (uint64_t)vm_stat.swapouts);
    */
}

void polling_loop(const Config* cfg) {
    auto host_port = mach_host_self();
    auto page_size = get_page_size(host_port);

    char page_size_buf[32];
    format_bytes(page_size, cfg->unit_mode, page_size_buf, sizeof(page_size_buf));
    printf("Mach Virtual Memory Statistics 2: (page size: %s)\n", page_size_buf);

    printf("%10s %10s %10s %10s %10s %10s %10s %10s\n",
           "Free", "App", "Wired", "Cmprssed", "Cache", "Swap", "PageIn/s", "PageOut/s");

    vm_statistics64_data_t prev_stat = {0};
    int iteration = 0;

    while (cfg->count < 0 || iteration < cfg->count) {
        auto vm_stat = get_vm_statistics64(host_port);
        auto swap_used = get_swap_used();
        auto mem_data = calc_memory_data(vm_stat);

        uint64_t free_bytes = (uint64_t)(vm_stat.free_count - vm_stat.speculative_count) * page_size;
        uint64_t app_bytes = mem_data.app_pages * page_size;
        uint64_t wired_bytes = mem_data.wired_pages * page_size;
        uint64_t compr_bytes = mem_data.compressed_pages * page_size;
        uint64_t cache_bytes = mem_data.cached_pages * page_size;

        char free_buf[32], app_buf[32], wired_buf[32], compr_buf[32], cache_buf[32], swap_buf[32];
        format_bytes(free_bytes, cfg->unit_mode, free_buf, sizeof(free_buf));
        format_bytes(app_bytes, cfg->unit_mode, app_buf, sizeof(app_buf));
        format_bytes(wired_bytes, cfg->unit_mode, wired_buf, sizeof(wired_buf));
        format_bytes(compr_bytes, cfg->unit_mode, compr_buf, sizeof(compr_buf));
        format_bytes(cache_bytes, cfg->unit_mode, cache_buf, sizeof(cache_buf));
        format_bytes(swap_used, cfg->unit_mode, swap_buf, sizeof(swap_buf));

        char in_buf[32], out_buf[32];
        if (iteration == 0) {
            snprintf(in_buf, sizeof(in_buf), "-");
            snprintf(out_buf, sizeof(out_buf), "-");
        } else {
            uint64_t pageins_diff = vm_stat.pageins - prev_stat.pageins;
            uint64_t pageouts_diff = vm_stat.pageouts - prev_stat.pageouts;
            uint64_t in_bytes_per_sec = (pageins_diff * page_size) / cfg->interval;
            uint64_t out_bytes_per_sec = (pageouts_diff * page_size) / cfg->interval;
            format_bytes(in_bytes_per_sec, cfg->unit_mode, in_buf, sizeof(in_buf));
            format_bytes(out_bytes_per_sec, cfg->unit_mode, out_buf, sizeof(out_buf));
        }

        printf("%10s %10s %10s %10s %10s %10s %10s %10s\n",
               free_buf, app_buf, wired_buf, compr_buf, cache_buf, swap_buf, in_buf, out_buf);
        fflush(stdout);

        prev_stat = vm_stat;
        iteration += 1;

        if (cfg->count >= 0 && iteration >= cfg->count) {
            break;
        }
        sleep(cfg->interval);
    }
}

void debug_print_config(const Config* cfg) {
    char* unit_mode_str;
    switch (cfg->unit_mode) {
        case UNIT_AUTO: unit_mode_str = "AUTO"; break;
        case UNIT_BYTE: unit_mode_str = "BYTE"; break;
        case UNIT_KB:   unit_mode_str = "KB";   break;
        case UNIT_MB:   unit_mode_str = "MB";   break;
        case UNIT_GB:   unit_mode_str = "GB";   break;
        default:        unit_mode_str = "UNKNOWN"; break;
    }
    printf("[DEBUG] Config:\n");
    printf("  unit_mode:   %s\n", unit_mode_str);
    printf("  interval:    %d\n", cfg->interval);
    printf("  count:       %d\n", cfg->count);
    printf("  show_all:    %s\n", cfg->show_all ? "true" : "false");
    printf("\n");
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);
#ifndef NDEBUG
    debug_print_config(&cfg);
#endif
    if (cfg.interval == 0) {
        snapshot(&cfg);
    } else {
        polling_loop(&cfg);
    }
    return 0;
}
