#ifndef SD_CARD_H
#define SD_CARD_H

#include <zephyr/kernel.h>
#include <stdint.h>

/* SD Card States */
typedef enum {
    SD_CARD_UNINITIALIZED,
    SD_CARD_INITIALIZED,
    SD_CARD_MOUNTED,
    SD_CARD_ERROR
} sd_card_state_t;

/* Enhanced Public API Functions */

/**
 * @brief Initialize and mount SD card
 * @return 0 on success, negative error code on failure
 */
int sd_card_start(void);

/**
 * @brief Stop SD card (unmount)
 * @return 0 on success, negative error code on failure
 */
int sd_card_stop(void);

/**
 * @brief Get SD card status
 * @return SD card state
 */
sd_card_state_t sd_card_get_state(void);

/**
 * @brief Run comprehensive SD card tests
 * @return 0 on success, negative error code on failure
 */
int sd_card_run_tests(void);

/* Standard File Operations */

/**
 * @brief Create a file with content
 * @param filename File name to create
 * @param content Content to write to file
 * @return 0 on success, negative error code on failure
 */
int sd_card_create_file(const char *filename, const char *content);

/**
 * @brief Read file content
 * @param filename File name to read
 * @param buffer Buffer to store file content
 * @param buffer_size Size of buffer
 * @return Number of bytes read on success, negative error code on failure
 */
int sd_card_read_file(const char *filename, char *buffer, size_t buffer_size);

/**
 * @brief Write content to file
 * @param filename File name to write
 * @param content Content to write
 * @return 0 on success, negative error code on failure
 */
int sd_card_write_file(const char *filename, const char *content);

/**
 * @brief Delete a file
 * @param filename File name to delete
 * @return 0 on success, negative error code on failure
 */
int sd_card_delete_file(const char *filename);

/**
 * @brief List files in directory
 * @param path Directory path to list
 * @return Number of entries on success, negative error code on failure
 */
int sd_card_list_files(const char *path);

/**
 * @brief Get SD card information
 * @param total_size_mb Pointer to store total size in MB
 * @param free_space_mb Pointer to store free space in MB
 * @return 0 on success, negative error code on failure
 */
int sd_card_get_info(uint64_t *total_size_mb, uint32_t *free_space_mb);

/* OMI Compatible Functions - Direct API Compatibility */

/**
 * @brief Mount the SD Card. Initializes the audio files 
 *
 * Mounts the SD Card and initializes the audio files. If the SD card does not contain those files, the 
 * function will create them.
 * 
 * @return 0 if successful, negative errno code if error
 */
int mount_sd_card(void);

/**
 * @brief Create a file
 *
 * Creates a file at the given path
 * 
 * @return 0 if successful, negative errno code if error
 */
int create_file(const char* file_path);

/**
 * @brief Initialize an audio file of number 1
 *
 * Initializes an audio file. It will be called a nn.txt, where nn is the number of the file.
 *  example: initialize_audio_file(1) will create a file called a01.txt
 * @return 0 if successful, negative errno code if error
 */
int initialize_audio_file(uint8_t num);

/**
 * @brief Write to the current audio file specified by the write pointer
 *
 * @param data Data to write
 * @param length Length of data to write
 * @return number of bytes written
 */
int write_to_file(uint8_t *data, uint32_t length);

/**
 * @brief Read from the current audio file specified by the read pointer
 *
 * @param buf Buffer to store read data
 * @param amount Amount of data to read
 * @param offset Offset in file to read from
 * @return number of bytes read
 */
int read_audio_data(uint8_t *buf, int amount, int offset);

/**
 * @brief Get the size of the specified audio file number
 *
 * @param num File number
 * @return size of the file in bytes
 */
uint32_t get_file_size(uint8_t num);

/**
 * @brief Move the read pointer to the specified audio file position
 *
 * @param num File number
 * @return 0 if successful, negative errno code if error
 */
int move_read_pointer(uint8_t num);

/**
 * @brief Move the write pointer to the specified audio file position
 *
 * @param num File number
 * @return 0 if successful, negative errno code if error
 */
int move_write_pointer(uint8_t num);

/**
 * @brief Clear the specified audio file
 *
 * @param num File number
 * @return 0 if successful, negative errno code if error
 */
int clear_audio_file(uint8_t num);

/**
 * @brief Delete the specified audio file
 *
 * @param num File number
 * @return 0 if successful, negative errno code if error
 */
int delete_audio_file(uint8_t num);

/**
 * @brief Clear the audio directory. 
 *
 * This deletes all audio files and leaves the audio directory with only one file left, a01.txt.
 * This automatically moves the read and write pointers to a01.txt.
 * @return 0 if successful, negative errno code if error
 */
int clear_audio_directory(void);

/**
 * @brief Save offset to info file
 *
 * @param offset Offset to save
 * @return 0 if successful, negative errno code if error
 */
int save_offset(uint32_t offset);

/**
 * @brief Get offset from info file
 *
 * @return Offset value or negative error code
 */
int get_offset(void);

/**
 * @brief Turn on SD card
 */
void sd_on(void);

/**
 * @brief Turn off SD card
 */
void sd_off(void);

/**
 * @brief Check if SD card is on
 *
 * @return true if SD card is mounted, false otherwise
 */
bool is_sd_on(void);

#endif /* SD_CARD_H */
