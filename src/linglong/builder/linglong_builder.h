/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
#define LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/cli/printer.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/util/error.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <nlohmann/json.hpp>

namespace linglong {
namespace service {
class AppManager;
}

namespace builder {
class Builder : public QObject
{
    Q_OBJECT
public:
    explicit Builder(repo::OSTreeRepo &ostree,
                     cli::Printer &p,
                     ocppi::cli::CLI &cli,
                     service::AppManager &appManager);

    utils::error::Result<void> config(const QString &userName, const QString &password);

    utils::error::Result<void> create(const QString &projectName);

    utils::error::Result<void> build();

    utils::error::Result<void> exportLayer(const QString &destination);

    utils::error::Result<void> extractLayer(const QString &layerPath, const QString &destination);

    utils::error::Result<void> exportBundle(const QString &outputFilepath, bool useLocalDir);

    utils::error::Result<void> push(const QString &repoUrl,
                                    const QString &repoName,
                                    const QString &channel,
                                    bool pushWithDevel);

    utils::error::Result<void> import();

    utils::error::Result<void> importLayer(const QString &path);

    utils::error::Result<void> run();

    utils::error::Result<void> track();

    utils::error::Result<void> appimageConvert(const QStringList &templateArgs);

private:
    repo::OSTreeRepo &repo;
    ocppi::cli::CLI &ociCLI;
    service::AppManager &appManager;

    utils::error::Result<api::types::v1::BuilderProject> buildStageProjectInit();
    utils::error::Result<ocppi::runtime::config::types::Config> buildStageConfigInit();
    utils::error::Result<void> buildStageClean(const api::types::v1::BuilderProject &project);
    utils::error::Result<QString> buildStageDepend(const api::types::v1::BuilderProject &project);
    utils::error::Result<void> buildStageRuntime(ocppi::runtime::config::types::Config &r,
                                                 const api::types::v1::BuilderProject &project,
                                                 const QString &overlayLowerDir,
                                                 const QString &overlayUpperDir,
                                                 const QString &overlayWorkDir,
                                                 const QString &overlayMergeDir);
    utils::error::Result<void> buildStageSource(ocppi::runtime::config::types::Config &r,
                                                const api::types::v1::BuilderProject &project);
    utils::error::Result<void> buildStageEnvrionment(ocppi::runtime::config::types::Config &r,
                                                     const api::types::v1::BuilderProject &project,
                                                     const api::types::v1::BuilderConfig &config);
    utils::error::Result<void> buildStageIDMapping(ocppi::runtime::config::types::Config &r);
    utils::error::Result<void> buildStageRootfs(ocppi::runtime::config::types::Config &r,
                                                const QDir &workdir,
                                                const QString &hostBasePath);
    utils::error::Result<void> buildStageRunContainer(QDir workdir,
                                                      ocppi::cli::CLI &cli,
                                                      ocppi::runtime::config::types::Config &r);
    utils::error::Result<void>
    buildStageCommitBuildOutput(const api::types::v1::BuilderProject &project,
                                const QString &upperdir,
                                const QString &workdir);
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_BUILDER_LINGLONG_BUILDER_H_
