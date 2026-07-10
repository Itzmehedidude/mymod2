#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "zygisk.hpp"

#define LOG_TAG "MyMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static std::string readTargetPackage() {
    FILE *f = fopen("/data/local/tmp/mymod/app.txt", "r");
    if (!f) {
        LOGI("app.txt not found");
        return "";
    }
    char buf[256] = {0};
    if (fgets(buf, sizeof(buf), f) == nullptr) {
        fclose(f);
        return "";
    }
    fclose(f);
    std::string pkg(buf);
    // trim newline/whitespace
    while (!pkg.empty() && (pkg.back() == '\n' || pkg.back() == '\r' || pkg.back() == ' '))
        pkg.pop_back();
    return pkg;
}

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        LOGI("module loaded onLoad()");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;

        const char *appName = env->GetStringUTFChars(args->nice_name, nullptr);
        std::string target = readTargetPackage();

        if (!target.empty() && target == appName) {
            LOGI("TARGET APP STARTED: %s (uid=%d)", appName, args->uid);
        } else {
            LOGI("app started (not target): %s", appName);
        }

        env->ReleaseStringUTFChars(args->nice_name, appName);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        // nothing yet — step 2 material
    }

private:
    zygisk::Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(MyModule)