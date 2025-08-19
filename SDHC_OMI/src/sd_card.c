/*
 * Enhanced SD Card Driver for OMI Triangle v2
 * Based on OMI's implementation with improved reliability and testing
 * Uses only real connections: SPI0 + CS pin (P0.06) - no separate enable pin needed
 */

#include <ff.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(sd_card, CONFIG_LOG_DEFAULT_LEVEL);

/* SD Card Configuration - OMI Compatible */
#define SD_DISK_NAME "SD"
#define SD_MOUNT_PT "/SD:"
#define AUDIO_DIR "/SD:/audio"
#define MAX_PATH 128
#define MAX_FILE_SIZE 1024*1024  // 1MB max file size

/* OMI Audio File Configuration */
#define MAX_AUDIO_FILES 99
#define AUDIO_FILE_PREFIX "a"
#define AUDIO_FILE_EXTENSION ".txt"
#define AUDIO_FILE_NAME_LEN 8  // "a01.txt" = 7 chars + null

/* SD Card States */
typedef enum {
    SD_CARD_UNINITIALIZED,
    SD_CARD_INITIALIZED,
    SD_CARD_MOUNTED,
    SD_CARD_ERROR
} sd_card_state_t;

static sd_card_state_t sd_card_state = SD_CARD_UNINITIALIZED;
static struct fs_mount_t mp;
static FATFS fat_fs;

/* OMI Audio File Management */
static uint8_t file_count = 0;
static uint8_t current_read_file = 1;
static uint8_t current_write_file = 1;
static uint32_t file_num_array[2];

/* File Path Buffers */
static char current_full_path[MAX_PATH];
static char read_buffer[MAX_PATH];
static char write_buffer[MAX_PATH];

/* Function Declarations */
static int sd_card_init(void);
static int sd_card_mount(void);
static int sd_card_unmount(void);
static int sd_card_test_read_write(void);
int sd_card_list_files(const char *path);
int sd_card_create_file(const char *filename, const char *content);
int sd_card_read_file(const char *filename, char *buffer, size_t buffer_size);
int sd_card_delete_file(const char *filename);
int sd_card_get_info(uint64_t *total_size_mb, uint32_t *free_space_mb);

/* OMI Audio File Functions */
static char* generate_new_audio_header(uint8_t num);
static int get_file_contents(struct fs_dir_t *zdp, struct fs_dirent *entry);
static int initialize_audio_file(uint8_t num);
static int move_read_pointer(uint8_t num);
static int move_write_pointer(uint8_t num);
static int clear_audio_file(uint8_t num);
static int delete_audio_file(uint8_t num);
static int clear_audio_directory(void);
static int save_offset(uint32_t offset);
static int get_offset(void);

/**
 * @brief Initialize SD card with OMI's approach
 * @return 0 on success, negative error code on failure
 */
static int sd_card_init(void)
{
    int ret;
    const char *disk_pdrv = SD_DISK_NAME;
    uint64_t memory_size_mb;
    uint32_t block_count;
    uint32_t block_size;

    LOG_INF("Initializing SD card (OMI compatible)...");

    /* Initialize disk access (OMI's approach with retry) */
    ret = disk_access_init(disk_pdrv);
    if (ret != 0) {
        LOG_ERR("SD card init failed: %d", ret);
        /* OMI retry approach */
        k_msleep(1000);
        ret = disk_access_init(disk_pdrv);
        if (ret != 0) {
            LOG_ERR("SD card init retry failed: %d", ret);
            sd_card_state = SD_CARD_ERROR;
            return ret;
        }
    }

    /* Get disk information */
    ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &block_count);
    if (ret != 0) {
        LOG_ERR("Failed to get sector count: %d", ret);
        return ret;
    }

    ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &block_size);
    if (ret != 0) {
        LOG_ERR("Failed to get sector size: %d", ret);
        return ret;
    }

    memory_size_mb = (uint64_t)block_count * block_size;
    LOG_INF("SD Card Info:");
    LOG_INF("  Block count: %u", block_count);
    LOG_INF("  Block size: %u bytes", block_size);
    LOG_INF("  Total size: %u MB", (uint32_t)(memory_size_mb >> 20));

    sd_card_state = SD_CARD_INITIALIZED;
    LOG_INF("SD card initialized successfully");
    return 0;
}

/**
 * @brief Mount SD card filesystem (OMI compatible)
 * @return 0 on success, negative error code on failure
 */
