/*
 * Copyright 2017, alex at staticlibs.net
 * Copyright 2018, myasnikov.mike at gmail.com
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
 * File:   cli_options.hpp
 * Author: alex
 *
 * Created on June 29, 2017, 3:09 PM
 */

#ifndef WILTON_CLI_OPTIONS_HPP
#define WILTON_CLI_OPTIONS_HPP

#include <algorithm>
#include <string>
#include <vector>

#include <popt.h>

namespace wilton {
namespace cli {

class cli_options {
    std::vector<struct poptOption> table;
    char* modules_dir_or_zip_ptr = nullptr;
    char* application_dir_ptr = nullptr;
    char* startup_module_name_ptr = nullptr;
    char* binary_modules_paths_ptr = nullptr;
    char* script_engine_name_ptr = nullptr;
    char* debug_port_ptr = nullptr;
    char* new_project_ptr = nullptr;
    char* environment_vars_ptr = nullptr;
    char* crypt_call_ptr = nullptr;

public:
    poptContext ctx = nullptr;
    std::string parse_error;
    std::vector<std::string> args;

    // public options list
    std::string modules_dir_or_zip;
    std::string application_dir;
    std::string startup_module_name;
    std::string binary_modules_paths;
    std::string script_engine_name;
    std::string debug_port;
    std::string new_project;
    std::string environment_vars;
    std::string crypt_call_lib;
    std::string crypt_call_name;
    int exec_one_liner = 0;
    int print_config = 0;
    int load_only = 0;
    int help = 0;
    int trace_enable = 0;
    int ghc_init = 0;
    int version = 0;

    std::string startup_script;
    std::string exec_deps;
    std::string exec_code;

