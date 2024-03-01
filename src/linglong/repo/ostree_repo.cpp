/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ostree_repo.h"

#include "linglong/api/types/helper.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/fuzz_reference.h"
#include "linglong/package/reference.h"
#include "linglong/repo/config.h"
#include "linglong/repo/repo.h"
#include "linglong/util/error.h"
#include "linglong/util/runner.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/version/semver.h"
#include "linglong/util/version/version.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"

#include <gio/gio.h>
#include <glib.h>
#include <ostree-repo.h>

#include <QDir>
#include <QProcess>
#include <QtWebSockets/QWebSocket>

#include <cstddef>
#include <utility>

#include <fcntl.h>

namespace linglong {
namespace repo {

OSTreeRepo::OSTreeRepo(const QString &localRepoPath,
                       const api::types::v1::RepoConfig &cfg,
                       api::client::ClientApi &client)
    : cfg(cfg)
    , repoRootPath(QDir(localRepoPath).absolutePath())
    , remoteEndpoint(QString::fromStdString(cfg.repos.at(cfg.defaultRepo)))
    , remoteRepoName(QString::fromStdString(cfg.defaultRepo))
    , repoClient(client)
    , apiClient(client)
{
    ostreePath = repoRootPath + "/repo";
    qDebug() << "ostree repo path is" << ostreePath;

    // FIXME(black_desk): Just a quick hack to make sure openRepo called after the repo is
    // created. This function might failed as the caller might not have right permission to
    // create that directory or modify files in this directory. This should be fixed by split
    // Repo into read-only Repo and modifiable Repo, and void using modifiable Repo in
    // ll-service.
    auto ret = initCreateRepoIfNotExists();
    if (ret) {
        return;
    }

    qDebug().noquote() << "Failed to create ostree based linglong repo." << Qt::endl
                       << ret.error().message();

    g_autoptr(GError) gErr = nullptr;

    g_autoptr(GFile) repoPath = g_file_new_for_path(ostreePath.toLocal8Bit());
    g_autoptr(OstreeRepo) repo = ostree_repo_new(repoPath);
    ostree_repo_open(repo, nullptr, &gErr);
    if (gErr != nullptr) {
        qCritical().noquote() << "open repo" << ostreePath << "failed"
                              << QString::fromStdString(std::string(gErr->message));
    }

    Q_ASSERT(repo);
    repoPtr.reset(g_steal_pointer(&repo));
}

api::types::v1::RepoConfig OSTreeRepo::getConfig() const noexcept
{
    return cfg;
}

utils::error::Result<void> OSTreeRepo::setConfig(const api::types::v1::RepoConfig &cfg) noexcept
{
    LINGLONG_TRACE("set config");

    if (cfg == this->cfg) {
        return LINGLONG_OK;
    }

    auto res = saveConfig(cfg, this->repoRootPath + "/config.yaml");
    if (!res) {
        return LINGLONG_ERR(res);
    }

    g_autoptr(GKeyFile) cfgGKeyFile = ostree_repo_copy_config(this->repoPtr.get());
    g_key_file_set_string(cfgGKeyFile, "core", "min-free-space-size", "600MB");
    // NOTE:
    // libcurl 8.2.1 has a http2 bug https://github.com/curl/curl/issues/11859
    // We disable http2 for now.
    g_key_file_set_string(
      cfgGKeyFile,
      QString("remote \"%1\"").arg(QString::fromStdString(cfg.defaultRepo)).toLocal8Bit(),
      "http2",
      "false");
    g_key_file_set_string(
      cfgGKeyFile,
      QString("remote \"%1\"").arg(QString::fromStdString(cfg.defaultRepo)).toLocal8Bit(),
      "gpg-verify",
      "false");
    g_key_file_set_string(
      cfgGKeyFile,
      QString("remote \"%1\"").arg(QString::fromStdString(cfg.defaultRepo)).toLocal8Bit(),
      "url",
      cfg.repos.at(cfg.defaultRepo).c_str());

    g_autoptr(GError) gErr = nullptr;
    ostree_repo_write_config(this->repoPtr.get(), cfgGKeyFile, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_write_config", gErr);
    }

    this->repoClient.setEndpoint(QString::fromStdString(cfg.repos.at(cfg.defaultRepo)));
    this->apiClient.setNewServerForAllOperations(
      QString::fromStdString(cfg.repos.at(cfg.defaultRepo)));

    this->cfg = cfg;
    this->remoteRepoName = QString::fromStdString(cfg.defaultRepo);
    this->remoteEndpoint = QString::fromStdString(cfg.repos.at(this->remoteRepoName.toStdString()));

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::importDirectory(const package::Reference &ref, const QString &path)
{
    LINGLONG_TRACE("import directory");

    auto ret = commit(DIR, ref, path, package::Ref(""));
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::importRef(const package::Ref &oldRef,
                                                 const package::Ref &newRef)
{
    LINGLONG_TRACE("import ref");

    auto ret = commit(REF, newRef, "", oldRef);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::commit(Tree treeType,
                                              const package::Ref &ref,
                                              const QString &path,
                                              const package::Ref &oldRef)
{
    LINGLONG_TRACE("commit");

    g_autoptr(GError) gErr = nullptr;
    ostree_repo_prepare_transaction(repoPtr.get(), NULL, NULL, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_prepare_transaction", gErr);
    }

    g_autoptr(OstreeMutableTree) mtree = ostree_mutable_tree_new();

    g_autoptr(OstreeRepoCommitModifier) modifier = nullptr;
    modifier =
      ostree_repo_commit_modifier_new(OSTREE_REPO_COMMIT_MODIFIER_FLAGS_CANONICAL_PERMISSIONS,
                                      nullptr,
                                      nullptr,
                                      nullptr);
    g_autoptr(GFile) gfile = nullptr;
    switch (treeType) {
    case DIR:
        gfile = g_file_new_for_path(path.toStdString().c_str());
        break;
    case REF:
        ostree_repo_read_commit(repoPtr.get(),
                                oldRef.toOSTreeRefLocalString().toStdString().c_str(),
                                &gfile,
                                NULL,
                                nullptr,
                                &gErr);
        if (gErr != nullptr) {
            return LINGLONG_ERR("ostree_repo_read_commit", gErr);
        }
        break;
    case TAR:
        qWarning() << "not impelement now.";
        break;
    }
    ostree_repo_write_directory_to_mtree(repoPtr.get(), gfile, mtree, modifier, nullptr, &gErr);
    if (gErr == nullptr) {
        return LINGLONG_ERR("ostree_repo_write_directory_to_mtree", gErr);
    }

    g_autoptr(OstreeRepoFile) repo_file = nullptr;
    ostree_repo_write_mtree(repoPtr.get(), mtree, (GFile **)&repo_file, nullptr, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_write_mtree", gErr);
    }

    g_autofree char *out_commit = nullptr;
    ostree_repo_write_commit(repoPtr.get(),
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr,
                             repo_file,
                             &out_commit,
                             NULL,
                             &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_write_commit", gErr);
    }

    ostree_repo_transaction_set_ref(repoPtr.get(),
                                    NULL,
                                    ref.toOSTreeRefLocalString().toStdString().c_str(),
                                    out_commit);

    ostree_repo_commit_transaction(repoPtr.get(), NULL, NULL, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_commit_transaction", gErr);
    }
    return LINGLONG_OK;
}

utils::error::Result<void> OSTreeRepo::push(const package::Reference &ref)
{
    LINGLONG_TRACE("push");

    auto ret = getToken();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    auto repoInfo = getRepoInfo(remoteRepoName);
    if (!repoInfo) {
        return LINGLONG_ERR(repoInfo);
    }

    QString taskID;
    {
        api::client::Schema_NewUploadTaskReq uploadReq;
        uploadReq.setRef(ref.toString());
        uploadReq.setRepoName(remoteRepoName);

        auto ret = newUploadTask(uploadReq);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        taskID = *ret;
    }

    // compress form data
    QString filePath;
    {
        auto ret = compressOstreeData(ref);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        filePath = *ret;
    }

    {
        auto ret = doUploadTask(taskID, filePath);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    {
        auto ret = getUploadStatus(taskID);
        if (!ret) {
            cleanUploadTask(ref, filePath);
            return ret;
        }
    }

    cleanUploadTask(ref, filePath);
    return LINGLONG_OK;
}

char *OSTreeRepo::_formatted_time_remaining_from_seconds(guint64 seconds_remaining)
{
    guint64 minutes_remaining = seconds_remaining / 60;
    guint64 hours_remaining = minutes_remaining / 60;
    guint64 days_remaining = hours_remaining / 24;

    GString *description = g_string_new(NULL);

    if (days_remaining != 0U) {
        g_string_append_printf(description, "%" G_GUINT64_FORMAT " days ", days_remaining);
    }

    if (hours_remaining != 0U) {
        g_string_append_printf(description, "%" G_GUINT64_FORMAT " hours ", hours_remaining % 24);
    }

    if (minutes_remaining != 0U) {
        g_string_append_printf(description,
                               "%" G_GUINT64_FORMAT " minutes ",
                               minutes_remaining % 60);
    }

    g_string_append_printf(description, "%" G_GUINT64_FORMAT " seconds ", seconds_remaining % 60);

    return g_string_free(description, FALSE);
}

void OSTreeRepo::progress_changed(OstreeAsyncProgress *progress, gpointer user_data)
{
    g_autofree char *status = NULL;
    gboolean caught_error = 0;
    gboolean scanning = 0;
    guint outstanding_fetches = 0;
    guint outstanding_metadata_fetches = 0;
    guint outstanding_writes = 0;
    guint n_scanned_metadata = 0;
    guint fetched_delta_parts = 0;
    guint total_delta_parts = 0;
    guint fetched_delta_part_fallbacks = 0;
    guint total_delta_part_fallbacks = 0;
    guint new_progress = 0;
    char const *formatted_bytes_sec = "0KB";

    g_autoptr(GString) buf = g_string_new("");

    ostree_async_progress_get(progress,
                              "outstanding-fetches",
                              "u",
                              &outstanding_fetches,
                              "outstanding-metadata-fetches",
                              "u",
                              &outstanding_metadata_fetches,
                              "outstanding-writes",
                              "u",
                              &outstanding_writes,
                              "caught-error",
                              "b",
                              &caught_error,
                              "scanning",
                              "u",
                              &scanning,
                              "scanned-metadata",
                              "u",
                              &n_scanned_metadata,
                              "fetched-delta-parts",
                              "u",
                              &fetched_delta_parts,
                              "total-delta-parts",
                              "u",
                              &total_delta_parts,
                              "fetched-delta-fallbacks",
                              "u",
                              &fetched_delta_part_fallbacks,
                              "total-delta-fallbacks",
                              "u",
                              &total_delta_part_fallbacks,
                              "status",
                              "s",
                              &status,
                              NULL);

    if (*status != '\0') {
        new_progress = 100;
        g_string_append(buf, status);
    } else if (caught_error != 0) {
        g_string_append_printf(buf, "Caught error, waiting for outstanding tasks");
    } else if (outstanding_fetches != 0U) {
        guint64 bytes_transferred = 0;
        guint64 start_time = 0;
        guint64 total_delta_part_size = 0;
        guint fetched = 0;
        guint metadata_fetched = 0;
        guint requested = 0;
        guint64 current_time = g_get_monotonic_time();
        g_autofree char *formatted_bytes_transferred = NULL;
        guint64 bytes_sec = 0;

        /* Note: This is not atomic wrt the above getter call. */
        ostree_async_progress_get(progress,
                                  "bytes-transferred",
                                  "t",
                                  &bytes_transferred,
                                  "fetched",
                                  "u",
                                  &fetched,
                                  "metadata-fetched",
                                  "u",
                                  &metadata_fetched,
                                  "requested",
                                  "u",
                                  &requested,
                                  "start-time",
                                  "t",
                                  &start_time,
                                  "total-delta-part-size",
                                  "t",
                                  &total_delta_part_size,
                                  NULL);

        formatted_bytes_transferred = g_format_size_full(bytes_transferred, (GFormatSizeFlags)0);
        /* Ignore the first second, or when we haven't transferred any
         * data, since those could cause divide by zero below.
         */
        if ((current_time - start_time) < G_USEC_PER_SEC || bytes_transferred == 0) {
            bytes_sec = 0;
            formatted_bytes_sec = g_strdup("-");
        } else {
            bytes_sec = bytes_transferred / ((current_time - start_time) / G_USEC_PER_SEC);
            formatted_bytes_sec = g_format_size(bytes_sec);
        }

        /* Are we doing deltas?  If so, we can be more accurate */
        if (total_delta_parts > 0) {
            guint64 fetched_delta_part_size =
              ostree_async_progress_get_uint64(progress, "fetched-delta-part-size");
            g_autofree char *formatted_fetched = NULL;
            g_autofree char *formatted_total = NULL;

            /* Here we merge together deltaparts + fallbacks to avoid bloating the text UI */
            fetched_delta_parts += fetched_delta_part_fallbacks;
            total_delta_parts += total_delta_part_fallbacks;

            formatted_fetched = g_format_size(fetched_delta_part_size);
            formatted_total = g_format_size(total_delta_part_size);

            if (bytes_sec > 0) {
                guint64 est_time_remaining = 0;
                if (total_delta_part_size > fetched_delta_part_size) {
                    est_time_remaining =
                      (total_delta_part_size - fetched_delta_part_size) / bytes_sec;
                }
                g_autofree char *formatted_est_time_remaining =
                  _formatted_time_remaining_from_seconds(est_time_remaining);
                /* No space between %s and remaining, since formatted_est_time_remaining has a
                 * trailing space */
                g_string_append_printf(buf,
                                       "Receiving delta parts: %u/%u %s/%s %s/s %sremaining",
                                       fetched_delta_parts,
                                       total_delta_parts,
                                       formatted_fetched,
                                       formatted_total,
                                       formatted_bytes_sec,
                                       formatted_est_time_remaining);
            } else {
                g_string_append_printf(buf,
                                       "Receiving delta parts: %u/%u %s/%s",
                                       fetched_delta_parts,
                                       total_delta_parts,
                                       formatted_fetched,
                                       formatted_total);
            }
        } else if ((scanning != 0) || (outstanding_metadata_fetches != 0U)) {
            new_progress += 5;
            g_object_set_data(G_OBJECT(progress), "last-was-metadata", GUINT_TO_POINTER(TRUE));
            g_string_append_printf(buf,
                                   "Receiving metadata objects: %u/(estimating) %s/s %s",
                                   metadata_fetched,
                                   formatted_bytes_sec,
                                   formatted_bytes_transferred);
        } else {
            g_string_append_printf(buf,
                                   "Receiving objects: %u%% (%u/%u) %s/s %s",
                                   (guint)((((double)fetched) / requested) * 100),
                                   fetched,
                                   requested,
                                   formatted_bytes_sec,
                                   formatted_bytes_transferred);
            new_progress = fetched * 97 / requested;
        }
    } else if (outstanding_writes != 0U) {
        g_string_append_printf(buf, "Writing objects: %u", outstanding_writes);
    } else {
        g_string_append_printf(buf, "Scanning metadata: %u", n_scanned_metadata);
    }

    auto *repo = static_cast<OSTreeRepo *>(user_data);
    Q_EMIT repo->progressChanged(new_progress, QString("%1").arg(formatted_bytes_sec));
}

linglong::utils::error::Result<void> OSTreeRepo::pull(package::Ref &ref, bool /*force*/)
{
    LINGLONG_TRACE(QString("pull %1 from %2").arg(ref.toLocalString(), remoteRepoName));

    // FIXME(black_desk): should implement force
    g_autoptr(GError) gErr = nullptr;
    auto str = ref.toOSTreeRefLocalString().toLocal8Bit();
    char *refs_to_fetch[2] = { str.data(), nullptr };
    auto *progress = ostree_async_progress_new_and_connect(progress_changed, (void *)this);
    ostree_repo_pull(repoPtr.get(),
                     remoteRepoName.toLocal8Bit(),
                     refs_to_fetch,
                     OSTREE_REPO_PULL_FLAGS_MIRROR,
                     progress,
                     NULL,
                     &gErr);
    ostree_async_progress_finish(progress);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_pull", gErr);
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<QList<package::Reference>> OSTreeRepo::listLocalRefs() noexcept
{
    LINGLONG_TRACE("list local refspecs");

    g_autoptr(GHashTable) table = NULL;
    g_autoptr(GError) gErr = NULL;
    ostree_repo_list_refs(repoPtr.get(), NULL, &table, NULL, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_list_refs", gErr);
    }

    auto *ordered_keys = g_hash_table_get_keys(table);
    QList<package::Ref> result;
    for (auto *iter = ordered_keys; iter != nullptr; iter = iter->next) {
        result.append(package::Ref((const char *)iter->data));
    }
    return result;
}

linglong::utils::error::Result<void> OSTreeRepo::pullAll(const package::Ref &ref, bool /*force*/)
{
    LINGLONG_TRACE(QString("pull all %1").arg(ref.toLocalString()));

    // FIXME(black-desk): pullAll should not belong to this class.
    auto refs = package::Ref(
      QStringList{ ref.channel, ref.appId, ref.version, ref.arch, "runtime" }.join("/"));
    auto ret = pull(refs, false);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    // FIXME: some old package have no devel, ignore error for now.
    refs =
      package::Ref(QStringList{ ref.channel, ref.appId, ref.version, ref.arch, "devel" }.join("/"));
    ret = pull(refs, false);
    if (!ret.has_value()) {
        qWarning() << ret.error();
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> OSTreeRepo::checkout(const package::Reference &ref,
                                                          const QString &subPath,
                                                          const QString &target)
{
    LINGLONG_TRACE(QString("checkout %1 to %2").arg(ref.toOSTreeRefLocalString()).arg(target));

    g_autoptr(GError) gErr = NULL;
    OstreeRepoCheckoutAtOptions checkout_options = {};
    checkout_options.overwrite_mode = OSTREE_REPO_CHECKOUT_OVERWRITE_UNION_FILES;
    checkout_options.force_copy = TRUE;

    auto subPathStr = subPath.toStdString();
    if (!subPath.isEmpty()) {
        checkout_options.subpath = subPathStr.c_str();
    }

    qInfo() << "print ref string for checkout:" << ref.toOSTreeRefLocalString();
    auto rev = resolveRev(ref.toOSTreeRefLocalString());
    if (!rev) {
        return LINGLONG_ERR(rev);
    }

    // FIXME: at least the "layers" directory should be ensured when OSTreeRepo is constructed.
    if (!util::ensureDir(target)) {
        return LINGLONG_ERR(QString("mkdir %1").arg(target), -1);
    }
    ostree_repo_checkout_at(repoPtr.get(),
                            &checkout_options,
                            AT_FDCWD,
                            target.toStdString().c_str(),
                            (*rev).toStdString().c_str(),
                            NULL,
                            &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_checkout_at", gErr);
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> OSTreeRepo::checkoutAll(const package::Ref &ref,
                                                             const QString &subPath,
                                                             const QString &target)
{
    LINGLONG_TRACE("checkout all");

    package::Ref reference = ref;
    reference.module = "runtime";
    auto ret = checkout(reference, subPath, target);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    reference.module = "devel";
    ret = checkout(reference, subPath, target);
    if (!ret.has_value()) {
        qWarning() << "failed to checkout modules for devel:" << ret.error().message();
    }

    return LINGLONG_OK;
}

QString OSTreeRepo::rootOfLayer(const package::Ref &ref)
{
    return QStringList{ repoRootPath, "layers", ref.appId, ref.version, ref.arch }.join(
      QDir::separator());
}

linglong::utils::error::Result<QString> OSTreeRepo::remoteShowUrl(const QString &repoName)
{
    LINGLONG_TRACE("show remote url");

    g_autofree char *out_url = nullptr;
    g_autoptr(GError) gErr = NULL;
    ostree_repo_remote_get_url(repoPtr.get(), repoName.toLocal8Bit(), &out_url, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_remote_get_url", gErr);
    }

    return QString::fromUtf8(out_url);
}

linglong::utils::error::Result<package::Ref> OSTreeRepo::localLatestRef(const package::Ref &ref)
{
    // FIXME: we should NOT call ostree binary
    LINGLONG_TRACE(QString("get local latest %1").arg(ref.toLocalString()));

    QString defaultChannel = "main";
    QString latestVer = "latest";
    QString args = QString("ostree refs --repo=%1 | grep %2 | grep %3 | grep %4")
                     .arg(ostreePath,
                          defaultChannel + "/" + ref.appId,
                          ref.version,
                          ref.arch + "/" + ref.module);
    auto output = QSharedPointer<QByteArray>::create();
    auto err = util::Exec("sh", { "-c", args }, -1, output);

    if (!output || output->isEmpty()) {
        qWarning() << "query local app with channel main failed, fallback to channel linglong";

        defaultChannel = "linglong";
        args = QString("ostree refs --repo=%1 | grep %2 | grep %3 | grep %4")
                 .arg(ostreePath,
                      defaultChannel + "/" + ref.appId,
                      ref.version,
                      ref.arch + "/" + ref.module);

        err = util::Exec("sh", { "-c", args }, -1, output);
        if (!output || output->isEmpty()) {
            return LINGLONG_ERR("not exist.", -1);
        }
    }

    if (!err) {
        auto outputText = QString::fromLocal8Bit(*output);
        auto lines = outputText.split('\n');
        // last line of result is null, remove it
        qDebug() << "ostree refs output: " << lines;
        // lines.removeLast();
        latestVer = linglong::util::latestVersion(lines);
    }

    return package::Ref("", defaultChannel, ref.appId, latestVer, ref.arch, ref.module);
}

utils::error::Result<package::Reference>
OSTreeRepo::remoteLatestRef(const package::FuzzReference &fuzzRef)
{
    LINGLONG_TRACE(QString("get remote latest %1").arg(fuzzRef.toString()));

    if (fuzzRef.version && fuzzRef.version->tweak) {
        // TODO(black_desk): better critical message
        qCritical() << "Fuzz reference used to get remote latest version should not come with "
                       "tweak version defined. This will lock version.";
    }

    auto ret = repoClient.QueryApps(fuzzRef, remoteRepoName);

    if (ret && ret->empty() && fuzzRef.channel && *fuzzRef.channel == "main") {

        qWarning() << fuzzRef.toString()
                   << "not found at channel \"main\", fallback to use channel \"linglong\"";

        auto fuzzRefWithChannelLinglong = fuzzRef;
        fuzzRefWithChannelLinglong.channel = "linglong";

        ret = repoClient.QueryApps(fuzzRefWithChannelLinglong, remoteRepoName);
    }
    if (!ret) {
        return LINGLONG_ERR(ret);
    }
    if (ret->empty()) {
        return LINGLONG_ERR("not found.");
    }

    const auto defaultVersion = package::Version::parse("0.0.0.0");
    Q_ASSERT(defaultVersion.has_value());

    auto latestVersion = package::Version::parse(QString::fromStdString(ret->front().version))
                           .value_or(*defaultVersion);
    size_t latestIndex = 0;
    for (size_t i = 1; i < ret->size(); i++) {
        qDebug() << "Found candidate:" << QString::fromStdString(ret->at(i).id)
                 << QString::fromStdString(ret->at(i).version);

        auto version = package::Version::parse(QString::fromStdString(ret->at(i).version));
        if (!version) {
            qCritical() << "invalid version from server:" << version.error();
            Q_ASSERT(false);
            continue;
        }

        auto architecture =
          package::Architecture::parse(QString::fromStdString(ret->at(i).architecture));
        if (!architecture) {
            qCritical() << "invalid architecture from server:" << architecture.error();
            Q_ASSERT(false);
            continue;
        }

        if (latestVersion < *version) {
            latestVersion = *version;
            latestIndex = i;
        }
    }

    auto info = ret->at(latestIndex);
    auto architecture = package::Architecture::parse(QString::fromStdString(info.architecture));
    Q_ASSERT(architecture.has_value());

    return package::Reference::create(QString::fromStdString(info.channel),
                                      QString::fromStdString(info.id),
                                      latestVersion,
                                      *architecture);
}

// 获取最新版本的ref, 版本号可以留空或传递latest
// 如果传递版本号不为空，则会被当作主版本号，仅查找副版本号最新的ref
utils::error::Result<package::Ref> OSTreeRepo::latestOfRef(const QString &appId,
                                                           const QString &appVersion)
{
    LINGLONG_TRACE(QString("get local latest %1").arg(appId));

    package::Ref ref(appId + "/" + appVersion + "/" + util::hostArch());

    QDir appRoot(repoRootPath + "/layers" + "/" + appId);

    // FIXME: DO NOT use "latest"
    if (appRoot.exists("latest")) {
        ref.version = "latest";
        return ref;
    }

    QString version;
    if (!appVersion.isEmpty() && appVersion != "latest") {
        version = appVersion;
    }

    QStringList verDirs;
    for (const auto &dir : appRoot.entryList(QDir::NoDotAndDotDot | QDir::Dirs)) {
        if (version.isEmpty()) {
            verDirs.push_back(dir);
        } else if (dir.startsWith((version))) {
            verDirs.push_back(dir);
        }
    }
    if (verDirs.empty()) {
        return LINGLONG_ERR("no version found", -1);
    }

    version = verDirs.value(0);
    for (const auto &item : verDirs) {
        linglong::util::AppVersion versionIter(item);
        linglong::util::AppVersion dstVersion(version);
        if (versionIter.isValid() && versionIter.isBigThan(dstVersion)) {
            version = item;
        }
    }
    ref.version = version;
    qDebug() << "available versions" << version << appRoot << verDirs;
    return ref;
}

utils::error::Result<QString> OSTreeRepo::compressOstreeData(const package::Reference &ref)
{
    LINGLONG_TRACE(QString("compress %1").arg(ref.toString()));

    // check out ostree data
    // Fixme: use /tmp
    const auto savePath =
      QStringList{ util::getUserFile(".linglong/builder"), ref.id }.join(QDir::separator());
    util::ensureDir(savePath);
    qInfo() << "print save path:" << savePath;

    auto ret = checkout(ref, "", savePath);
    if (!ret) {
        return LINGLONG_ERR(QString("checkout to %2").arg(savePath), ret);
    }

    // compress data
    QStringList args;
    const QString fileName = QString("%1.tgz").arg(ref.appId);
    QString filePath =
      QStringList{ util::getUserFile(".linglong/builder"), fileName }.join(QDir::separator());
    auto currentPath = QDir::currentPath();
    QDir::setCurrent(savePath);

    args << "-zcf" << filePath << ".";

    // TODO: handle error of tar
    auto error = util::Exec("tar", args);
    qDebug() << "tar with exit code:" << error.code() << error.message();
    QDir::setCurrent(currentPath);
    return { filePath };
}

linglong::utils::error::Result<void> OSTreeRepo::getRemoteRepoList(QVector<QString> &vec)
{
    LINGLONG_TRACE("get remote repos");

    g_auto(GStrv) res = nullptr;
    res = ostree_repo_remote_list(repoPtr.get(), nullptr);

    if (res == nullptr) {
        return LINGLONG_ERR("no remote repository found", -1);
    }

    for (int i = 0; res[i] != nullptr; i++) {
        vec.push_back(QString::fromUtf8(res[i]));
    }

    return LINGLONG_OK;
}

/*
 * 通过 ostree 命令将软件包数据从远端仓库 pull 到本地
 *
 * @param destPath: 仓库路径
 * @param remoteName: 远端仓库名称
 * @param ref: 软件包对应的仓库索引 ref
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
linglong::utils::error::Result<void> OSTreeRepo::repoPullbyCmd(const QString &destPath,
                                                               const QString &remoteName,
                                                               const QString &ref)
{
    LINGLONG_TRACE("pull");

    // 创建临时仓库
    auto tmpPath = createTmpRepo(destPath + "/repo");
    if (!tmpPath) {
        return LINGLONG_ERR(tmpPath);
    }

    g_autoptr(GError) gErr = nullptr;
    std::string refString = ref.toStdString();
    char *refs = (char *)refString.c_str();
    char *refs_to_fetch[2] = { refs, nullptr };
    g_autoptr(GFile) src_repo_path = g_file_new_for_path((*tmpPath).toStdString().c_str());
    g_autoptr(OstreeRepo) tmpRepo = ostree_repo_new(src_repo_path);
    auto remote = remoteRepoName.toStdString();

    g_autoptr(GVariant) options = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "http2", g_variant_new_boolean(FALSE));
    g_variant_builder_add(&builder, "{sv}", "gpg-verify", g_variant_new_boolean(FALSE));
    options = g_variant_ref_sink(g_variant_builder_end(&builder));
    ostree_repo_open(tmpRepo, NULL, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_open", gErr);
    }

    ostree_repo_remote_add(tmpRepo,
                           remoteName.toStdString().c_str(),
                           (remoteEndpoint + "/repos/" + remoteName).toStdString().c_str(),
                           options,
                           NULL,
                           &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_remote_add", gErr);
    }

    ostree_repo_pull(tmpRepo,
                     remote.c_str(),
                     refs_to_fetch,
                     OSTREE_REPO_PULL_FLAGS_MIRROR,
                     NULL,
                     NULL,
                     &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_pull", gErr);
    }
    qInfo() << "repoPullbyCmd pull success";

    auto path = destPath + "/repo";
    auto *dest_repo_path = g_file_new_for_path(path.toStdString().c_str());
    g_autoptr(OstreeRepo) repoDest = ostree_repo_new(dest_repo_path);
    ostree_repo_open(repoDest, NULL, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_open", gErr);
    }

    g_autofree auto *base_url = g_strconcat("file://", tmpPath->toStdString().c_str(), NULL);
    builder = {};
    options = NULL;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(
      &builder,
      "{s@v}",
      "refs",
      g_variant_new_variant(g_variant_new_strv((const char *const *)refs_to_fetch, -1)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-static-deltas",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-sign-verify",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));
    g_variant_builder_add(&builder,
                          "{s@v}",
                          "disable-sign-verify-summary",
                          g_variant_new_variant(g_variant_new_boolean(TRUE)));

    options = g_variant_ref_sink(g_variant_builder_end(&builder));

    ostree_repo_pull_with_options(repoDest, (char *)base_url, options, NULL, NULL, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_pull_with_options", gErr);
    }

    // 删除临时仓库
    QString tmpRepoDir = (*tmpPath).left((*tmpPath).length() - QString("/repoTmp").length());
    qInfo() << "delete tmp repo path:" << tmpRepoDir;
    linglong::util::removeDir(tmpRepoDir);
    return LINGLONG_OK;
}

/*
 * 删除本地 repo 仓库中软件包对应的 ref 分支信息及数据
 *
 * @param repoPath: 仓库路径
 * @param remoteName: 远端仓库名称
 * @param ref: 软件包对应的仓库索引 ref
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
linglong::utils::error::Result<void> OSTreeRepo::repoDeleteDatabyRef(const QString &repoPath,
                                                                     const QString &ref)
{
    LINGLONG_TRACE(QString("delete %1").arg(ref));

    if (repoPath.isEmpty() || ref.isEmpty()) {
        return LINGLONG_ERR("repoDeleteDatabyRef param error");
    }

    g_autoptr(GError) error = nullptr;
    ostree_repo_set_ref_immediate(repoPtr.get(),
                                  nullptr,
                                  ref.toLocal8Bit(),
                                  nullptr,
                                  nullptr,
                                  &error);
    if (error != nullptr) {
        return LINGLONG_ERR("ostree_repo_set_ref_immediate", error);
    }

    qInfo() << "repoDeleteDatabyRef delete " << ref << " success";

    gint objectsTotal = 0;
    gint objectsPruned = 0;
    guint64 objsizeTotal = 0;
    g_autofree char *formattedFreedSize = NULL;
    ostree_repo_prune(repoPtr.get(),
                      OSTREE_REPO_PRUNE_FLAGS_REFS_ONLY,
                      0,
                      &objectsTotal,
                      &objectsPruned,
                      &objsizeTotal,
                      nullptr,
                      &error);
    if (error != nullptr) {
        return LINGLONG_ERR("ostree_repo_prune", error);
    }

    formattedFreedSize = g_format_size_full(objsizeTotal, (GFormatSizeFlags)0);
    qInfo() << "repoDeleteDatabyRef Total objects:" << objectsTotal;
    if (objectsPruned == 0) {
        qInfo() << "repoDeleteDatabyRef No unreachable objects";
    } else {
        qInfo() << "Deleted " << objectsPruned << " objects," << formattedFreedSize << " freed";
    }

    return LINGLONG_OK;
}

/*
 * 在玲珑应用安装目录创建一个临时 repo 子仓库
 *
 * @param parentRepo: 父 repo 仓库路径
 *
 * @return QString: 临时 repo 路径
 */
linglong::utils::error::Result<QString> OSTreeRepo::createTmpRepo(const QString &parentRepo)
{
    LINGLONG_TRACE(QString("create temporary repo for %1").arg(parentRepo));

    QString baseDir = linglong::util::getLinglongRootPath() + "/.cache";
    linglong::util::createDir(baseDir);
    QTemporaryDir dir(baseDir + "/linglong-cache-XXXXXX");
    QString tmpPath;
    if (dir.isValid()) {
        tmpPath = dir.path();
    } else {
        qCritical() << "create tmpPath failed, please check " << baseDir << ","
                    << dir.errorString();
        return QString();
    }
    dir.setAutoRemove(false);
    tmpPath += "/repoTmp";
    g_autoptr(GFile) file = g_file_new_for_path(tmpPath.toLocal8Bit());
    g_autoptr(OstreeRepo) tmpRepo = ostree_repo_new(file);
    g_autoptr(GError) gErr = nullptr;
    ostree_repo_create(tmpRepo, OSTREE_REPO_MODE_BARE_USER_ONLY, NULL, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_create", gErr);
    }

    g_autoptr(GKeyFile) config = ostree_repo_copy_config(tmpRepo);
    Q_ASSERT(config != nullptr);

    g_key_file_set_string(config, "core", "min-free-space-size", "600MB");
    g_key_file_set_string(config, "core", "parent", parentRepo.toUtf8());

    ostree_repo_write_config(tmpRepo, config, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_write_config", gErr);
    }

    qInfo() << "create tmp repo path:" << tmpPath << ", ret:" << QDir().exists(tmpPath);
    return tmpPath;
}

// NOTE(black_desk):
// This function is used to called in OSTreeRepo's constructor,
// to make sure the ostree-based linglong repository we using exists.
linglong::utils::error::Result<void> OSTreeRepo::initCreateRepoIfNotExists()
{
    Q_ASSERT(!repoPtr);
    Q_ASSERT(!repoRootPath.isEmpty());

    LINGLONG_TRACE("create ostree repository if not exists");

    QString repoPath = repoRootPath + "/repo";
    if (!QDir(repoPath).mkpath(".")) {
        return LINGLONG_ERR("mkdir " + repoPath, -1);
    }

    g_autoptr(GFile) repodir = g_file_new_for_path(repoPath.toLocal8Bit());

    g_autoptr(OstreeRepo) repo = ostree_repo_new(repodir);
    g_autoptr(GError) gErr = nullptr;
    ostree_repo_open(repo, nullptr, &gErr);
    if (gErr != nullptr) {
        qDebug() << "ostree_repo_open failed:" << gErr->message << "[" << gErr->code << "]";
        g_clear_error(&gErr);
        g_clear_object(&repo);
        repo = ostree_repo_new(repodir);

        qInfo() << "Creating linglong ostree repo at" << repoPath;
        if (ostree_repo_create(repo, OSTREE_REPO_MODE_BARE_USER_ONLY, nullptr, &gErr) == 0) {
            return LINGLONG_ERR("ostree_repo_create error", gErr);
        }
    }

    QString url = this->remoteEndpoint;

    url += "/repos/" + this->remoteRepoName;

    g_autoptr(GVariant) options = NULL;
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&builder, "{sv}", "gpg-verify", g_variant_new_boolean(FALSE)); // NOLINT
    options = g_variant_ref_sink(g_variant_builder_end(&builder));
    ostree_repo_remote_change(repo,
                              nullptr,
                              OSTREE_REPO_REMOTE_CHANGE_REPLACE,
                              this->remoteRepoName.toLocal8Bit(),
                              url.toLocal8Bit(),
                              options,
                              nullptr,
                              &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR(
          QString("add remote repo [%1 %2]").arg(this->remoteRepoName, this->remoteEndpoint),
          gErr);
    }

    GKeyFile *configKeyFile = ostree_repo_get_config(repo);
    Q_ASSERT(configKeyFile != nullptr);

    g_key_file_set_string(configKeyFile, "core", "min-free-space-size", "600MB");
    // NOTE:
    // libcurl 8.2.1 has a http2 bug https://github.com/curl/curl/issues/11859
    // We disable http2 for now.
    g_key_file_set_string(configKeyFile,
                          QString("remote \"%1\"").arg(remoteRepoName).toLocal8Bit(),
                          "http2",
                          "false");
    g_key_file_set_string(configKeyFile,
                          QString("remote \"%1\"").arg(remoteRepoName).toLocal8Bit(),
                          "gpg-verify",
                          "false");

    ostree_repo_write_config(repo, configKeyFile, &gErr);
    if (gErr != nullptr) {
        return LINGLONG_ERR("ostree_repo_write_config", gErr);
    }

    qDebug() << "new OstreeRepo" << repo;

    this->repoPtr.reset(g_steal_pointer(&repo));

    return LINGLONG_OK;
}

OSTreeRepo::~OSTreeRepo() = default;

} // namespace repo
} // namespace linglong
