#include <dlfcn.h>
#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "zygisk.hpp"

#define LOG_TAG "MyMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static std::string readTargetPackage() {
    // Ensure your app.txt contains the exact package name (e.g., com.example.game)
    FILE *f = fopen("/data/local/tmp/mymod/app.txt", "r");
    if (!f) {
        LOGI("app.txt not found at /data/local/tmp/mymod/app.txt");
        return "";
    }
    char buf[256] = {0};
    if (fgets(buf, sizeof(buf), f) == nullptr) {
        fclose(f);
        return "";
    }
    fclose(f);
    
    std::string pkg(buf);
    // Trim potential whitespace or newline characters
    while (!pkg.empty() && (pkg.back() == '\n' || pkg.back() == '\r' || pkg.back() == ' '))
        pkg.pop_back();
    return pkg;
}

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        LOGI("module loaded via onLoad()");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        // We only check the package name here
        if (!args || !args->nice_name) return;

        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);
        std::string target = readTargetPackage();

        if (!target.empty() && target == appName) {
            LOGI("Target detected in preAppSpecialize: %s", appName);
        }

        env->ReleaseStringUTFChars(args->nice_name, appName);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;

        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);
        std::string target = readTargetPackage();

        if (!target.empty() && target == appName) {
            LOGI("Target detected in postAppSpecialize: %s. Attempting load...", appName);

            // Path to your target library
            const char* libPath = "/data/local/tmp/random_library.so";

            // Attempt to load the library
            // RTLD_NOW: resolve symbols immediately
            // RTLD_GLOBAL: make symbols visible to other modules
            void* handle = dlopen(libPath, RTLD_NOW | RTLD_GLOBAL);

            if (handle) {
                LOGI("SUCCESS: Library loaded at address: %p", handle);
            } else {
                LOGI("FAILURE: dlopen failed! Error: %s", dlerror());
            }
        }

        env->ReleaseStringUTFChars(args->nice_name, appName);
    }

private:
    zygisk::Api *api;
    JNIEnv *env;
};

// Register the module
REGISTER_ZYGISK_MODULE(MyModule)
