/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_H_

#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/package/fuzz_reference.h"
#include "linglong/package/reference.h"
#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong {

namespace package {
class Bundle;
} // namespace package

namespace repo {

class Repo
{
public:
    Repo() = default;
    Repo(const Repo &) = delete;
    Repo(Repo &&) = delete;
    Repo &operator=(const Repo &) = delete;
    Repo &operator=(Repo &&) = delete;
    virtual ~Repo() = default;

    virtual utils::error::Result<QList<package::Reference>> listLocalRefs() = 0;

    virtual utils::error::Result<void> importDirectory(const package::Reference &ref,
                                                       const QString &path) = 0;

    virtual utils::error::Result<void> push(const package::Reference &reference) = 0;

    virtual utils::error::Result<void> pull(package::Reference &ref, bool force) = 0;
    virtual utils::error::Result<void> pullAll(const package::Reference &ref, bool force) = 0;
    virtual utils::error::Result<void> checkout(const package::Reference &ref,
                                                const QString &subPath,
                                                const QString &target) = 0;

    virtual utils::error::Result<void> checkoutAll(const package::Reference &ref,
                                                   const QString &subPath,
                                                   const QString &target) = 0;

    virtual QString rootOfLayer(const package::Reference &ref) = 0;

    virtual utils::error::Result<QString> remoteShowUrl(const QString &repoName) = 0;

    virtual utils::error::Result<package::Reference>
    localLatestRef(const package::FuzzReference &ref) = 0;

    virtual utils::error::Result<package::Reference>
    remoteLatestRef(const package::FuzzReference &fuzzRef) = 0;

    virtual utils::error::Result<package::Reference> latestOfRef(const QString &appId,
                                                                 const QString &appVersion) = 0;

    virtual utils::error::Result<void> getRemoteRepoList(QVector<QString> &vec) = 0;
    virtual utils::error::Result<void> repoPullbyCmd(const QString &destPath,
                                                     const QString &remoteName,
                                                     const QString &ref) = 0;
    virtual utils::error::Result<void> repoDeleteDatabyRef(const QString &repoPath,
                                                           const QString &ref) = 0;

    virtual api::types::v1::RepoConfig getConfig() const noexcept = 0;
    virtual utils::error::Result<void>
    setConfig(const api::types::v1::RepoConfig &cfg) noexcept = 0;
};

} // namespace repo
} // namespace linglong

#endif /* LINGLONG_SRC_MODULE_REPO_REPO_H_ */
