/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_

#include "ClientApi.h"
#include "linglong/api/types/v1/PackageInfo.hpp"
#include "linglong/package/fuzz_reference.h"
#include "linglong/repo/repo.h"
#include "linglong/utils/error/error.h"

#include <QNetworkReply>
#include <QNetworkRequest>

#include <tuple>

namespace linglong {
namespace repo {

class RepoClient : public QObject
{
public:
    explicit RepoClient(api::client::ClientApi &api, QObject *parent = nullptr);

    // FIXME(black_desk):
    // This method is just a workaround used to
    // update endpoint when endpoint get updated
    // by PackageManager::RepoModify.
    // It's not thread-safe.
    void setEndpoint(const QString &endpoint);

    utils::error::Result<std::vector<api::types::v1::PackageInfo>>
    QueryApps(const package::FuzzReference &ref, const QString &remoteRepoName);

private:
    api::client::ClientApi &client;
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
