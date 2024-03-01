/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container_builder.h"

#include "linglong/util/configure.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"
#include "ocppi/runtime/config/types/Generators.hpp"
#include "ocppi/runtime/config/types/Mount.hpp"

namespace linglong::runtime {

namespace {

void applyJSONPatch(nlohmann::json &origin, const QFileInfo &info) noexcept
{
    if (!info.isFile()) {
        return;
    }

    QFile patchFile = info.absoluteFilePath();
    patchFile.open(QFile::ReadOnly);
    if (!patchFile.isOpen()) {
        qCritical() << "Failed to process" << info.absoluteFilePath() << patchFile.errorString()
                    << "[" << patchFile.error() << "]";
        Q_ASSERT(false);
        return;
    }

    auto contents = patchFile.readAll();
    if (patchFile.error() != QFile::NoError) {
        qCritical() << "Failed to process" << info.absoluteFilePath()
                    << "while reading file content" << patchFile.errorString() << "["
                    << patchFile.error() << "]";
        Q_ASSERT(false);
        return;
    }

    try {
        auto patch = nlohmann::json::parse(contents);
        // FIXME: should check semver
        if (patch.at("ociVersion") != origin["ociVersion"]) {
            qCritical() << "Failed to process" << info.absoluteFilePath()
                        << "ociVersion mismatched";
            Q_ASSERT(false);
            return;
        }
        origin = origin.patch(patch.at("patch"));
    } catch (const std::exception &e) {
        qCritical() << "Failed to process" << info.absoluteFilePath()
                    << "while parsing or applying patch" << e.what();
        Q_ASSERT(false);
        return;
    }
}

void applyExecutablePatch(nlohmann::json &origin, const QFileInfo &info) noexcept
{
    QProcess patchProcess;
    patchProcess.setProgram(info.absoluteFilePath());
    patchProcess.write(QByteArray::fromStdString(origin.dump()));

    constexpr auto timeout = 200;
    if (!patchProcess.waitForFinished(timeout)) {
        qCritical() << "Failed to process" << info.absoluteFilePath()
                    << "timeout or execute failed";
        Q_ASSERT(false);
        return;
    }

    if (patchProcess.exitStatus() != QProcess::ExitStatus::NormalExit) {
        qCritical() << "Failed to process" << info.absoluteFilePath() << "crashed";
        Q_ASSERT(false);
        return;
    }

    if (patchProcess.exitCode() != 0) {
        qCritical() << "Failed to process" << info.absoluteFilePath() << "process exit with"
                    << patchProcess.exitCode();
        Q_ASSERT(false);
        return;
    }

    auto result = patchProcess.readAllStandardOutput();
    try {
        origin = nlohmann::json::parse(result);
    } catch (const std::exception &e) {
        qCritical() << "Failed to process" << info.absoluteFilePath() << "while parse result"
                    << e.what();
        Q_ASSERT(false);
        return;
    } catch (...) {
        qCritical() << "Failed to process" << info.absoluteFilePath() << "while parse result";
        Q_ASSERT(false);
        return;
    }
}

void applyPatches(nlohmann::json &origin, const QDir &patchesDir) noexcept
{
    if (!patchesDir.exists()) {
        Q_ASSERT(false);
        return;
    }

    for (const auto &info : patchesDir.entryInfoList()) {
        if (!info.isFile()) {
            continue;
        }

        if (info.isExecutable()) {
            applyExecutablePatch(origin, info);
            continue;
        }

        applyJSONPatch(origin, info);
    }
}

} // namespace

ContainerBuilder::ContainerBuilder(ocppi::cli::CLI &cli)
    : cli(cli)
{
}

utils::error::Result<QSharedPointer<Container>>
ContainerBuilder::create(const ContainerOptions &opts) noexcept
{
    LINGLONG_TRACE("create container");

    QFile defaultOCIConfigFile(LINGLONG_DATA_DIR "/config.json");
    defaultOCIConfigFile.open(QFile::ReadOnly);
    if (!defaultOCIConfigFile.isOpen()) {
        qCritical() << "Cannot found default oci config file:" << defaultOCIConfigFile.errorString()
                    << "[" << defaultOCIConfigFile.error() << "]";
        defaultOCIConfigFile.setFileName(":/config.json");
        defaultOCIConfigFile.open(QFile::ReadOnly);
    }
    Q_ASSERT(defaultOCIConfigFile.isOpen());

    QString containerConfigFilePath = qgetenv("LINGLONG_CONTAINER_CONFIG");
    if (containerConfigFilePath.isEmpty()) {
        containerConfigFilePath = LINGLONG_INSTALL_PREFIX "/lib/linglong/config.json";
    }

    QFile containerConfigFile = containerConfigFilePath;
    containerConfigFile.open(QFile::ReadOnly);
    if (!containerConfigFile.isOpen()) {
        return LINGLONG_ERR(containerConfigFile);
    }

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(containerConfigFile.readAll());
    } catch (...) {
        return LINGLONG_ERR(std::current_exception());
    }

    {
        nlohmann::json mounts;
        try {
            mounts = json.at("mounts");
        } catch (...) {
            qCritical() << "mounts not found in config.json";
            mounts = nlohmann::json::array();
        }

        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/",
          .options = { { "rbind", "ro" } },
          .source = { opts.base.absoluteFilePath("files").toStdString() },
          .type = { "bind" } });

        if (opts.runtime) {
            mounts.push_back(ocppi::runtime::config::types::Mount{
              .destination = "/runtime",
              .options = { { "rbind", "ro" } },
              .source = { opts.runtime->absoluteFilePath("files").toStdString() },
              .type = { "bind" } });
        }

        mounts.push_back(
          ocppi::runtime::config::types::Mount{ .destination = "/opt/apps/",
                                                .options = { { "nodev", "nosuid", "mode=700" } },
                                                .source = { "tmpfs" },
                                                .type = { "tmpfs" } });

        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = QString("/opt/apps/%1/files").arg(opts.appID).toStdString(),
          .options = { { "rbind", "ro" } },
          .source = { opts.app.absoluteFilePath("files").toStdString() },
          .type = { "bind" } });

        json["mounts"] = mounts;
    }

    QDir configDotDDir = QFileInfo(containerConfigFilePath).dir().filePath("../config.d");
    applyPatches(json, configDotDDir);

    utils::error::Result<ocppi::runtime::config::types::Config> config;
    try {
        config = json.get<ocppi::runtime::config::types::Config>();
    } catch (...) {
        config = LINGLONG_ERR(std::current_exception());
    }

    if (!config) {
        Q_ASSERT(false);
        return LINGLONG_ERR(config);
    }

    Q_ASSERT(config->mounts.has_value());
    auto &mounts = *config->mounts;

    mounts.insert(mounts.end(), opts.mounts.begin(), opts.mounts.end());

    return QSharedPointer<Container>::create(config, opts.containerID, this->cli);
}

} // namespace linglong::runtime
