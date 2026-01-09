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
    UnitMode unit_mode;   // Unit mode (Auto, Byte, KB, MB, GB)
    int interval;         // Interval in seconds (0 for snapshot mode)
    int count;            // Number of times to poll
    bool show_all;        // Show all details
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

bool humanize_bytes(uint64_t bytes, char* buf, size_t bufsize) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = (double)bytes;
    while (size >= 1024 && i < 4) {
        size /= 1024;
        i += 1;
    }
    auto written = snprintf(buf, bufsize, "%6.2f %s", size, units[i]);
    return written > 0 && (size_t)written < bufsize;
}
#define HUMANIZE_BYTES(bytes, buf) ( humanize_bytes(bytes, buf, sizeof(buf)) ? buf : "N/A" )

void puts_2col(int width, const char* left, const char* fmt_right, ...) {
    va_list args;

    int left_len = (int)strlen(left);
    va_start(args, fmt_right);
    int right_len = vsnprintf(NULL, 0, fmt_right, args);
    va_end(args);
    int spaces = width - left_len - right_len;
    if (spaces < 1) spaces = 1;

    fputs(left, stdout);
    for (int i = 0; i < spaces; i++) {
        putchar(' ');
    }
    va_start(args, fmt_right);
    vprintf(fmt_right, args);
    va_end(args);
    putchar('\n');
}

void snapshot() {
    auto host_port = mach_host_self();

    auto page_size = get_page_size(host_port);
    auto vm_stat = get_vm_statistics64(host_port);
    auto total_memory = get_total_memory();
    auto swap_used = get_swap_used();

    auto mem_data = calc_memory_data(vm_stat);

    auto mem_app_bytes = mem_data.app_pages * page_size;
    auto mem_wired_bytes = mem_data.wired_pages * page_size;
    auto mem_compressed_bytes = mem_data.compressed_pages * page_size;
    auto mem_used_bytes = mem_app_bytes + mem_wired_bytes + mem_compressed_bytes;
    auto mem_cached_bytes = mem_data.cached_pages * page_size;

    char buf[32];
    puts("--- Mach Virtual Memory Statistics 2 ---");
    puts_2col(35, "Total Memory:",   "%s         ", HUMANIZE_BYTES(total_memory, buf));
    puts_2col(35, "Used Memory:",    "%s  (%4.1lf%%)", HUMANIZE_BYTES(mem_used_bytes, buf), (mem_used_bytes * 100.0) / total_memory);
    puts_2col(35, "  App Memory:",   "%s  (%4.1lf%%)", HUMANIZE_BYTES(mem_app_bytes, buf), (mem_app_bytes * 100.0) / total_memory);
    puts_2col(35, "  Wired Memory:", "%s  (%4.1lf%%)", HUMANIZE_BYTES(mem_wired_bytes, buf), (mem_wired_bytes * 100.0) / total_memory);
    puts_2col(35, "  Compressed",    "%s  (%4.1lf%%)", HUMANIZE_BYTES(mem_compressed_bytes, buf), (mem_compressed_bytes * 100.0) / total_memory);
    puts_2col(35, "Cached Files:",   "%s  (%4.1lf%%)", HUMANIZE_BYTES(mem_cached_bytes, buf), (mem_cached_bytes * 100.0) / total_memory);
    puts_2col(35, "Swap Used:",      "%s         ",  HUMANIZE_BYTES(swap_used, buf));
    puts("-----------------------------------------");
    puts_2col(40, "Pages free:",                   HUMANIZE_BYTES((uint64_t)vm_stat.free_count * page_size, buf));
    puts_2col(40, "Pages active:",                 HUMANIZE_BYTES((uint64_t)vm_stat.active_count * page_size, buf));
    puts_2col(40, "Pages inactive:",               HUMANIZE_BYTES((uint64_t)vm_stat.inactive_count * page_size, buf));
    puts_2col(40, "Pages speculative:",            HUMANIZE_BYTES((uint64_t)vm_stat.speculative_count * page_size, buf));
    puts_2col(40, "Pages throttled:",              HUMANIZE_BYTES((uint64_t)vm_stat.throttled_count * page_size, buf));
    puts_2col(40, "Pages wired down:",             HUMANIZE_BYTES((uint64_t)vm_stat.wire_count * page_size, buf));
    puts_2col(40, "Pages purgeable:",              HUMANIZE_BYTES((uint64_t)vm_stat.purgeable_count * page_size, buf));
    puts_2col(40, "\"Translation faults\":",       HUMANIZE_BYTES((uint64_t)vm_stat.faults * page_size, buf));
    puts_2col(40, "Pages copy-on-write:",          HUMANIZE_BYTES((uint64_t)vm_stat.cow_faults * page_size, buf));
    puts_2col(40, "Pages zero filled:",            HUMANIZE_BYTES((uint64_t)vm_stat.zero_fill_count * page_size, buf));
    puts_2col(40, "Pages reactivated:",            HUMANIZE_BYTES((uint64_t)vm_stat.reactivations * page_size, buf));
    puts_2col(40, "Pages purged:",                 HUMANIZE_BYTES((uint64_t)vm_stat.purges * page_size, buf));
    puts_2col(40, "File-backed pages:",            HUMANIZE_BYTES((uint64_t)vm_stat.external_page_count * page_size, buf));
    puts_2col(40, "Anonymous pages:",              HUMANIZE_BYTES((uint64_t)vm_stat.internal_page_count * page_size, buf));
    puts_2col(40, "Pages stored in compressor:",   HUMANIZE_BYTES((uint64_t)vm_stat.total_uncompressed_pages_in_compressor * page_size, buf));
    puts_2col(40, "Pages occupied by compressor:", HUMANIZE_BYTES((uint64_t)vm_stat.compressor_page_count * page_size, buf));
    puts_2col(40, "Decompressions:",               HUMANIZE_BYTES((uint64_t)vm_stat.decompressions * page_size, buf));
    puts_2col(40, "Compressions:",                 HUMANIZE_BYTES((uint64_t)vm_stat.compressions * page_size, buf));
    puts_2col(40, "Pageins:",                      HUMANIZE_BYTES((uint64_t)vm_stat.pageins * page_size, buf));
    puts_2col(40, "Pageouts:",                     HUMANIZE_BYTES((uint64_t)vm_stat.pageouts * page_size, buf));
    puts_2col(40, "Swapins:",                      HUMANIZE_BYTES((uint64_t)vm_stat.swapins * page_size, buf));
    puts_2col(40, "Swapouts:",                     HUMANIZE_BYTES((uint64_t)vm_stat.swapouts * page_size, buf));
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

    snapshot();

    return 0;
}
