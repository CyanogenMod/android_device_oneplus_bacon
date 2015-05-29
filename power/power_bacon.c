/*
 * Copyright (C) 2014 The Android Open Source Project
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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <cutils/uevent.h>
#include <errno.h>
#include <sys/poll.h>
#include <pthread.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <stdbool.h>

#define LOG_TAG "PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define MAX_LENGTH			50
#define UEVENT_MSG_LEN			2048
#define TOTAL_CPUS			4
#define RETRY_TIME_CHANGING_FREQ	20
#define SLEEP_USEC_BETWN_RETRY		200
#define LOW_POWER_MAX_FREQ		"729600"
#define LOW_POWER_MIN_FREQ		"300000"
#define NORMAL_MAX_FREQ			"2457600"
#define UEVENT_STRING			"online@/devices/system/cpu/"
#define CPU_INPUT_BOOST_PATH		"/sys/kernel/cpu_input_boost/userspace_minfreq"

static struct pollfd pfd;
static char *cpu_path_min[] = {
    "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq",
    "/sys/devices/system/cpu/cpu1/cpufreq/scaling_min_freq",
    "/sys/devices/system/cpu/cpu2/cpufreq/scaling_min_freq",
    "/sys/devices/system/cpu/cpu3/cpufreq/scaling_min_freq",
};
static char *cpu_path_max[] = {
    "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
    "/sys/devices/system/cpu/cpu1/cpufreq/scaling_max_freq",
    "/sys/devices/system/cpu/cpu2/cpufreq/scaling_max_freq",
    "/sys/devices/system/cpu/cpu3/cpufreq/scaling_max_freq",
};
static bool freq_set[TOTAL_CPUS];
static bool low_power_mode = false;
static pthread_mutex_t low_power_mode_lock = PTHREAD_MUTEX_INITIALIZER;

static int sysfs_write(const char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return -1;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
        return -1;
    }

    close(fd);
    return 0;
}

static int uevent_event()
{
    char msg[UEVENT_MSG_LEN];
    char *cp;
    int n, cpu, ret, retry = RETRY_TIME_CHANGING_FREQ;

    n = recv(pfd.fd, msg, UEVENT_MSG_LEN, MSG_DONTWAIT);
    if (n <= 0) {
        return -1;
    }
    if (n >= UEVENT_MSG_LEN) {   /* overflow -- discard */
        return -1;
    }

    cp = msg;

    if (strstr(cp, UEVENT_STRING)) {
        n = strlen(cp);
        errno = 0;
        cpu = strtol(cp + n - 1, NULL, 10);

        if (errno == EINVAL || errno == ERANGE || cpu < 0 || cpu >= TOTAL_CPUS) {
            return -1;
        }

        pthread_mutex_lock(&low_power_mode_lock);
        if (low_power_mode && !freq_set[cpu]) {
            sysfs_write(CPU_INPUT_BOOST_PATH, LOW_POWER_MIN_FREQ);
            while (retry) {
                sysfs_write(cpu_path_min[cpu], LOW_POWER_MIN_FREQ);
                ret = sysfs_write(cpu_path_max[cpu], LOW_POWER_MAX_FREQ);
                if (!ret) {
                    freq_set[cpu] = true;
                    break;
                }
                usleep(SLEEP_USEC_BETWN_RETRY);
                retry--;
           }
        } else if (!low_power_mode && freq_set[cpu]) {
             sysfs_write(CPU_INPUT_BOOST_PATH, "0");
             while (retry) {
                  ret = sysfs_write(cpu_path_max[cpu], NORMAL_MAX_FREQ);
                  if (!ret) {
                      freq_set[cpu] = false;
                      break;
                  }
                  usleep(SLEEP_USEC_BETWN_RETRY);
                  retry--;
             }
        }
        pthread_mutex_unlock(&low_power_mode_lock);
    }
    return 0;
}

void *thread_uevent(__attribute__((unused)) void *x)
{
    while (1) {
        int nevents, ret;

        nevents = poll(&pfd, 1, -1);

        if (nevents == -1) {
            if (errno == EINTR)
                continue;
            ALOGE("powerhal: thread_uevent: poll_wait failed\n");
            break;
        }
        ret = uevent_event();
        if (ret < 0)
            ALOGE("Error processing the uevent event");
    }
    return NULL;
}

static void uevent_init()
{
    struct sockaddr_nl client;
    pthread_t tid;
    pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

    if (pfd.fd < 0) {
        ALOGE("%s: failed to open: %s", __func__, strerror(errno));
        return;
    }
    memset(&client, 0, sizeof(struct sockaddr_nl));
    pthread_create(&tid, NULL, thread_uevent, NULL);
    client.nl_family = AF_NETLINK;
    client.nl_pid = tid;
    client.nl_groups = -1;
    pfd.events = POLLIN;
    bind(pfd.fd, (void *)&client, sizeof(struct sockaddr_nl));
    return;
}

static void power_init(__attribute__((unused)) struct power_module *module)
{
    ALOGI("%s", __func__);
    uevent_init();
}

static void power_set_interactive(__attribute__((unused)) struct power_module *module, 
                      __attribute__((unused)) int on)
{
}

static void power_hint( __attribute__((unused)) struct power_module *module,
                      power_hint_t hint, __attribute__((unused)) void *data)
{
    int cpu, ret;

    if (hint != POWER_HINT_LOW_POWER)
        return;

    pthread_mutex_lock(&low_power_mode_lock);
    if (data) {
        low_power_mode = true;
        sysfs_write(CPU_INPUT_BOOST_PATH, LOW_POWER_MIN_FREQ);
        for (cpu = 0; cpu < TOTAL_CPUS; cpu++) {
            sysfs_write(cpu_path_min[cpu], LOW_POWER_MIN_FREQ);
            ret = sysfs_write(cpu_path_max[cpu], LOW_POWER_MAX_FREQ);
            if (!ret) {
                freq_set[cpu] = true;
            }
        }
    } else {
        low_power_mode = false;
        sysfs_write(CPU_INPUT_BOOST_PATH, "0");
        for (cpu = 0; cpu < TOTAL_CPUS; cpu++) {
            ret = sysfs_write(cpu_path_max[cpu], NORMAL_MAX_FREQ);
            if (!ret) {
                freq_set[cpu] = false;
            }
        }
    }
    pthread_mutex_unlock(&low_power_mode_lock);
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = POWER_MODULE_API_VERSION_0_2,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = "Bacon Power HAL",
        .author = "The CyanogenMod Project",
        .methods = &power_module_methods,
    },
    .init = power_init,
    .setInteractive = power_set_interactive,
    .powerHint = power_hint,
};
