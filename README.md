# 🎮 DS4 to MIDI Converter

A professional-grade tool that converts Sony DualShock 4 controller input into MIDI messages for music production and live performance.

https://img.shields.io/badge/DS4-MIDI_Controller-blue?style=for-the-badge
https://img.shields.io/badge/Linux-Compatible-success?style=for-the-badge&logo=linux
https://img.shields.io/badge/License-MIT-green?style=for-the-badge
✨ Features

    🎛️ Complete Controller Mapping: All buttons, triggers, and analog sticks mapped to MIDI

    🎯 Smart Device Detection: Automatically finds main controller, excludes motion sensors

    ⚡ Low Latency: Real-time input processing with ALSA MIDI output

    🔧 Drift Correction: Built-in deadzone to handle stick drift

    🎹 DAW Ready: Works with any MIDI-compatible software (Ardour, Reaper, Bitwig, etc.)

🚀 Quick Start
Prerequisites
bash

# Ubuntu/Debian
sudo apt install libevdev-dev libasound2-dev build-essential

# Fedora
sudo dnf install libevdev-devel alsa-lib-devel gcc

# Arch
sudo pacman -S libevdev alsa-lib base-devel

Installation
bash

git clone https://github.com/yourusername/ds4-midi-converter
cd ds4-midi-converter
make
sudo ./gcmidi

🎮 MIDI Mapping
Analog Controls (CC Messages)
Control	MIDI CC	Behavior
L2 Trigger	CC 20	0-127 pressure sensitive
R2 Trigger	CC 21	0-127 pressure sensitive
Left Stick X	CC 22/23	Split negative/positive
Left Stick Y	CC 24/25	Split negative/positive
Right Stick X	CC 26/27	Split negative/positive
Right Stick Y	CC 28/29	Split negative/positive
Buttons (MIDI Notes)
Button	MIDI Note	Button	MIDI Note
Square	36	L1	40
Cross	37	R1	41
Circle	38	L2 Button	42
Triangle	39	R2 Button	43
Share	44	L3	46
Options	45	R3	47
PS Button	48	Touchpad	49
🛠️ Building from Source
bash

# Clone and build
git clone https://github.com/yourusername/ds4-midi-converter
cd ds4-midi-converter
make

# Install system-wide (optional)
sudo make install

📖 Usage
Basic Usage
bash

# Automatic device detection
sudo ./gcmidi

# Manual device specification
sudo ./gcmidi /dev/input/event20

System Service (Optional)
bash

# Copy to system
sudo cp gcmidi /usr/local/bin/

# Create systemd service
sudo nano /etc/systemd/system/gcmidi.service

Add to service file:
ini

[Unit]
Description=DS4 MIDI Controller
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/gcmidi
Restart=always
User=root

[Install]
WantedBy=multi-user.target

Enable service:
bash

sudo systemctl enable gcmidi.service
sudo systemctl start gcmidi.service

⚙️ Configuration
Adjusting Deadzone

Edit gcmidi.c line ~217 to adjust stick sensitivity:
c

int deadzone_value = 15;  // Increase for more drift tolerance

Custom MIDI Mapping

Modify the CC and Note definitions in the source code to create custom mappings.
🔧 Troubleshooting
Controller Not Detected
bash

# List all input devices
evtest

# Test specific device
sudo evtest /dev/input/event20

MIDI Output Not Working
bash

# Check ALSA MIDI ports
aconnect -l

# Monitor MIDI output
aseqdump -p "DS4 Controller"

Permission Issues
bash

# Add user to input group
sudo usermod -a -G input $USER

# Or run with sudo
sudo ./gcmidi

🎹 Creative Applications

    🎵 Live Performance: Map sticks to filter sweeps and effects

    🎹 Drum Trigger: Use buttons for drum pads

    🎛️ Parameter Control: Assign triggers to expression pedals

    🎚️ Mixing: Use sticks for fader and pan control

📁 Project Structure
text

ds4-midi-converter/
├── gcmidi.c          # Main source code
├── Makefile          # Build configuration
├── README.md         # This file
└── LICENSE           # MIT License

🤝 Contributing

Contributions welcome! Feel free to:

    Report bugs

    Suggest new features

    Submit pull requests

    Improve documentation

📜 License

MIT License - see LICENSE file for details.
🙏 Acknowledgments

    Based on original gcmidi by Jeff Kaufman

    DS4 Linux driver community

    ALSA and libevdev developers

Happy music making! 🎶

If this project helped you, consider giving it a ⭐ on GitHub!
