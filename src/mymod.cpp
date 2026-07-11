#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <string>
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
        target = readTargetPackage();  // read ONCE per process fork, not per hook
        LOGI("module loaded onLoad(), target=%s", target.empty() ? "(none)" : target.c_str());
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;
        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);

        if (!target.empty() && target == appName) {
            LOGI("TARGET APP STARTED: %s (uid=%d)", appName, args->uid);
        } else {
            LOGI("app started (not target): %s", appName);
        }
        env->ReleaseStringUTFChars(args->nice_name, appName);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;
        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);

        if (!target.empty() && target == appName) {
            LOGI("PostAppSpecialize: Loading library for %s", appName);
            void* handle = dlopen("/data/local/tmp/mymod/random_library.so", RTLD_NOW | RTLD_GLOBAL);
            if (handle) {
                LOGI("SUCCESS: Library loaded at %p", handle);
            } else {
                LOGI("FAILURE: dlopen error: %s", dlerror());
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, appName);
    }

private:
    std::string target;
    zygisk::Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(MyModule)
