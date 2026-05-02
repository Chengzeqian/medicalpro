#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace mesh_gpu {
namespace ascend_runtime {

using AclError = int;
using AclrtStream = void*;

#if defined(_WIN32)
using DynamicLibHandle = HMODULE;

inline DynamicLibHandle openDynamicLibrary(const std::string& path) {
    return LoadLibraryA(path.c_str());
}

inline void closeDynamicLibrary(DynamicLibHandle handle) {
    if (handle) {
        FreeLibrary(handle);
    }
}

inline void* getDynamicSymbol(DynamicLibHandle handle, const char* symbol_name) {
    if (!handle || !symbol_name) {
        return nullptr;
    }
    return reinterpret_cast<void*>(GetProcAddress(handle, symbol_name));
}
#else
using DynamicLibHandle = void*;

inline DynamicLibHandle openDynamicLibrary(const std::string& path) {
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
}

inline void closeDynamicLibrary(DynamicLibHandle handle) {
    if (handle) {
        dlclose(handle);
    }
}

inline void* getDynamicSymbol(DynamicLibHandle handle, const char* symbol_name) {
    if (!handle || !symbol_name) {
        return nullptr;
    }
    return dlsym(handle, symbol_name);
}
#endif

struct RuntimeApi {
    using FnAclInit = AclError (*)(const char*);
    using FnAclFinalize = AclError (*)();
    using FnAclrtSetDevice = AclError (*)(int);
    using FnAclrtResetDevice = AclError (*)(int);
    using FnAclrtCreateContext = AclError (*)(void**, int);
    using FnAclrtDestroyContext = AclError (*)(void*);
    using FnAclrtCreateStream = AclError (*)(AclrtStream*);
    using FnAclrtDestroyStream = AclError (*)(AclrtStream);
    using FnAclrtMemcpy = AclError (*)(void*, std::size_t, const void*, std::size_t, int);
    using FnAclrtSynchronizeStream = AclError (*)(AclrtStream);
    using FnAclrtMalloc = AclError (*)(void**, std::size_t, int);
    using FnAclrtFree = AclError (*)(void*);
    using FnAclCreateTensorDesc = void* (*)(int, int, const std::int64_t*, int);
    using FnAclDestroyTensorDesc = AclError (*)(void*);
    using FnAclCreateDataBuffer = void* (*)(void*, std::size_t);
    using FnAclDestroyDataBuffer = AclError (*)(void*);
    using FnAclopCreateAttr = void* (*)();
    using FnAclopDestroyAttr = AclError (*)(void*);
    using FnAclopCompileAndExecute = AclError (*)(
        const char*,
        int,
        void* const*,
        void* const*,
        int,
        void* const*,
        void* const*,
        void*,
        int,
        int,
        const char*,
        AclrtStream);

    std::vector<DynamicLibHandle> handles;
    std::vector<std::string> loaded_paths;
    FnAclInit aclInit = nullptr;
    FnAclFinalize aclFinalize = nullptr;
    FnAclrtSetDevice aclrtSetDevice = nullptr;
    FnAclrtResetDevice aclrtResetDevice = nullptr;
    FnAclrtCreateContext aclrtCreateContext = nullptr;
    FnAclrtDestroyContext aclrtDestroyContext = nullptr;
    FnAclrtCreateStream aclrtCreateStream = nullptr;
    FnAclrtDestroyStream aclrtDestroyStream = nullptr;
    FnAclrtMemcpy aclrtMemcpy = nullptr;
    FnAclrtSynchronizeStream aclrtSynchronizeStream = nullptr;
    FnAclrtMalloc aclrtMalloc = nullptr;
    FnAclrtFree aclrtFree = nullptr;
    FnAclCreateTensorDesc aclCreateTensorDesc = nullptr;
    FnAclDestroyTensorDesc aclDestroyTensorDesc = nullptr;
    FnAclCreateDataBuffer aclCreateDataBuffer = nullptr;
    FnAclDestroyDataBuffer aclDestroyDataBuffer = nullptr;
    FnAclopCreateAttr aclopCreateAttr = nullptr;
    FnAclopDestroyAttr aclopDestroyAttr = nullptr;
    FnAclopCompileAndExecute aclopCompileAndExecute = nullptr;

    void unload() {
        aclInit = nullptr;
        aclFinalize = nullptr;
        aclrtSetDevice = nullptr;
        aclrtResetDevice = nullptr;
        aclrtCreateContext = nullptr;
        aclrtDestroyContext = nullptr;
        aclrtCreateStream = nullptr;
        aclrtDestroyStream = nullptr;
        aclrtMemcpy = nullptr;
        aclrtSynchronizeStream = nullptr;
        aclrtMalloc = nullptr;
        aclrtFree = nullptr;
        aclCreateTensorDesc = nullptr;
        aclDestroyTensorDesc = nullptr;
        aclCreateDataBuffer = nullptr;
        aclDestroyDataBuffer = nullptr;
        aclopCreateAttr = nullptr;
        aclopDestroyAttr = nullptr;
        aclopCompileAndExecute = nullptr;
        for (auto handle : handles) {
            closeDynamicLibrary(handle);
        }
        handles.clear();
        loaded_paths.clear();
    }

