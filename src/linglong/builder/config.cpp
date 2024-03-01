/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/config.h"

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/serialize/yaml.h"

#include <fstream>
#include <mutex>

namespace linglong::builder {

// QString BuilderConfig::repoPath() const
// {
// return QStringList{ util::userCacheDir().path(), "linglong-builder" }.join(QDir::separator());
// }

// QString BuilderConfig::ostreePath() const
// {
// return QStringList{ util::userCacheDir().path(), "linglong-builder/repo" }.join("/");
// }

// QString BuilderConfig::targetFetchCachePath() const
// {
// auto target =
// QStringList{ this->projectRoot, ".linglong-target", this->projectName, "fetch", "cache" }
// .join("/");
// util::ensureDir(target);
// return target;
// }

// QString BuilderConfig::targetSourcePath() const
// {
// auto target =
// QStringList{ this->projectRoot, ".linglong-target", this->projectName, "source" }.join("/");
// util::ensureDir(target);
// return target;
// }

// QString BuilderConfig::layerPath(const QStringList &subPathList) const
// {
// QStringList list{ util::userCacheDir().path(), "linglong-builder/layers" };
// list.append(subPathList);
// return list.join(QDir::separator());
// }

// QString BuilderConfig::templatePath() const
// {
// for (auto dataPath : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
// QString templatePath =
// QStringList{ dataPath, "linglong", "builder", "templates" }.join(QDir::separator());
// if (util::dirExists(templatePath)) {
// return templatePath;
// }
// }
// return QString();
// }

// static QStringList projectConfigPaths()
// {
// QStringList result{};

// auto pwd = QDir::current();

// do {
// auto configPath =
// QStringList{ pwd.absolutePath(), ".ll-builder", "config.yaml" }.join(QDir::separator());
// result << std::move(configPath);
// } while (pwd.cdUp());

// return result;
// }

// static QStringList nonProjectConfigPaths()
// {
// QStringList result{};

// auto configLocations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
// configLocations.append(SYSCONFDIR);

// for (const auto &configLocation : configLocations) {
// result << QStringList{ configLocation, "linglong", "builder", "config.yaml" }.join(
// QDir::separator());
// }

// result << QStringList{ DATADIR, "linglong", "builder", "config.yaml" }.join(QDir::separator());

// return result;
// }

// static QString getConfigPath()
// {
// QStringList configPaths = {};

// configPaths << projectConfigPaths();
// configPaths << nonProjectConfigPaths();

// qDebug() << "Searching ll-builder config in:" << configPaths;

// for (const auto &configPath : configPaths) {
// if (QFile::exists(configPath)) {
// return configPath;
// }
// }

// return ":/config.yaml";
// }

// BuilderConfig &BuilderConfig::instance()
// {
// Q_INIT_RESOURCE(builder_releases);

// static BuilderConfig cfg;
// static api::types::v1::BuilderConfig apiCfg;
// static std::once_flag flag;
// std::call_once(flag, []() {
// const auto configPath = getConfigPath();
// qDebug() << "load config from" << configPath;
// auto apiCfg = utils::serialize::LoadYAML<api::types::v1::BuilderConfig>(
// QFile(configPath).readAll().constData());
// if (!apiCfg) {
// qCritical() << "invalid builder config:" << apiCfg.error();
// abort();
// }

// cfg.buildArch = linglong::util::hostArch();
// cfg.remoteRepoName = QString::fromStdString(apiCfg->remoteRepoName);
// cfg.remoteRepoEndpoint = QString::fromStdString(apiCfg->remoteRepoEndpoint);
// });

// return cfg;
// }

utils::error::Result<api::types::v1::BuilderConfig> loadConfig(const QString &file) noexcept
{
    LINGLONG_TRACE(QString("load config from %1").arg(file));

    try {
        auto ifs = std::ifstream(file.toLocal8Bit());
        if (!ifs.is_open()) {
            return LINGLONG_ERR("open failed");
        }

        auto config = utils::serialize::LoadYAML<api::types::v1::BuilderConfig>(ifs);
        if (config->version != 1) {
            return LINGLONG_ERR(
              QString("wrong configuration file version %1").arg(config->version));
        }

        return config;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

utils::error::Result<api::types::v1::BuilderConfig> loadConfig(const QStringList &files) noexcept
{
    LINGLONG_TRACE(QString("load config from %1").arg(files.join(" ")));

    for (const auto &file : files) {
        auto config = loadConfig(file);
        if (!config.has_value()) {
            qDebug() << "Failed to load config from" << file << ":" << config.error();
            continue;
        }

        qDebug() << "Load config from" << file;
        return config;
    }

    return LINGLONG_ERR("all failed");
}

utils::error::Result<void> saveConfig(const api::types::v1::BuilderConfig &cfg,
                                      const QString &path) noexcept
{
    LINGLONG_TRACE(QString("save config to %1").arg(path));

    try {
        auto ofs = std::ofstream(path.toLocal8Bit());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("open failed");
        }

        auto node = ytj::to_yaml(cfg);
        ofs << node;

        return LINGLONG_OK;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}
} // namespace linglong::builder
