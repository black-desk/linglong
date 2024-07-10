/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_RUNTIME_CONTAINER_OPTION_H_
#define LINGLONG_RUNTIME_CONTAINER_OPTION_H_

#include "linglong/api/types/v1/OciConfigurationPatch.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <QDir>
#include <QString>

#include <optional>

namespace linglong::runtime {

struct ContainerOptions
{
    QString appID;
    QString containerID;

    std::optional<QDir> runtimeDir; // mount to /runtime
    QDir baseDir;                   // mount to /
    std::optional<QDir> appDir;     // mount to /opt/apps/${info.appid}/files

    std::vector<api::types::v1::OciConfigurationPatch> patches;
    std::vector<ocppi::runtime::config::types::Mount> mounts; // extra mounts
    std::vector<std::string> masks;
};

} // namespace linglong::runtime

#endif
