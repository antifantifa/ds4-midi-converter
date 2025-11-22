#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <alsa/asoundlib.h>
#include <dirent.h>
#include <errno.h>

#define MIDI_PORT_NAME "DS4 Controller"
#define MIDI_CHANNEL 0

// CC mappings (matching original gcmidi)
#define CC_L2 20
#define CC_R2 21
#define CC_LEFT_X_NEG 22
#define CC_LEFT_X_POS 23
#define CC_LEFT_Y_NEG 24
#define CC_LEFT_Y_POS 25
#define CC_RIGHT_X_NEG 26
#define CC_RIGHT_X_POS 27
#define CC_RIGHT_Y_NEG 28
#define CC_RIGHT_Y_POS 29

// Note mappings for buttons
#define NOTE_SQUARE 36
#define NOTE_CROSS 37
#define NOTE_CIRCLE 38
#define NOTE_TRIANGLE 39
#define NOTE_L1 40
#define NOTE_R1 41
#define NOTE_L2 42
#define NOTE_R2 43
#define NOTE_SHARE 44
#define NOTE_OPTIONS 45
#define NOTE_L3 46
#define NOTE_R3 47
#define NOTE_PS 48
#define NOTE_TOUCHPAD 49

typedef struct {
    // Analog inputs
    int l2, r2;
    int left_x, left_y;
    int right_x, right_y;
    
    // Digital buttons state
    int square, cross, circle, triangle;
    int l1, r1, l2_btn, r2_btn;
    int share, options, l3, r3, ps, touchpad;
} controller_state_t;

snd_seq_t *seq;
int port;
volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
    printf("\nShutting down...\n");
}

void init_midi() {
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        fprintf(stderr, "Error opening ALSA sequencer\n");
        exit(1);
    }
    snd_seq_set_client_name(seq, MIDI_PORT_NAME);
    port = snd_seq_create_simple_port(seq, MIDI_PORT_NAME,
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        fprintf(stderr, "Error creating sequencer port\n");
        exit(1);
    }
    printf("MIDI port '%s' created (client %d, port %d)\n", 
           MIDI_PORT_NAME, snd_seq_client_id(seq), port);
}

void send_cc(int cc, int value) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_controller(&ev, MIDI_CHANNEL, cc, value);
    snd_seq_event_output_direct(seq, &ev);
}

void send_note_on(int note, int velocity) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_noteon(&ev, MIDI_CHANNEL, note, velocity);
    snd_seq_event_output_direct(seq, &ev);
}

void send_note_off(int note, int velocity) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_noteoff(&ev, MIDI_CHANNEL, note, velocity);
    snd_seq_event_output_direct(seq, &ev);
}

int scale_trigger(int value, int max) {
    return value * 127 / max;
}

int scale_axis_split(int value, int min, int max, int center) {
    if (value < center) {
        // Negative side: scale from min-center to 0-127
        return (center - value) * 127 / (center - min);
    } else {
        // Positive side: scale from center-max to 0-127
        return (value - center) * 127 / (max - center);
    }
}

int apply_deadzone(int value, int center, int deadzone) {
    if (abs(value - center) <= deadzone) {
        return center;
    }
    return value;
}

