/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "pwm_audio.h"
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <math.h>
#include <stdlib.h>

/* Define M_PI if not already defined */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LOG_MODULE_REGISTER(pwm_audio, CONFIG_LOG_DEFAULT_LEVEL);

/* PWM device specifications */
static const struct device *pwm_audio_l = DEVICE_DT_GET(DT_NODELABEL(pwm0));
static const struct device *pwm_audio_r = DEVICE_DT_GET(DT_NODELABEL(pwm1));

/* Global state */
static uint8_t current_volume = PWM_AUDIO_MAX_VOLUME;
static bool is_muted = false;
static bool is_initialized = false;

/* Timer for anti-pop ramping */
static void mute_ramp_callback(struct k_timer *timer);
K_TIMER_DEFINE(mute_ramp_timer, mute_ramp_callback, NULL);

/* Convert 16-bit audio sample to PWM duty cycle with enhanced quality */
static uint32_t audio_sample_to_pwm(int16_t sample)
{
    /* Apply volume scaling with higher precision */
    int32_t scaled = ((int32_t)sample * current_volume) / 256;
    
    /* Enhanced clipping protection for PAM8403 */
    if (scaled > 127) scaled = 127;
    if (scaled < -128) scaled = -128;
    
    /* Convert to unsigned 8-bit (0-255) with center at 128 */
    uint8_t unsigned_sample = (uint8_t)(scaled + 128);
    
    /* Convert to PWM pulse width in nanoseconds with higher precision */
    uint32_t pulse_width = (uint32_t)((unsigned_sample * PWM_AUDIO_PERIOD_NS) / 256);
    
    /* Ensure minimum pulse width to avoid PWM artifacts */
    if (pulse_width < PWM_AUDIO_PERIOD_NS / 256) {
        pulse_width = PWM_AUDIO_PERIOD_NS / 256;
    }
    if (pulse_width > PWM_AUDIO_PERIOD_NS - (PWM_AUDIO_PERIOD_NS / 256)) {
        pulse_width = PWM_AUDIO_PERIOD_NS - (PWM_AUDIO_PERIOD_NS / 256);
    }
    
    return pulse_width;
}

/* Anti-pop mute ramp function */
static void mute_ramp_callback(struct k_timer *timer)
{
    static uint8_t ramp_step = 0;
    static uint8_t target_volume = 0;
    static bool ramping_to_mute = false;
    
    if (ramp_step == 0) {
        /* Start of ramp */
        if (is_muted) {
            /* Ramping to mute */
            target_volume = 0;
            ramping_to_mute = true;
        } else {
            /* Ramping to unmute */
            target_volume = PWM_AUDIO_MAX_VOLUME;
            ramping_to_mute = false;
        }
    }
    
    /* Calculate current volume for this step */
    uint8_t step_volume = (target_volume * ramp_step) / PWM_AUDIO_MUTE_RAMP_STEPS;
    if (ramping_to_mute) {
        step_volume = current_volume - step_volume;
    }
    
    /* Apply volume */
    pwm_audio_set_volume(step_volume);
    
    ramp_step++;
    
    if (ramp_step >= PWM_AUDIO_MUTE_RAMP_STEPS) {
        /* Ramp complete */
        ramp_step = 0;
        k_timer_stop(timer);
        
        if (ramping_to_mute) {
            /* Set PWM to 50% duty cycle (silence) */
            int err_l = pwm_set(pwm_audio_l, 0, PWM_AUDIO_PERIOD_NS, PWM_AUDIO_PERIOD_NS / 2, 0);
            int err_r = pwm_set(pwm_audio_r, 0, PWM_AUDIO_PERIOD_NS, PWM_AUDIO_PERIOD_NS / 2, 0);
            if (err_l || err_r) {
                LOG_WRN("Failed to set PWM silence during mute ramp");
            }
        }
    }
}

