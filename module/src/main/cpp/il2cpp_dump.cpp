//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <unistd.h>
#include <sys/uio.h>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        /*if (method->slot != 65535) {
            outPut << " Slot: " << std::dec << method->slot;
        }*/
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        //TODO 获取构造函数初始化后的字段值
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        //使用il2cpp_image_get_class
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        //使用反射
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            //LOGD("image name : %s", image->name);
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    }
    LOGI("write dump file");
    auto outPath = std::string(outDir).append("/files/dump.cs");
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump done!");
}

// =================== generic file-driven engine ===================
// All il2cpp_* pointers are file-global here, so the engine lives in this TU.
static bool g_attached = false;
static void engine_attach() {
    if (g_attached) return;
    if (il2cpp_thread_attach && il2cpp_domain_get) {
        il2cpp_thread_attach(il2cpp_domain_get());
        g_attached = true;
    }
}
// safe in-process read: process_vm_readv on self returns -EFAULT on bad addr (no crash)
static bool safe_read(uint64_t addr, void *out, size_t len) {
    struct iovec liov{out, len};
    struct iovec riov{(void *) addr, len};
    return process_vm_readv(getpid(), &liov, 1, &riov, 1, 0) == (ssize_t) len;
}
static Il2CppClass *find_class(const char *ns, const char *name) {
    if (!il2cpp_domain_get || !il2cpp_domain_get_assemblies) return nullptr;
    size_t n = 0;
    auto dom = il2cpp_domain_get();
    auto asms = il2cpp_domain_get_assemblies(dom, &n);
    for (size_t i = 0; i < n; i++) {
        auto img = il2cpp_assembly_get_image(asms[i]);
        if (!img) continue;
        auto k = il2cpp_class_from_name(img, ns, name);
        if (k) return k;
    }
    return nullptr;
}
struct HeapCtx { Il2CppClass *want; uint64_t *buf; int cap; int cnt; };
static HeapCtx *g_hc = nullptr;
static void heap_cb(void *obj, void *user) {
    if (!obj || !g_hc || g_hc->cnt >= g_hc->cap) return;
    Il2CppClass *k = *(Il2CppClass **) obj;   // Il2CppObject.klass @ +0
    if (k == g_hc->want) g_hc->buf[g_hc->cnt++] = (uint64_t) obj;
}
static int fields_of(Il2CppClass *k, const char *fn) {
    if (!k || !il2cpp_class_get_field_from_name || !il2cpp_field_get_offset) return -1;
    auto f = il2cpp_class_get_field_from_name(k, fn);
    return f ? (int) il2cpp_field_get_offset(f) : -1;
}
static int collect_instances(Il2CppClass *k, uint64_t *buf, int cap) {
    HeapCtx hc{k, buf, cap, 0};
    g_hc = &hc;
    if (il2cpp_gc_disable) il2cpp_gc_disable();
    if (il2cpp_gc_foreach_heap) il2cpp_gc_foreach_heap(heap_cb, nullptr);
    if (il2cpp_gc_enable) il2cpp_gc_enable();
    g_hc = nullptr;
    return hc.cnt;
}
// robust in-process heap scan for objects whose klass-ptr (@+0) == k.
// in-process read of own memory is safe (no external-read detection).
static int collect_instances_scan(Il2CppClass *k, uint64_t *buf, int cap) {
    int cnt = 0;
    FILE *mf = fopen("/proc/self/maps", "r");
    if (!mf) return 0;
    char *chunk = (char *) malloc(1 << 20);
    char line[512];
    while (fgets(line, sizeof(line), mf) && cnt < cap) {
        unsigned long lo, hi; char perms[8] = {0}, name[256] = {0};
        if (sscanf(line, "%lx-%lx %7s %*s %*s %*s %255[^\n]", &lo, &hi, perms, name) < 3) continue;
        if (perms[0] != 'r' || perms[1] != 'w') continue;
        if (strstr(name, "/dev") || strstr(name, "dmabuf") || strstr(name, "stack")) continue;
        if (strstr(name, ".so") || strstr(name, ".dex") || strstr(name, ".art") || strstr(name, ".odex")) continue;
        if (strstr(name, "dalvik")) continue;  // skip ART/Java heap (il2cpp objs not there)
        unsigned long sz = hi - lo;
        if (sz > 800UL * 1024 * 1024) continue;
        for (unsigned long off = 0; off < sz && cnt < cap;) {
            unsigned long want = sz - off; if (want > (1UL << 20)) want = 1UL << 20;
            if (safe_read(lo + off, chunk, want)) {
                for (unsigned long i = 0; i + 8 <= want && cnt < cap; i += 8) {
                    uint64_t v; memcpy(&v, chunk + i, 8);
                    if (v == (uint64_t) (uintptr_t) k) buf[cnt++] = lo + off + i;
                }
            }
            off += (want > 8 ? want - 8 : want);
        }
    }
    fclose(mf); free(chunk);
    return cnt;
}

