//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#ifndef __CONFIG_SINGL_HPP__
#define __CONFIG_SINGL_HPP__


#include "dev_support/verify.hpp"

#include <string>

namespace config_common {

// config singleton-wrap
template<class Config>
struct config {
    static std::string dirnameOf(const std::string &fname) {
        size_t pos = fname.find_last_of("\\/");
        return (std::string::npos == pos) ? "" : fname.substr(0, pos);
    }

    static void create_instance(std::string const &filename) {
        load(inner_cfg(), filename);
        is_initialized() = true;
    }

    static Config const &get() {
        VERIFY_MSG(is_initialized(), "Config not initialized");
        return inner_cfg();
    }

    static Config &get_writable() {
        VERIFY_MSG(is_initialized(), "Config not initialized");
        return inner_cfg();
    }

private:
    static Config &inner_cfg() {
        static Config config;
        return config;
    }

    static bool &is_initialized() {
        static bool is_initialized = false;
        return is_initialized;
    }
};

}


#endif // __CONFIG_SINGLE_HPP__