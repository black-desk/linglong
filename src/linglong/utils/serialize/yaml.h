/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_SERIALIZE_YAML_H
#define LINGLONG_UTILS_SERIALIZE_YAML_H

#include "linglong/utils/error/error.h"
#include "ytj/ytj.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <exception>

namespace linglong::utils::serialize {

template<typename T, typename Source>
error::Result<T> LoadYAML(const Source &content)
{
    LINGLONG_TRACE("load yaml");
    try {
        YAML::Node node = YAML::Load(content);
        nlohmann::json json = ytj::to_json(node);
        return json.template get<T>();
    } catch (...) {
        return LINGLONG_ERR(std::current_exception());
    }
}

} // namespace linglong::utils::serialize

#endif
