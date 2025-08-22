/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
 
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include "pwm_audio.h"

/* Define M_PI if not already defined */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LOG_MODULE_REGISTER(PAM8403_PWM_Audio, LOG_LEVEL_INF);

/* Audio buffer for testing */
#define AUDIO_BUFFER_SIZE 8000  // 1 second at 8kHz
static int16_t audio_buffer[AUDIO_BUFFER_SIZE];

int main(void)
{
    int err;
    
    LOG_INF("PAM8403 PWM Audio Demo Starting");
    
    /* Initialize PWM audio system */
    err = pwm_audio_init();
    if (err) {
        LOG_ERR("Failed to initialize PWM audio: %d", err);
        return err;
    }
    
    /* Wait a moment for PAM8403 to stabilize */
    k_sleep(K_MSEC(100));
    
    /* Unmute with anti-pop ramp */
    pwm_audio_unmute();
    
    /* Wait for unmute ramp to complete */
    k_sleep(K_MSEC(100));
    
    /* Generate a test tone (440Hz A note) */
    LOG_INF("Generating test tone (440Hz)");
    err = pwm_audio_generate_tone(audio_buffer, AUDIO_BUFFER_SIZE, 440.0f, 0.3f);
    if (err) {
        LOG_ERR("Failed to generate tone: %d", err);
        return err;
    }
    
    /* Play the tone */
    LOG_INF("Playing test tone");
    err = pwm_audio_play_mono(audio_buffer, AUDIO_BUFFER_SIZE);
    if (err) {
        LOG_ERR("Failed to play audio: %d", err);
        return err;
    }
    
    /* Wait for audio to finish */
    k_sleep(K_MSEC(1500));
    
    /* Generate a chord (C major: C, E, G) */
    LOG_INF("Generating C major chord");
    for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
        float t = (float)i / PWM_AUDIO_SAMPLE_RATE;
        float sample = 0.0f;
        
        /* C4 (261.63 Hz) */
        sample += sinf(2.0f * (float)M_PI * 261.63f * t) * 0.2f;
        /* E4 (329.63 Hz) */
        sample += sinf(2.0f * (float)M_PI * 329.63f * t) * 0.2f;
        /* G4 (392.00 Hz) */
        sample += sinf(2.0f * (float)M_PI * 392.00f * t) * 0.2f;
        
        /* Apply envelope */
        float envelope = 1.0f;
        if (i < 1000) {
            envelope = (float)i / 1000.0f; // Fade in
        } else if (i > AUDIO_BUFFER_SIZE - 1000) {
            envelope = (float)(AUDIO_BUFFER_SIZE - i) / 1000.0f; // Fade out
        }
        
        audio_buffer[i] = (int16_t)(sample * envelope * 32767.0f);
    }
    
    /* Play the chord */
    LOG_INF("Playing C major chord");
    err = pwm_audio_play_mono(audio_buffer, AUDIO_BUFFER_SIZE);
    if (err) {
        LOG_ERR("Failed to play chord: %d", err);
        return err;
    }
    
    /* Wait for audio to finish */
    k_sleep(K_MSEC(1500));
    
    /* Print audio system statistics */
    pwm_audio_print_stats();
    
    /* Test different frequencies for quality assessment */
    LOG_INF("Testing different frequencies");
    pwm_audio_test_sine_wave(440.0f, 150, 1000);  // A4
    k_sleep(K_MSEC(500));
    pwm_audio_test_sine_wave(880.0f, 150, 1000);  // A5
    k_sleep(K_MSEC(500));
    pwm_audio_test_sine_wave(1760.0f, 150, 1000); // A6
    k_sleep(K_MSEC(500));
    
    /* Test frequency sweep to check frequency response */
    LOG_INF("Testing frequency sweep (100Hz to 2kHz)");
    pwm_audio_test_sweep(100, 2000, 3000);
    k_sleep(K_MSEC(1000));
    
    /* Test white noise for distortion detection */
    LOG_INF("Testing white noise (listen for distortion)");
    pwm_audio_test_white_noise(2000);
    k_sleep(K_MSEC(1000));
    
    /* Demonstrate volume control with quality monitoring */
    LOG_INF("Demonstrating volume control");
    for (int vol = 50; vol <= PWM_AUDIO_MAX_VOLUME; vol += 50) {
        pwm_audio_set_volume(vol);
        LOG_INF("Volume: %d", vol);
        
        /* Generate and play a short beep */
        err = pwm_audio_generate_tone(audio_buffer, 2000, 800.0f, 0.5f);
        if (!err) {
            pwm_audio_play_mono(audio_buffer, 2000);
        }
        
        k_sleep(K_MSEC(500));
    }
    
    /* Test PAM8403 gain settings */
    LOG_INF("Testing PAM8403 gain settings");
    pam8403_set_gain(PAM8403_GAIN_6DB);
    pwm_audio_test_sine_wave(1000.0f, 100, 1000);
    k_sleep(K_MSEC(500));
    
    pam8403_set_gain(PAM8403_GAIN_15DB);
    pwm_audio_test_sine_wave(1000.0f, 100, 1000);
    k_sleep(K_MSEC(500));
    
    pam8403_set_gain(PAM8403_GAIN_20DB);
    pwm_audio_test_sine_wave(1000.0f, 100, 1000);
    k_sleep(K_MSEC(500));
    
    /* Reset to default gain */
    pam8403_set_gain(PAM8403_GAIN_15DB);
    
    /* Test mute/unmute with anti-pop protection */
    LOG_INF("Testing mute/unmute with anti-pop protection");
    pwm_audio_mute();
    k_sleep(K_MSEC(1000));
    pwm_audio_unmute();
    k_sleep(K_MSEC(1000));
    
    /* Final mute with anti-pop ramp */
    LOG_INF("Final mute");
    pwm_audio_mute();
    
    /* Wait for mute ramp to complete */
    k_sleep(K_MSEC(100));
    
    LOG_INF("PAM8403 PWM Audio Quality Test Complete");
    
    return 0;
}
