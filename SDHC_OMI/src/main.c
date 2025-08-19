/*
 * SD Card Application for OMI Triangle v2
 * XIAO BLE (nRF52840) with Generic MicroSD Module
 *
 * This application demonstrates SD card functionality for the OMI Triangle v2
 * hardware adaptation, replacing the original Adafruit SD card with a generic
 * MicroSD module while maintaining all original OMI functionalities.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <string.h>
#include <stdio.h>

#include "sd_card.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/* Test data for file operations */
static const char *test_files[] = {
    "omi_config.txt",
    "audio_sample.wav",
    "transcription.txt",
    "system_log.txt"
};

static const char *test_contents[] = {
    "OMI Triangle v2 Configuration\nVersion: 2.0\nSD Card: Generic MicroSD\nAudio: PAM8403 PWM",
    "WAV audio sample data for testing PAM8403 amplifier",
    "Transcribed conversation data from OMI microphone",
    "System log entries for debugging and monitoring"
};

/* Shell Commands */

static int cmd_sd_init(const struct shell *shell, size_t argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    int ret = sd_card_start();
    if (ret == 0) {
        shell_print(shell, "SD card initialized and mounted successfully");
    } else {
        shell_error(shell, "SD card initialization failed: %d", ret);
    }
    return ret;
}

static int cmd_sd_stop(const struct shell *shell, size_t argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    int ret = sd_card_stop();
    if (ret == 0) {
        shell_print(shell, "SD card stopped successfully");
    } else {
        shell_error(shell, "SD card stop failed: %d", ret);
    }
    return ret;
}

static int cmd_sd_status(const struct shell *shell, size_t argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    sd_card_state_t state = sd_card_get_state();
    const char *state_str;
    
    switch (state) {
        case SD_CARD_UNINITIALIZED:
            state_str = "UNINITIALIZED";
            break;
        case SD_CARD_INITIALIZED:
            state_str = "INITIALIZED";
            break;
        case SD_CARD_MOUNTED:
            state_str = "MOUNTED";
            break;
        case SD_CARD_ERROR:
            state_str = "ERROR";
            break;
        default:
            state_str = "UNKNOWN";
			break;
		}

    shell_print(shell, "SD Card State: %s", state_str);
    return 0;
}

static int cmd_sd_info(const struct shell *shell, size_t argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    uint64_t total_size_mb;
    uint32_t free_space_mb;
    
    int ret = sd_card_get_info(&total_size_mb, &free_space_mb);
    if (ret == 0) {
        shell_print(shell, "SD Card Information:");
        shell_print(shell, "  Total Size: %u MB", (uint32_t)total_size_mb);
        shell_print(shell, "  Free Space: %u MB", free_space_mb);
    } else {
        shell_error(shell, "Failed to get SD card info: %d", ret);
    }
    return ret;
}

static int cmd_sd_list(const struct shell *shell, size_t argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    shell_print(shell, "Listing SD card contents:");
    int ret = sd_card_list_files("/SD:");
    if (ret >= 0) {
        shell_print(shell, "Found %d entries", ret);
    } else {
        shell_error(shell, "Failed to list files: %d", ret);
    }
    return ret;
}

static int cmd_sd_test(const struct shell *shell, size_t argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    shell_print(shell, "Running SD card comprehensive tests...");
    int ret = sd_card_run_tests();
    if (ret == 0) {
        shell_print(shell, "All SD card tests passed!");
    } else {
        shell_error(shell, "SD card tests failed: %d", ret);
    }
    return ret;
}

static int cmd_sd_create(const struct shell *shell, size_t argc, const char **argv)
{
    if (argc != 3) {
        shell_error(shell, "Usage: sd_create <filename> <content>");
        return -EINVAL;
    }
    
    const char *filename = argv[1];
    const char *content = argv[2];
    
    int ret = sd_card_create_file(filename, content);
    if (ret == 0) {
        shell_print(shell, "File '%s' created successfully", filename);
    } else {
        shell_error(shell, "Failed to create file '%s': %d", filename, ret);
    }
    return ret;
}

static int cmd_sd_read(const struct shell *shell, size_t argc, const char **argv)
{
    if (argc != 2) {
        shell_error(shell, "Usage: sd_read <filename>");
        return -EINVAL;
    }
    
    const char *filename = argv[1];
    char buffer[512];
    
    int ret = sd_card_read_file(filename, buffer, sizeof(buffer));
    if (ret >= 0) {
        shell_print(shell, "File '%s' content (%d bytes):", filename, ret);
        shell_print(shell, "%s", buffer);
    } else {
        shell_error(shell, "Failed to read file '%s': %d", filename, ret);
    }
    return ret;
}

