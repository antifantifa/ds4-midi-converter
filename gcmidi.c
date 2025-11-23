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
#define NOTE_DPAD_LEFT 50
#define NOTE_DPAD_RIGHT 51
#define NOTE_DPAD_UP 52
#define NOTE_DPAD_DOWN 53

typedef struct {
    int l2, r2;
    int left_x, left_y;
    int right_x, right_y;
    int square, cross, circle, triangle;
    int l1, r1, l2_btn, r2_btn;
    int share, options, l3, r3, ps, touchpad;
    int dpad_x, dpad_y;
} controller_state_t;

snd_seq_t *seq;
int port;
volatile sig_atomic_t running = 1;

typedef struct {
    char path[256];
    char name[256];
    char type[32];
    int score;
} device_info_t;

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
        return (center - value) * 127 / (center - min);
    } else {
        return (value - center) * 127 / (max - center);
    }
}

int apply_deadzone(int value, int center, int deadzone) {
    if (abs(value - center) <= deadzone) {
        return center;
    }
    return value;
}

void print_usage() {
    printf("DS4 to MIDI Converter\n");
    printf("Based on gcmidi by Jeff Kaufman\n");
    printf("\n");
    printf("Usage: ./gcmidi [OPTIONS]\n");
    printf("Options:\n");
    printf("  -d, --device PATH    Use specific input device\n");
    printf("  -l, --list-devices   List all available DS4 devices\n");
    printf("  -c, --controller     Use controller inputs (buttons, sticks, triggers) [DEFAULT]\n");
    printf("  -m, --motion         Use motion sensors (accelerometer, gyroscope)\n");
    printf("  -t, --touchpad       Use touchpad input\n");
    printf("  -h, --help           Show this help\n");
    printf("\n");
    printf("Device Selection:\n");
    printf("  The DS4 creates 3 separate HID devices. Choose ONE type to avoid conflicts.\n");
    printf("  Default: controller inputs (recommended for MIDI control)\n");
}

void list_available_devices() {
    DIR *dir;
    struct dirent *ent;
    device_info_t devices[10];
    int device_count = 0;
    
    printf("Scanning for DS4 devices...\n");
    printf("============================\n");
    
    dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "Cannot open /dev/input\n");
        return;
    }
    
    while ((ent = readdir(dir)) != NULL && device_count < 10) {
        if (strncmp(ent->d_name, "event", 5) == 0) {
            char path[256];
            snprintf(path, sizeof(path), "/dev/input/%.200s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                struct libevdev *dev;
                if (libevdev_new_from_fd(fd, &dev) >= 0) {
                    const char *name = libevdev_get_name(dev);
                    
                    if (name && (strstr(name, "Wireless Controller") || 
                                strstr(name, "Sony Interactive Entertainment Wireless Controller"))) {
                        
                        strncpy(devices[device_count].path, path, sizeof(devices[device_count].path)-1);
                        strncpy(devices[device_count].name, name ? name : "Unknown", sizeof(devices[device_count].name)-1);
                        
                        if (strstr(name, "Motion")) {
                            strcpy(devices[device_count].type, "motion");
                        } else if (strstr(name, "Touchpad")) {
                            strcpy(devices[device_count].type, "touchpad");
                        } else {
                            if (libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH) &&
                                libevdev_has_event_code(dev, EV_ABS, ABS_X)) {
                                strcpy(devices[device_count].type, "controller");
                            } else {
                                strcpy(devices[device_count].type, "unknown");
                            }
                        }
                        
                        device_count++;
                    }
                    libevdev_free(dev);
                }
                close(fd);
            }
        }
    }
    closedir(dir);
    
    if (device_count == 0) {
        printf("No DS4 devices found.\n");
        return;
    }
    
    printf("Found %d DS4 devices:\n", device_count);
    for (int i = 0; i < device_count; i++) {
        printf("  [%d] %s\n", i, devices[i].path);
        printf("      Type: %s\n", devices[i].type);
        printf("      Name: %s\n", devices[i].name);
    }
    printf("\n");
}

