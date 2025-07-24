// gpio_pulse_thread_cfg.c
#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include "utildbg.h"

static volatile int keep_running = 1;
static struct gpiod_line_request *request;

static void sigint_handler(int sig)
{
    (void)sig;
    keep_running = 0;
}

// pulse thread
static void *pulse_thread(void *arg)
{
    unsigned int line_offset = *(unsigned int *)arg;
    unsigned int pulse_count = 0;
    while (keep_running) {
        // Set GPIO high
        gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_ACTIVE);
        //log_info("Set GPIO high\n");
        pulse_count += 1;
        log_info("pulse_count : %d", pulse_count);
        sleep(1); // 1 秒 HIGH

        // Set GPIO low
        gpiod_line_request_set_value(request, line_offset, GPIOD_LINE_VALUE_INACTIVE);
        //log_info("Set GPIO low\n");
        
        
        sleep(1); // 等待下一個 2 秒週期
    }
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
    if (enable_log_file != 0) {
        log_fatal("ERROR! Can't enable log file\n");
    }

    log_info("GPIO Pulse Generator\n");

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <gpiochipX> <line_offset>\n", argv[0]);
        return EXIT_FAILURE;
    }

    chipname = argv[1];
    line_offset = strtoul(argv[2], NULL, 0);
    signal(SIGINT, sigint_handler);

    chip = gpiod_chip_open(chipname);
    if (!chip) {
        log_fatal("Open chip %s failed: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "gpio_pulse");

    ls = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(ls, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_settings_set_bias(ls, GPIOD_LINE_BIAS_DISABLED); // disable bias for output

    lc = gpiod_line_config_new();
    unsigned int offs[] = { line_offset };
    gpiod_line_config_add_line_settings(lc, offs, 1, ls);

    request = gpiod_chip_request_lines(chip, req_cfg, lc);
    if (!request) {
        log_fatal("Request lines failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(lc);
    gpiod_line_settings_free(ls);

    pthread_create(&tid, NULL, pulse_thread, &line_offset);
    log_info("Start pulsing on %s offset %u. Ctrl+C to stop.\n", argv[1], line_offset);
    pthread_join(tid, NULL);

    gpiod_line_request_release(request);
    gpiod_chip_close(chip);
    log_info("Exiting...\n");

    return EXIT_SUCCESS;
}

