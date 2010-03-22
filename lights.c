/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "lights"

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <cutils/log.h>
#include <hardware/lights.h>

#define LIGHT_LED_OFF   0
#define LIGHT_LED_FULL  255

#define LIGHT_PATH_BASE "/sys/class"
#define LIGHT_ID_BACKLIGHT_PATH                         \
    LIGHT_PATH_BASE"/backlight/psb-bl/brightness"
#define LIGHT_ID_MAX_BACKLIGHT_PATH                     \
    LIGHT_PATH_BASE"/backlight/psb-bl/max_brightness"

/* if cdk board have leds, new sys path related to leds should be defined. */
#define LIGHT_ID_KEYBOARD_PATH                          \
    LIGHT_PATH_BASE"/keyboard-backlight/brightness"
#define LIGHT_ID_BUTTONS_PATH                           \
    LIGHT_PATH_BASE"/button-backlight/brightness"
#define LIGHT_ID_BATTERY_PATH                           \
    LIGHT_PATH_BASE"/battery-backlight/brightness"
#define LIGHT_ID_NOTIFICATIONS_PATH                     \
    LIGHT_PATH_BASE"/notifications-backlight/brightness"
#define LIGHT_ID_ATTENTION_PATH                         \
    LIGHT_PATH_BASE"/attention-baklight/brightness"


#define BRIGHT_MAX_BAR      255
#define bright_to_intensity(__max, __br, __its)     \
        do {                                        \
                __its = __max * __br;      \
                __its = __its / BRIGHT_MAX_BAR;     \
        } while (0)

struct lights_fds {
    int backlight;
    int keyboard;
    int buttons;
    int battery;
    int notifications;
    int attention;
};

struct lights_ctx {
    struct lights_fds fds;
} *context;

static int get_max_brightness()
{
    char tmp_s[8];
    int fd, value, ret;
    char *path = LIGHT_ID_MAX_BACKLIGHT_PATH;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
            LOGE("faild to open %s, ret = %d\n", path, errno);
            return -errno;
    }

    ret = read(fd, &tmp_s[0], sizeof(tmp_s));
    if (ret < 0)
        return ret;

    value = atoi(&tmp_s[0]);

    close(fd);

    return value;
}

static int write_brightness(int fd, unsigned char brightness)
{
    char buff[32];
    int intensity;
    int bytes, ret, max_br;

    max_br = get_max_brightness(); 

    if(max_br < 0){
        LOGE("fail to read max brightness\n");
        return -1;
    }

    bright_to_intensity(max_br, brightness, intensity);

    bytes = sprintf(buff, "%d\n", intensity);
    ret = write(fd, buff, bytes);
    if (ret < 0) {
        LOGE("faild to write %d (fd = %d, errno = %d)\n",
             intensity, fd, errno);
        return -errno;
    }

    return 0;
}

static inline int __is_on(const struct light_state_t *state)
{
    return state->color & 0x00ffffff;
}

static inline unsigned char
__rgb_to_brightness(const struct light_state_t *state)
{
    int color = state->color & 0x00ffffff;
    unsigned char brightness = ((77 * ((color >> 16) & 0x00ff))
                                + (150 * ((color >> 8) & 0x00ff))
                                + (29 * (color & 0x00ff))) >> 8;
    return brightness;
}

static int
set_light_backlight(struct light_device_t *dev,
                    const struct light_state_t *state)
{
    int brightness = __rgb_to_brightness(state);
    int ret;

    return write_brightness(context->fds.backlight, brightness);
}

