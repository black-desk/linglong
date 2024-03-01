/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
#define LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_

#include "linglong/cli/printer.h"
#include "linglong/utils/error/error.h"

#include <QFileInfo>
#include <QObject>
#include <QUrl>

namespace linglong {
namespace builder {

class Source;
class Project;
class SourceFetcherPrivate;

class SourceFetcher : public QObject
{
    Q_OBJECT
public:
    explicit SourceFetcher(QSharedPointer<Source> s, cli::Printer &p, Project *project);
    ~SourceFetcher() override;

    QString sourceRoot() const;

    void setSourceRoot(const QString &path);

    linglong::util::Error fetch();

private:
    QString srcRoot;
    cli::Printer &printer;

    static constexpr auto CompressedFileTarXz = "tar.xz";
    static constexpr auto CompressedFileTarGz = "tar.gz";
    static constexpr auto CompressedFileTarBz2 = "tar.bz2";
    static constexpr auto CompressedFileTgz = "tgz";
    static constexpr auto CompressedFileTar = "tar";
    static constexpr auto CompressedFileZip = "zip";

    QString filename();

    // TODO: use share cache for all project
    QString sourceTargetPath() const;

    std::tuple<QString, linglong::util::Error> fetchArchiveFile();

    QString fixSuffix(const QFileInfo &fi);
    utils::Result<void> extractFile(const QString &path, const QString &dir);

    util::Error fetchGitRepo();

    util::Error handleLocalSource();

    util::Error handleLocalPatch();

    Project *project;
    QSharedPointer<Source> source;
    QScopedPointer<QFile> file;
};

} // namespace builder
} // namespace linglong

#endif // LINGLONG_SRC_BUILDER_SOURCE_FETCHER_H_
