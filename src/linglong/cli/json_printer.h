/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_CLI_JSON_PRINTER_H_
#define LINGLONG_CLI_JSON_PRINTER_H_

#include "linglong/cli/printer.h"

namespace linglong::cli {

class JSONPrinter : public Printer
{
public:
    void printErr(const utils::error::Error &) override;

    void printPackageInfos(const std::vector<api::types::v1::PackageInfo> &) override;
    void printContainers(const std::vector<api::types::v1::CliContainer> &) override;
    void printCommonResult(const api::types::v1::CommonResult &) override;
    void printRepoInfos(const api::types::v1::PackageManager1GetRepoInfoResultRepoInfo &) override;
    void printLayerInfo(const api::types::v1::LayerInfo &) override;

    void printMessage(const QString &, const int num = -1) override;
    void printReplacedText(const QString &, const int num = -1) override;
};

} // namespace linglong::cli

#endif
