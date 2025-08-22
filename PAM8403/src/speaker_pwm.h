/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SPEAKER_PWM_H
#define SPEAKER_PWM_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <math.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include "pwm_audio.h"

/* LOG_MODULE_REGISTER is defined in speaker_pwm.c */

#define MAX_BLOCK_SIZE   10000 //24000 * 2
#define BLOCK_COUNT 2     
#define SAMPLE_FREQUENCY 8000
#define NUMBER_OF_CHANNELS 2
#define PACKET_SIZE 400
#define WORD_SIZE 16
#define NUM_CHANNELS 2
#define PI 3.14159265358979323846
/* Compatibility with original speaker interface */
extern struct device *audio_speaker; // Dummy for compatibility

/* Function prototypes - same interface as original speaker.h */
int speaker_init(void);
uint16_t speak(uint16_t len, const void *buf);
void generate_gentle_chime(int16_t *buffer, int num_samples);
int play_boot_sound(void);
void speaker_off(void);

/* Additional PWM-specific functions */
int pwm_speaker_init(void);
void pwm_speaker_set_volume(uint8_t volume);
void pwm_speaker_mute(void);
void pwm_speaker_unmute(void);

#endif /* SPEAKER_PWM_H */
