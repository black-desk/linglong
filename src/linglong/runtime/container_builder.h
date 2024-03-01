/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_RUNTIME_CONTAINER_BUILDER_H_
#define LINGLONG_RUNTIME_CONTAINER_BUILDER_H_

#include "linglong/api/types/v1/PackageInfo.hpp"
#include "linglong/package/layer_dir.h"
#include "linglong/runtime/container.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"
#include "ocppi/runtime/config/types/Process.hpp"

#include <QDir>
#include <QProcess>

namespace linglong::runtime {

struct ContainerOptions
{
    QString appID;
    QUuid containerID;
    std::optional<QDir> runtime; // mount to /runtime
    QDir base;                   // mount to /
    QDir app;                    // mount to /opt/apps/${info.appid}/files
    std::vector<ocppi::runtime::config::types::Mount> mounts; // extra mounts
};

class ContainerBuilder
{
public:
    explicit ContainerBuilder(ocppi::cli::CLI &cli);

    utils::error::Result<QSharedPointer<Container>> create(const ContainerOptions &opts) noexcept;

private:
    ocppi::cli::CLI &cli;
};

}; // namespace linglong::runtime

#endif
