/* 
 * File:   cli.cpp
 * Author: alex
 *
 * Created on June 6, 2017, 6:31 PM
 */

#include <iostream>
#include <string>
#include <vector>

#include <popt.h>

#include "staticlib/io.hpp"
#include "staticlib/json.hpp"
#include "staticlib/utils.hpp"
#include "staticlib/tinydir.hpp"

#include "wilton/wiltoncall.h"
#include "wilton/wilton_signal.h"

#include "wilton/support/exception.hpp"

#include "cli_options.hpp"

namespace { // anonymous

int find_launcher_args_end(int argc, char** argv) {
    for (int i = 0; i < argc; i++) {
        if ("--" ==  std::string(argv[i])) {
            return i;
        }
    }
    return argc;
}

std::string find_exedir() {
    auto exepath = sl::utils::current_executable_path();
    auto exedir = sl::utils::strip_filename(exepath);
    std::replace(exedir.begin(), exedir.end(), '\\', '/');
    return exedir;
}

std::string find_startup_module(const std::string& opts_startup_module_name, const std::string& idxfile_or_dir) {
    if (!opts_startup_module_name.empty()) {
        return opts_startup_module_name;
    }
    auto sm = sl::utils::strip_parent_dir(idxfile_or_dir);
    if (sl::utils::ends_with(sm, ".js")) {
        sm.resize(sm.length() - 3);
    }
    if (sl::utils::ends_with(sm, "/")) {
        sm.resize(sm.length() - 1);
    }
    return sm;
}

std::string find_statup_module_path(const std::string& idxfile_or_dir) {
    auto smp = idxfile_or_dir;
    if (sl::utils::ends_with(smp, ".js")) {
        smp.resize(smp.length() - 3);
    }
    return smp;
}

std::string find_app_dir(const std::string& idxfile_or_dir, const std::string& startmod) {
    // starting a standalone script
    if ('/' != idxfile_or_dir.back()) {
        return sl::utils::strip_filename(idxfile_or_dir);
    }
    
    // starting module
    size_t depth = 1;
    for (size_t i = 0; i < startmod.length(); i++) {
        if ('/' == startmod.at(i)) {
            depth += 1;
        }
    }
    auto res = std::string(idxfile_or_dir.data(), idxfile_or_dir.length());
    for (size_t i = 0; i < depth; i++) {
        res.append("../");
    }
    auto abs = sl::tinydir::full_path(res);
    abs.push_back('/');
    return abs;
}

void init_signals() {
    static std::string name = "wilton_signal";
    auto err_dyload = wilton_dyload(name.c_str(), static_cast<int>(name.length()), nullptr, 0);
    if (nullptr != err_dyload) {
        auto msg = TRACEMSG(err_dyload);
        wilton_free(err_dyload);
        throw wilton::support::exception(msg);
    }
    auto err_init = wilton_signal_initialize();
    if (nullptr != err_init) {
        auto msg = TRACEMSG(err_init);
        wilton_free(err_init);
        throw wilton::support::exception(msg);
    }
}

std::vector<sl::json::field> collect_env_vars(char** envp) {
    auto res = std::vector<sl::json::field>();
    for (char** el = envp; *el != nullptr; el++) {
        auto var = std::string(*el);
        auto parts = sl::utils::split(var, '=');
        if (2 == parts.size()) {
            res.emplace_back(parts.at(0), parts.at(1));
        }
    }
    return res;
}

std::string write_temp_one_liner(const std::string& exedir, const std::string& deps, const std::string& code) {
    // prepare tmp file path
    auto rsg = sl::utils::random_string_generator();
    auto path_str = exedir + "../work/" + rsg.generate(8) + ".js";
    auto path = sl::tinydir::path(path_str);
    auto tmpl_path = sl::tinydir::path(exedir + "../conf/one-liner-template.js");
    
    // prepare deps
    auto deps_line = std::string();
    auto args_line = std::string();
    if (deps.length() > 0) {
        auto parts = sl::utils::split(deps, ':');
        for (size_t i = 0; i < parts.size(); i++) {
            auto dep = parts.at(i);
            if (dep.length() > 0) {
                deps_line.append("\"").append(dep).append("\"");
                if (i < parts.size() - 1) {
                    deps_line.append(", ");
                }
                auto dep_parts = sl::utils::split(dep, '/');
                if (dep_parts.size() > 0) {
                    auto dep_name = dep_parts.back();
                    args_line.append(dep_name);
                    if (i < parts.size() - 1) {
                        args_line.append(", ");
                    }
                }
            }
        }
    }

    // load and format template
    auto tmpl_src = tmpl_path.open_read();
    auto src = sl::io::make_replacer_source(tmpl_src, {
        {"deps_line", deps_line},
        {"args_line", args_line},
        {"code", code}
    }, [](const std::string& err) {
        std::cerr << err << std::endl;
    });
    auto sink = path.open_write();
    sl::io::copy_all(src, sink);
    return path_str;
}

} // namespace