char* find_ps4_controller() {
    DIR *dir;
    struct dirent *ent;
    static char best_device[256] = {0};
    int best_score = 0;
    
    dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "Cannot open /dev/input\n");
        return NULL;
    }
    
    printf("Searching for DS4 controller (strict mode)...\n");
    
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) == 0) {
            char path[256];
            snprintf(path, sizeof(path), "/dev/input/%.200s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                struct libevdev *dev;
                if (libevdev_new_from_fd(fd, &dev) >= 0) {
                    const char *name = libevdev_get_name(dev);
                    
                    // STRICTLY exclude motion sensors and touchpad
                    if (name && (strstr(name, "Motion") || strstr(name, "Touchpad"))) {
                        printf("Skipping motion/touchpad device: %s at %s\n", name, path);
                        libevdev_free(dev);
                        close(fd);
                        continue;
                    }
                    
                    // Only consider main controller devices
                    if (name && (strstr(name, "Wireless Controller") || 
                                strstr(name, "Sony Interactive Entertainment Wireless Controller"))) {
                        
                        // Check if this is actually the main controller (not motion sensors)
                        // Main controller should use 0-255 range for sticks
                        int has_correct_range = 0;
                        if (libevdev_has_event_code(dev, EV_ABS, ABS_X)) {
                            int min = libevdev_get_abs_minimum(dev, ABS_X);
                            int max = libevdev_get_abs_maximum(dev, ABS_X);
                            // Main controller uses 0-255 range, motion sensors use much larger ranges
                            if (min == 0 && max == 255) {
                                has_correct_range = 1;
                            }
                        }
                        
                        if (!has_correct_range) {
                            printf("Skipping device (wrong axis range - likely motion): %s at %s\n", name, path);
                            libevdev_free(dev);
                            close(fd);
                            continue;
                        }
                        
                        // Score the device based on capabilities
                        int score = 0;
                        if (libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH)) score += 10;
                        if (libevdev_has_event_code(dev, EV_KEY, BTN_EAST)) score += 10;
                        if (libevdev_has_event_code(dev, EV_KEY, BTN_WEST)) score += 10;
                        if (libevdev_has_event_code(dev, EV_KEY, BTN_NORTH)) score += 10;
                        if (libevdev_has_event_code(dev, EV_ABS, ABS_X)) score += 5;
                        if (libevdev_has_event_code(dev, EV_ABS, ABS_Y)) score += 5;
                        if (libevdev_has_event_code(dev, EV_ABS, ABS_RX)) score += 5;
                        if (libevdev_has_event_code(dev, EV_ABS, ABS_RY)) score += 5;
                        if (libevdev_has_event_code(dev, EV_ABS, ABS_Z)) score += 3;
                        if (libevdev_has_event_code(dev, EV_ABS, ABS_RZ)) score += 3;
                        
                        printf("Found main controller: %s at %s (score: %d)\n", name, path, score);
                        
                        // Keep the device with the highest score
                        if (score > best_score) {
                            best_score = score;
                            strncpy(best_device, path, sizeof(best_device)-1);
                            best_device[sizeof(best_device)-1] = '\0';
                        }
                    }
                    libevdev_free(dev);
                }
                close(fd);
            }
        }
    }
    
    closedir(dir);
    
    if (best_score > 0) {
        printf("Selected controller: %s (score: %d)\n", best_device, best_score);
        return best_device;
    }
    
    printf("No suitable main controller found!\n");
    return NULL;
}

