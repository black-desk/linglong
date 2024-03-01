/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_

#include "linglong/package/reference.h"
#include "linglong/repo/repo.h"
#include "linglong/repo/repo_client.h"
#include "linglong/util/erofs.h"
#include "linglong/util/file.h"
#include "linglong/utils/error/error.h"

#include <ostree.h>

#include <QHttpPart>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QScopedPointer>
#include <QThread>

namespace linglong {
namespace repo {
// ostree 仓库对象信息

struct OstreeRepoObject
{
    QString objectName;
    QString rev;
    QString path;
};

class OSTreeRepo : public QObject, public Repo
{
    Q_OBJECT
public:
    enum Tree : quint32 {
        DIR,
        TAR,
        REF,
    };

    explicit OSTreeRepo(const QString &localRepoPath,
                        const api::types::v1::RepoConfig &cfg,
                        api::client::ClientApi &client);

    ~OSTreeRepo() override;

    api::types::v1::RepoConfig getConfig() const noexcept override;
    utils::error::Result<void> setConfig(const api::types::v1::RepoConfig &cfg) noexcept override;

    utils::error::Result<QList<package::Reference>> listLocalRefs() noexcept override;

    utils::error::Result<void> importRef(const package::Reference &oldRef,
                                         const package::Reference &newRef);

    utils::error::Result<void> importDirectory(const package::Reference &ref,
                                               const QString &path) override;

    utils::error::Result<void> commit(const Tree treeType,
                                      const package::Reference &ref,
                                      const QString &path,
                                      const package::Reference &oldRef);

    utils::error::Result<void> push(const package::Reference &reference) override;

    utils::error::Result<void> pull(package::Reference &ref, bool force) override;

    utils::error::Result<void> pullAll(const package::Reference &ref, bool force) override;

    utils::error::Result<void> checkout(const package::Reference &ref,
                                        const QString &subPath,
                                        const QString &target) override;

    utils::error::Result<void> checkoutAll(const package::Reference &ref,
                                           const QString &subPath,
                                           const QString &target) override;

    utils::error::Result<QString> compressOstreeData(const package::Reference &ref);

    QString rootOfLayer(const package::Reference &ref) override;

    utils::error::Result<QString> remoteShowUrl(const QString &repoName) override;

    utils::error::Result<package::Reference>
    localLatestRef(const package::FuzzReference &ref) override;

    utils::error::Result<package::Reference>
    remoteLatestRef(const package::FuzzReference &fuzzRef) override;

    utils::error::Result<package::Reference> latestOfRef(const QString &appId,
                                                         const QString &appVersion) override;

    /*
     * 查询ostree远端仓库列表
     *
     * @param repoPath: 远端仓库对应的本地仓库路径
     * @param vec: 远端仓库列表
     * @param err: 错误信息
     *
     * @return bool: true:查询成功 false:失败
     */
    utils::error::Result<void> getRemoteRepoList(QVector<QString> &vec) override;

    /*
     * 通过ostree命令将软件包数据从远端仓库pull到本地
     *
     * @param destPath: 仓库路径
     * @param remoteName: 远端仓库名称
     * @param ref: 软件包对应的仓库索引ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    utils::error::Result<void> repoPullbyCmd(const QString &destPath,
                                             const QString &remoteName,
                                             const QString &ref) override;

    /*
     * 删除本地repo仓库中软件包对应的ref分支信息及数据
     *
     * @param repoPath: 仓库路径
     * @param remoteName: 远端仓库名称
     * @param ref: 软件包对应的仓库索引ref
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    utils::error::Result<void> repoDeleteDatabyRef(const QString &repoPath,
                                                   const QString &ref) override;

    utils::error::Result<void> initCreateRepoIfNotExists();

    /*
     * 获取下载任务对应的进程Id
     *
     * @param ref: ostree软件包对应的ref
     *
     * @return int: ostree命令任务对应的进程id
     */
    int getOstreeJobId(const QString &ref)
    {
        if (jobMap.contains(ref)) {
            return jobMap[ref];
        } else {
            for (auto item : jobMap.keys()) {
                if (item.indexOf(ref) > -1) {
                    return jobMap[item];
                }
            }
        }
        return -1;
    }

    /*
     * 获取正在下载的任务列表
     *
     * @return QStringList: 正在下载的应用对应的ref列表
     */
    QStringList getOstreeJobList()
    {
        if (jobMap.isEmpty()) {
            return {};
        }
        return jobMap.keys();
    }

private:
    /*
     * 在/tmp目录下创建一个临时repo子仓库
     *
     * @param parentRepo: 父repo仓库路径
     *
     * @return QString: 临时repo路径
     */
    static utils::error::Result<QString> createTmpRepo(const QString &parentRepo);

    utils::error::Result<QString> resolveRev(const QString &ref)
    {
        LINGLONG_TRACE(QString("resolve refspec %1").arg(ref));

        g_autoptr(GError) gErr = nullptr;
        g_autofree char *commitID = nullptr;

        if (ostree_repo_resolve_rev(repoPtr.get(), ref.toUtf8(), 0, &commitID, &gErr) == 0) {
            return LINGLONG_ERR("ostree_repo_resolve_rev", gErr);
        }

        return QString::fromUtf8(commitID);
    }

    utils::error::Result<QSharedPointer<api::client::GetRepo_200_response>>
    getRepoInfo(const QString &repoName)
    {
        LINGLONG_TRACE("get repo");

        linglong::utils::error::Result<QSharedPointer<api::client::GetRepo_200_response>> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::getRepoSignal,
          &loop,
          [&](const api::client::GetRepo_200_response &resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }

              ret = QSharedPointer<api::client::GetRepo_200_response>::create(resp);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::getRepoSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        apiClient.getRepo(repoName);
        loop.exec();

        return ret;
    }

