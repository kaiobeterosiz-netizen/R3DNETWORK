#include <jni.h>
#include <string>
#include <thread>
#include <chrono>
#include <android/log.h>
#include <dlfcn.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.h"
#include "Menu/Menu.h"

uintptr_t base_address = 0;
bool auto_nexus_enabled = true;
int nexus_hp_threshold = 25;

typedef void* (*il2cpp_string_new_t)(const char* str);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_runtime_invoke_t)(void* method, void* obj, void** params, void** exc);
typedef void* (*il2cpp_domain_get_t)();
typedef void* (*il2cpp_domain_get_assemblies_t)(void* domain, size_t* size);
typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
typedef void* (*il2cpp_class_get_type_t)(void* klass);

il2cpp_string_new_t il2cpp_string_new;
il2cpp_class_from_name_t il2cpp_class_from_name;
il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name;
il2cpp_runtime_invoke_t il2cpp_runtime_invoke;
il2cpp_domain_get_t il2cpp_domain_get;
il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies;
il2cpp_assembly_get_image_t il2cpp_assembly_get_image;
il2cpp_class_get_type_t il2cpp_class_get_type;

void* GetAssemblyCSharpImage() {
    void* domain = il2cpp_domain_get();
    size_t size = 0;
    void** assemblies = (void**)il2cpp_domain_get_assemblies(domain, &size);
    for (size_t i = 0; i < size; i++) {
        return il2cpp_assembly_get_image(assemblies[i]);
    }
    return nullptr;
}

void* hack_thread(void*) {
    LOGI("Aguardando libil2cpp.so...");
    while (base_address == 0) {
        base_address = (uintptr_t)dlopen("libil2cpp.so", RTLD_NOLOAD);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    il2cpp_string_new = (il2cpp_string_new_t)dlsym((void*)base_address, "il2cpp_string_new");
    il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym((void*)base_address, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name = (il2cpp_class_get_method_from_name_t)dlsym((void*)base_address, "il2cpp_class_get_method_from_name");
    il2cpp_runtime_invoke = (il2cpp_runtime_invoke_t)dlsym((void*)base_address, "il2cpp_runtime_invoke");
    il2cpp_domain_get = (il2cpp_domain_get_t)dlsym((void*)base_address, "il2cpp_domain_get");
    il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym((void*)base_address, "il2cpp_domain_get_assemblies");
    il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym((void*)base_address, "il2cpp_assembly_get_image");
    il2cpp_class_get_type = (il2cpp_class_get_type_t)dlsym((void*)base_address, "il2cpp_class_get_type");

    void* image = GetAssemblyCSharpImage();
    if (!image) return nullptr;

    void* entity_class = il2cpp_class_from_name(image, "", "Entity");
    void* ronin_class = il2cpp_class_from_name(image, "", "RoninGameService");
    void* object_class = il2cpp_class_from_name(image, "UnityEngine", "Object");

    if (!entity_class || !ronin_class || !object_class) return nullptr;

    void* find_object_method = il2cpp_class_get_method_from_name(object_class, "FindObjectOfType", 1);
    if (!find_object_method) return nullptr;

    void* entity_type = il2cpp_class_get_type(entity_class);
    void* ronin_type = il2cpp_class_get_type(ronin_class);

    typedef int (*GetHealth_t)(void* obj);
    typedef void (*Disconnect_t)(void* obj);
    
    GetHealth_t get_health = (GetHealth_t)(base_address + 0x1BA5C40);
    Disconnect_t disconnect = (Disconnect_t)(base_address + 0x1B5263C);

    while (true) {
        if (auto_nexus_enabled) {
            void* exc = nullptr;

            void* params_entity[1] = { entity_type };
            void* player_entity = il2cpp_runtime_invoke(find_object_method, nullptr, params_entity, &exc);

            if (player_entity) {
                int current_hp = get_health(player_entity);
                int max_hp = *(int*)((uintptr_t)player_entity + 0x1A8 + 4);

                if (max_hp > 0 && current_hp > 0) {
                    int hp_percent = (current_hp * 100) / max_hp;
                    
                    if (hp_percent <= nexus_hp_threshold) {
                        LOGI("HP baixo (%d%%). Acionando Nexus...", hp_percent);
                        
                        void* params_ronin[1] = { ronin_type };
                        void* ronin_instance = il2cpp_runtime_invoke(find_object_method, nullptr, params_ronin, &exc);

                        if (ronin_instance) {
                            disconnect(ronin_instance);
                            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                        }
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}

void *main_thread(void *) {
    Menu *menu = new Menu("Born Again", "Auto Nexus");
    
    menu->addToggle("Auto Nexus ON", &auto_nexus_enabled);
    menu->addSlider("HP para Nexus", &nexus_hp_threshold, 1, 100);

    std::thread(hack_thread, nullptr).detach();

    while (true) {
        menu->loop();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return nullptr;
}

__attribute__((constructor))
void lib_main() {
    std::thread(main_thread, nullptr).detach();
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    return JNI_VERSION_1_6;
}
