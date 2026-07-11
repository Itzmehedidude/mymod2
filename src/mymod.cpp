#include <android/log.h>
#include <android/dlext.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include "zygisk.hpp"

#define LOG_TAG "MyMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static std::string readTargetPackage() {
    FILE *f = fopen("/data/local/tmp/mymod/app.txt", "r");
    if (!f) {
        LOGI("app.txt not found, errno=%d (%s)", errno, strerror(errno));
        return "";
    }
    char buf[256] = {0};
    if (fgets(buf, sizeof(buf), f) == nullptr) {
        fclose(f);
        return "";
    }
    fclose(f);
    std::string pkg(buf);
    while (!pkg.empty() && (pkg.back() == '\n' || pkg.back() == '\r' || pkg.back() == ' '))
        pkg.pop_back();
    return pkg;
}

// Reads delay in milliseconds from /data/local/tmp/mymod/delay.txt
// Defaults to 0 (no delay) if the file is missing or invalid.
static int readDelayMs() {
    FILE *f = fopen("/data/local/tmp/mymod/delay.txt", "r");
    if (!f) {
        return 0; // no delay configured - not an error, just default
    }
    char buf[32] = {0};
    if (fgets(buf, sizeof(buf), f) == nullptr) {
        fclose(f);
        return 0;
    }
    fclose(f);
    int ms = atoi(buf);
    if (ms < 0) ms = 0;
    if (ms > 60000) ms = 60000; // sanity cap at 60s so a bad value can't hang the app forever
    return ms;
}

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        target = readTargetPackage();
        delayMs = readDelayMs();
        LOGI("module loaded onLoad(), target=%s, delayMs=%d",
             target.empty() ? "(none)" : target.c_str(), delayMs);
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;
        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);

        if (!target.empty() && target == appName) {
            LOGI("TARGET APP STARTED: %s (uid=%d)", appName, args->uid);

            // Open the library FD now, while still root/zygote-privileged.
            libFd = open("/data/local/tmp/mymod/random_library.so", O_RDONLY);
            if (libFd < 0) {
                LOGI("Failed to open library as root: errno=%d (%s)", errno, strerror(errno));
            } else {
                LOGI("Opened library fd=%d while still privileged", libFd);
            }
        } else {
            LOGI("app started (not target): %s", appName);
        }

        env->ReleaseStringUTFChars(args->nice_name, appName);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (libFd < 0) return;

        if (!args || !args->nice_name) {
            close(libFd);
            libFd = -1;
            return;
        }

        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);

        if (delayMs > 0) {
            LOGI("Delaying library load by %d ms for %s", delayMs, appName);
            usleep((useconds_t)delayMs * 1000);
        }

        LOGI("PostAppSpecialize: loading library via fd for %s", appName);

        android_dlextinfo extinfo = {};
        extinfo.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
        extinfo.library_fd = libFd;

        void *handle = android_dlopen_ext("random_library.so", RTLD_NOW | RTLD_GLOBAL, &extinfo);

        close(libFd);
        libFd = -1;

        if (handle) {
            LOGI("SUCCESS: library loaded via fd, handle=%p", handle);
        } else {
            LOGI("FAILURE: android_dlopen_ext error: %s", dlerror());
        }

        env->ReleaseStringUTFChars(args->nice_name, appName);
    }

private:
    std::string target;
    int libFd = -1;
    int delayMs = 0;
    zygisk::Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(MyModule)