int pwm_audio_init(void)
{
    int err;
    
    if (is_initialized) {
        return 0;
    }
    
    LOG_INF("Initializing PWM audio for PAM8403");
    
    /* Check if PWM devices are ready */
    if (!device_is_ready(pwm_audio_l)) {
        LOG_ERR("Left channel PWM device is not ready");
        return -ENODEV;
    }
    
    if (!device_is_ready(pwm_audio_r)) {
        LOG_ERR("Right channel PWM device is not ready");
        return -ENODEV;
    }
    
    /* Set initial PWM to 50% duty cycle (silence) BEFORE enabling PAM8403 */
    err = pwm_set(pwm_audio_l, 0, PWM_AUDIO_PERIOD_NS, PWM_AUDIO_PERIOD_NS / 2, 0);
    if (err) {
        LOG_ERR("Failed to set left channel PWM: %d", err);
        return err;
    }
    
    err = pwm_set(pwm_audio_r, 0, PWM_AUDIO_PERIOD_NS, PWM_AUDIO_PERIOD_NS / 2, 0);
    if (err) {
        LOG_ERR("Failed to set right channel PWM: %d", err);
        return err;
    }
    
    /* Initialize PAM8403 with anti-pop sequence */
    err = pam8403_init();
    if (err) {
        LOG_ERR("Failed to initialize PAM8403: %d", err);
        return err;
    }
    
    /* Small delay to let PAM8403 stabilize */
    k_sleep(K_MSEC(10));
    
    /* Start with muted state */
    is_muted = true;
    is_initialized = true;
    
    LOG_INF("PWM audio initialized successfully");
    return 0;
}

int pwm_audio_play(const int16_t *buffer, size_t samples)
{
    if (!is_initialized) {
        LOG_ERR("PWM audio not initialized");
        return -ENODEV;
    }
    
    if (is_muted) {
        LOG_WRN("Audio is muted, not playing");
        return 0;
    }
    
    int err;
    size_t i;
    
    /* Process stereo samples (interleaved L/R) */
    for (i = 0; i < samples && i + 1 < samples; i += 2) {
        int16_t left_sample = buffer[i];
        int16_t right_sample = buffer[i + 1];
        
        /* Convert to PWM duty cycles */
        uint32_t left_pulse = audio_sample_to_pwm(left_sample);
        uint32_t right_pulse = audio_sample_to_pwm(right_sample);
        
        /* Set PWM outputs */
        err = pwm_set(pwm_audio_l, 0, PWM_AUDIO_PERIOD_NS, left_pulse, 0);
        if (err) {
            LOG_ERR("Failed to set left PWM: %d", err);
            return err;
        }
        
        err = pwm_set(pwm_audio_r, 0, PWM_AUDIO_PERIOD_NS, right_pulse, 0);
        if (err) {
            LOG_ERR("Failed to set right PWM: %d", err);
            return err;
        }
        
        /* Small delay to maintain sample rate */
        k_busy_wait(125); // 8kHz = 125μs per sample
    }
    
    return 0;
}

int pwm_audio_play_mono(const int16_t *buffer, size_t samples)
{
    if (!is_initialized) {
        LOG_ERR("PWM audio not initialized");
        return -ENODEV;
    }
    
    if (is_muted) {
        LOG_WRN("Audio is muted, not playing");
        return 0;
    }
    
    int err;
    size_t i;
    
    /* Process mono samples (duplicate to both channels) */
    for (i = 0; i < samples; i++) {
        int16_t sample = buffer[i];
        
        /* Convert to PWM duty cycle */
        uint32_t pulse = audio_sample_to_pwm(sample);
        
        /* Set both PWM outputs to same value */
        err = pwm_set(pwm_audio_l, 0, PWM_AUDIO_PERIOD_NS, pulse, 0);
        if (err) {
            LOG_ERR("Failed to set left PWM: %d", err);
            return err;
        }
        
        err = pwm_set(pwm_audio_r, 0, PWM_AUDIO_PERIOD_NS, pulse, 0);
        if (err) {
            LOG_ERR("Failed to set right PWM: %d", err);
            return err;
        }
        
        /* Small delay to maintain sample rate */
        k_busy_wait(125); // 8kHz = 125μs per sample
    }
    
    return 0;
}

