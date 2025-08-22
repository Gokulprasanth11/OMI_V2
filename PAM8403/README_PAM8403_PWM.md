# PAM8403 PWM Audio System - Omi I2S Replacement

This project implements a **PAM8403 PWM audio system** that **replaces Omi's I2S speaker system**, allowing you to use existing Omi apps with your custom PAM8403 amplifier hardware.

## 🎯 Project Goal

**Replace Omi's I2S-based speaker system** with a **PAM8403 PWM audio system** while maintaining **full compatibility** with existing Omi applications and BLE protocols.

## 🔄 System Replacement Overview

### **Omi's Original System:**
```
Omi App → BLE → I2S Codec → Speakers
```

### **Your PAM8403 System:**
```
Omi App → BLE → PWM Audio → PAM8403 → Speakers
```

**The key insight:** Your system acts as a **drop-in replacement** for Omi's speaker hardware, using the same BLE interface but different audio output method.

## 📁 File Replacement Strategy

### **Files to Replace in Omi Project:**

| **Omi File** | **Your Replacement** | **Purpose** |
|--------------|---------------------|-------------|
| `omi/src/lib/dk2/speaker.c` | `PAM8403/src/pwm_audio.c` | Audio output system |
| `omi/src/lib/dk2/speaker.h` | `PAM8403/src/pwm_audio.h` | Audio API definitions |
| `omi/boards/omi/omi.overlay` | `PAM8403/boards/xiao_ble.overlay` | Hardware configuration |
| `omi/src/transport.c` (audio parts) | `PAM8403/src/main.c` | BLE audio handling |

### **Files to Keep from Omi:**
- `omi/src/transport.c` (BLE service definitions)
- `omi/src/ble.c` (BLE connection handling)
- `omi/src/battery.c` (battery management)
- All other non-audio related files

## 🔧 Integration Approach

### **1. BLE Audio Reception (Same as Omi)**
```c
// Your system receives audio exactly like Omi does
static ssize_t audio_data_write_handler(struct bt_conn *conn, 
                                       const struct bt_gatt_attr *attr,
                                       const void *buf, uint16_t len, 
                                       uint16_t offset, uint8_t flags)
{
    // Convert received audio to PWM (instead of I2S)
    int16_t *audio_samples = (int16_t *)buf;
    pwm_audio_play_mono(audio_samples, len / 2);
    return len;
}
```

### **2. Volume Control (Same as Omi)**
```c
// Omi app sends volume commands, you handle them
static ssize_t volume_write_handler(struct bt_conn *conn, 
                                   const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, 
                                   uint16_t offset, uint8_t flags)
{
    uint8_t volume = *(uint8_t*)buf;
    pwm_audio_set_volume(volume);
    
    // Automatically adjust PAM8403 gain based on volume
    if (volume < 64) {
        pam8403_set_gain(0);  // 6dB for low volume
    } else if (volume > 192) {
        pam8403_set_gain(2);  // 20dB for high volume
    } else {
        pam8403_set_gain(1);  // 15dB for normal volume
    }
    return len;
}
```

## 🎵 Audio Flow Comparison

### **Omi's I2S Flow:**
```
BLE Audio Data → I2S Buffer → I2S Codec → Speakers
```

### **Your PWM Flow:**
```
BLE Audio Data → PWM Buffer → PAM8403 → Speakers
```

**The difference:** Instead of sending audio to an I2S codec, you send it to PWM pins that drive the PAM8403 amplifier.

## 🔌 Hardware Replacement

### **Omi's Hardware:**
- **I2S Codec** (e.g., MAX98357A)
- **I2S Interface** (BCLK, LRCLK, SDIN)
- **Power Amplifier** (integrated or separate)

### **Your Hardware:**
- **PWM Outputs** (P0.03, P0.28)
- **PAM8403 Amplifier** (Class D, 3W stereo)
- **GPIO Control** (shutdown, gain pins)

## 📋 Implementation Steps

### **Step 1: Replace Audio Files**
```bash
# Copy your PWM audio system to Omi project
cp PAM8403/src/pwm_audio.c omi/src/lib/dk2/speaker.c
cp PAM8403/src/pwm_audio.h omi/src/lib/dk2/speaker.h
```

### **Step 2: Update Device Tree**
```bash
# Replace Omi's overlay with your PAM8403 configuration
cp PAM8403/boards/xiao_ble.overlay omi/boards/omi/omi.overlay
```

### **Step 3: Modify Transport Layer**
```c
// In omi/src/transport.c, replace I2S calls with PWM calls
// OLD: i2s_write(audio_speaker, buffer, size);
// NEW: pwm_audio_play_mono(buffer, samples);
```

### **Step 4: Update Build Configuration**
```conf
# In prj.conf, replace I2S config with PWM config
# OLD: CONFIG_I2S=y
# NEW: CONFIG_PWM=y
CONFIG_PWM=y
CONFIG_PWM_NRFX=y
CONFIG_GPIO=y
```

## 🎛️ PAM8403 Integration

### **Automatic Gain Control:**
```c
// Your system automatically adjusts PAM8403 gain based on volume
void handle_omi_volume_change(uint8_t volume)
{
    if (volume < 64) {
        pam8403_set_gain(0);  // 6dB - quiet environment
    } else if (volume > 192) {
        pam8403_set_gain(2);  // 20dB - loud environment
    } else {
        pam8403_set_gain(1);  // 15dB - normal listening
    }
}
```

