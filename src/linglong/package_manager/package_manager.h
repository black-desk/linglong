/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_H_

#include "linglong/api/dbus/v1/package_manager_helper.h"
#include "linglong/repo/repo.h"
#include "linglong/repo/repo_client.h"

#include <QDBusArgument>
#include <QDBusContext>
#include <QFuture>
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <QThreadPool>

namespace linglong::service {
/**
 * @brief The PackageManager class
 * @details PackageManager is a singleton class, and it is used to manage the package
 *          information.
 */
class PackageManager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.PackageManager")

public:
    PackageManager(api::dbus::v1::PackageManagerHelper &helper,
                   linglong::repo::Repo &,
                   linglong::repo::RepoClient &client,
                   QObject *parent);

    ~PackageManager() override = default;
    PackageManager(const PackageManager &) = delete;
    PackageManager(PackageManager &&) = delete;
    auto operator=(const PackageManager &) -> PackageManager & = delete;
    auto operator=(PackageManager &&) -> PackageManager & = delete;
    virtual auto InstallSync(const QVariantMap &parameters) -> QVariantMap;

public Q_SLOTS:
    auto getRepoInfo() -> QVariantMap;

    virtual auto ModifyRepo(const QVariantMap &parameters) -> QVariantMap;

    virtual auto Install(const QVariantMap &parameters) -> QVariantMap;

    virtual auto InstallLayer(const QVariantMap &parameters) -> QVariantMap;

    virtual auto InstallLayerFD(const QDBusUnixFileDescriptor &fileDescriptor) -> QVariantMap;

    virtual auto Uninstall(const QVariantMap &parameters) -> QVariantMap;

    virtual auto Update(const QVariantMap &parameters) -> QVariantMap;

    virtual auto Query(const QVariantMap &parameters) -> QVariantMap;

public:
    // FIXME: ??? why this public?
    QScopedPointer<QThreadPool> pool; ///< 下载、卸载、更新应用线程池

    /**
     * @brief 设置是否为nodbus安装模式
     *
     * @param enable true:nodbus false:dbus
     */
    virtual void setNoDBusMode(bool enable);

private:
    /*
     * 从给定的软件包列表中查找最新版本的runtime
     *
     * @param appId: 待匹配runtime的appId
     * @param appList: 待搜索的软件包列表信息
     *
     * @return AppMetaInfo: 最新版本的runtime
     *
     */
    auto getLatestRuntime(const QString &appId,
                          const QString &version,
                          const QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList)
      -> QSharedPointer<linglong::package::AppMetaInfo>;
    /*
     * 从给定的软件包列表中查找最新版本的软件包
     *
     * @param appId: 待匹配应用的appId
     * @param appList: 待搜索的软件包列表信息
     *
     * @return AppMetaInfo: 最新版本的软件包
     *
     */
    auto getLatestApp(const QString &appId,
                      const QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList)
      -> QSharedPointer<linglong::package::AppMetaInfo>;

    /*
     * 从json字符串中提取软件包对应的JsonArray数据
     *
     * @param jsonString: 软件包对应的json字符串
     * @param jsonValue: 转换结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    auto getAppJsonArray(const QString &jsonString, QJsonValue &jsonValue, QString &err) -> bool;

    /*
     * 将json字符串转化为软件包数据
     *
     * @param jsonString: 软件包对应的json字符串
     * @param appList: 转换结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    auto loadAppInfo(const QString &jsonString,
                     QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList,
                     QString &err) -> bool;

    /*
     * 从服务器查询指定包名/版本/架构的软件包数据
     *
     * @param pkgName: 软件包包名
     * @param pkgVer: 软件包版本号
     * @param pkgArch: 软件包对应的架构
     * @param appData: 查询结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    auto getAppInfoFromServer(const QString &pkgName,
                              const QString &pkgVer,
                              const QString &pkgArch,
                              QString &appData,
                              QString &errString) -> bool;
    /*
     * 将在线包数据部分签出到指定目录
     *
     * @param pkgName: 软件包包名
     * @param pkgVer: 软件包版本号
     * @param pkgArch: 软件包对应的架构
     * @param dstPath: 在线包数据部分存储路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    auto downloadAppData(const QString &pkgName,
                         const QString &pkgVer,
                         const QString &pkgArch,
                         const QString &channel,
                         const QString &module,
                         const QString &dstPath,
                         QString &err) -> bool;

    /*
     * 安装应用runtime
     *
     * @param appInfo: runtime对象
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    auto installRuntime(QSharedPointer<linglong::package::AppMetaInfo> appInfo, QString &err)
      -> bool;

    /*
     * 检查应用runtime安装状态
     *
     * @param runtime: 应用runtime字符串
     * @param channel: 软件包对应的渠道
     * @param module: 软件包类型
     * @param err: 错误信息
     *
     * @return bool: true:安装成功或已安装返回true false:安装失败
     */
    auto checkAppRuntime(const QString &runtime,
                         const QString &channel,
                         const QString &module,
                         QString &err) -> bool;

    /*
     * 针对非deepin发行版检查应用base安装状态
     *
     * @param runtime: runtime ref
     * @param channel: 软件包对应的渠道
     * @param module: 软件包类型
     * @param err: 错误信息
     *
     * @return bool: true:安装成功或已安装返回true false:安装失败
     */
    auto checkAppBase(const QString &runtime,
                      const QString &channel,
                      const QString &module,
                      QString &err) -> bool;

    /*
     * 安装应用时更新包括desktop文件在内的配置文件
     *
     * @param appId: 应用的appId
     * @param version: 应用的版本号
     * @param arch: 应用对应的架构
     */
    void addAppConfig(const QString &appId, const QString &version, const QString &arch);

    /*
     * 卸载应用时更新包括desktop文件在内的配置文件
     *
     * @param appId: 应用的appId
     * @param version: 应用的版本号
     * @param arch: 应用对应的架构
     */
    void delAppConfig(const QString &appId, const QString &version, const QString &arch);

    /*
     * 通过用户uid获取对应的用户名
     *
     * @param uid: 用户uid
     *
     * @return QString: uid对应的用户名
     */
    auto getUserName(uid_t uid) -> QString;

private:
    QString sysLinglongInstallation;
    QString kAppInstallPath;
    QString kLocalRepoPath;
    QString remoteRepoName = "repo";

    repo::RepoClient &repoClient;
    linglong::repo::Repo &repoMan;
    // 记录子线程安装及更新状态 供查询进度信息使用
    QMap<QString, Reply> appState;

    bool noDBusMode = false;

    api::dbus::v1::PackageManagerHelper &packageManagerHelper;
};

} // namespace linglong::service

#define POOL_MAX_THREAD 10 ///< 下载、卸载、更新应用线程池最大线程数
#endif