    cli_options(int argc, char** argv) :
    table({
        { "js-modules-dir-or-zip", 'm', POPT_ARG_STRING, std::addressof(modules_dir_or_zip_ptr), static_cast<int> ('m'), "Path to JavaScript modules directory or ZIP bundle", nullptr},
        { "startup-module-name", 's', POPT_ARG_STRING, std::addressof(startup_module_name_ptr), static_cast<int> ('s'), "Name of the startup module", nullptr},
        { "binary-modules", 'b', POPT_ARG_STRING, std::addressof(binary_modules_paths_ptr), static_cast<int> ('b'), "Binary modules paths list with ':' separator", nullptr},
        { "application-dir", 'a', POPT_ARG_STRING, std::addressof(application_dir_ptr), static_cast<int> ('a'), "Path to application directory", nullptr},
        { "javascript-engine", 'j', POPT_ARG_STRING, std::addressof(script_engine_name_ptr), static_cast<int> ('j'), "Name of the JavaScript engine to use", nullptr},
        { "debug-enable-on-port", 'd', POPT_ARG_STRING, std::addressof(debug_port_ptr), static_cast<int> ('d'), "Port to use for debugger", nullptr},
        { "load-only", 'l', POPT_ARG_NONE, std::addressof(load_only), static_cast<int> ('l'), "Load specified script without calling 'main' function", nullptr},
        { "exec-one-liner", 'e', POPT_ARG_NONE, std::addressof(exec_one_liner), static_cast<int> ('e'), "Execute one-liner script", nullptr},
        { "print-config", 'p', POPT_ARG_NONE, std::addressof(print_config), static_cast<int> ('p'), "Print config on startup", nullptr},
        { "trace-enable", 't', POPT_ARG_NONE, std::addressof(trace_enable), static_cast<int> ('t'), "Enables trace calls gathering", nullptr},
        { "ghc-init", 'g', POPT_ARG_NONE, std::addressof(ghc_init), static_cast<int> ('g'), "Initialize GHC runtime instead of JS engine", nullptr},
        { "new-project", 'n', POPT_ARG_STRING, std::addressof(new_project_ptr), static_cast<int> ('n'), "Create a new 'wilton application' project", nullptr},
        { "environment-vars", 'r', POPT_ARG_STRING, std::addressof(environment_vars_ptr), static_cast<int> ('r'), "Additional environment variables with ':' separator", nullptr},
        { "crypt-call", 'c', POPT_ARG_STRING, std::addressof(crypt_call_ptr), static_cast<int> ('c'), "Description of the native call in 'libname:callname' format to use for loading encrypted .wlib modules", nullptr},
        { "version", 'v', POPT_ARG_NONE, std::addressof(version), static_cast<int> ('v'), "Show version number", nullptr},
        { "help", 'h', POPT_ARG_NONE, std::addressof(help), static_cast<int> ('h'), "Show this help message", nullptr},
        { nullptr, 0, 0, nullptr, 0, nullptr, nullptr}
    }) {

        { // create context
            ctx = poptGetContext(nullptr, argc, const_cast<const char**> (argv), table.data(), POPT_CONTEXT_NO_EXEC);
            if (!ctx) {
                parse_error.append("'poptGetContext' error");
                return;
            }
        }

        { // parse options
            int val;
            while ((val = poptGetNextOpt(ctx)) >= 0);
            if (val < -1) {
                parse_error.append(poptStrerror(val));
                parse_error.append(": ");
                parse_error.append(poptBadOption(ctx, POPT_BADOPTION_NOALIAS));
                return;
            }
        }

        { // collect arguments
            const char* ar;
            while (nullptr != (ar = poptGetArg(ctx))) {
                args.emplace_back(std::string(ar));
            }
        }

        if (0 == help && 0 == version) {
            // check script specified
            if (0 == exec_one_liner && nullptr == new_project_ptr &&
                    (1 != args.size() || args.at(0).empty())) {
                parse_error.append("invalid arguments, startup script not specified");
                return;
            }

            // set options and fix slashes
            if (nullptr == new_project_ptr) {
                if (0 == exec_one_liner) {
                    startup_script = args.at(0);
                    std::replace(startup_script.begin(), startup_script.end(), '\\', '/');
                } else if (2 != args.size()) {
                    parse_error.append("invalid one-liner arguments, expected: [<dep1:dep2:...> \"<code>\"]");
                    return;
                } else {
                    exec_deps = args.at(0);
                    exec_code = args.at(1);
                }
            }

            modules_dir_or_zip = nullptr != modules_dir_or_zip_ptr ? std::string(modules_dir_or_zip_ptr) : "";
            std::replace(modules_dir_or_zip.begin(), modules_dir_or_zip.end(), '\\', '/');
            startup_module_name = nullptr != startup_module_name_ptr ? std::string(startup_module_name_ptr) : "";
            std::replace(startup_module_name.begin(), startup_module_name.end(), '\\', '/');
            binary_modules_paths = nullptr != binary_modules_paths_ptr ? std::string(binary_modules_paths_ptr): "";
            std::replace(binary_modules_paths.begin(), binary_modules_paths.end(), '\\', '/');
            application_dir = nullptr != application_dir_ptr ? std::string(application_dir_ptr) : "";
            std::replace(application_dir.begin(), application_dir.end(), '\\', '/');
            script_engine_name = nullptr != script_engine_name_ptr ? std::string(script_engine_name_ptr) : "";
            debug_port = (nullptr != debug_port_ptr) ? std::string(debug_port_ptr) : "";
            new_project = (nullptr != new_project_ptr) ? std::string(new_project_ptr) : "";
            environment_vars = (nullptr != environment_vars_ptr) ? std::string(environment_vars_ptr) : "";

            if (nullptr != crypt_call_ptr) {
                auto crypt_call = std::string(crypt_call_ptr);
                auto parts = sl::utils::split(crypt_call, ':');
                if (2 != parts.size()) {
                    parse_error.append("Invalid 'crypt-call' parameter specified,");
                    parse_error.append(" expected format: [libname:callname], value: [");
                    parse_error.append(crypt_call);
                    parse_error.append("]");
                }
                crypt_call_lib = std::move(parts.at(0));
                crypt_call_name = std::move(parts.at(1));
            }
        }
    }

    const std::string usage() {
        std::string msg = "USAGE: wilton path/to/script.js"
                " [OPTION...]"
                " [-- <app arguments>]";
        return msg;
    }

    ~cli_options() {
        poptFreeContext(ctx);
    }

    cli_options(const cli_options& other) = delete;

    cli_options& operator=(const cli_options& other) = delete;
};


} // namespace
}

#endif /* WILTON_CLI_OPTIONS_HPP */

