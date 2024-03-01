/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_
#define LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/utils/error/error.h"

#include <QStandardPaths>
#include <QString>

namespace linglong::builder {

utils::error::Result<api::types::v1::BuilderConfig> loadConfig(const QString &file) noexcept;
utils::error::Result<api::types::v1::BuilderConfig> loadConfig(const QStringList &files) noexcept;
utils::error::Result<void> saveConfig(const api::types::v1::BuilderConfig &cfg,
                                      const QString &path) noexcept;
// class BuilderConfig
// {
// public:
// static BuilderConfig &instance();

// QString repoPath() const;

// QString ostreePath() const;

// // TODO: remove later
// QString layerPath(const QStringList &subPathList) const;

// QString projectRoot;

// QString projectName;

// QStringList exec;

// bool offline;

// QString buildArch;

// // TODO: remove later
// QString targetFetchCachePath() const;

// // TODO: remove later
// QString targetSourcePath() const;

// QString templatePath() const;

// QStringList buildEnv;

// QString remoteRepoName;
// QString remoteRepoEndpoint;
// };

} // namespace linglong::builder

#endif // LINGLONG_SRC_BUILDER_BUILDER_BUILDER_CONFIG_H_
