/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "speaker_pwm.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <math.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include "pwm_audio.h"

/* Define PI if not already defined */
#ifndef PI
#define PI 3.14159265358979323846
#endif

LOG_MODULE_REGISTER(speaker_pwm, CONFIG_LOG_DEFAULT_LEVEL);

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 2);

/* Dummy device for compatibility with original interface */
struct device *audio_speaker = (struct device *)0x12345678; // Dummy address

static void* rx_buffer;
static void* buzz_buffer;
static int16_t *ptr2;
static int16_t *clear_ptr;

static uint16_t current_length;
static uint16_t offset;



int speaker_init() 
{
    LOG_INF("PWM Speaker init");
    
    /* Initialize PWM audio system */
    int err = pwm_audio_init();
    if (err) {
        LOG_ERR("Failed to initialize PWM audio: %d", err);
        return err;
    }
    

    
    /* Allocate memory buffers */
    err = k_mem_slab_alloc(&mem_slab, &rx_buffer, K_MSEC(200));
	if (err)
    {
		LOG_INF("Failed to allocate memory for speaker%d)", err);
        return -1;
	}

	err = k_mem_slab_alloc(&mem_slab, &buzz_buffer, K_MSEC(200));
	if (err) 
    {
		LOG_INF("Failed to allocate for chime (%d)", err);
        return -1;
	}
      
    memset(rx_buffer, 0, MAX_BLOCK_SIZE);
    memset(buzz_buffer, 0, MAX_BLOCK_SIZE);
    
    /* Unmute with anti-pop protection */
    pwm_audio_unmute();
    
    return 0;
}

uint16_t speak(uint16_t len, const void *buf) //direct from bt
{
	uint16_t amount = 0;
    amount = len;
	if (len == 4)  //if stage 1 
	{
        current_length = ((uint32_t *)buf)[0];
	    LOG_INF("About to write %u bytes", current_length);
        ptr2 = (int16_t *)rx_buffer;
        clear_ptr = (int16_t *)rx_buffer;
	}
    else 
    { //if not stage 1
        if (current_length > PACKET_SIZE) 
        {
            LOG_INF("Data length: %u", len);
            current_length = current_length - PACKET_SIZE;
            LOG_INF("remaining data: %u", current_length);

            for (int i = 0; i < (int)(len/2); i++) 
            {
                *ptr2++ = ((int16_t *)buf)[i];  
                *ptr2++ = ((int16_t *)buf)[i]; 
            }
            offset = offset + len;
        }
        else if (current_length < PACKET_SIZE) 
        {
            LOG_INF("entered the final stretch");
            LOG_INF("Data length: %u", len);
            current_length = current_length - len;
            LOG_INF("remaining data: %u", current_length);
            
            for (int i = 0; i < len/2; i++) 
            {
                *ptr2++ = ((int16_t *)buf)[i];  
                *ptr2++ = ((int16_t *)buf)[i];  
            }
            offset = offset + len;
            LOG_INF("offset: %u", offset);
            offset = 0;
            
            /* Play audio using PWM instead of I2S */
            int res = pwm_audio_play_mono(rx_buffer, MAX_BLOCK_SIZE / sizeof(int16_t));
            if (res < 0)
            {
                LOG_PRINTK("Failed to play PWM audio: %d\n", res);
            }
            
            //clear the buffer
            k_sleep(K_MSEC(4000));
            memset(clear_ptr, 0, MAX_BLOCK_SIZE);
        }
    }
    return amount;
}

void generate_gentle_chime(int16_t *buffer, int num_samples)
{
    LOG_INF("Generating gentle chime");
    const float frequencies[] = {523.25, 659.25, 783.99, 1046.50}; // C5, E5, G5, C6
    const int num_freqs = sizeof(frequencies) / sizeof(frequencies[0]);

    for (int i = 0; i < num_samples; i++) 
    { 
        float t = (float)i / SAMPLE_FREQUENCY;
        float sample = 0;
        for (int j = 0; j < num_freqs; j++) 
        {
           sample += sinf(2.0f * (float)PI * frequencies[j] * t) * (1.0f - t);
        }
        int16_t int_sample = (int16_t)(sample / num_freqs * 32767.0f * 0.5f);
        buffer[i * NUM_CHANNELS] = int_sample;
        buffer[i * NUM_CHANNELS + 1] = int_sample;
    }
    LOG_INF("Done generating gentle chime");
}

int play_boot_sound(void)
{
    int ret;
    int16_t *buffer = (int16_t *) buzz_buffer;
    const int samples_per_block = MAX_BLOCK_SIZE / (NUM_CHANNELS * sizeof(int16_t));

    generate_gentle_chime(buffer, samples_per_block);
    LOG_INF("Writing to PWM speaker");
    k_sleep(K_MSEC(100));
    
    /* Play using PWM audio instead of I2S */
    ret = pwm_audio_play_mono(buffer, samples_per_block);
    if (ret) 
    {
        LOG_ERR("Failed to play PWM audio: %d", ret);
        return ret;
    }
    
    k_sleep(K_MSEC(3000));  
    return 0;
}

void speaker_off()
{
    /* Mute PWM audio with anti-pop protection */
    pwm_audio_mute();
}

/* PWM-specific functions */
int pwm_speaker_init(void)
{
    return pwm_audio_init();
}

void pwm_speaker_set_volume(uint8_t volume)
{
    pwm_audio_set_volume(volume);
}

void pwm_speaker_mute(void)
{
    pwm_audio_mute();
}

void pwm_speaker_unmute(void)
{
    pwm_audio_unmute();
}