void print_usage() {
    printf("DS4 to MIDI Converter\n");
    printf("Based on gcmidi by Jeff Kaufman\n");
    printf("\n");
    printf("Controller Mapping:\n");
    printf("  Triggers: L2 -> CC%d, R2 -> CC%d\n", CC_L2, CC_R2);
    printf("  Left Stick: X -> CC%d/%d, Y -> CC%d/%d\n", 
           CC_LEFT_X_NEG, CC_LEFT_X_POS, CC_LEFT_Y_NEG, CC_LEFT_Y_POS);
    printf("  Right Stick: X -> CC%d/%d, Y -> CC%d/%d\n",
           CC_RIGHT_X_NEG, CC_RIGHT_X_POS, CC_RIGHT_Y_NEG, CC_RIGHT_Y_POS);
    printf("  Buttons: Square=%d, Cross=%d, Circle=%d, Triangle=%d\n",
           NOTE_SQUARE, NOTE_CROSS, NOTE_CIRCLE, NOTE_TRIANGLE);
    printf("  Shoulders: L1=%d, R1=%d\n", NOTE_L1, NOTE_R1);
    printf("\n");
    printf("Press Ctrl+C to exit\n");
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    char *controller_path = NULL;
    
    // Check for command line argument
    if (argc > 1) {
        controller_path = argv[1];
        printf("Using specified device: %s\n", controller_path);
    } else {
        print_usage();
        controller_path = find_ps4_controller();
        if (!controller_path) {
            fprintf(stderr, "PS4 controller not found. Please connect it.\n");
            fprintf(stderr, "You can also specify the device path: sudo ./gcmidi /dev/input/eventX\n");
            return 1;
        }
    }
    
    int fd = open(controller_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", controller_path, strerror(errno));
        fprintf(stderr, "Try running with sudo: sudo ./gcmidi\n");
        return 1;
    }
    
    struct libevdev *dev;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        fprintf(stderr, "Failed to init libevdev\n");
        close(fd);
        return 1;
    }
    
    printf("Controller: %s\n", libevdev_get_name(dev));
    
    init_midi();
    
    controller_state_t state = {0};
    
    // DS4 uses 0-255 range for sticks and triggers
    int stick_min = 0;
    int stick_max = 255;
    int stick_center = 127;
    int trigger_max = 255;
    
    // Large deadzone to handle stick drift - adjust as needed
    int deadzone_value = 15;
    
    printf("Stick range: %d to %d (center: %d)\n", stick_min, stick_max, stick_center);
    printf("Trigger range: 0 to %d\n", trigger_max);
    printf("Deadzone: ±%d (to handle stick drift)\n", deadzone_value);
    printf("Listening for controller input...\n");
    
    struct input_event ev;
    while (running) {
        int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);
        if (rc == 0) {
            switch (ev.type) {
                case EV_ABS:
                    switch (ev.code) {
                        case ABS_Z: // L2
                            state.l2 = scale_trigger(ev.value, trigger_max);
                            send_cc(CC_L2, state.l2);
                            break;
                        case ABS_RZ: // R2
                            state.r2 = scale_trigger(ev.value, trigger_max);
                            send_cc(CC_R2, state.r2);
                            break;
                        case ABS_X: // Left stick X
                            {
                                int filtered_value = apply_deadzone(ev.value, stick_center, deadzone_value);
                                
                                // Only process if value actually changed beyond deadzone
                                if (filtered_value != state.left_x) {
                                    int neg_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    int pos_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    neg_val = (neg_val > 127) ? 127 : neg_val;
                                    pos_val = (pos_val > 127) ? 127 : pos_val;
                                    
                                    send_cc(CC_LEFT_X_NEG, (filtered_value < stick_center) ? neg_val : 0);
                                    send_cc(CC_LEFT_X_POS, (filtered_value > stick_center) ? pos_val : 0);
                                    state.left_x = filtered_value;
                                }
                            }
                            break;
                        case ABS_Y: // Left stick Y
                            {
                                int filtered_value = apply_deadzone(ev.value, stick_center, deadzone_value);
                                
                                if (filtered_value != state.left_y) {
                                    int neg_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    int pos_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    neg_val = (neg_val > 127) ? 127 : neg_val;
                                    pos_val = (pos_val > 127) ? 127 : pos_val;
                                    
                                    send_cc(CC_LEFT_Y_NEG, (filtered_value < stick_center) ? neg_val : 0);
                                    send_cc(CC_LEFT_Y_POS, (filtered_value > stick_center) ? pos_val : 0);
                                    state.left_y = filtered_value;
                                }
                            }
                            break;
                        case ABS_RX: // Right stick X
                            {
                                int filtered_value = apply_deadzone(ev.value, stick_center, deadzone_value);
                                
                                if (filtered_value != state.right_x) {
                                    int neg_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    int pos_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    neg_val = (neg_val > 127) ? 127 : neg_val;
                                    pos_val = (pos_val > 127) ? 127 : pos_val;
                                    
                                    send_cc(CC_RIGHT_X_NEG, (filtered_value < stick_center) ? neg_val : 0);
                                    send_cc(CC_RIGHT_X_POS, (filtered_value > stick_center) ? pos_val : 0);
                                    state.right_x = filtered_value;
                                }
                            }
                            break;
                        case ABS_RY: // Right stick Y
                            {
                                int filtered_value = apply_deadzone(ev.value, stick_center, deadzone_value);
                                
                                if (filtered_value != state.right_y) {
                                    int neg_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    int pos_val = scale_axis_split(filtered_value, stick_min, stick_max, stick_center);
                                    neg_val = (neg_val > 127) ? 127 : neg_val;
                                    pos_val = (pos_val > 127) ? 127 : pos_val;
                                    
                                    send_cc(CC_RIGHT_Y_NEG, (filtered_value < stick_center) ? neg_val : 0);
                                    send_cc(CC_RIGHT_Y_POS, (filtered_value > stick_center) ? pos_val : 0);
                                    state.right_y = filtered_value;
                                }
                            }
                            break;
                    }
                    break;
                    
                case EV_KEY:
                    switch (ev.code) {
                        case BTN_WEST: // Square
                            if (ev.value && !state.square) {
                                send_note_on(NOTE_SQUARE, 127);
                            } else if (!ev.value && state.square) {
                                send_note_off(NOTE_SQUARE, 0);
                            }
                            state.square = ev.value;
                            break;
                        case BTN_SOUTH: // Cross
                            if (ev.value && !state.cross) {
                                send_note_on(NOTE_CROSS, 127);
                            } else if (!ev.value && state.cross) {
                                send_note_off(NOTE_CROSS, 0);
                            }
                            state.cross = ev.value;
                            break;
                        case BTN_EAST: // Circle
                            if (ev.value && !state.circle) {
                                send_note_on(NOTE_CIRCLE, 127);
                            } else if (!ev.value && state.circle) {
                                send_note_off(NOTE_CIRCLE, 0);
                            }
                            state.circle = ev.value;
                            break;
                        case BTN_NORTH: // Triangle
                            if (ev.value && !state.triangle) {
                                send_note_on(NOTE_TRIANGLE, 127);
                            } else if (!ev.value && state.triangle) {
                                send_note_off(NOTE_TRIANGLE, 0);
                            }
                            state.triangle = ev.value;
                            break;
                        case BTN_TL: // L1
                            if (ev.value && !state.l1) {
                                send_note_on(NOTE_L1, 127);
                            } else if (!ev.value && state.l1) {
                                send_note_off(NOTE_L1, 0);
                            }
                            state.l1 = ev.value;
                            break;
                        case BTN_TR: // R1
                            if (ev.value && !state.r1) {
                                send_note_on(NOTE_R1, 127);
                            } else if (!ev.value && state.r1) {
                                send_note_off(NOTE_R1, 0);
                            }
                            state.r1 = ev.value;
                            break;
                        case BTN_TL2: // L2 Button
                            if (ev.value && !state.l2_btn) {
                                send_note_on(NOTE_L2, 127);
                            } else if (!ev.value && state.l2_btn) {
                                send_note_off(NOTE_L2, 0);
                            }
                            state.l2_btn = ev.value;
                            break;
                        case BTN_TR2: // R2 Button
                            if (ev.value && !state.r2_btn) {
                                send_note_on(NOTE_R2, 127);
                            } else if (!ev.value && state.r2_btn) {
                                send_note_off(NOTE_R2, 0);
                            }
                            state.r2_btn = ev.value;
                            break;
                        case BTN_SELECT: // Share
                            if (ev.value && !state.share) {
                                send_note_on(NOTE_SHARE, 127);
                            } else if (!ev.value && state.share) {
                                send_note_off(NOTE_SHARE, 0);
                            }
                            state.share = ev.value;
                            break;
                        case BTN_START: // Options
                            if (ev.value && !state.options) {
                                send_note_on(NOTE_OPTIONS, 127);
                            } else if (!ev.value && state.options) {
                                send_note_off(NOTE_OPTIONS, 0);
                            }
                            state.options = ev.value;
                            break;
                        case BTN_MODE: // PS Button
                            if (ev.value && !state.ps) {
                                send_note_on(NOTE_PS, 127);
                            } else if (!ev.value && state.ps) {
                                send_note_off(NOTE_PS, 0);
                            }
                            state.ps = ev.value;
                            break;
                        case BTN_THUMBL: // L3
                            if (ev.value && !state.l3) {
                                send_note_on(NOTE_L3, 127);
                            } else if (!ev.value && state.l3) {
                                send_note_off(NOTE_L3, 0);
                            }
                            state.l3 = ev.value;
                            break;
                        case BTN_THUMBR: // R3
                            if (ev.value && !state.r3) {
                                send_note_on(NOTE_R3, 127);
                            } else if (!ev.value && state.r3) {
                                send_note_off(NOTE_R3, 0);
                            }
                            state.r3 = ev.value;
                            break;
                    }
                    break;
            }
        } else if (rc == -EAGAIN) {
            // No data available, sleep a bit using nanosleep instead of usleep
            struct timespec ts = {0, 1000000}; // 1 millisecond
            nanosleep(&ts, NULL);
        }
    }
    
    printf("Cleaning up...\n");
    libevdev_free(dev);
    close(fd);
    snd_seq_close(seq);
    
    return 0;
}