### **Power Management:**
```c
// Automatic power management like Omi
void omi_audio_start(void)
{
    pam8403_wakeup();     // Turn on amplifier
    pwm_audio_unmute();   // Start audio output
}

void omi_audio_stop(void)
{
    pwm_audio_mute();     // Stop audio output
    pam8403_shutdown();   // Turn off amplifier
}
```

## 🔄 BLE Service Compatibility

### **Same BLE Services as Omi:**
- **Audio Service UUID**: `0x19B10000-E8F2-537E-4F6C-D104768A1214`
- **Audio Data Characteristic**: `0x19B10001-E8F2-537E-4F6C-D104768A1214`
- **Volume Control**: Same protocol as Omi
- **Status Reporting**: Same format as Omi

### **Your Implementation:**
```c
// Your BLE handlers work exactly like Omi's
static ssize_t audio_data_write_handler(struct bt_conn *conn, 
                                       const struct bt_gatt_attr *attr,
                                       const void *buf, uint16_t len, 
                                       uint16_t offset, uint8_t flags)
{
    // Convert BLE audio data to PWM output
    int16_t *audio_samples = (int16_t *)buf;
    pwm_audio_play_mono(audio_samples, len / 2);
    return len;
}
```

## 🎯 Key Advantages

### **1. Drop-in Replacement**
- **Same BLE interface** as Omi
- **Same app compatibility** - no app changes needed
- **Same audio protocols** - just different hardware

### **2. Better Hardware Control**
- **PAM8403 gain control** for different speaker types
- **Power management** with shutdown pin
- **Anti-pop protection** for clean audio

### **3. Cost Effective**
- **No I2S codec** required
- **Direct PWM connection** to PAM8403
- **Fewer components** than I2S system

## 🚀 Usage with Omi Apps

### **1. Connect Omi App**
- Your device appears as **same Omi device**
- **Same BLE services** and characteristics
- **Same connection process**

### **2. Send Audio**
- App sends audio data via BLE
- Your system converts to PWM
- PAM8403 amplifies and plays

### **3. Control Volume**
- App sends volume commands
- Your system adjusts PWM + PAM8403 gain
- Automatic optimization for best quality

## 🔧 Configuration

### **Project Configuration (`prj.conf`)**
```conf
# PWM Audio (replaces I2S)
CONFIG_PWM=y
CONFIG_PWM_NRFX=y
CONFIG_GPIO=y

# Audio and Math support
CONFIG_FPU=y
CONFIG_FP_HARDABI=y
CONFIG_CBPRINTF_FP_SUPPORT=y

# Timer support for anti-pop ramping
CONFIG_TIMER=y

# Memory slab for audio buffers (like Omi)
CONFIG_HEAP_MEM_POOL_SIZE=4096
```

### **Device Tree Overlay (`xiao_ble.overlay`)**
```dts
/ {
    /* PWM Audio Output */
    pwm0: &pwm0 {
        status = "okay";
        pinctrl-0 = <&pwm0_default>;
        pinctrl-names = "default";
    };
    
    pwm1: &pwm1 {
        status = "okay";
        pinctrl-0 = <&pwm1_default>;
        pinctrl-names = "default";
    };
    
    /* PAM8403 Control Pins */
    pam8403_shutdown_pin: pam8403-shutdown-pin {
        compatible = "nordic,gpio-pins";
        gpios = <&gpio0 29 GPIO_ACTIVE_HIGH>;
        status = "okay";
    };
    
    pam8403_gain0_pin: pam8403-gain0-pin {
        compatible = "nordic,gpio-pins";
        gpios = <&gpio0 30 GPIO_ACTIVE_HIGH>;
        status = "okay";
    };
    
    pam8403_gain1_pin: pam8403-gain1-pin {
        compatible = "nordic,gpio-pins";
        gpios = <&gpio0 31 GPIO_ACTIVE_HIGH>;
        status = "okay";
    };
};
```

## 🎵 Audio Quality

### **PWM vs I2S Comparison:**

| **Aspect** | **Omi I2S** | **Your PWM** |
|------------|-------------|--------------|
| **Audio Quality** | High (24-bit) | Good (8-bit PWM) |
| **Hardware Complexity** | High (I2S codec) | Low (direct PWM) |
| **Power Consumption** | Higher | Lower |
| **Cost** | Higher | Lower |
| **Compatibility** | Omi apps | Omi apps ✅ |

### **Your Audio Specifications:**
- **PWM Frequency**: 1MHz (excellent quality)
- **Sample Rate**: 16kHz (good for voice/music)
- **Dynamic Range**: ~48dB
- **Audio Bandwidth**: Up to 8kHz

## 🔄 Migration Guide

### **For Omi Developers:**

1. **Replace audio files** with your PWM system
2. **Update device tree** for PAM8403 pins
3. **Modify transport layer** to use PWM instead of I2S
4. **Test with existing Omi apps**

### **For New Projects:**

1. **Copy your PAM8403 system** as starting point
2. **Add BLE services** from Omi transport.c
3. **Connect PAM8403 hardware** according to pinout
4. **Test with Omi apps**

## 🎯 Summary

**Your PAM8403 PWM system is a complete drop-in replacement for Omi's I2S speaker system.** It maintains full compatibility with existing Omi applications while providing better hardware control and cost efficiency.

**The magic:** Same BLE interface, same app compatibility, different (and better) audio hardware! 🎵✨