static int cmd_sd_delete(const struct shell *shell, size_t argc, const char **argv)
{
    if (argc != 2) {
        shell_error(shell, "Usage: sd_delete <filename>");
        return -EINVAL;
    }
    
    const char *filename = argv[1];
    
    int ret = sd_card_delete_file(filename);
    if (ret == 0) {
        shell_print(shell, "File '%s' deleted successfully", filename);
    } else {
        shell_error(shell, "Failed to delete file '%s': %d", filename, ret);
    }
    return ret;
}

static int cmd_sd_demo(const struct shell *shell, size_t argc, const char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    shell_print(shell, "Running SD card demo...");
    
    // Create test files
    for (int i = 0; i < 4; i++) {
        int ret = sd_card_create_file(test_files[i], test_contents[i]);
        if (ret == 0) {
            shell_print(shell, "Created file: %s", test_files[i]);
        } else {
            shell_error(shell, "Failed to create file %s: %d", test_files[i], ret);
        }
    }
    
    // List files
    shell_print(shell, "Listing files after creation:");
    sd_card_list_files("/SD:");
    
    // Read one file
    char buffer[256];
    int ret = sd_card_read_file(test_files[0], buffer, sizeof(buffer));
    if (ret >= 0) {
        shell_print(shell, "Read file %s (%d bytes):", test_files[0], ret);
        shell_print(shell, "%s", buffer);
    }
    
    shell_print(shell, "SD card demo completed");
    return 0;
}

/* Shell Command Registration */
SHELL_STATIC_SUBCMD_SET_CREATE(sd_cmd,
    SHELL_CMD(init, NULL, "Initialize and mount SD card", cmd_sd_init),
    SHELL_CMD(stop, NULL, "Stop and unmount SD card", cmd_sd_stop),
    SHELL_CMD(status, NULL, "Get SD card status", cmd_sd_status),
    SHELL_CMD(info, NULL, "Get SD card information", cmd_sd_info),
    SHELL_CMD(list, NULL, "List files on SD card", cmd_sd_list),
    SHELL_CMD(test, NULL, "Run comprehensive SD card tests", cmd_sd_test),
    SHELL_CMD(create, NULL, "Create a file with content", cmd_sd_create),
    SHELL_CMD(read, NULL, "Read file content", cmd_sd_read),
    SHELL_CMD(delete, NULL, "Delete a file", cmd_sd_delete),
    SHELL_CMD(demo, NULL, "Run SD card demo", cmd_sd_demo),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sd, &sd_cmd, "SD card commands", NULL);

/* Main Application */
int main(void)
{
    LOG_INF("SD Card Application for OMI Triangle v2");
    LOG_INF("XIAO BLE (nRF52840) with Generic MicroSD Module");
    LOG_INF("Initializing...");

    /* Initialize SD card */
    int ret = sd_card_start();
    if (ret != 0) {
        LOG_ERR("Failed to initialize SD card: %d", ret);
        LOG_ERR("Please check hardware connections and try again");
        LOG_ERR("Use 'sd init' command to retry initialization");
    } else {
        LOG_INF("SD card initialized successfully");
        
        /* Run initial tests */
        LOG_INF("Running initial SD card tests...");
        ret = sd_card_run_tests();
        if (ret == 0) {
            LOG_INF("SD card tests passed - system ready");
		} else {
            LOG_WRN("SD card tests failed: %d", ret);
        }
    }

    LOG_INF("SD Card Application started");
    LOG_INF("Available commands:");
    LOG_INF("  sd init    - Initialize SD card");
    LOG_INF("  sd status  - Get SD card status");
    LOG_INF("  sd info    - Get SD card information");
    LOG_INF("  sd list    - List files");
    LOG_INF("  sd test    - Run comprehensive tests");
    LOG_INF("  sd demo    - Run demonstration");
    LOG_INF("  sd create  - Create a file");
    LOG_INF("  sd read    - Read a file");
    LOG_INF("  sd delete  - Delete a file");
    LOG_INF("  sd stop    - Stop SD card");

    /* Main loop */
    while (1) {
        k_sleep(K_MSEC(5000));
        
        /* Periodic status check */
        sd_card_state_t state = sd_card_get_state();
        if (state == SD_CARD_ERROR) {
            LOG_WRN("SD card in error state - use 'sd init' to retry");
        }
    }

    return 0;
}
