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
 * File:   ghc_init.hpp
 * Author: alex
 *
 * Created on July 27, 2018, 12:19 PM
 */

#ifndef WILTON_CLI_GHC_INIT_HPP
#define WILTON_CLI_GHC_INIT_HPP

#include <string>

#include "staticlib/config.hpp"
#include "staticlib/json.hpp"
#include "staticlib/ranges.hpp"
#include "staticlib/utils.hpp"

#include "wilton/wiltoncall.h"

#include "wilton/support/exception.hpp"

namespace wilton {
namespace cli {
namespace ghc {

void init_and_run_main(const std::string& wilton_home, const std::string& startup_desc,
        const std::vector<std::string>& appargs) {
    auto startup_parts = sl::utils::split(startup_desc, ':');
    if (2 != startup_parts.size() || startup_parts.at(0).empty() || startup_parts.at(1).empty()) {
        throw support::exception(TRACEMSG("Invalid GHC startup module specified: [" + startup_desc + "]"));
    }
    auto& startmod = startup_parts.at(0);
    auto& startcall = startup_parts.at(1);

    // prepare wilton config
    auto wilton_config = sl::json::dumps({
        {"defaultScriptEngine", "ghc"},
        {"wiltonHome", wilton_home},
// add compile-time OS
#if defined(STATICLIB_ANDROID)
        {"compileTimeOS", "android"}
#elif defined(STATICLIB_WINDOWS)
        {"compileTimeOS", "windows"}
#elif defined(STATICLIB_LINUX)
        {"compileTimeOS", "linux"}
#elif defined(STATICLIB_MAC)
        {"compileTimeOS", "macos"}
#endif // OS
    });

    // init wilton
    auto err_wilton_init = wiltoncall_init(wilton_config.c_str(), static_cast<int> (wilton_config.length()));
    if (nullptr != err_wilton_init) {
        support::throw_wilton_error(err_wilton_init, TRACEMSG(err_wilton_init));
    }

    // load ghc module
    auto ghcmod = std::string("wilton_ghc");
    auto err_dyload_ghcmod = wilton_dyload(ghcmod.c_str(), static_cast<int>(ghcmod.length()), nullptr, 0);
    if (nullptr != err_dyload_ghcmod) {
        support::throw_wilton_error(err_dyload_ghcmod, TRACEMSG(err_dyload_ghcmod));
    }

    // init ghc runtime
    auto ghc_config = sl::json::dumps({
        {"shimLibDirectory", wilton_home + "bin"}
    });
    auto ghc_init = std::string("ghc_init");
    char* ghc_init_res = nullptr;
    int ghc_init_res_len = 0;
    auto err_ghc_init = wiltoncall(ghc_init.c_str(), static_cast<int>(ghc_init.length()), 
            ghc_config.c_str(), static_cast<int>(ghc_config.length()),
            std::addressof(ghc_init_res), std::addressof(ghc_init_res_len));
    wilton_free(ghc_init_res);
    if (nullptr != err_ghc_init) {
        support::throw_wilton_error(err_ghc_init, TRACEMSG(err_ghc_init));
    }

    // load startup module
    auto err_dyload_startup = wilton_dyload(startmod.c_str(), static_cast<int>(startmod.length()), nullptr, 0);
    if (nullptr != err_dyload_startup) {
        support::throw_wilton_error(err_dyload_startup, TRACEMSG(err_dyload_startup));
    }

    // call startup function
    auto args_json_vec = sl::ranges::transform(appargs, [](const std::string& st) -> sl::json::value {
        return sl::json::value(st);
    }).to_vector();
    auto args_json = sl::json::dumps(std::move(args_json_vec));
    char* startup_res = nullptr;
    int startup_res_len = 0;
    auto err_startup = wiltoncall(startcall.c_str(), static_cast<int>(startcall.length()), 
            args_json.c_str(), static_cast<int>(args_json.length()),
            std::addressof(startup_res), std::addressof(startup_res_len));
    wilton_free(startup_res);
    if (nullptr != err_startup) {
        support::throw_wilton_error(err_startup, TRACEMSG(err_startup));
    }
}


} // namespace
}
}

#endif /* WILTON_CLI_GHC_INIT_HPP */

