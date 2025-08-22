/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef PWM_AUDIO_H
#define PWM_AUDIO_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>

/* Audio configuration for PAM8403 - Optimized for high fidelity */
#define PWM_AUDIO_SAMPLE_RATE     16000   // 16kHz sample rate (better quality)
#define PWM_AUDIO_PWM_FREQ        1000000 // 1MHz PWM frequency (>> audio bandwidth, better SNR)
#define PWM_AUDIO_RESOLUTION      256     // 8-bit PWM resolution
#define PWM_AUDIO_PERIOD_NS       (1000000000ULL / PWM_AUDIO_PWM_FREQ)
#define PWM_AUDIO_MAX_VOLUME      180     // Max volume to avoid clipping (out of 256) - conservative

/* Anti-pop configuration - Enhanced for PAM8403 */
#define PWM_AUDIO_MUTE_RAMP_MS    100     // Longer mute/unmute ramp time (prevents pops)
#define PWM_AUDIO_MUTE_RAMP_STEPS 20      // More steps for smoother ramping

/* PAM8403 gain settings */
#define PAM8403_GAIN_6DB          0       // 6dB gain
#define PAM8403_GAIN_15DB         1       // 15dB gain (default)
#define PAM8403_GAIN_20DB         2       // 20dB gain
#define PAM8403_GAIN_24DB         3       // 24dB gain (max)

/* PAM8403 control pins using devicetree macros */
static const struct gpio_dt_spec pam8403_shutdown_pin =
    GPIO_DT_SPEC_GET_OR(DT_NODELABEL(pam8403_shutdown_pin), gpios, {0});
static const struct gpio_dt_spec pam8403_gain0_pin =
    GPIO_DT_SPEC_GET_OR(DT_NODELABEL(pam8403_gain0_pin), gpios, {0});
static const struct gpio_dt_spec pam8403_gain1_pin =
    GPIO_DT_SPEC_GET_OR(DT_NODELABEL(pam8403_gain1_pin), gpios, {0});

/* PWM device aliases */
#define PWM_AUDIO_L_CHANNEL       DT_NODELABEL(pwm0)
#define PWM_AUDIO_R_CHANNEL       DT_NODELABEL(pwm1)

/* Function prototypes */
int pwm_audio_init(void);
int pwm_audio_play(const int16_t *buffer, size_t samples);
int pwm_audio_play_mono(const int16_t *buffer, size_t samples);
void pwm_audio_mute(void);
void pwm_audio_unmute(void);
void pwm_audio_set_volume(uint8_t volume);
int pwm_audio_generate_tone(int16_t *buffer, size_t samples, float frequency, float amplitude);

/* PAM8403 specific functions */
int pam8403_init(void);
void pam8403_shutdown(void);
void pam8403_wakeup(void);
void pam8403_set_gain(uint8_t gain_level);

/* Audio quality test functions */
int pwm_audio_test_sine_wave(float frequency, uint8_t volume, uint16_t duration_ms);
int pwm_audio_test_sweep(uint16_t start_freq, uint16_t end_freq, uint16_t duration_ms);
int pwm_audio_test_white_noise(uint16_t duration_ms);
void pwm_audio_print_stats(void);

#endif /* PWM_AUDIO_H */