void il2cpp_engine_cmd(const char *line, const char *dataDir, const char *outPath) {
    engine_attach();
    std::ofstream o(outPath);
    std::istringstream iss(line);
    std::string tok;
    iss >> tok;
    char s[160];
    if (tok == "dump") {
        il2cpp_dump(dataDir);
        o << "dumped\n";
    } else if (tok == "class") {
        std::string ns, nm;
        iss >> ns >> nm;
        if (ns == ".") ns = "";
        auto k = find_class(ns.c_str(), nm.c_str());
        snprintf(s, sizeof(s), "%llx\n", (unsigned long long) (uintptr_t) k);
        o << s;
    } else if (tok == "field") {
        unsigned long long kp = 0; std::string fn;
        iss >> std::hex >> kp >> fn;
        o << fields_of((Il2CppClass *) (uintptr_t) kp, fn.c_str()) << "\n";
    } else if (tok == "read") {
        unsigned long long a = 0; size_t len = 0;
        iss >> std::hex >> a >> std::dec >> len;
        if (len > 4096) len = 4096;
        std::vector<unsigned char> b(len);
        if (safe_read(a, b.data(), len)) {
            char hx[3];
            for (size_t i = 0; i < len; i++) { snprintf(hx, 3, "%02x", b[i]); o << hx; }
            o << "\n";
        } else o << "ERR\n";
    } else if (tok == "rfloat") {
        unsigned long long a = 0; int cnt = 0;
        iss >> std::hex >> a >> std::dec >> cnt;
        if (cnt > 128) cnt = 128;
        std::vector<float> f(cnt);
        if (safe_read(a, f.data(), cnt * 4)) {
            for (int i = 0; i < cnt; i++) { snprintf(s, sizeof(s), "%+0.4f ", f[i]); o << s; }
            o << "\n";
        } else o << "ERR\n";
    } else if (tok == "instances") {
        unsigned long long kp = 0; int mx = 64;
        iss >> std::hex >> kp >> std::dec >> mx;
        if (mx <= 0 || mx > 8192) mx = 64;
        std::vector<uint64_t> buf(mx);
        int n = collect_instances_scan((Il2CppClass *) (uintptr_t) kp, buf.data(), mx);
        o << n << "\n";
        for (int i = 0; i < n; i++) { snprintf(s, sizeof(s), "%llx\n", (unsigned long long) buf[i]); o << s; }
    } else if (tok == "fields") {
        unsigned long long kp = 0; iss >> std::hex >> kp;
        auto k = (Il2CppClass *) (uintptr_t) kp;
        void *it = nullptr; int c = 0;
        while (auto f = il2cpp_class_get_fields(k, &it)) {
            auto t = il2cpp_field_get_type(f);
            char *tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            snprintf(s, sizeof(s), "+0x%x %s %s\n", (unsigned) il2cpp_field_get_offset(f),
                     tn ? tn : "?", il2cpp_field_get_name(f));
            o << s;
            if (tn && il2cpp_free) il2cpp_free(tn);
            if (++c > 600) break;
        }
    } else if (tok == "methods") {
        unsigned long long kp = 0; iss >> std::hex >> kp;
        auto k = (Il2CppClass *) (uintptr_t) kp;
        void *it = nullptr; int c = 0;
        while (auto m = il2cpp_class_get_methods(k, &it)) {
            snprintf(s, sizeof(s), "%s argc=%u\n", il2cpp_method_get_name(m),
                     il2cpp_method_get_param_count ? il2cpp_method_get_param_count(m) : 0);
            o << s;
            if (++c > 600) break;
        }
    } else if (tok == "getfield") {
        unsigned long long obj = 0, kp = 0; std::string fn;
        iss >> std::hex >> obj >> kp >> fn;
        auto f = il2cpp_class_get_field_from_name((Il2CppClass *) (uintptr_t) kp, fn.c_str());
        if (!f) { o << "no field\n"; }
        else {
            unsigned char b[64] = {0};
            il2cpp_field_get_value((Il2CppObject *) (uintptr_t) obj, f, b);
            o << "off=0x" << std::hex << il2cpp_field_get_offset(f) << std::dec << " hex=";
            for (int i = 0; i < 32; i++) { char h[3]; snprintf(h, 3, "%02x", b[i]); o << h; }
            o << "\nfloats=";
            float fv[8]; memcpy(fv, b, 32);
            for (int i = 0; i < 8; i++) { snprintf(s, sizeof(s), "%+0.4f ", fv[i]); o << s; }
            int64_t iv; memcpy(&iv, b, 8);
            snprintf(s, sizeof(s), "\nint64=%lld u64=%llx\n", (long long) iv, (unsigned long long) iv);
            o << s;
        }
    } else if (tok == "getprop") {
        unsigned long long obj = 0, kp = 0; std::string pn;
        iss >> std::hex >> obj >> kp >> pn;
        auto prop = il2cpp_class_get_property_from_name((Il2CppClass *) (uintptr_t) kp, pn.c_str());
        if (!prop) { o << "no prop\n"; }
        else {
            auto gm = il2cpp_property_get_get_method((PropertyInfo *) prop);
            if (!gm) { o << "no getter\n"; }
            else {
                Il2CppException *exc = nullptr;
                auto res = il2cpp_runtime_invoke(gm, (void *) (uintptr_t) obj, nullptr, &exc);
                if (exc) o << "EXC\n";
                else if (!res) o << "null\n";
                else {
                    void *ub = il2cpp_object_unbox ? il2cpp_object_unbox(res) : ((char *) res + 0x10);
                    unsigned char b[32] = {0}; memcpy(b, ub, 32);
                    o << "obj=" << std::hex << (unsigned long long) (uintptr_t) res << std::dec << " hex=";
                    for (int i = 0; i < 32; i++) { char h[3]; snprintf(h, 3, "%02x", b[i]); o << h; }
                    o << "\nfloats=";
                    float fv[8]; memcpy(fv, b, 32);
                    for (int i = 0; i < 8; i++) { snprintf(s, sizeof(s), "%+0.4f ", fv[i]); o << s; }
                    o << "\n";
                }
            }
        }
    } else if (tok == "invoke") {
        unsigned long long kp = 0, obj = 0; std::string mn; int argc = 0;
        iss >> std::hex >> kp >> mn >> std::dec >> argc >> std::hex >> obj;
        auto m = il2cpp_class_get_method_from_name((Il2CppClass *) (uintptr_t) kp, mn.c_str(), argc);
        if (!m) { o << "no method\n"; }
        else {
            Il2CppException *exc = nullptr;
            auto res = il2cpp_runtime_invoke(m, (void *) (uintptr_t) obj, nullptr, &exc);
            snprintf(s, sizeof(s), "exc=%d res=%llx\n", exc ? 1 : 0,
                     (unsigned long long) (uintptr_t) res);
            o << s;
        }
    } else if (tok == "setfield") {   // setfield <objHex> <klassHex> <field> <hexbytes>
        unsigned long long obj = 0, kp = 0; std::string fn, hx;
        iss >> std::hex >> obj >> kp >> fn >> hx;
        auto f = il2cpp_class_get_field_from_name((Il2CppClass *) (uintptr_t) kp, fn.c_str());
        if (!f) { o << "no field\n"; }
        else {
            unsigned char b[64] = {0}; int len = (int) hx.size() / 2; if (len > 64) len = 64;
            for (int i = 0; i < len; i++) { unsigned v; sscanf(hx.c_str() + i * 2, "%2x", &v); b[i] = (unsigned char) v; }
            il2cpp_field_set_value((Il2CppObject *) (uintptr_t) obj, f, b);
            snprintf(s, sizeof(s), "set %d bytes @off 0x%x\n", len, (unsigned) il2cpp_field_get_offset(f)); o << s;
        }
    } else if (tok == "write") {       // write <addrHex> <hexbytes>
        unsigned long long a = 0; std::string hx; iss >> std::hex >> a >> hx;
        int len = (int) hx.size() / 2; if (len > 4096) len = 4096;
        std::vector<unsigned char> b(len);
        for (int i = 0; i < len; i++) { unsigned v; sscanf(hx.c_str() + i * 2, "%2x", &v); b[i] = (unsigned char) v; }
        struct iovec li{b.data(), (size_t) len}, ri{(void *) (uintptr_t) a, (size_t) len};
        ssize_t w = process_vm_writev(getpid(), &li, 1, &ri, 1, 0);
        snprintf(s, sizeof(s), "%s %d\n", (w == len ? "wrote" : "ERR"), len); o << s;
    } else if (tok == "statics") {     // statics <klassHex> : static fields + raw u64 value
        unsigned long long kp = 0; iss >> std::hex >> kp;
        auto k = (Il2CppClass *) (uintptr_t) kp; void *it = nullptr; int c = 0;
        while (auto f = il2cpp_class_get_fields(k, &it)) {
            int fl = il2cpp_field_get_flags ? il2cpp_field_get_flags(f) : 0;
            if (!(fl & 0x10)) continue;  // FIELD_ATTRIBUTE_STATIC
            unsigned char b[16] = {0};
            if (il2cpp_field_static_get_value) il2cpp_field_static_get_value(f, b);
            uint64_t v; memcpy(&v, b, 8);
            auto t = il2cpp_field_get_type(f); char *tn = (t && il2cpp_type_get_name) ? il2cpp_type_get_name(t) : nullptr;
            snprintf(s, sizeof(s), "%s %s = 0x%llx\n", tn ? tn : "?", il2cpp_field_get_name(f), (unsigned long long) v);
            o << s; if (tn && il2cpp_free) il2cpp_free(tn); if (++c > 300) break;
        }
    } else if (tok == "props") {       // props <klassHex>
        unsigned long long kp = 0; iss >> std::hex >> kp;
        auto k = (Il2CppClass *) (uintptr_t) kp; void *it = nullptr; int c = 0;
        while (auto p = il2cpp_class_get_properties(k, &it)) {
            o << il2cpp_property_get_name((PropertyInfo *) p) << "\n"; if (++c > 400) break;
        }
    } else if (tok == "str") {         // str <addrHex> : Il2CppString -> utf8
        unsigned long long a = 0; iss >> std::hex >> a;
        auto sp = (Il2CppString *) (uintptr_t) a;
        int len = il2cpp_string_length ? il2cpp_string_length(sp) : 0;
        Il2CppChar *ch = il2cpp_string_chars ? il2cpp_string_chars(sp) : nullptr;
        if (ch && len > 0 && len < 4096) {
            for (int i = 0; i < len; i++) { unsigned cc = ch[i]; if (cc < 128) o << (char) cc; else { char u[8]; snprintf(u, 8, "\\u%04x", cc); o << u; } }
            o << "\n";
        } else o << "ERR\n";
    } else if (tok == "method") {      // method <klassHex> <name> <argc> : native code ptr + rva
        unsigned long long kp = 0; std::string mn; int argc = 0;
        iss >> std::hex >> kp >> mn >> std::dec >> argc;
        auto m = il2cpp_class_get_method_from_name((Il2CppClass *) (uintptr_t) kp, mn.c_str(), argc);
        if (!m) { o << "no method\n"; }
        else {
            uint64_t mp = (uint64_t) m->methodPointer;
            snprintf(s, sizeof(s), "method=%llx codePtr=%llx base=%llx rva=0x%llx\n",
                     (unsigned long long) (uintptr_t) m, (unsigned long long) mp,
                     (unsigned long long) il2cpp_base, (unsigned long long) (mp - il2cpp_base));
            o << s;
        }
    } else {
        o << "unknown cmd\n";
    }
    o.close();
}