    bool hasRequiredRuntime() const {
        return aclrtMemcpy && aclrtSynchronizeStream && aclrtMalloc && aclrtFree;
    }

    bool hasLifecycleApi() const {
        return aclInit && aclFinalize &&
               aclrtSetDevice && aclrtResetDevice &&
               aclrtCreateContext && aclrtDestroyContext &&
               aclrtCreateStream && aclrtDestroyStream &&
               hasRequiredRuntime();
    }

    bool hasOpApi() const {
        return aclCreateTensorDesc && aclDestroyTensorDesc &&
               aclCreateDataBuffer && aclDestroyDataBuffer &&
               aclopCreateAttr && aclopDestroyAttr &&
               aclopCompileAndExecute;
    }

    bool load(std::string& reason, const std::string& context_label = "runtime") {
        if (!handles.empty() && hasRequiredRuntime()) {
            reason = "Ascend runtime symbols already loaded for " + context_label + ".";
            return true;
        }

        unload();

        std::vector<std::string> runtime_candidates;
#if defined(_WIN32)
        runtime_candidates.emplace_back("ascendcl.dll");
        runtime_candidates.emplace_back("acl.dll");
        runtime_candidates.emplace_back("acl_rt.dll");
#else
        runtime_candidates.emplace_back("libascendcl.so");
        runtime_candidates.emplace_back("libascendcl.so.1");
        runtime_candidates.emplace_back("libacl_rt.so");
        runtime_candidates.emplace_back("libacl.so");
#endif

        const char* ascend_home = std::getenv("ASCEND_HOME_PATH");
        if (ascend_home && ascend_home[0] != '\0') {
            const std::string base(ascend_home);
#if defined(_WIN32)
            appendPathCandidate(runtime_candidates, base, "runtime\\lib64\\ascendcl.dll");
            appendPathCandidate(runtime_candidates, base, "lib64\\ascendcl.dll");
            appendPathCandidate(runtime_candidates, base, "runtime\\lib64\\acl.dll");
            appendPathCandidate(runtime_candidates, base, "lib64\\acl.dll");
#else
            appendPathCandidate(runtime_candidates, base, "runtime/lib64/libascendcl.so");
            appendPathCandidate(runtime_candidates, base, "lib64/libascendcl.so");
            appendPathCandidate(runtime_candidates, base, "lib64/libacl_rt.so");
            appendPathCandidate(runtime_candidates, base, "lib64/libacl.so");
            appendPathCandidate(runtime_candidates, base, "devlib/libascendcl.so");
            appendPathCandidate(runtime_candidates, base, "devlib/linux/x86_64/libascendcl.so");
            appendPathCandidate(runtime_candidates, base, "x86_64-linux/lib64/libascendcl.so");
            appendPathCandidate(runtime_candidates, base, "x86_64-linux/devlib/libascendcl.so");
#endif
        }

        std::string first_runtime_hit;
        for (const auto& candidate : runtime_candidates) {
            if (!openAndTrack(candidate)) {
                continue;
            }
            if (first_runtime_hit.empty()) {
                first_runtime_hit = candidate;
            }
            resolveSymbols();
            if (hasRequiredRuntime()) {
                break;
            }
        }

        if (!hasRequiredRuntime()) {
            reason = "Ascend runtime not found or missing required runtime symbols in " + context_label + ".";
            unload();
            return false;
        }

        if (!hasOpApi()) {
            std::vector<std::string> op_candidates;
#if defined(_WIN32)
            op_candidates.emplace_back("acl_op_compiler.dll");
            op_candidates.emplace_back("acl_op_executor.dll");
#else
            op_candidates.emplace_back("libacl_op_compiler.so");
            op_candidates.emplace_back("libacl_op_executor.so");
            op_candidates.emplace_back("libopat.so");
#endif

            if (ascend_home && ascend_home[0] != '\0') {
                const std::string base(ascend_home);
#if defined(_WIN32)
                appendPathCandidate(op_candidates, base, "lib64\\acl_op_compiler.dll");
                appendPathCandidate(op_candidates, base, "lib64\\acl_op_executor.dll");
#else
                appendPathCandidate(op_candidates, base, "lib64/libacl_op_compiler.so");
                appendPathCandidate(op_candidates, base, "lib64/libacl_op_executor.so");
                appendPathCandidate(op_candidates, base, "lib64/libopat.so");
                appendPathCandidate(op_candidates, base, "devlib/libacl_op_compiler.so");
                appendPathCandidate(op_candidates, base, "devlib/linux/x86_64/libacl_op_compiler.so");
                appendPathCandidate(op_candidates, base, "x86_64-linux/lib64/libacl_op_compiler.so");
                appendPathCandidate(op_candidates, base, "x86_64-linux/devlib/libacl_op_compiler.so");
#endif
            }

            for (const auto& candidate : op_candidates) {
                if (!openAndTrack(candidate)) {
                    continue;
                }
                resolveSymbols();
                if (hasOpApi()) {
                    break;
                }
            }
        }

        if (first_runtime_hit.empty()) {
            reason = "Loaded Ascend runtime for " + context_label + ".";
        } else {
            reason = "Loaded Ascend runtime for " + context_label + ": " + first_runtime_hit;
        }
        reason += hasOpApi() ? " (op API ready)" : " (op API partially available)";
        return true;
    }

private:
    static void appendPathCandidate(std::vector<std::string>& out,
                                    const std::string& base,
                                    const std::string& leaf) {
        if (base.empty() || leaf.empty()) {
            return;
        }
#if defined(_WIN32)
        out.push_back(base + "\\" + leaf);
#else
        out.push_back(base + "/" + leaf);
#endif
    }