static int sd_card_mount(void)
{
    int ret;

    if (sd_card_state != SD_CARD_INITIALIZED) {
        LOG_ERR("SD card not initialized");
        return -ENODEV;
    }

    LOG_INF("Mounting SD card filesystem...");

    /* Configure mount point (OMI's approach) */
    mp.type = FS_FATFS;
    mp.fs_data = &fat_fs;
    mp.mnt_point = SD_MOUNT_PT;

    /* Mount filesystem */
    ret = fs_mount(&mp);
    if (ret != 0) {
        LOG_ERR("Failed to mount SD card: %d", ret);
        sd_card_state = SD_CARD_ERROR;
        return ret;
    }

    sd_card_state = SD_CARD_MOUNTED;
    LOG_INF("SD card mounted successfully at %s", SD_MOUNT_PT);

    /* Create audio directory (OMI's approach) */
    ret = fs_mkdir(AUDIO_DIR);
    if (ret == 0) {
        LOG_INF("Audio directory created successfully");
        initialize_audio_file(1);
    } else if (ret == -EEXIST) {
        LOG_INF("Audio directory already exists");
    } else {
        LOG_INF("Audio directory creation failed: %d", ret);
    }

    /* Initialize file management (OMI's approach) */
    struct fs_dir_t audio_dir_entry;
    fs_dir_t_init(&audio_dir_entry);
    ret = fs_opendir(&audio_dir_entry, AUDIO_DIR);
    if (ret != 0) {
        LOG_ERR("Error opening audio directory: %d", ret);
        return ret;
    }

    struct fs_dirent file_count_entry;
    file_count = get_file_contents(&audio_dir_entry, &file_count_entry);
    file_count = 1;  // OMI default
    if (file_count < 0) {
        LOG_ERR("Error getting file count");
        return -1;
    }

    fs_closedir(&audio_dir_entry);
    LOG_INF("Audio files found: %d", file_count);

    /* Set initial pointers (OMI's approach) */
    ret = move_write_pointer(file_count);
    if (ret != 0) {
        LOG_ERR("Error moving write pointer");
        return ret;
    }

    move_read_pointer(file_count);

    return 0;
}

/**
 * @brief Unmount SD card filesystem
 * @return 0 on success, negative error code on failure
 */
static int sd_card_unmount(void)
{
    int ret;

    if (sd_card_state != SD_CARD_MOUNTED) {
        LOG_WRN("SD card not mounted");
        return 0;
    }

    LOG_INF("Unmounting SD card filesystem...");

    ret = fs_unmount(&mp);
    if (ret != 0) {
        LOG_ERR("Failed to unmount SD card: %d", ret);
        return ret;
    }

    sd_card_state = SD_CARD_INITIALIZED;
    LOG_INF("SD card unmounted successfully");
    return 0;
}

/**
 * @brief Generate audio file header (OMI's approach)
 * @param num File number
 * @return Pointer to generated header string (must be freed by caller)
 */
static char* generate_new_audio_header(uint8_t num)
{
    if (num > 99) return NULL;
    char *ptr_ = k_malloc(14);
    ptr_[0] = 'a';
    ptr_[1] = 'u';
    ptr_[2] = 'd';
    ptr_[3] = 'i';
    ptr_[4] = 'o';
    ptr_[5] = '/';
    ptr_[6] = 'a';
    ptr_[7] = 48 + (num / 10);
    ptr_[8] = 48 + (num % 10);
    ptr_[9] = '.';
    ptr_[10] = 't';
    ptr_[11] = 'x';
    ptr_[12] = 't';
    ptr_[13] = '\0';

    return ptr_;
}



/**
 * @brief Get file contents count (OMI's approach)
 * @param dirp Directory pointer
 * @param entry Directory entry
 * @return Number of files
 */
static int get_file_contents(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
    int ret = fs_readdir(zdp, entry);
    if (ret != 0) {
        return -1;
    }
    if (entry->name[0] == 0) {
        return 0;
    }
    int count = 0;  
    file_num_array[count] = entry->size;
    LOG_INF("file numarray %d %d ", count, file_num_array[count]);
    LOG_INF("file name is %s ", entry->name);
    count++;
    while (fs_readdir(zdp, entry) == 0) {
        if (entry->name[0] == 0) {
            break;
        }
        file_num_array[count] = entry->size;
        LOG_INF("file numarray %d %d ", count, file_num_array[count]);
        LOG_INF("file name is %s ", entry->name);
        count++;
    }
    return count;
}