char* find_ds4_device_by_type(const char* preferred_type) {
    DIR *dir;
    struct dirent *ent;
    device_info_t best_device = {0};
    int found_count = 0;
    
    printf("Looking for DS4 %s device...\n", preferred_type);
    
    dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "Cannot open /dev/input\n");
        return NULL;
    }
    
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) == 0) {
            char path[256];
            snprintf(path, sizeof(path), "/dev/input/%.200s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                struct libevdev *dev;
                if (libevdev_new_from_fd(fd, &dev) >= 0) {
                    const char *name = libevdev_get_name(dev);
                    
                    if (name && (strstr(name, "Wireless Controller") || 
                                strstr(name, "Sony Interactive Entertainment Wireless Controller"))) {
                        
                        if (strstr(name, "Motion") && strcmp(preferred_type, "motion") == 0) {
                            strncpy(best_device.path, path, sizeof(best_device.path)-1);
                            strncpy(best_device.name, name, sizeof(best_device.name)-1);
                            found_count++;
                            printf("Found motion device: %s\n", path);
                        } else if (strstr(name, "Touchpad") && strcmp(preferred_type, "touchpad") == 0) {
                            strncpy(best_device.path, path, sizeof(best_device.path)-1);
                            strncpy(best_device.name, name, sizeof(best_device.name)-1);
                            found_count++;
                            printf("Found touchpad device: %s\n", path);
                        } else if (!strstr(name, "Motion") && !strstr(name, "Touchpad") && 
                                   strcmp(preferred_type, "controller") == 0) {
                            if (libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH) &&
                                libevdev_has_event_code(dev, EV_ABS, ABS_X)) {
                                strncpy(best_device.path, path, sizeof(best_device.path)-1);
                                strncpy(best_device.name, name, sizeof(best_device.name)-1);
                                found_count++;
                                printf("Found controller device: %s\n", path);
                            }
                        }
                    }
                    libevdev_free(dev);
                }
                close(fd);
            }
        }
    }
    
    closedir(dir);
    
    if (found_count > 0) {
        if (found_count > 1) {
            printf("Warning: Found %d %s devices. Using first one: %s\n", 
                   found_count, preferred_type, best_device.path);
        }
        printf("Selected: %s\n", best_device.path);
        return strdup(best_device.path);
    }
    
    printf("No %s device found!\n", preferred_type);
    return NULL;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    char *controller_path = NULL;
    const char *device_type = "controller";
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list-devices") == 0) {
            list_available_devices();
            return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) {
                controller_path = argv[i + 1];
                i++;
            } else {
                fprintf(stderr, "Error: --device requires a path argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--motion") == 0) {
            device_type = "motion";
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--touchpad") == 0) {
            device_type = "touchpad";
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--controller") == 0) {
            device_type = "controller";
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage();
            return 1;
        } else {
            controller_path = argv[i];
        }
    }
    
    if (!controller_path) {
        printf("Auto-detecting DS4 %s device...\n", device_type);
        controller_path = find_ds4_device_by_type(device_type);
        if (!controller_path) {
            fprintf(stderr, "No DS4 %s device found.\n", device_type);
            fprintf(stderr, "Use --list-devices to see available devices.\n");
            return 1;
        }
    }
    
    printf("Using device: %s\n", controller_path);
    printf("Device type: %s\n", device_type);
    
    int fd = open(controller_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", controller_path, strerror(errno));
        if (controller_path != argv[1]) free(controller_path);
        return 1;
    }
    
    struct libevdev *dev;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        fprintf(stderr, "Failed to init libevdev\n");
        close(fd);
        if (controller_path != argv[1]) free(controller_path);
        return 1;
    }
    
    printf("Controller: %s\n", libevdev_get_name(dev));
    
    init_midi();
    
    controller_state_t state = {0};
    
    int stick_min = 0;
    int stick_max = 255;
    int stick_center = 127;
    int trigger_max = 255;
    int deadzone_value = 15;
    
    printf("Stick range: %d to %d (center: %d)\n", stick_min, stick_max, stick_center);
    printf("Trigger range: 0 to %d\n", trigger_max);
    printf("Deadzone: ±%d\n", deadzone_value);
    printf("Listening for controller input...\n");
    
    struct input_event ev;
    while (running) {
        int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);
        if (rc == 0) {
            switch (ev.type) {
                case EV_ABS:
                    switch (ev.code) {
                        case ABS_Z:
                            state.l2 = scale_trigger(ev.value, trigger_max);
                            send_cc(CC_L2, state.l2);
                            break;
                        case ABS_RZ:
                            state.r2 = scale_trigger(ev.value, trigger_max);
                            send_cc(CC_R2, state.r2);
                            break;
                        case ABS_X:
                            {
                                int filtered_value = apply_deadzone(ev.value, stick_center, deadzone_value);
                                
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
                        case ABS_Y:
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
                        case ABS_RX:
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
                        case ABS_RY:
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
                        case ABS_HAT0X:
                            if (ev.value != state.dpad_x) {
                                if (state.dpad_x == -1) send_note_off(NOTE_DPAD_LEFT, 0);
                                if (state.dpad_x == 1) send_note_off(NOTE_DPAD_RIGHT, 0);
                                
                                if (ev.value == -1) {
                                    send_note_on(NOTE_DPAD_LEFT, 127);
                                    printf("D-pad Left\n");
                                } else if (ev.value == 1) {
                                    send_note_on(NOTE_DPAD_RIGHT, 127);
                                    printf("D-pad Right\n");
                                }
                                state.dpad_x = ev.value;
                            }
                            break;
                        case ABS_HAT0Y:
                            if (ev.value != state.dpad_y) {
                                if (state.dpad_y == -1) send_note_off(NOTE_DPAD_UP, 0);
                                if (state.dpad_y == 1) send_note_off(NOTE_DPAD_DOWN, 0);
                                
                                if (ev.value == -1) {
                                    send_note_on(NOTE_DPAD_UP, 127);
                                    printf("D-pad Up\n");
                                } else if (ev.value == 1) {
                                    send_note_on(NOTE_DPAD_DOWN, 127);
                                    printf("D-pad Down\n");
                                }
                                state.dpad_y = ev.value;
                            }
                            break;
                    }
                    break;
                    
                case EV_KEY:
                    switch (ev.code) {
                        case BTN_WEST:
                            if (ev.value && !state.square) {
                                send_note_on(NOTE_SQUARE, 127);
                            } else if (!ev.value && state.square) {
                                send_note_off(NOTE_SQUARE, 0);
                            }
                            state.square = ev.value;
                            break;
                        case BTN_SOUTH:
                            if (ev.value && !state.cross) {
                                send_note_on(NOTE_CROSS, 127);
                            } else if (!ev.value && state.cross) {
                                send_note_off(NOTE_CROSS, 0);
                            }
                            state.cross = ev.value;
                            break;
                        case BTN_EAST:
                            if (ev.value && !state.circle) {
                                send_note_on(NOTE_CIRCLE, 127);
                            } else if (!ev.value && state.circle) {
                                send_note_off(NOTE_CIRCLE, 0);
                            }
                            state.circle = ev.value;
                            break;
                        case BTN_NORTH:
                            if (ev.value && !state.triangle) {
                                send_note_on(NOTE_TRIANGLE, 127);
                            } else if (!ev.value && state.triangle) {
                                send_note_off(NOTE_TRIANGLE, 0);
                            }
                            state.triangle = ev.value;
                            break;
                        case BTN_TL:
                            if (ev.value && !state.l1) {
                                send_note_on(NOTE_L1, 127);
                            } else if (!ev.value && state.l1) {
                                send_note_off(NOTE_L1, 0);
                            }
                            state.l1 = ev.value;
                            break;
                        case BTN_TR:
                            if (ev.value && !state.r1) {
                                send_note_on(NOTE_R1, 127);
                            } else if (!ev.value && state.r1) {
                                send_note_off(NOTE_R1, 0);
                            }
                            state.r1 = ev.value;
                            break;
                        case BTN_TL2:
                            if (ev.value && !state.l2_btn) {
                                send_note_on(NOTE_L2, 127);
                            } else if (!ev.value && state.l2_btn) {
                                send_note_off(NOTE_L2, 0);
                            }
                            state.l2_btn = ev.value;
                            break;
                        case BTN_TR2:
                            if (ev.value && !state.r2_btn) {
                                send_note_on(NOTE_R2, 127);
                            } else if (!ev.value && state.r2_btn) {
                                send_note_off(NOTE_R2, 0);
                            }
                            state.r2_btn = ev.value;
                            break;
                        case BTN_SELECT:
                            if (ev.value && !state.share) {
                                send_note_on(NOTE_SHARE, 127);
                            } else if (!ev.value && state.share) {
                                send_note_off(NOTE_SHARE, 0);
                            }
                            state.share = ev.value;
                            break;
                        case BTN_START:
                            if (ev.value && !state.options) {
                                send_note_on(NOTE_OPTIONS, 127);
                            } else if (!ev.value && state.options) {
                                send_note_off(NOTE_OPTIONS, 0);
                            }
                            state.options = ev.value;
                            break;
                        case BTN_MODE:
                            if (ev.value && !state.ps) {
                                send_note_on(NOTE_PS, 127);
                            } else if (!ev.value && state.ps) {
                                send_note_off(NOTE_PS, 0);
                            }
                            state.ps = ev.value;
                            break;
                        case BTN_THUMBL:
                            if (ev.value && !state.l3) {
                                send_note_on(NOTE_L3, 127);
                            } else if (!ev.value && state.l3) {
                                send_note_off(NOTE_L3, 0);
                            }
                            state.l3 = ev.value;
                            break;
                        case BTN_THUMBR:
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
            struct timespec ts = {0, 1000000};
            nanosleep(&ts, NULL);
        }
    }
    
    printf("Cleaning up...\n");
    libevdev_free(dev);
    close(fd);
    snd_seq_close(seq);
    
    if (controller_path != argv[1] && controller_path != NULL) {
        free(controller_path);
    }
    
    return 0;
}