// valgrind run:
// valgrind --leak-check=yes --show-reachable=yes --track-origins=yes --error-exitcode=42 --track-fds=yes --suppressions=../deps/cmake/resources/valgrind/openssl_malloc.supp  ./bin/wilton_cli ../test/scripts/runWiltonTests.js -m ../../modules.zip

int main(int argc, char** argv, char** envp) {    
    try {
        // parse laucher args
        int launcher_argc = find_launcher_args_end(argc, argv);
        wilton::launcher::cli_options opts(launcher_argc, argv);
        
        // collect app args
        auto apprags = std::vector<std::string>();
        for (int i = launcher_argc + 1; i < argc; i++) {
            apprags.emplace_back(argv[i]);
        }

        // check invalid options
        if (!opts.parse_error.empty()) {
            std::cerr << "ERROR: " << opts.parse_error << std::endl;
            std::cerr << opts.usage() << std::endl;
            return 1;
        }

        // show help
        if (0 != opts.help) {
            std::cout << opts.usage() << std::endl;
            poptPrintHelp(opts.ctx, stdout, 0);
            return 0;
        }                
        
        auto exedir = find_exedir();
        
        // check startup script
        auto idxfile_or_dir = 0 == opts.exec_one_liner ? opts.indexjs : 
                write_temp_one_liner(exedir, opts.exec_deps, opts.exec_code);
        auto tmpcleaner = sl::support::defer([&opts, &idxfile_or_dir]() STATICLIB_NOEXCEPT {
            if (0 != opts.exec_one_liner) {
                std::remove(idxfile_or_dir.c_str());
            }
        });
        auto indexpath = sl::tinydir::path(idxfile_or_dir);
        if (!indexpath.exists()) {
            std::cerr << "ERROR: specified script file not found: [" + idxfile_or_dir + "]" << std::endl;
            return 1;
        }
        if (indexpath.is_directory() && '/' != idxfile_or_dir.back()) {
            idxfile_or_dir.push_back('/');
        }                
        
        // check modules dir
        auto moddir = !opts.modules_dir_or_zip.empty() ? opts.modules_dir_or_zip : exedir + "../js.zip";
        auto modpath = sl::tinydir::path(moddir);
        if (!modpath.exists()) {
            std::cerr << "ERROR: specified modules directory (or zip bundle) not found: [" + moddir + "]" << std::endl;
            return 1;
        }
        auto modurl = modpath.is_directory() ?
                std::string("file://") + moddir :
                std::string("zip://") + moddir;
        if (modpath.is_directory() && '/' != modurl.at(modurl.length() - 1)) {
            modurl.push_back('/');
        }
        
        // get index module
        auto startmod = find_startup_module(opts.startup_module_name, idxfile_or_dir);
        auto startmodpath = find_statup_module_path(idxfile_or_dir);
        auto appdir = find_app_dir(idxfile_or_dir, startmod);
                
        // env vars
        auto env_vars = collect_env_vars(envp);
        
        // wilton init
        auto script_engine = std::string("duktape");
        auto config = sl::json::dumps({
            {"defaultScriptEngine", script_engine},
            {"applicationDirectory", appdir},
            {"environmentVariables", std::move(env_vars)},
            {"requireJs", {
                    {"waitSeconds", 0},
                    {"enforceDefine", true},
                    {"nodeIdCompat", true},
                    {"baseUrl", modurl},
                    {"paths", {
                        { startmod, "file://" + startmodpath }
                    }}
                }
            }
        });

        auto err_init = wiltoncall_init(config.c_str(), static_cast<int> (config.length()));
        if (nullptr != err_init) {
            std::cerr << "ERROR: " << err_init << std::endl;
            wilton_free(err_init);
            return 1;
        }

        // load script engine
        auto enginelib = "wilton_" + script_engine;
        auto err_dyload = wilton_dyload(enginelib.c_str(), static_cast<int>(enginelib.length()),
                nullptr, 0);
        if (nullptr != err_dyload) {
            std::cerr << "ERROR: " << err_dyload << std::endl;
            wilton_free(err_dyload);
            return 1;
        }

        // index.js input
        auto input = sl::json::dumps({
            {"module", startmod},
            {"func", "main"},
            {"args", [&apprags] {
                    auto res = std::vector<sl::json::value>();
                    for (auto& st : apprags) {
                        res.emplace_back(st);
                    }
                    return res;
                }()}
        });

        // init signals/ctrl+c to allow their use from js
        init_signals();

        // call index.js
        char* out = nullptr;
        int out_len = 0;
        char* err_run = wiltoncall_runscript(script_engine.c_str(), static_cast<int>(script_engine.length()),
                input.c_str(), static_cast<int> (input.length()), &out, &out_len);
        if (nullptr != err_run) {
            std::cerr << "ERROR: " << err_run << std::endl;
            wilton_free(err_run);
            return 1;
        }
        wilton_free(out);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}