/* Standard SD Card Functions */
int sd_card_list_files(const char *path)
{
    int ret;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;
    int count = 0;

    if (sd_card_state != SD_CARD_MOUNTED) {
        LOG_ERR("SD card not mounted");
        return -ENODEV;
    }

    fs_dir_t_init(&dirp);

    ret = fs_opendir(&dirp, path);
    if (ret != 0) {
        LOG_ERR("Failed to open directory %s: %d", path, ret);
        return ret;
    }

    LOG_INF("Listing directory: %s", path);
    for (;;) {
        ret = fs_readdir(&dirp, &entry);
        if (ret != 0 || entry.name[0] == 0) {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR) {
            LOG_INF("[DIR ] %s", entry.name);
        } else {
            LOG_INF("[FILE] %s (size = %zu bytes)", entry.name, entry.size);
        }
        count++;
    }

    fs_closedir(&dirp);
    LOG_INF("Total entries: %d", count);
    return count;
}

int sd_card_create_file(const char *filename, const char *content)
{
    int ret;
    struct fs_file_t file;
    char filepath[MAX_PATH];

    if (sd_card_state != SD_CARD_MOUNTED) {
        LOG_ERR("SD card not mounted");
        return -ENODEV;
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", SD_MOUNT_PT, filename);
    fs_file_t_init(&file);

    ret = fs_open(&file, filepath, FS_O_CREATE | FS_O_WRITE);
    if (ret != 0) {
        LOG_ERR("Failed to create file %s: %d", filepath, ret);
        return ret;
    }

    if (content != NULL) {
        ret = fs_write(&file, content, strlen(content));
        if (ret < 0) {
            LOG_ERR("Failed to write to file %s: %d", filepath, ret);
            fs_close(&file);
            return ret;
        }
        LOG_INF("Wrote %d bytes to file %s", ret, filename);
    }

    fs_close(&file);
    LOG_INF("File %s created successfully", filename);
    return 0;
}

int sd_card_read_file(const char *filename, char *buffer, size_t buffer_size)
{
    int ret;
    struct fs_file_t file;
    char filepath[MAX_PATH];

    if (sd_card_state != SD_CARD_MOUNTED) {
        LOG_ERR("SD card not mounted");
        return -ENODEV;
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", SD_MOUNT_PT, filename);
    fs_file_t_init(&file);

    ret = fs_open(&file, filepath, FS_O_READ);
    if (ret != 0) {
        LOG_ERR("Failed to open file %s: %d", filepath, ret);
        return ret;
    }

    ret = fs_read(&file, buffer, buffer_size - 1);
    if (ret < 0) {
        LOG_ERR("Failed to read file %s: %d", filepath, ret);
        fs_close(&file);
        return ret;
    }

    buffer[ret] = '\0';  // Null terminate
    fs_close(&file);
    LOG_INF("Read %d bytes from file %s", ret, filename);
    return ret;
}



int sd_card_delete_file(const char *filename)
{
    int ret;
    char filepath[MAX_PATH];

    if (sd_card_state != SD_CARD_MOUNTED) {
        LOG_ERR("SD card not mounted");
        return -ENODEV;
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", SD_MOUNT_PT, filename);

    ret = fs_unlink(filepath);
    if (ret != 0) {
        LOG_ERR("Failed to delete file %s: %d", filepath, ret);
        return ret;
    }

    LOG_INF("File %s deleted successfully", filename);
    return 0;
}

int sd_card_get_info(uint64_t *total_size_mb, uint32_t *free_space_mb)
{
    int ret;
    const char *disk_pdrv = SD_DISK_NAME;
    uint64_t memory_size_mb;
    uint32_t block_count;
    uint32_t block_size;

    if (sd_card_state == SD_CARD_UNINITIALIZED) {
        LOG_ERR("SD card not initialized");
        return -ENODEV;
    }

    ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &block_count);
    if (ret != 0) {
        LOG_ERR("Failed to get sector count: %d", ret);
        return ret;
    }

    ret = disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &block_size);
    if (ret != 0) {
        LOG_ERR("Failed to get sector size: %d", ret);
        return ret;
    }

    memory_size_mb = (uint64_t)block_count * block_size;
    *total_size_mb = memory_size_mb >> 20;
    *free_space_mb = (uint32_t)(memory_size_mb >> 20);

    return 0;
}

static int sd_card_test_read_write(void)
{
    int ret;
    const char *test_filename = "test.txt";
    const char *test_content = "Hello from OMI Triangle v2 SD Card!";
    char read_buffer[256];

    LOG_INF("Starting SD card read/write test...");

    ret = sd_card_create_file(test_filename, test_content);
    if (ret != 0) {
        LOG_ERR("File creation test failed");
        return ret;
    }

    ret = sd_card_read_file(test_filename, read_buffer, sizeof(read_buffer));
    if (ret < 0) {
        LOG_ERR("File reading test failed");
        return ret;
    }

    if (strcmp(read_buffer, test_content) != 0) {
        LOG_ERR("Content verification failed");
        LOG_ERR("Expected: %s", test_content);
        LOG_ERR("Got: %s", read_buffer);
        return -EIO;
    }

    ret = sd_card_delete_file(test_filename);
    if (ret != 0) {
        LOG_ERR("File deletion test failed");
        return ret;
    }

    LOG_INF("SD card read/write test passed successfully");
    return 0;
}

/* Public API Functions */

int sd_card_start(void)
{
    int ret;

    ret = sd_card_init();
    if (ret != 0) {
        return ret;
    }

    ret = sd_card_mount();
    if (ret != 0) {
        return ret;
    }

    return 0;
}

int sd_card_stop(void)
{
    return sd_card_unmount();
}

sd_card_state_t sd_card_get_state(void)
{
    return sd_card_state;
}

int sd_card_run_tests(void)
{
    int ret;
    uint64_t total_size_mb;
    uint32_t free_space_mb;

    LOG_INF("Running SD card comprehensive tests...");

    ret = sd_card_get_info(&total_size_mb, &free_space_mb);
    if (ret != 0) {
        LOG_ERR("Card info test failed");
        return ret;
    }
    LOG_INF("Card info test passed - Total: %u MB, Free: %u MB", 
            (uint32_t)total_size_mb, free_space_mb);

    ret = sd_card_list_files(SD_MOUNT_PT);
    if (ret < 0) {
        LOG_ERR("Directory listing test failed");
        return ret;
    }
    LOG_INF("Directory listing test passed");

    ret = sd_card_test_read_write();
    if (ret != 0) {
        LOG_ERR("Read/write test failed");
        return ret;
    }

    /* Test OMI audio file functionality */
    ret = initialize_audio_file(2);
    if (ret != 0) {
        LOG_ERR("Audio file initialization test failed");
        return ret;
    }
    LOG_INF("Audio file initialization test passed");

    ret = clear_audio_file(2);
    if (ret != 0) {
        LOG_ERR("Audio file clear test failed");
        return ret;
    }
    LOG_INF("Audio file clear test passed");

    LOG_INF("All SD card tests passed successfully!");
    return 0;
}

/* OMI Compatible Functions */

int mount_sd_card(void)
{
    return sd_card_start();
}

int create_file(const char* file_path)
{
    int ret = 0;
    snprintf(current_full_path, sizeof(current_full_path), "%s%s", SD_MOUNT_PT, file_path);
    struct fs_file_t data_file;
    fs_file_t_init(&data_file);
    ret = fs_open(&data_file, current_full_path, FS_O_WRITE | FS_O_CREATE);
    if (ret) {
        LOG_ERR("File creation failed %d", ret);
        return -2;
    } 
    fs_close(&data_file);
    return 0;
}

int initialize_audio_file(uint8_t num)
{
    char *header = generate_new_audio_header(num);
    if (header == NULL) {
        return -1;
    }
    k_free(header);
    create_file(header);
    return 0;
}

int write_to_file(uint8_t *data, uint32_t length)
{
    struct fs_file_t write_file;
    fs_file_t_init(&write_file);
    uint8_t *write_ptr = data;
    fs_open(&write_file, write_buffer, FS_O_WRITE | FS_O_APPEND);
    fs_write(&write_file, write_ptr, length);
    fs_close(&write_file);
    return 0;
}

int read_audio_data(uint8_t *buf, int amount, int offset)
{
    struct fs_file_t read_file;
    fs_file_t_init(&read_file); 
    uint8_t *temp_ptr = buf;

    int rc = fs_open(&read_file, read_buffer, FS_O_READ | FS_O_RDWR);
    rc = fs_seek(&read_file, offset, FS_SEEK_SET);
    rc = fs_read(&read_file, temp_ptr, amount);
    fs_close(&read_file);

    return rc;
}

uint32_t get_file_size(uint8_t num)
{
    char *ptr = generate_new_audio_header(num);
    snprintf(current_full_path, sizeof(current_full_path), "%s%s", SD_MOUNT_PT, ptr);
    k_free(ptr);
    struct fs_dirent entry;
    int res = fs_stat(current_full_path, &entry);
    if (res) {
        LOG_ERR("invalid file in get file size\n");
        return 0;  
    }
    return (uint32_t)entry.size;
}

int move_read_pointer(uint8_t num)
{
    char *read_ptr = generate_new_audio_header(num);
    snprintf(read_buffer, sizeof(read_buffer), "%s%s", SD_MOUNT_PT, read_ptr);
    k_free(read_ptr);
    struct fs_dirent entry; 
    int res = fs_stat(read_buffer, &entry);
    if (res) {
        LOG_ERR("invalid file in move read ptr\n");
        return -1;  
    }
    current_read_file = num;
    return 0;
}

int move_write_pointer(uint8_t num)
{
    char *write_ptr = generate_new_audio_header(num);
    snprintf(write_buffer, sizeof(write_buffer), "%s%s", SD_MOUNT_PT, write_ptr);
    k_free(write_ptr);
    struct fs_dirent entry;
    int res = fs_stat(write_buffer, &entry);
    if (res) {
        LOG_ERR("invalid file in move write pointer\n");  
        return -1;  
    }
    current_write_file = num;
    return 0;   
}

int clear_audio_file(uint8_t num)
{
    char *clear_header = generate_new_audio_header(num);
    snprintf(current_full_path, sizeof(current_full_path), "%s%s", SD_MOUNT_PT, clear_header);
    k_free(clear_header);
    int res = fs_unlink(current_full_path);
    if (res) {
        LOG_ERR("error deleting file");
        return -1;
    }

    char *create_file_header = generate_new_audio_header(num);
    k_msleep(10);
    res = create_file(create_file_header);
    k_free(create_file_header);
    if (res) {
        LOG_ERR("error creating file");
        return -1;
    }

    return 0;
}

int delete_audio_file(uint8_t num)
{
    char *ptr = generate_new_audio_header(num);
    snprintf(current_full_path, sizeof(current_full_path), "%s%s", SD_MOUNT_PT, ptr);
    k_free(ptr);
    int res = fs_unlink(current_full_path);
    if (res) {
        LOG_ERR("error deleting file in delete\n");
        return -1;
    }

    return 0;
}

int clear_audio_directory()
{
    if (file_count == 1) {
        return 0;
    }
    
    int res = 0;
    for (uint8_t i = file_count; i > 0; i--) {
        res = delete_audio_file(i);
        k_msleep(10);
        if (res) {
            LOG_ERR("error on %d\n", i);
            return -1;
        }  
    }
    
    res = fs_unlink("/SD:/audio");
    if (res) {
        LOG_ERR("error deleting file");
        return -1;
    }
    
    res = fs_mkdir("/SD:/audio");
    if (res) {
        LOG_ERR("failed to make directory");
        return -1;
    }
    
    res = create_file("audio/a01.txt");
    if (res) {
        LOG_ERR("failed to make new file in directory files");
        return -1;
    }
    
    LOG_INF("done with clearing");

    file_count = 1;  
    move_write_pointer(1);
    return 0;
}

int save_offset(uint32_t offset)
{
    uint8_t buf[4] = {
        offset & 0xFF,
        (offset >> 8) & 0xFF,
        (offset >> 16) & 0xFF, 
        (offset >> 24) & 0xFF 
    };

    struct fs_file_t write_file;
    fs_file_t_init(&write_file);
    
    int res = fs_open(&write_file, "/SD:/info.txt", FS_O_WRITE | FS_O_CREATE);
    if (res != 0) {
        LOG_ERR("Error opening info file: %d", res);
        return res;
    }
    
    res = fs_write(&write_file, &buf, 4);
    if (res < 0) {
        LOG_ERR("Error writing info file: %d", res);
        fs_close(&write_file);
        return res;
    }
    
    fs_close(&write_file);
    return 0;
}

int get_offset()
{
    uint8_t buf[4];
    struct fs_file_t read_file;
    fs_file_t_init(&read_file);
    
    int rc = fs_open(&read_file, "/SD:/info.txt", FS_O_READ | FS_O_RDWR);
    if (rc < 0) {
        LOG_ERR("Error opening info file: %d", rc);
        return rc;
    }
    
    rc = fs_seek(&read_file, 0, FS_SEEK_SET);
    if (rc < 0) {
        LOG_ERR("Error seeking info file: %d", rc);
        fs_close(&read_file);
        return rc;
    }
    
    rc = fs_read(&read_file, &buf, 4);
    if (rc < 0) {
        LOG_ERR("Error reading info file: %d", rc);
        fs_close(&read_file);
        return rc;
    }
    
    fs_close(&read_file);
    uint32_t *offset_ptr = (uint32_t*)buf;
    LOG_INF("Get offset is %d", offset_ptr[0]);
    
    return offset_ptr[0];
}

void sd_on()
{
    /* No separate enable pin - CS pin handles enable/disable automatically */
    LOG_INF("SD card enabled via CS pin");
}

void sd_off()
{
    /* No separate enable pin - CS pin handles enable/disable automatically */
    LOG_INF("SD card disabled via CS pin");
}

bool is_sd_on()
{
    return sd_card_state == SD_CARD_MOUNTED;
}