    bool openAndTrack(const std::string& candidate) {
        if (candidate.empty()) {
            return false;
        }
        for (const auto& path : loaded_paths) {
            if (path == candidate) {
                return false;
            }
        }
        DynamicLibHandle handle = openDynamicLibrary(candidate);
        if (!handle) {
            return false;
        }
        handles.push_back(handle);
        loaded_paths.push_back(candidate);
        return true;
    }

    void* findSymbolAny(const char* symbol_name) const {
        if (!symbol_name) {
            return nullptr;
        }
        for (auto it = handles.rbegin(); it != handles.rend(); ++it) {
            void* ptr = getDynamicSymbol(*it, symbol_name);
            if (ptr) {
                return ptr;
            }
        }
        return nullptr;
    }

    void resolveSymbols() {
        if (!aclInit) {
            aclInit = reinterpret_cast<FnAclInit>(findSymbolAny("aclInit"));
        }
        if (!aclFinalize) {
            aclFinalize = reinterpret_cast<FnAclFinalize>(findSymbolAny("aclFinalize"));
        }
        if (!aclrtSetDevice) {
            aclrtSetDevice = reinterpret_cast<FnAclrtSetDevice>(findSymbolAny("aclrtSetDevice"));
        }
        if (!aclrtResetDevice) {
            aclrtResetDevice = reinterpret_cast<FnAclrtResetDevice>(findSymbolAny("aclrtResetDevice"));
        }
        if (!aclrtCreateContext) {
            aclrtCreateContext = reinterpret_cast<FnAclrtCreateContext>(findSymbolAny("aclrtCreateContext"));
        }
        if (!aclrtDestroyContext) {
            aclrtDestroyContext = reinterpret_cast<FnAclrtDestroyContext>(findSymbolAny("aclrtDestroyContext"));
        }
        if (!aclrtCreateStream) {
            aclrtCreateStream = reinterpret_cast<FnAclrtCreateStream>(findSymbolAny("aclrtCreateStream"));
        }
        if (!aclrtDestroyStream) {
            aclrtDestroyStream = reinterpret_cast<FnAclrtDestroyStream>(findSymbolAny("aclrtDestroyStream"));
        }
        if (!aclrtMemcpy) {
            aclrtMemcpy = reinterpret_cast<FnAclrtMemcpy>(findSymbolAny("aclrtMemcpy"));
        }
        if (!aclrtSynchronizeStream) {
            aclrtSynchronizeStream =
                reinterpret_cast<FnAclrtSynchronizeStream>(findSymbolAny("aclrtSynchronizeStream"));
        }
        if (!aclrtMalloc) {
            aclrtMalloc = reinterpret_cast<FnAclrtMalloc>(findSymbolAny("aclrtMalloc"));
        }
        if (!aclrtFree) {
            aclrtFree = reinterpret_cast<FnAclrtFree>(findSymbolAny("aclrtFree"));
        }
        if (!aclCreateTensorDesc) {
            aclCreateTensorDesc =
                reinterpret_cast<FnAclCreateTensorDesc>(findSymbolAny("aclCreateTensorDesc"));
        }
        if (!aclDestroyTensorDesc) {
            aclDestroyTensorDesc =
                reinterpret_cast<FnAclDestroyTensorDesc>(findSymbolAny("aclDestroyTensorDesc"));
        }
        if (!aclCreateDataBuffer) {
            aclCreateDataBuffer =
                reinterpret_cast<FnAclCreateDataBuffer>(findSymbolAny("aclCreateDataBuffer"));
        }
        if (!aclDestroyDataBuffer) {
            aclDestroyDataBuffer =
                reinterpret_cast<FnAclDestroyDataBuffer>(findSymbolAny("aclDestroyDataBuffer"));
        }
        if (!aclopCreateAttr) {
            aclopCreateAttr = reinterpret_cast<FnAclopCreateAttr>(findSymbolAny("aclopCreateAttr"));
        }
        if (!aclopDestroyAttr) {
            aclopDestroyAttr = reinterpret_cast<FnAclopDestroyAttr>(findSymbolAny("aclopDestroyAttr"));
        }
        if (!aclopCompileAndExecute) {
            aclopCompileAndExecute =
                reinterpret_cast<FnAclopCompileAndExecute>(findSymbolAny("aclopCompileAndExecute"));
        }
    }
};

} // namespace ascend_runtime
} // namespace mesh_gpu
