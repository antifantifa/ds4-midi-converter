# DS4 to MIDI Converter

A high-performance C-based bridge that converts PlayStation DualShock 4 controller input into MIDI messages. Based on the original gcmidi by Jeff Kaufman, with enhanced device detection, user-friendly command-line interface, and optimized trigger behavior for DJ software.

## Features

- **Low Latency**: C-based implementation for minimal delay
- **Multiple Device Support**: Choose between controller inputs, motion sensors, or touchpad
- **Comprehensive MIDI Mapping**:
  - Buttons send MIDI notes
  - Analog sticks and triggers send Control Change messages
  - D-pad sends directional note commands
- **Optimized Trigger Behavior**: Center detent for L2/R2 triggers (64 when released) for smooth crossfader control
- **Deadzone Handling**: Built-in stick drift compensation
- **ALSA MIDI Output**: Creates virtual MIDI port for DAW integration
- **User-Friendly Interface**: Command-line options for easy device selection

## Installation

### Dependencies

```bash
sudo apt-get install libasound2-dev libevdev-dev
```

### Building

```bash
make
```

Or compile manually:
```bash
gcc -Wall -Wextra -O2 -std=gnu11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -I/usr/include/libevdev-1.0/ -o gcmidi gcmidi.c -levdev -lasound
```

## Usage

### Basic Usage
```bash
sudo ./gcmidi
```

### List Available Devices
```bash
./gcmidi --list-devices
```

### Use Specific Device Type
```bash
# Use controller inputs (buttons, sticks, triggers) - DEFAULT
sudo ./gcmidi --controller

# Use motion sensors (accelerometer, gyroscope)
sudo ./gcmidi --motion

# Use touchpad input
sudo ./gcmidi --touchpad
```

### Use Specific Device Path
```bash
sudo ./gcmidi --device /dev/input/event4
```

## MIDI Mapping

### Buttons (Note Messages)
- **Face Buttons**: Square=36, Cross=37, Circle=38, Triangle=39
- **Shoulders**: L1=40, R1=41, L2=42, R2=43
- **System**: Share=44, Options=45, PS=48
- **Stick Press**: L3=46, R3=47
- **D-pad**: Left=50, Right=51, Up=52, Down=53

### Analog Controls (CC Messages)
- **Triggers**: L2=CC20, R2=CC21 (with center detent at 64)
- **Left Stick**: 
  - X-axis: CC22 (left) / CC23 (right)
  - Y-axis: CC24 (up) / CC25 (down)
- **Right Stick**:
  - X-axis: CC26 (left) / CC27 (right)
  - Y-axis: CC28 (up) / CC29 (down)

## Device Selection

The DS4 creates three separate HID devices. To avoid conflicts, select only one type:

- **Controller**: Buttons, analog sticks, triggers, D-pad (recommended for MIDI)
- **Motion**: Accelerometer and gyroscope data
- **Touchpad**: Touch position and tap events

Use `--list-devices` to see all available DS4 devices and their types.

## Recent Improvements

- **Enhanced Device Detection**: Automatic filtering of controller, motion, and touchpad devices
- **Command-Line Interface**: Easy device selection without recompilation
- **Warning-Free Compilation**: Clean build output
- **Better Error Handling**: Clear messages for common connection issues
- **Optimized Triggers**: Center detent (64) when triggers are released for smooth crossfader control in DJ software

## Troubleshooting

### Permission Issues
```bash
sudo chmod a+rw /dev/input/event*
```
Or add your user to the `input` group:
```bash
sudo usermod -a -G input $USER
```

### No Devices Found
- Ensure DS4 is connected via USB or Bluetooth
- Check `dmesg | tail` for connection events
- Use `--list-devices` to verify detection

### Multiple Controllers
If multiple DS4 controllers are connected, use `--device` with the specific path shown in `--list-devices`.

## Technical Details

- **MIDI Channel**: 0
- **Stick Range**: 0-255 (center: 127)
- **Trigger Range**: 0-255 (center detent at 64 when released)
- **Deadzone**: ±15
- **Sample Rate**: ~1ms latency

## License

Based on gcmidi by Jeff Kaufman. Modified with improved device handling and user interface.

## Contributing

Feel free to pull requests for improvements and bug fixes.
