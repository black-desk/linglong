/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_CLI_PRINTER_H_
#define LINGLONG_SRC_CLI_PRINTER_H_

#include "linglong/api/types/v1/CliContainer.hpp"
#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/api/types/v1/PackageInfo.hpp"
#include "linglong/api/types/v1/PackageManager1GetRepoInfoResultRepoInfo.hpp"
#include "linglong/utils/error/error.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <cstddef>

namespace linglong::cli {

class Printer
{
public:
    Printer() = default;
    Printer(const Printer &) = delete;
    Printer(Printer &&) = delete;
    Printer &operator=(const Printer &) = delete;
    Printer &operator=(Printer &&) = delete;
    virtual ~Printer() = default;

    virtual void printErr(const utils::error::Error &);

    virtual void printPackageInfos(const std::vector<api::types::v1::PackageInfo> &);
    virtual void printContainers(const std::vector<api::types::v1::CliContainer> &);
    virtual void printCommonResult(const api::types::v1::CommonResult &);
    virtual void printRepoInfos(const api::types::v1::PackageManager1GetRepoInfoResultRepoInfo &);
    virtual void printLayerInfo(const api::types::v1::LayerInfo &);

    virtual void printMessage(const QString &, const int num = -1);
    virtual void printReplacedText(const QString &, const int num = -1);
};

} // namespace linglong::cli

#endif // LINGLONG_SRC_PRINTER_PRINTER_H