void pwm_audio_mute(void)
{
    if (!is_initialized) {
        return;
    }
    
    if (!is_muted) {
        LOG_INF("Muting audio with anti-pop ramp");
        is_muted = true;
        
        /* Start mute ramp */
        k_timer_start(&mute_ramp_timer, 
                     K_MSEC(PWM_AUDIO_MUTE_RAMP_MS / PWM_AUDIO_MUTE_RAMP_STEPS), 
                     K_MSEC(PWM_AUDIO_MUTE_RAMP_MS / PWM_AUDIO_MUTE_RAMP_STEPS));
    }
}

void pwm_audio_unmute(void)
{
    if (!is_initialized) {
        return;
    }
    
    if (is_muted) {
        LOG_INF("Unmuting audio with anti-pop ramp");
        is_muted = false;
        
        /* Start unmute ramp */
        k_timer_start(&mute_ramp_timer, 
                     K_MSEC(PWM_AUDIO_MUTE_RAMP_MS / PWM_AUDIO_MUTE_RAMP_STEPS), 
                     K_MSEC(PWM_AUDIO_MUTE_RAMP_MS / PWM_AUDIO_MUTE_RAMP_STEPS));
    }
}

void pwm_audio_set_volume(uint8_t volume)
{
    if (volume > PWM_AUDIO_MAX_VOLUME) {
        volume = PWM_AUDIO_MAX_VOLUME;
    }
    
    current_volume = volume;
    LOG_DBG("Volume set to %d", volume);
}

int pwm_audio_generate_tone(int16_t *buffer, size_t samples, float frequency, float amplitude)
{
    if (!buffer || samples == 0) {
        return -EINVAL;
    }
    
    /* Clamp amplitude to 0.0 - 1.0 */
    if (amplitude > 1.0f) amplitude = 1.0f;
    if (amplitude < 0.0f) amplitude = 0.0f;
    
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / PWM_AUDIO_SAMPLE_RATE;
        float sample = sinf(2.0f * (float)M_PI * frequency * t) * amplitude;
        
        /* Convert to 16-bit signed integer */
        buffer[i] = (int16_t)(sample * 32767.0f);
    }
    
    return 0;
}

/* PAM8403 specific functions */
int pam8403_init(void)
{
    int err;
    
    LOG_INF("Initializing PAM8403 amplifier");
    
    /* Configure shutdown pin using devicetree */
    if (gpio_is_ready_dt(&pam8403_shutdown_pin)) {
        err = gpio_pin_configure_dt(&pam8403_shutdown_pin, GPIO_OUTPUT_INACTIVE);
        if (err) {
            LOG_ERR("Failed to configure shutdown pin: %d", err);
            return err;
        }
    } else {
        LOG_ERR("PAM8403 shutdown pin not ready");
        return -ENODEV;
    }
    
    /* Configure gain pins if available */
    if (gpio_is_ready_dt(&pam8403_gain0_pin)) {
        err = gpio_pin_configure_dt(&pam8403_gain0_pin, GPIO_OUTPUT_INACTIVE);
        if (err) {
            LOG_WRN("Failed to configure gain0 pin: %d", err);
        }
    }
    
    if (gpio_is_ready_dt(&pam8403_gain1_pin)) {
        err = gpio_pin_configure_dt(&pam8403_gain1_pin, GPIO_OUTPUT_INACTIVE);
        if (err) {
            LOG_WRN("Failed to configure gain1 pin: %d", err);
        }
    }
    
    /* Set conservative default gain (15dB) to avoid clipping */
    pam8403_set_gain(PAM8403_GAIN_15DB);
    
    /* Wake up the amplifier with anti-pop sequence */
    pam8403_wakeup();
    
    /* Additional delay for PAM8403 to fully wake up */
    k_sleep(K_MSEC(5));
    
    LOG_INF("PAM8403 initialized successfully");
    return 0;
}

