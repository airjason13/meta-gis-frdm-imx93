// gpio_event_pthread_cfg.c
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include "utildbg.h"

static volatile int keep_running = 1;
static struct gpiod_line_request *request;

// Ctrl+C 中斷處理
static void sigint_handler(int sig)
{
    (void)sig;
    keep_running = 0;
}

// 事件監聽執行緒
static void *event_thread(void *arg)
{
    struct gpiod_edge_event_buffer *buffer;
    int64_t timeout_ns = 5LL * 1000000000; // 5 秒
    uint32_t pre_edge = 0;
    uint32_t cur_edge = 0;
    uint32_t rising_edge_count = 0;   
    // 建立使用者空間事件緩衝區
    buffer = gpiod_edge_event_buffer_new(64);
    if (!buffer) {
        log_fatal("Failed to alloc edge event buffer\n");
        return NULL;
    }

    while (keep_running) {
        int ret = gpiod_line_request_wait_edge_events(request, timeout_ns);
        if (ret < 0) {
            log_fatal(stderr, "wait_edge_events error: %s\n", strerror(errno));
            break;
        } else if (ret == 0) {
            log_debug("timeout!\n");
            continue; // timeout
        }
        // printf("ret : %d\n", ret);

        // 讀取所有事件
        ret = gpiod_line_request_read_edge_events(request, buffer, 64);
        if (ret < 0) {
            log_fatal( "read_edge_events error: %s\n", strerror(errno));
            break;
        }

        // 處理事件
        size_t nevents = gpiod_edge_event_buffer_get_num_events(buffer);
        for (size_t i = 0; i < nevents; i++) {
            struct gpiod_edge_event *e = gpiod_edge_event_buffer_get_event(buffer, i);
            uint64_t ts = gpiod_edge_event_get_timestamp_ns(e);
            unsigned int offs = gpiod_edge_event_get_line_offset(e);

            // 先確認實際電平
            /*enum gpiod_line_value val = gpiod_line_request_get_value(request, offs);
            if (val != GPIOD_LINE_VALUE_ACTIVE)
                continue;  // 不是 high，就忽略*/
            cur_edge = gpiod_edge_event_get_event_type(e);
            if (pre_edge != GPIOD_EDGE_EVENT_RISING_EDGE){
                if(cur_edge == GPIOD_EDGE_EVENT_RISING_EDGE){ 
                    rising_edge_count += 1;
                    const char *etype = (gpiod_edge_event_get_event_type(e) == GPIOD_EDGE_EVENT_RISING_EDGE)
                                    ? "RISING" : "FALLING";
                    log_info("%lld, nevents: %lld, [offset %u] %s at %lu.%09lu, count: %u \n",
                        i, nevents,offs, etype, (unsigned long)(ts/1000000000), (unsigned long)(ts%1000000000), rising_edge_count);
                }
            }
            pre_edge = cur_edge;
        }
    }

    gpiod_edge_event_buffer_free(buffer);
    return NULL;
}

int main(int argc, char **argv)
{
    const char* chipname;
    struct gpiod_chip *chip;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_settings *ls;
    struct gpiod_line_config *lc;
    pthread_t tid;
    unsigned int line_offset;
    
    int enable_log_file = log_init(true, LOG_PREFIX_ID);
	if(enable_log_file != 0){
		log_fatal("ERROR!Can't enable log file\n");
	}
    log_info("Test!");
    log_debug("Welcome to v-gpio-detect!");    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <gpiochipX> <line_offset>\n", argv[0]);
        return EXIT_FAILURE;
    }
    chipname = argv[1];
    line_offset = strtoul(argv[2], NULL, 0);
    signal(SIGINT, sigint_handler);

    // 1. 開啟 GPIO chip
    chip = gpiod_chip_open(chipname);
    if (!chip) {
        log_fatal( "Open chip %s failed: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    // 2. 建立並設定 request config
    req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "gpio_event");              // consumer name :contentReference[oaicite:2]{index=2}
    gpiod_request_config_set_event_buffer_size(req_cfg, 64);               // kernel buffer size :contentReference[oaicite:3]{index=3}

    // 3. 建立並設定 line settings
    ls = gpiod_line_settings_new();
    log_info("add bias on ls\n");
    gpiod_line_settings_set_bias(ls, GPIOD_LINE_BIAS_PULL_DOWN);
    
    //printf("add anti jitter\n");
    //gpiod_line_settings_set_debounce_period_us(ls, 10000);    

    gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_INPUT);     // input :contentReference[oaicite:4]{index=4}
    gpiod_line_settings_set_edge_detection(ls, GPIOD_LINE_EDGE_BOTH);      // both edges :contentReference[oaicite:5]{index=5}

    // 4. 建立並設定 line config
    lc = gpiod_line_config_new();
    unsigned int offs[] = { line_offset };
    gpiod_line_config_add_line_settings(lc, offs, 1, ls);                  // apply settings :contentReference[oaicite:6]{index=6}

    // 5. Request lines
    request = gpiod_chip_request_lines(chip, req_cfg, lc);                 // request API :contentReference[oaicite:7]{index=7}
    if (!request) {
        log_fatal("Request lines failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // cleanup config objects
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(lc);
    gpiod_line_settings_free(ls);

    // 6. 建立並啟動事件執行緒
    pthread_create(&tid, NULL, event_thread, NULL);
    log_info("Listening on %s offset %u. Press Ctrl+C to exit.\n", argv[1], line_offset);

    pthread_join(tid, NULL);

    // 7. 釋放資源
    gpiod_line_request_release(request);                                   // release :contentReference[oaicite:8]{index=8}
    gpiod_chip_close(chip);

    log_info("Exiting...\n");
    return EXIT_SUCCESS;
}
