/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_client.h"

#include "linglong/util/error.h"
#include "linglong/util/file.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"

#include <QEventLoop>
#include <QJsonObject>

#include <tuple>

namespace linglong {
namespace repo {

using namespace api::client;

utils::error::Result<std::vector<api::types::v1::PackageInfo>>
RepoClient::QueryApps(const package::FuzzReference &ref, const QString &remoteRepoName)
{
    LINGLONG_TRACE("query apps");

    Request_FuzzySearchReq req;
    if (ref.channel) {
        req.setChannel(*ref.channel);
    }
    req.setAppId(ref.id);
    if (ref.version) {
        req.setVersion(ref.version->toString());
    }
    if (ref.arch) {
        req.setArch(ref.arch->toString());
    }
    req.setRepoName(remoteRepoName);

    utils::error::Result<std::vector<api::types::v1::PackageInfo>> ret =
      LINGLONG_ERR("unknown error");

    QEventLoop loop;
    QEventLoop::connect(
      &client,
      &ClientApi::fuzzySearchAppSignal,
      &loop,
      [&](FuzzySearchApp_200_response resp) {
          loop.exit();
          if (resp.getCode() != 200) {
              ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
              return;
          }

          ret = utils::serialize::fromQJsonValue<std::vector<api::types::v1::PackageInfo>>(
            api::client::toJsonValue(resp.getData()));
          return;
      },
      // 当 RepoClient 不在主线程时，要使用 BlockingQueuedConnection，避免QEventLoop::exec不工作
      // TODO(wurongjie) 将RepoClient和OStreeRepo改到主线程工作
      loop.thread() == client.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

    QEventLoop::connect(
      &client,
      &ClientApi::fuzzySearchAppSignalEFull,
      &loop,
      [&](auto, auto error_type, const QString &error_str) {
          loop.exit();
          ret = LINGLONG_ERR(error_str, error_type);
      },
      loop.thread() == client.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

    client.fuzzySearchApp(req);
    loop.exec();
    return ret;
}

RepoClient::RepoClient(api::client::ClientApi &client, QObject *parent)
    : QObject(parent)
    , client(client)
{
}

void RepoClient::setEndpoint(const QString &endpoint)
{
    // FIXME: We should remove old server.
    this->client.setNewServerForAllOperations(endpoint);
}

} // namespace repo
} // namespace linglong
