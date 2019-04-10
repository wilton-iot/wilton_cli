/*
 * Copyright 2018, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * File:   load_rhino.hpp
 * Author: alex
 *
 * Created on April 6, 2018, 7:53 PM
 */

#ifndef WILTON_CLI_JVM_ENGINE_HPP
#define WILTON_CLI_JVM_ENGINE_HPP

#include <string>
#include <vector>
#include <utility>

#include <jni.h>

#include "staticlib/config/os.hpp"

#ifdef STATICLIB_WINDOWS
#include "staticlib/support/windows.hpp"
#else // !STATICLIB_WINDOWS
#include <dlfcn.h>
#endif // STATICLIB_WINDOWS

#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/utils.hpp"

#include "wilton/wilton_loader.h"

#include "wilton/support/exception.hpp"

namespace wilton {
namespace cli {
namespace jvm {

typedef jint(*JNI_CreateJavaVM_type)(JavaVM** p_vm, JNIEnv** p_env, void* vm_args);

#ifdef STATICLIB_WINDOWS

JNI_CreateJavaVM_type load_jvm_platform(const std::string& libdir) {
    auto libpath = libdir + "jvm.dll";
    auto wpath = sl::utils::widen(libpath);
    auto lib = ::LoadLibraryW(wpath.c_str());
    if (nullptr == lib) {
        throw support::exception(TRACEMSG(
                "Error loading shared library on path: [" + libpath + "],"
                " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
    }
    auto sym = ::GetProcAddress(static_cast<HMODULE>(lib), "JNI_CreateJavaVM");
    if (nullptr == sym) {
        throw support::exception(TRACEMSG(
                "Error loading symbol: [JNI_CreateJavaVM], " +
                " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
    }
    return reinterpret_cast<JNI_CreateJavaVM_type>(sym);
}

#else // !STATICLIB_WINDOWS

JNI_CreateJavaVM_type load_jvm_platform(const std::string& libdir) {
    auto dlerr_str = []() -> std::string {
        auto res = ::dlerror();
        return nullptr != res ? std::string(res) : "";
    };
#ifdef STATICLIB_MAC
    auto libpath = libdir + "libjvm.dylib";
#else // !STATICLIB_MAC
    auto libpath = libdir + "libjvm.so";
#endif // STATICLIB_MAC
    auto lib = ::dlopen(libpath.c_str(), RTLD_LAZY);
    if (nullptr == lib) throw support::exception(TRACEMSG(
            "Error loading shared library on path: [" + libpath + "]," +
            " error: [" + dlerr_str() + "]"));
    auto sym = ::dlsym(lib, "JNI_CreateJavaVM");
    if (nullptr == sym) throw support::exception(TRACEMSG(
            "Error loading symbol: [JNI_CreateJavaVM], " +
            " error: [" + dlerr_str() + "]"));
    return reinterpret_cast<JNI_CreateJavaVM_type>(sym);
}

#endif // STATICLIB_WINDOWS

JNI_CreateJavaVM_type load_jvm(const std::vector<std::pair<std::string, std::string>>& env_vars) {
    auto java_home = std::string();
    for (auto& pa : env_vars) {
        if ("JAVA_HOME" == pa.first) {
            java_home = sl::tinydir::normalize_path(pa.second) + "/";
            break;
        }
    }
    if (java_home.empty()) throw wilton::support::exception(TRACEMSG(
            "Cannot find JAVA_HOME environment variable"));
    if (!sl::tinydir::path(java_home).exists()) throw wilton::support::exception(TRACEMSG(
            "JAVA_HOME directory not found, path: [" + java_home + "]"));
    if (sl::tinydir::path(java_home + "jre").exists()) {
        java_home.append("jre/");
    }
#ifdef STATICLIB_WINDOWS
    auto libdir = java_home + "bin/server/";
    if (!sl::tinydir::path(libdir).exists()) throw wilton::support::exception(TRACEMSG(
            "Cannot find JVM shared library, dir: [" + libdir + "]"));
#else // !STATICLIB_WINDOWS
    auto basedir = java_home + "lib/";
    auto libdir = std::string();
    if (sl::tinydir::path(basedir + "amd64").exists()) {
        libdir = basedir + "amd64/server/";
    } else if (sl::tinydir::path(basedir + "i386").exists()) {
        libdir = basedir + "i386/server/";
    } else if (sl::tinydir::path(basedir + "aarch64").exists()) {
        libdir = basedir + "aarch64/server/";
    } else if (sl::tinydir::path(basedir + "arm32").exists()) {
        libdir = basedir + "arm32/server/";
    } else if (sl::tinydir::path(basedir + "arm").exists()) {
        libdir = basedir + "arm/client/";
    } else throw wilton::support::exception(TRACEMSG(
            "Cannot find JVM shared library, base dir: [" + basedir + "]"));
#endif // !STATICLIB_WINDOWS
    // platform load
    return load_jvm_platform(libdir);
}

std::string load_resource(const std::string& url) {
    char* out;
    int out_len;
    auto err = wilton_load_resource(url.c_str(), static_cast<int> (url.length()),
            std::addressof(out), std::addressof(out_len));
    if (nullptr != err) {
        auto msg = TRACEMSG(err);
        wilton_free(err);
        throw wilton::support::exception(msg);
    }
    auto deferred = sl::support::defer([out]() STATICLIB_NOEXCEPT {
        wilton_free(out);
    });
    return std::string(out, static_cast<size_t>(out_len));
}

std::string describe_java_exception(JNIEnv* env, jthrowable exc) {
    jclass clazz = env->FindClass("wilton/WiltonJni");
    if (nullptr == clazz) return "EXC_DESCRIBE_ERROR: class";
    auto class_deferred = sl::support::defer([env, clazz]() STATICLIB_NOEXCEPT {
        env->DeleteLocalRef(clazz);
    });
    auto method_name = std::string("describeThrowable");
    auto method_sig = std::string("(Ljava/lang/Throwable;)Ljava/lang/String;");
    jmethodID method = env->GetStaticMethodID(clazz, method_name.c_str(), method_sig.c_str());
    if (nullptr == method) return "EXC_DESCRIBE_ERROR: method";
    jobject jobj = env->CallStaticObjectMethod(clazz, method, exc);
    if (env->ExceptionCheck()) return "EXC_DESCRIBE_ERROR: call";
    jstring jstr = static_cast<jstring>(jobj);
    auto deferred = sl::support::defer([env, jstr]() STATICLIB_NOEXCEPT {
        env->DeleteLocalRef(jstr);
    });
    const char* cstr = env->GetStringUTFChars(jstr, 0);
    auto deferred_jstr = sl::support::defer([env, jstr, cstr]() STATICLIB_NOEXCEPT {
        env->ReleaseStringUTFChars(jstr, cstr);
    });
    size_t cstr_len = static_cast<size_t> (env->GetStringUTFLength(jstr));
    return std::string(cstr, cstr_len);
}

void load_engine(const std::string& script_engine, const std::string& exedir,
        const std::string& modurl, const std::vector<std::pair<std::string, std::string>>& env_vars) {
    // start jvm
    auto opt_libpath = std::string("-Djava.library.path=") + exedir;
    auto opt_classpath = std::string("-Djava.class.path=") + exedir + "wilton_rhino.jar";
    JavaVM* jvm = nullptr;
    JNIEnv* env = nullptr;
    JavaVMInitArgs vm_args;
    std::memset(std::addressof(vm_args), '\0', sizeof(vm_args));
    std::array<JavaVMOption, 2> vm_opts;
    std::memset(std::addressof(vm_opts), '\0', sizeof(vm_opts));
    vm_opts[0].optionString = opt_libpath.c_str();
    vm_opts[1].optionString = opt_classpath.c_str();
    vm_args.version = JNI_VERSION_1_6;
    vm_args.nOptions = static_cast<jint>(vm_opts.size());
    vm_args.options = vm_opts.data();
    vm_args.ignoreUnrecognized = 0;
    auto JNI_CreateJavaVM_fun = load_jvm(env_vars);
    auto err = JNI_CreateJavaVM_fun(std::addressof(jvm), std::addressof(env), std::addressof(vm_args));
    if (JNI_OK  != err) throw wilton::support::exception(TRACEMSG(
            "JVM startup error, code: [" + sl::support::to_string(err) + "]"));

    // load init code
    auto prefix = modurl + "/wilton-requirejs/";
    auto code_jni = load_resource(prefix + "wilton-jni.js");
    auto code_req = load_resource(prefix + "wilton-require.js");
    auto code = code_jni + code_req;
    jstring jcode = env->NewStringUTF(code.c_str());
    auto code_deferred = sl::support::defer([env, jcode]() STATICLIB_NOEXCEPT {
        env->DeleteLocalRef(jcode);
    });

    // init rhino
    auto class_name = "rhino" == script_engine ? 
        std::string("wilton/support/rhino/WiltonRhinoInitializer") :
        std::string("wilton/support/nashorn/WiltonNashornInitializer");
    jclass clazz = env->FindClass(class_name.c_str());
    if (nullptr == clazz) throw wilton::support::exception(TRACEMSG( 
            "JVM startup error, class not found: [" + class_name + "]"));
    auto class_deferred = sl::support::defer([env, clazz]() STATICLIB_NOEXCEPT {
        env->DeleteLocalRef(clazz);
    });
    auto method_name = std::string("initialize");
    auto method_sig = std::string("(Ljava/lang/String;)V");
    jmethodID method = env->GetStaticMethodID(clazz, method_name.c_str(), method_sig.c_str());
    if (nullptr == method) throw wilton::support::exception(TRACEMSG( 
            "JVM startup error, method not found, class: [" + class_name + "]," + 
            " name: [" + method_name + "], signature: [" + method_sig + "]"));
    env->CallStaticVoidMethod(clazz, method, jcode);
    jthrowable exc = env->ExceptionOccurred();
    if (nullptr != exc) {
        env->ExceptionClear();
        std::string trace = describe_java_exception(env, exc);
        throw wilton::support::exception(TRACEMSG(trace));
    }
    //return [jvm]() STATICLIB_NOEXCEPT {
    //    https://stackoverflow.com/a/10991021/314015
    //    jvm->DestroyJavaVM();
    //};
}

} // namespace
}
}

#endif /* WILTON_CLI_JVM_ENGINE_HPP */