void pam8403_shutdown(void)
{
    LOG_INF("Shutting down PAM8403");
    int err = gpio_pin_set_dt(&pam8403_shutdown_pin, 0);
    if (err) {
        LOG_ERR("Failed to shutdown PAM8403: %d", err);
    }
}

void pam8403_wakeup(void)
{
    LOG_INF("Waking up PAM8403");
    int err = gpio_pin_set_dt(&pam8403_shutdown_pin, 1);
    if (err) {
        LOG_ERR("Failed to wake up PAM8403: %d", err);
    }
}

void pam8403_set_gain(uint8_t gain_level)
{
    /* PAM8403 gain settings:
     * 0: 6dB, 1: 15dB, 2: 20dB, 3: 24dB
     */
    if (gain_level > 3) {
        gain_level = 1; // Default to 15dB
    }
    
    if (gpio_is_ready_dt(&pam8403_gain0_pin)) {
        int err = gpio_pin_set_dt(&pam8403_gain0_pin, gain_level & 0x01);
        if (err) {
            LOG_WRN("Failed to set gain0 pin: %d", err);
        }
    }
    
    if (gpio_is_ready_dt(&pam8403_gain1_pin)) {
        int err = gpio_pin_set_dt(&pam8403_gain1_pin, (gain_level >> 1) & 0x01);
        if (err) {
            LOG_WRN("Failed to set gain1 pin: %d", err);
        }
    }
    
    LOG_INF("PAM8403 gain set to level %d", gain_level);
}

/* Memory slab for audio buffers (like Omi) */
#define MAX_BLOCK_SIZE 10000  // Same as Omi
#define BLOCK_COUNT 2         // Same as Omi
K_MEM_SLAB_DEFINE_STATIC(audio_mem_slab, MAX_BLOCK_SIZE, BLOCK_COUNT, 2);

/* Audio quality test functions */

int pwm_audio_test_sine_wave(float frequency, uint8_t volume, uint16_t duration_ms)
{
    if (!is_initialized) {
        LOG_ERR("PWM audio not initialized");
        return -ENODEV;
    }
    
    LOG_INF("Testing sine wave: %.1f Hz, volume %d, duration %d ms", (double)frequency, volume, duration_ms);
    
    uint8_t original_volume = current_volume;
    pwm_audio_set_volume(volume);
    
    /* Use memory slab allocation (like Omi) */
    size_t samples = (PWM_AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    if (samples > MAX_BLOCK_SIZE / sizeof(int16_t)) {
        samples = MAX_BLOCK_SIZE / sizeof(int16_t);
        LOG_WRN("Test duration limited to %d ms", (int)((MAX_BLOCK_SIZE / sizeof(int16_t) * 1000) / PWM_AUDIO_SAMPLE_RATE));
    }
    
    int16_t *buffer;
    if (k_mem_slab_alloc(&audio_mem_slab, (void **)&buffer, K_NO_WAIT) != 0) {
        LOG_ERR("Failed to allocate audio buffer");
        return -ENOMEM;
    }
    
    /* Generate sine wave */
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / PWM_AUDIO_SAMPLE_RATE;
        float sample = sinf(2.0f * (float)M_PI * frequency * t);
        buffer[i] = (int16_t)(sample * 16384.0f); // Half amplitude to avoid clipping
    }
    
    /* Play the test tone */
    int ret = pwm_audio_play_mono(buffer, samples);
    
    /* Free the buffer */
    k_mem_slab_free(&audio_mem_slab, buffer);
    
    pwm_audio_set_volume(original_volume);
    
    return ret;
}

