/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_RUNTIME_CONTAINER_BUILDER_H_
#define LINGLONG_RUNTIME_CONTAINER_BUILDER_H_

#include "linglong/runtime/container.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"

#include <QDir>
#include <QProcess>

namespace linglong::runtime {

class ContainerOptions;

class ContainerBuilder : public QObject
{
    Q_OBJECT
public:
    explicit ContainerBuilder(ocppi::cli::CLI &cli);

    auto create(const ContainerOptions &opts) noexcept
      -> utils::error::Result<QSharedPointer<Container>>;

private:
    ocppi::cli::CLI &cli;
};

}; // namespace linglong::runtime

#endif
