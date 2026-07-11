#include <android/log.h>
#include <android/dlext.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
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

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        target = readTargetPackage(); // read once per forked process
        LOGI("module loaded onLoad(), target=%s", target.empty() ? "(none)" : target.c_str());
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;
        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);

        if (!target.empty() && target == appName) {
            LOGI("TARGET APP STARTED: %s (uid=%d)", appName, args->uid);

            // Open the library FD now, while still root/zygote-privileged.
            // The permission check happens here, at open() time - not later
            // when the process has transitioned into the app's own domain.
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
        if (libFd < 0) return; // not our target, or the open() failed earlier

        if (!args || !args->nice_name) {
            close(libFd);
            libFd = -1;
            return;
        }

        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);
        LOGI("PostAppSpecialize: loading library via fd for %s", appName);

        android_dlextinfo extinfo = {};
        extinfo.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
        extinfo.library_fd = libFd;

        // Name here is just a label for the linker/debugger; the fd is what
        // actually gets loaded, so the app's domain never has to open() the
        // path itself post-transition.
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
    zygisk::Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(MyModule)