int pwm_audio_test_sweep(uint16_t start_freq, uint16_t end_freq, uint16_t duration_ms)
{
    if (!is_initialized) {
        LOG_ERR("PWM audio not initialized");
        return -ENODEV;
    }
    
    LOG_INF("Testing frequency sweep: %d Hz to %d Hz, duration %d ms", start_freq, end_freq, duration_ms);
    
    /* Use memory slab allocation (like Omi) */
    size_t samples = (PWM_AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    if (samples > MAX_BLOCK_SIZE / sizeof(int16_t)) {
        samples = MAX_BLOCK_SIZE / sizeof(int16_t);
        LOG_WRN("Test duration limited to %d ms", (int)((MAX_BLOCK_SIZE / sizeof(int16_t) * 1000) / PWM_AUDIO_SAMPLE_RATE));
    }
    
    int16_t *buffer;
    if (k_mem_slab_alloc(&audio_mem_slab, (void **)&buffer, K_NO_WAIT) != 0) {
        LOG_ERR("Failed to allocate audio buffer");
        return -ENOMEM;
    }
    
    /* Generate frequency sweep */
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / samples; // Normalized time 0-1
        float freq = start_freq + (end_freq - start_freq) * t;
        float phase = 2.0f * (float)M_PI * freq * (float)i / PWM_AUDIO_SAMPLE_RATE;
        float sample = sinf(phase);
        buffer[i] = (int16_t)(sample * 8192.0f); // Quarter amplitude
    }
    
    /* Play the sweep */
    int ret = pwm_audio_play_mono(buffer, samples);
    
    /* Free the buffer */
    k_mem_slab_free(&audio_mem_slab, buffer);
    
    return ret;
}

int pwm_audio_test_white_noise(uint16_t duration_ms)
{
    if (!is_initialized) {
        LOG_ERR("PWM audio not initialized");
        return -ENODEV;
    }
    
    LOG_INF("Testing white noise, duration %d ms", duration_ms);
    
    /* Use memory slab allocation (like Omi) */
    size_t samples = (PWM_AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    if (samples > MAX_BLOCK_SIZE / sizeof(int16_t)) {
        samples = MAX_BLOCK_SIZE / sizeof(int16_t);
        LOG_WRN("Test duration limited to %d ms", (int)((MAX_BLOCK_SIZE / sizeof(int16_t) * 1000) / PWM_AUDIO_SAMPLE_RATE));
    }
    
    int16_t *buffer;
    if (k_mem_slab_alloc(&audio_mem_slab, (void **)&buffer, K_NO_WAIT) != 0) {
        LOG_ERR("Failed to allocate audio buffer");
        return -ENOMEM;
    }
    
    /* Generate white noise (simple random) */
    for (size_t i = 0; i < samples; i++) {
        int16_t noise = (int16_t)((rand() % 65536) - 32768);
        buffer[i] = noise / 4; // Reduce amplitude
    }
    
    /* Play the noise */
    int ret = pwm_audio_play_mono(buffer, samples);
    
    /* Free the buffer */
    k_mem_slab_free(&audio_mem_slab, buffer);
    
    return ret;
}

void pwm_audio_print_stats(void)
{
    LOG_INF("PWM Audio Statistics:");
    LOG_INF("  Sample Rate: %d Hz", PWM_AUDIO_SAMPLE_RATE);
    LOG_INF("  PWM Frequency: %d Hz", PWM_AUDIO_PWM_FREQ);
    LOG_INF("  PWM Period: %llu ns", PWM_AUDIO_PERIOD_NS);
    LOG_INF("  Max Volume: %d/256", PWM_AUDIO_MAX_VOLUME);
    LOG_INF("  Current Volume: %d/256", current_volume);
    LOG_INF("  Muted: %s", is_muted ? "Yes" : "No");
    LOG_INF("  Initialized: %s", is_initialized ? "Yes" : "No");
}
