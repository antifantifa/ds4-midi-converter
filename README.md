# DS4 to MIDI Converter

![Python](https://img.shields.io/badge/Python-3.8%2B-blue) ![License](https://img.shields.io/badge/License-MIT-green)

A real-time bridge that transforms your Sony PlayStation DualShock 4 controller into a fully customizable, multi-purpose MIDI controller. Built in Python, this tool unlocks the unique sensors of the DS4—including the accelerometer, gyroscope, and touchpad—for musical expression, going far beyond standard button-to-key mapping.

## Features

*   **Multi-Sensor Support:** Map a wide range of controller inputs to MIDI:
    *   **Buttons & Triggers:** Standard and shoulder buttons, analog triggers (L2/R2).
    *   **Motion Controls:** 3-axis Accelerometer and Gyroscope for dynamic, gesture-based control.
    *   **Touchpad:** Use finger position and tap events on the touchpad.
    *   **D-Pad & Sticks:** Directional pad and analog sticks.

*   **Flexible MIDI Output:**
    *   **Note On/Off:** Trigger melodic notes or drum hits.
    *   **Control Change (CC):** Send continuous data for parameters like volume, filter cutoff, and modulation.
    *   **Program Change:** Switch presets or scenes in your DAW.

*   **Real-Time & Low-Latency:** Utilizes `pygame` for robust controller input and `python-rtmidi` for efficient MIDI output.

*   **Dynamic Configuration:** Easily customize all mappings (button-to-MIDI, sensor-to-CC, etc.) via a clean and well-documented configuration file (`config.py`). No code changes needed for most adjustments.

## Use Cases

*   **Live Electronic Performance:** Use gestures and taps to trigger clips, control effects, and manipulate synths on stage.
*   **Studio Production:** Add expressive, non-traditional modulation to your tracks using the motion sensors and touchpad.
*   **Accessible Music Making:** Provides an alternative, game-like interface for music creation.
*   **Interactive Installations:** A low-cost hardware interface for interactive art and sound installations.

## Quick Start

1.  **Connect your DS4** to your computer via Bluetooth or USB.
2.  **Install dependencies:**
    ```bash
    pip install pygame python-rtmidi
    ```
3.  **Clone and run:**
    ```bash
    git clone https://github.com//antifantifa/ds4-to-midi.git
    cd ds4-to-midi
    python main.py
    ```
4.  **Configure your DAW** to receive MIDI from the "DS4-MIDI" virtual port.

## Configuration

Edit `config.py` to define your own mappings. The structure is intuitive:
```python
# Example: Map the Cross button to MIDI Note 36
BUTTON_MAPPINGS = {
    'cross': {'type': 'note', 'channel': 1, 'note': 36, 'velocity': 127}
}

# Example: Map X-axis tilt to CC #1 (Mod Wheel)
GYRO_MAPPINGS = {
    'x': {'type': 'cc', 'channel': 1, 'controller': 1}
}
```