static int set_light_keyboard(struct light_device_t *dev,
                              const struct light_state_t *state)
{
    int on = __is_on(state);
    int ret;

    return write_brightness(context->fds.keyboard,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

static int set_light_buttons(struct light_device_t *dev,
                             const struct light_state_t *state)
{
    int on = __is_on(state);
    int ret;

    return write_brightness(context->fds.buttons,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

static int set_light_battery(struct light_device_t *dev,
                             const struct light_state_t *state)
{
    int on = __is_on(state);
    int ret;

    return write_brightness(context->fds.battery,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

static int set_light_notifications(struct light_device_t *dev,
                                   const struct light_state_t *state)
{
    int on = __is_on(state);
    int ret;

    return write_brightness(context->fds.notifications,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

static int set_light_attention(struct light_device_t *dev,
                               const struct light_state_t *state)
{
    int on = __is_on(state);
    int ret;

    return write_brightness(context->fds.attention,
                            on ? LIGHT_LED_FULL : LIGHT_LED_OFF);
}

/* lights close method */
static int close_lights_dev(struct light_device_t *dev)
{
    if (dev)
        free(dev);

    return 0;
}

static int lights_open_node(struct lights_ctx *ctx, struct light_device_t *dev,
                            const char *id)
{
    int fd;
    int *pfd;
    char *path;
    int (*set_light)(struct light_device_t* dev,
                     struct light_state_t const* state);

    if (!strcmp(LIGHT_ID_BACKLIGHT, id)) {
        path = LIGHT_ID_BACKLIGHT_PATH;
        pfd = &ctx->fds.backlight;
        set_light = set_light_backlight;
    }
    else if (!strcmp(LIGHT_ID_KEYBOARD, id)) {
        path = LIGHT_ID_KEYBOARD_PATH;
        pfd = &ctx->fds.keyboard;
        set_light = set_light_keyboard;
    }
    else if (!strcmp(LIGHT_ID_BUTTONS, id)) {
        path = LIGHT_ID_BUTTONS_PATH;
        pfd = &ctx->fds.buttons;
        set_light = set_light_buttons;
    }
    else if (!strcmp(LIGHT_ID_BATTERY, id)) {
        path = LIGHT_ID_BATTERY_PATH;
        pfd = &ctx->fds.battery;
        set_light = set_light_battery;
    }
    else if (!strcmp(LIGHT_ID_NOTIFICATIONS, id)) {
        path = LIGHT_ID_NOTIFICATIONS_PATH;
        pfd = &ctx->fds.notifications;
        set_light = set_light_notifications;
    }
    else if (!strcmp(LIGHT_ID_ATTENTION, id)) {
        path = LIGHT_ID_ATTENTION_PATH;
        pfd = &ctx->fds.attention;
        set_light = set_light_attention;
    }
    else
        return -EINVAL;

    fd = open(path, O_RDWR);
    if (fd < 0) {
        LOGE("faild to open %s, ret = %d\n", path, errno);
        return -errno;
    }

    LOGD("opened %s, fd = %d\n", path, fd);

    *pfd = fd;
    dev->set_light = set_light;
    return 0;
}

static struct lights_ctx *lights_init_context(void)
{
    struct lights_ctx *ctx;

    ctx = malloc(sizeof(struct lights_ctx));
    memset(ctx, 0, sizeof(*ctx));

    ctx->fds.backlight = -1;
    ctx->fds.keyboard = -1;
    ctx->fds.buttons = -1;
    ctx->fds.battery = -1;
    ctx->fds.notifications = -1;
    ctx->fds.attention = -1;

    return ctx;
}

/*
 * module open method
 *
 * LIGHT_ID_BACKLIGHT          "backlight"
 * LIGHT_ID_KEYBOARD           "keyboard"
 * LIGHT_ID_BUTTONS            "buttons"
 * LIGHT_ID_BATTERY            "battery"
 * LIGHT_ID_NOTIFICATIONS      "notifications"
 * LIGHT_ID_ATTENTION          "attention"
 */
static int open_lights(const struct hw_module_t *module, const char *id,
                       struct hw_device_t **device)
{
    struct light_device_t *dev;
    int ret;

    dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    if (!context)
        context = lights_init_context();

    ret = lights_open_node(context, dev, id);
    if (ret < 0) {
        free(dev);
        return ret;
    }

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *)module;
    dev->common.close = (int (*)(struct hw_device_t* device))close_lights_dev;

    *device = (struct hw_device_t *)dev;
    return 0;
}

/* module method */
static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/* lights module */
const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 0,
    .version_minor = 1,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Moorestown CDK lights Module",
	.author = "The Android Open Source Project",
    .methods = &lights_module_methods,
};
