#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libevdev/libevdev.h>
#include <dirent.h>

int main() {
    DIR *dir;
    struct dirent *ent;
    char path[256];
    
    dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "Cannot open /dev/input\n");
        return 1;
    }
    
    printf("Available input devices:\n");
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                struct libevdev *dev;
                if (libevdev_new_from_fd(fd, &dev) >= 0) {
                    printf("%s: %s\n", path, libevdev_get_name(dev));
                    printf("  Has buttons: %s\n", libevdev_has_event_type(dev, EV_KEY) ? "yes" : "no");
                    printf("  Has ABS: %s\n", libevdev_has_event_type(dev, EV_ABS) ? "yes" : "no");
                    
                    // Check for specific buttons
                    if (libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH)) {
                        printf("  Has Cross button: yes\n");
                    }
                    if (libevdev_has_event_code(dev, EV_ABS, ABS_X)) {
                        printf("  Has Left Stick X: yes\n");
                    }
                    printf("---\n");
                    libevdev_free(dev);
                }
                close(fd);
            }
        }
    }
    closedir(dir);
    return 0;
}
