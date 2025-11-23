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

// ... [keep all your existing MIDI defines and structures] ...

typedef struct {
    char path[256];
    char name[256];
    char type[32];  // "controller", "motion", "touchpad"
    int score;
} device_info_t;

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
                        
                        // Classify device type
                        if (strstr(name, "Motion")) {
                            strcpy(devices[device_count].type, "motion");
                        } else if (strstr(name, "Touchpad")) {
                            strcpy(devices[device_count].type, "touchpad");
                        } else {
                            // Check if this is the main controller by capabilities
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
                        
                        // Classify and check if it matches preferred type
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
                            // Verify it's actually the main controller
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
    const char *device_type = "controller"; // default
    
    // Parse command line arguments
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
                i++; // skip next argument
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
            // Assume it's a device path
            controller_path = argv[i];
        }
    }
    
    // If no device specified, auto-detect based on preferred type
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
    
    // Rest of your existing main() function continues here...
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
    
    // ... [rest of your existing main function] ...
    
    // Cleanup
    if (controller_path != argv[1] && controller_path != NULL) {
        free(controller_path);
    }
    
    return 0;
}
