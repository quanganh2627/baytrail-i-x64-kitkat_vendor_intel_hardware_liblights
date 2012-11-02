/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "lights"

#include <cutils/log.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/lights.h>

/******************************************************************************/

const int BRIGHTNESS_MASK = 0xFFFF;

struct backlight_device_t {
    char* name;
    char* backlight_file;
    char* backlight_max_file;
    int   brightness_default_max;
    int   brightness_min_visible;
};

enum {
    INTEL_VIDEO_BL_CTRL,
    ACPI_VIDEO_BL_CTRL,

    MAX_BL_CTRL_DEVICES
};

static struct backlight_device_t backlight_devices[] = {
    {
        .name = "Intel video backlight control",
        .backlight_file =
            "/sys/class/backlight/intel_backlight/brightness",
        .backlight_max_file =
            "/sys/class/backlight/intel_backlight/max_brightness",
        .brightness_default_max = 4648,
        .brightness_min_visible = 20
    },
    {
        .name = "ACPI video backlight control",
        .backlight_file =
            "/sys/class/backlight/acpi_video0/brightness",
        .backlight_max_file =
            "/sys/class/backlight/acpi_video0/max_brightness",
        .brightness_default_max = 15,
        .brightness_min_visible = 1
    },
};

static struct backlight_device_t* cur_backlight_dev = NULL;

static int get_max_brightness()
{
    char tmp_s[8];
    int fd = -1;
    int ret;
    int value = 0;

    if (!cur_backlight_dev) {
        goto fail;
    }

    value = cur_backlight_dev->brightness_default_max;
    fd = open(cur_backlight_dev->backlight_max_file, O_RDONLY);
    if (fd < 0) {
            ALOGE("faild to open %s, errno = %d\n",
                  cur_backlight_dev->backlight_max_file, errno);
            return value;
    }

    ret = read(fd, &tmp_s[0], sizeof(tmp_s));
    if (ret < 0)
        goto fail;

    value = atoi(&tmp_s[0]);

fail:
    close(fd);

    return value;
}

/* this changes the backlight brightness */
static int set_light_backlight(struct light_device_t* dev,
                            struct light_state_t const* state)
{
    int fd;
    char buf[20];
    int written, to_write;
    int brightness = (state->color & BRIGHTNESS_MASK);
    int max_brightness = get_max_brightness();

    if (!cur_backlight_dev) {
        return -ENXIO;
    }

    if (max_brightness <= cur_backlight_dev->brightness_min_visible) {
      ALOGE("Invalid maximum backlight output %d <= min visible %d\n",
	   max_brightness, cur_backlight_dev->brightness_min_visible);
      return -EINVAL;
    }

    /* If desired brightness is zero, we should set a '0' to driver to
     * turn off backlight. (e.g. shutdown screen by pressing power key)
     * otherwise, we have to ensure:
     * The min backlight output should be still visible.
     * The change should be linear in the whole acceptable range.
     */

    if (brightness) {
      brightness *= (max_brightness -
                         cur_backlight_dev->brightness_min_visible);
      brightness = brightness / BRIGHTNESS_MASK +
                       cur_backlight_dev->brightness_min_visible;
    }

    fd = open(cur_backlight_dev->backlight_file, O_RDWR);
    if (fd < 0) {
        ALOGE("unable to open %s: %s\n",
                  cur_backlight_dev->backlight_file, strerror(errno));
        return -errno;
    }
    to_write = sprintf(buf, "%d\n", brightness);
    written = write(fd, buf, to_write);
    close(fd);

    return written < 0 ? -errno : 0;
}

/** Close the lights device */
static int close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}

static void determine_backlight_device()
{
    int i;

    for (i = 0; i < MAX_BL_CTRL_DEVICES; i++) {
        /* see if brightness can be written */
        if (access(backlight_devices[i].backlight_file, W_OK)) {
            continue;
        }

        /* see if max_brightness can be read */
        if (access(backlight_devices[i].backlight_max_file, R_OK)) {
            continue;
        }

        /* both files are fine, so we use this backlight control */
        cur_backlight_dev = &backlight_devices[i];
        ALOGI("Selected %s\n", cur_backlight_dev->name);
        return;
    }

    cur_backlight_dev = NULL;
    ALOGE("Cannot find supported backlight controls\n");
}

/******************************************************************************/

/**
 * module methods
 */

/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    struct light_device_t *dev;

    if (strcmp(LIGHT_ID_BACKLIGHT, name))
        return -EINVAL;

    determine_backlight_device();

    dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag     = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module  = (struct hw_module_t*) module;
    dev->common.close   = (int (*)(struct hw_device_t*)) close_lights;
    dev->set_light      = set_light_backlight;

    *device = (struct hw_device_t*) dev;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/*
 * The lights Module
 */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Intel PC Compatible Lights HAL",
    .author = "The Android Open Source Project",
    .methods = &lights_module_methods,
};