    utils::error::Result<QString> getToken()
    {
        LINGLONG_TRACE("get token");

        linglong::utils::error::Result<QString> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::signInSignal,
          &loop,
          [&](api::client::SignIn_200_response resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
              ret = resp.getData().getToken();
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::signInSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        // get username and password from environment
        auto env = QProcessEnvironment::systemEnvironment();
        api::client::Request_Auth auth;
        auth.setUsername(env.value("LINGLONG_USERNAME"));
        auth.setPassword(env.value("LINGLONG_PASSWORD"));
        qDebug() << auth.asJson();
        apiClient.signIn(auth);
        loop.exec();
        return ret;
    }

    utils::error::Result<QString> newUploadTask(const api::client::Schema_NewUploadTaskReq &req)
    {
        LINGLONG_TRACE("new upload task");

        linglong::utils::error::Result<QString> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::newUploadTaskIDSignal,
          &loop,
          [&](api::client::NewUploadTaskID_200_response resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
              ret = resp.getData().getId();
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::newUploadTaskIDSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);
        apiClient.newUploadTaskID(remoteToken, req);
        loop.exec();
        return ret;
    }

    utils::error::Result<void> doUploadTask(const QString &taskID, const QString &filePath)
    {
        LINGLONG_TRACE("do upload task");

        linglong::utils::error::Result<void> ret;

        QEventLoop loop;
        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::uploadTaskFileSignal,
          &loop,
          [&](const api::client::Api_UploadTaskFileResp &resp) {
              loop.exit();
              const qint32 HTTP_OK = 200;
              if (resp.getCode() != HTTP_OK) {
                  ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                  return;
              }
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        QEventLoop::connect(
          &apiClient,
          &api::client::ClientApi::uploadTaskFileSignalEFull,
          &loop,
          [&](auto, auto error_type, const QString &error_str) {
              loop.exit();
              ret = LINGLONG_ERR(error_str, error_type);
          },
          loop.thread() == apiClient.thread() ? Qt::AutoConnection : Qt::BlockingQueuedConnection);

        api::client::HttpFileElement file;
        file.setFileName(filePath);
        file.setRequestFileName(filePath);
        apiClient.uploadTaskFile(remoteToken, taskID, file);
        loop.exec();
        return ret;
    }

    static void cleanUploadTask(const package::Reference &ref, const QString &filePath)
    {
        const auto savePath =
          QStringList{ util::getUserFile(".linglong/builder"), ref.appId }.join(QDir::separator());

        if (!util::removeDir(savePath)) {
            Q_ASSERT(false);
            qCritical() << "Failed to remove" << savePath;
        }

        QFile file(filePath);
        if (!file.remove()) {
            Q_ASSERT(false);
            qCritical() << "Failed to remove" << filePath;
        }
    }

    utils::error::Result<void> getUploadStatus(const QString &taskID)
    {
        LINGLONG_TRACE("get upload status");

        while (true) {
            linglong::utils::error::Result<QString> ret = LINGLONG_OK;
            qDebug() << "OK have value" << ret.has_value();
            QEventLoop loop;
            QEventLoop::connect(
              &apiClient,
              &api::client::ClientApi::uploadTaskInfoSignal,
              &loop,
              [&](const api::client::UploadTaskInfo_200_response &resp) {
                  loop.exit();
                  const qint32 HTTP_OK = 200;
                  if (resp.getCode() != HTTP_OK) {
                      ret = LINGLONG_ERR(resp.getMsg(), resp.getCode());
                      return;
                  }
                  ret = resp.getData().getStatus();
              },
              loop.thread() == apiClient.thread() ? Qt::AutoConnection
                                                  : Qt::BlockingQueuedConnection);

            QEventLoop::connect(
              &apiClient,
              &api::client::ClientApi::uploadTaskInfoSignalEFull,
              &loop,
              [&](auto, auto error_type, const QString &error_str) {
                  loop.exit();
                  ret = LINGLONG_ERR(error_str, error_type);
              },
              loop.thread() == apiClient.thread() ? Qt::AutoConnection
                                                  : Qt::BlockingQueuedConnection);
            apiClient.uploadTaskInfo(remoteToken, taskID);
            loop.exec();
            if (!ret) {
                return LINGLONG_ERR("get upload taks info", ret);
            }
            auto status = *ret;
            if (status == "complete") {
                break;
            } else if (status == "failed") {
                return LINGLONG_ERR("task status failed", -1);
            }

            qInfo().noquote() << status;
            QThread::sleep(1);
        }

        return LINGLONG_OK;
    }

    static char *_formatted_time_remaining_from_seconds(guint64 seconds_remaining);

    static void progress_changed(OstreeAsyncProgress *progress, gpointer user_data);

Q_SIGNALS:
    void progressChanged(const uint &progress, const QString &speed);

private:
    api::types::v1::RepoConfig cfg;
    QString repoRootPath;
    QString remoteEndpoint;
    QString remoteRepoName;

    QString remoteToken;

    struct OstreeRepoDeleter
    {
        void operator()(OstreeRepo *repo)
        {
            qDebug() << "delete OstreeRepo" << repo;
            g_clear_object(&repo);
        }
    };

    std::unique_ptr<OstreeRepo, OstreeRepoDeleter> repoPtr = nullptr;
    QString ostreePath;

    repo::RepoClient repoClient;
    api::client::ClientApi &apiClient;
    // ostree 仓库对象信息

    QMap<QString, int> jobMap;
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_REPO_OSTREE_REPO_H_