// ===================== embedded Lua scripting engine =====================
extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}
static lua_State *g_L = nullptr;
static std::string g_lua_log;
static std::string g_overlay;          // overlay draw-command file
static std::string g_dataDir;          // game data dir (for il2.dumpcs)
static std::string g_draw;             // accumulated draw commands for current frame
static int g_tick_ref = LUA_NOREF;     // registry ref to repeating tick fn
static long g_tick_interval = 100;     // ms between ticks
static long g_tick_last = 0;
static long now_ms() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }

static int l_find(lua_State *L) {
    auto k = find_class(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    lua_pushinteger(L, (lua_Integer) (uintptr_t) k); return 1;
}
static int l_instances(lua_State *L) {
    auto k = (Il2CppClass *) (uintptr_t) luaL_checkinteger(L, 1);
    int mx = (int) luaL_optinteger(L, 2, 512); if (mx < 1 || mx > 8192) mx = 512;
    std::vector<uint64_t> b(mx); int n = collect_instances_scan(k, b.data(), mx);
    lua_createtable(L, n, 0);
    for (int i = 0; i < n; i++) { lua_pushinteger(L, (lua_Integer) b[i]); lua_rawseti(L, -2, i + 1); }
    return 1;
}
static int l_field(lua_State *L) {
    auto k = (Il2CppClass *) (uintptr_t) luaL_checkinteger(L, 1);
    lua_pushinteger(L, fields_of(k, luaL_checkstring(L, 2))); return 1;
}
static int l_read(lua_State *L) {
    uint64_t a = (uint64_t) luaL_checkinteger(L, 1); size_t n = (size_t) luaL_checkinteger(L, 2);
    if (n > 65536) n = 65536; std::vector<char> b(n);
    if (safe_read(a, b.data(), n)) lua_pushlstring(L, b.data(), n); else lua_pushnil(L);
    return 1;
}
static int l_u64(lua_State *L) { uint64_t a = (uint64_t) luaL_checkinteger(L, 1), v = 0;
    if (safe_read(a, &v, 8)) lua_pushinteger(L, (lua_Integer) v); else lua_pushnil(L); return 1; }
static int l_u32(lua_State *L) { uint64_t a = (uint64_t) luaL_checkinteger(L, 1); uint32_t v = 0;
    if (safe_read(a, &v, 4)) lua_pushinteger(L, (lua_Integer) v); else lua_pushnil(L); return 1; }
static int l_f32(lua_State *L) { uint64_t a = (uint64_t) luaL_checkinteger(L, 1); float v = 0;
    if (safe_read(a, &v, 4)) lua_pushnumber(L, (lua_Number) v); else lua_pushnil(L); return 1; }
static int l_write(lua_State *L) {
    uint64_t a = (uint64_t) luaL_checkinteger(L, 1); size_t n; const char *p = luaL_checklstring(L, 2, &n);
    struct iovec li{(void *) p, n}, ri{(void *) (uintptr_t) a, n};
    ssize_t w = process_vm_writev(getpid(), &li, 1, &ri, 1, 0);
    lua_pushboolean(L, w == (ssize_t) n); return 1;
}
static int l_classname(lua_State *L) {
    auto k = (Il2CppClass *) (uintptr_t) luaL_checkinteger(L, 1);
    const char *nm = (k && il2cpp_class_get_name) ? il2cpp_class_get_name(k) : nullptr;
    lua_pushstring(L, nm ? nm : ""); return 1;
}
static int l_log(lua_State *L) {
    int n = lua_gettop(L); std::ofstream f(g_lua_log, std::ios::app);
    for (int i = 1; i <= n; i++) { if (i > 1) f << "\t"; size_t l; const char *s = luaL_tolstring(L, i, &l); f.write(s, l); lua_pop(L, 1); }
    f << "\n"; return 0;
}
static int l_sleep(lua_State *L) { usleep((useconds_t) luaL_checkinteger(L, 1) * 1000); return 0; }
// read a static field's value (pointer/int) by name -- non-scan navigation root
static int l_staticobj(lua_State *L) {
    auto k = (Il2CppClass *) (uintptr_t) luaL_checkinteger(L, 1);
    auto f = k ? il2cpp_class_get_field_from_name(k, luaL_checkstring(L, 2)) : nullptr;
    if (!f) { lua_pushnil(L); return 1; }
    uint64_t v = 0; if (il2cpp_field_static_get_value) il2cpp_field_static_get_value(f, &v);
    lua_pushinteger(L, (lua_Integer) v); return 1;
}
// scheduling: register a repeating callback (engine drives it; non-blocking, interruptible)
static int l_loop(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    long ms = (long) luaL_optinteger(L, 2, 100); if (ms < 0) ms = 0;
    if (g_tick_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, g_tick_ref);
    lua_pushvalue(L, 1); g_tick_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    g_tick_interval = ms; g_tick_last = 0; return 0;
}
static int l_stop(lua_State *L) {
    if (g_tick_ref != LUA_NOREF) { luaL_unref(L, LUA_REGISTRYINDEX, g_tick_ref); g_tick_ref = LUA_NOREF; }
    return 0;
}
// drawing: emit primitives to overlay file (consumed by external overlay app)
static int l_clear(lua_State *L) { g_draw.clear(); return 0; }
static int l_box(lua_State *L) {
    char b[160]; snprintf(b, sizeof(b), "B %d %d %d %d %x\n",
        (int) luaL_checkinteger(L, 1), (int) luaL_checkinteger(L, 2), (int) luaL_checkinteger(L, 3),
        (int) luaL_checkinteger(L, 4), (unsigned) luaL_optinteger(L, 5, 0xff00ff00));
    g_draw += b; return 0;
}
static int l_text(lua_State *L) {
    char b[320]; snprintf(b, sizeof(b), "T %d %d %x %s\n",
        (int) luaL_checkinteger(L, 1), (int) luaL_checkinteger(L, 2),
        (unsigned) luaL_optinteger(L, 4, 0xffffffff), luaL_checkstring(L, 3));
    g_draw += b; return 0;
}
static int l_present(lua_State *L) {
    std::string tmp = g_overlay + ".tmp";
    { std::ofstream f(tmp, std::ios::trunc); f << g_draw; }
    rename(tmp.c_str(), g_overlay.c_str());
    return 0;
}

// ---- reflection invoke (call game's own methods, e.g. Teleport/set_position/SetMovementInput) ----
static int l_method(lua_State *L) {  // method(klass, name, argc) -> MethodInfo*
    auto k = (Il2CppClass *) (uintptr_t) luaL_checkinteger(L, 1);
    auto m = il2cpp_class_get_method_from_name(k, luaL_checkstring(L, 2), (int) luaL_checkinteger(L, 3));
    lua_pushinteger(L, (lua_Integer) (uintptr_t) m); return 1;
}
static int l_invoke(lua_State *L) {  // invoke(method, obj) -> result ptr (0 on exc/void)
    auto m = (const MethodInfo *) (uintptr_t) luaL_checkinteger(L, 1);
    void *obj = (void *) (uintptr_t) luaL_checkinteger(L, 2);
    Il2CppException *exc = nullptr;
    auto r = il2cpp_runtime_invoke(m, obj, nullptr, &exc);
    lua_pushinteger(L, exc ? -1 : (lua_Integer) (uintptr_t) r); return 1;
}
static int l_invoke_v3(lua_State *L) {  // invoke_v3(method, obj, x,y,z) -> for Teleport/set_position
    auto m = (const MethodInfo *) (uintptr_t) luaL_checkinteger(L, 1);
    void *obj = (void *) (uintptr_t) luaL_checkinteger(L, 2);
    float v[3] = {(float) luaL_checknumber(L, 3), (float) luaL_checknumber(L, 4), (float) luaL_checknumber(L, 5)};
    void *params[1] = {v};
    Il2CppException *exc = nullptr;
    auto r = il2cpp_runtime_invoke(m, obj, params, &exc);
    lua_pushinteger(L, exc ? -1 : (lua_Integer) (uintptr_t) r); return 1;
}
static int l_invoke_v3b(lua_State *L) {  // invoke_v3b(method, obj, x,y,z, bool) -> SetMovementInput(Vector3,bool)
    auto m = (const MethodInfo *) (uintptr_t) luaL_checkinteger(L, 1);
    void *obj = (void *) (uintptr_t) luaL_checkinteger(L, 2);
    float v[3] = {(float) luaL_checknumber(L, 3), (float) luaL_checknumber(L, 4), (float) luaL_checknumber(L, 5)};
    uint8_t b = (uint8_t) lua_toboolean(L, 6);
    void *params[2] = {v, &b};
    Il2CppException *exc = nullptr;
    auto r = il2cpp_runtime_invoke(m, obj, params, &exc);
    lua_pushinteger(L, exc ? -1 : (lua_Integer) (uintptr_t) r); return 1;
}
static int l_invoke_f(lua_State *L) {  // invoke_f(method, obj, float)
    auto m = (const MethodInfo *) (uintptr_t) luaL_checkinteger(L, 1);
    void *obj = (void *) (uintptr_t) luaL_checkinteger(L, 2);
    float f = (float) luaL_checknumber(L, 3);
    void *params[1] = {&f};
    Il2CppException *exc = nullptr;
    auto r = il2cpp_runtime_invoke(m, obj, params, &exc);
    lua_pushinteger(L, exc ? -1 : (lua_Integer) (uintptr_t) r); return 1;
}

// invoke_args(method, obj, spec, ...) — spec chars: i=int32 l=int64 f=float b=bool 3=Vector3 2=Vector2
static int l_invoke_args(lua_State *L) {
    auto m = (const MethodInfo *) (uintptr_t) luaL_checkinteger(L, 1);
    void *obj = (void *) (uintptr_t) luaL_checkinteger(L, 2);
    const char *spec = luaL_checkstring(L, 3);
    union Buf { int32_t i; int64_t l; float f; uint8_t b; float v[3]; };
    Buf bufs[16]; void *params[16];
    int nb = 0, np = 0, li = 4;
    for (const char *p = spec; *p && np < 16 && nb < 16; p++) {
        Buf &bf = bufs[nb++];
        if (*p == 'i') { bf.i = (int32_t) luaL_checkinteger(L, li++); params[np++] = &bf.i; }
        else if (*p == 'l') { bf.l = (int64_t) luaL_checkinteger(L, li++); params[np++] = &bf.l; }
        else if (*p == 'f') { bf.f = (float) luaL_checknumber(L, li++); params[np++] = &bf.f; }
        else if (*p == 'b') { bf.b = (uint8_t) lua_toboolean(L, li++); params[np++] = &bf.b; }
        else if (*p == '2') { bf.v[0] = (float) luaL_checknumber(L, li++); bf.v[1] = (float) luaL_checknumber(L, li++); params[np++] = bf.v; }
        else if (*p == '3') { bf.v[0] = (float) luaL_checknumber(L, li++); bf.v[1] = (float) luaL_checknumber(L, li++); bf.v[2] = (float) luaL_checknumber(L, li++); params[np++] = bf.v; }
        else if (*p == 'o') { nb--; params[np++] = (void *) (uintptr_t) luaL_checkinteger(L, li++); }  // object/string ptr (ref type: pass ptr directly)
    }
    Il2CppException *exc = nullptr;
    auto r = il2cpp_runtime_invoke(m, obj, params, &exc);
    lua_pushinteger(L, exc ? -1 : (lua_Integer) (uintptr_t) r); return 1;
}

// ---- more typed reads + string helpers (generic primitives) ----
static int l_i32(lua_State *L) { uint64_t a = (uint64_t) luaL_checkinteger(L, 1); int32_t v = 0; if (safe_read(a, &v, 4)) lua_pushinteger(L, v); else lua_pushnil(L); return 1; }
static int l_i64(lua_State *L) { uint64_t a = (uint64_t) luaL_checkinteger(L, 1); int64_t v = 0; if (safe_read(a, &v, 8)) lua_pushinteger(L, (lua_Integer) v); else lua_pushnil(L); return 1; }
static int l_u16(lua_State *L) { uint64_t a = (uint64_t) luaL_checkinteger(L, 1); uint16_t v = 0; if (safe_read(a, &v, 2)) lua_pushinteger(L, v); else lua_pushnil(L); return 1; }
static int l_u8(lua_State *L)  { uint64_t a = (uint64_t) luaL_checkinteger(L, 1); uint8_t v = 0; if (safe_read(a, &v, 1)) lua_pushinteger(L, v); else lua_pushnil(L); return 1; }
static int l_f64(lua_State *L) { uint64_t a = (uint64_t) luaL_checkinteger(L, 1); double v = 0; if (safe_read(a, &v, 8)) lua_pushnumber(L, v); else lua_pushnil(L); return 1; }
static int l_str(lua_State *L) {   // Il2CppString ptr -> lua string (utf8)
    auto sp = (Il2CppString *) (uintptr_t) luaL_checkinteger(L, 1);
    if (!sp) { lua_pushstring(L, ""); return 1; }
    int len = il2cpp_string_length ? il2cpp_string_length(sp) : 0;
    Il2CppChar *ch = il2cpp_string_chars ? il2cpp_string_chars(sp) : nullptr;
    if (!ch || len <= 0 || len > 2048) { lua_pushstring(L, ""); return 1; }
    std::string out; out.reserve(len * 3);
    for (int i = 0; i < len; i++) {
        unsigned c = ch[i];
        if (c < 0x80) out += (char) c;
        else if (c < 0x800) { out += (char) (0xC0 | (c >> 6)); out += (char) (0x80 | (c & 0x3F)); }
        else { out += (char) (0xE0 | (c >> 12)); out += (char) (0x80 | ((c >> 6) & 0x3F)); out += (char) (0x80 | (c & 0x3F)); }
    }
    lua_pushlstring(L, out.data(), out.size()); return 1;
}
static int l_newstr(lua_State *L) {  // lua string -> Il2CppString ptr (for String-arg methods)
    auto r = il2cpp_string_new ? il2cpp_string_new(luaL_checkstring(L, 1)) : nullptr;
    lua_pushinteger(L, (lua_Integer) (uintptr_t) r); return 1;
}
// dumpcs() — il2cppdumper 原始功能: 把当前 il2cpp 元数据 dump 成 dump.cs (写到 <dataDir>/files/).
// 只想 dump 的用户: 写个脚本自行决定时机调一次即可 (热更类需等其加载后再调).
static int l_dumpcs(lua_State *L) {
    if (g_dataDir.empty()) { lua_pushboolean(L, 0); return 1; }
    il2cpp_dump(g_dataDir.c_str());
    lua_pushboolean(L, 1); return 1;
}

static const luaL_Reg il2_lib[] = {
        {"find", l_find}, {"instances", l_instances}, {"field", l_field}, {"read", l_read},
        {"i32", l_i32}, {"i64", l_i64}, {"u16", l_u16}, {"u8", l_u8}, {"f64", l_f64},
        {"str", l_str}, {"newstr", l_newstr}, {"dumpcs", l_dumpcs},
        {"u64", l_u64}, {"u32", l_u32}, {"f32", l_f32}, {"write", l_write},
        {"classname", l_classname}, {"log", l_log}, {"sleep", l_sleep},
        {"staticobj", l_staticobj}, {"loop", l_loop}, {"stop", l_stop},
        {"clear", l_clear}, {"box", l_box}, {"text", l_text}, {"present", l_present},
        {"method", l_method}, {"invoke", l_invoke}, {"invoke_v3", l_invoke_v3},
        {"invoke_v3b", l_invoke_v3b}, {"invoke_f", l_invoke_f}, {"invoke_args", l_invoke_args},
        {nullptr, nullptr}
};

void il2cpp_lua_init(const char *dataDir) {
    g_dataDir = dataDir;
    g_lua_log = std::string(dataDir) + "/files/script.log";
    g_overlay = std::string(dataDir) + "/files/overlay.txt";
    g_L = luaL_newstate();
    if (!g_L) return;
    luaL_openlibs(g_L);
    luaL_newlib(g_L, il2_lib);
    lua_setglobal(g_L, "il2");
}
void il2cpp_lua_run_file(const char *path) {
    if (!g_L) return;
    if (luaL_dofile(g_L, path) != LUA_OK) {
        std::ofstream f(g_lua_log, std::ios::app);
        f << "[lua error] " << lua_tostring(g_L, -1) << "\n";
        lua_pop(g_L, 1);
    }
}
// scheduler entry: called frequently by the watch loop; runs the tick fn when due.
// Quick + interruptible: between ticks the watch loop still services .cmd and run.lua.
void il2cpp_lua_tick() {
    if (!g_L || g_tick_ref == LUA_NOREF) return;
    long t = now_ms();
    if (g_tick_last != 0 && t - g_tick_last < g_tick_interval) return;
    g_tick_last = t;
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, g_tick_ref);
    if (lua_pcall(g_L, 0, 0, 0) != LUA_OK) {
        std::ofstream f(g_lua_log, std::ios::app);
        f << "[tick error] " << lua_tostring(g_L, -1) << "\n";
        lua_pop(g_L, 1);
        luaL_unref(g_L, LUA_REGISTRYINDEX, g_tick_ref);
        g_tick_ref = LUA_NOREF;
    }
}