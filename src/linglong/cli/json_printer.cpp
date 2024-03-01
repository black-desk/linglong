/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/json_printer.h"

#include "linglong/api/types/v1/Generators.hpp"

#include <QJsonArray>

#include <iostream>

namespace linglong::cli {

void JSONPrinter::printErr(const utils::error::Error &err)
{
    std::cout << nlohmann::json{
        { "code", err.code() },
        { "message", err.message().toStdString() }
    }.dump() << std::endl;
}

void JSONPrinter::printPackageInfos(const std::vector<api::types::v1::PackageInfo> &list)
{
    std::cout << nlohmann::json(list).dump() << std::endl;
}

void JSONPrinter::printContainers(const std::vector<api::types::v1::CliContainer> &list)
{
    std::cout << nlohmann::json(list).dump() << std::endl;
}

void JSONPrinter::printCommonResult(const api::types::v1::CommonResult &reply)
{
    std::cout << nlohmann::json(reply).dump() << std::endl;
}

void JSONPrinter::printRepoInfos(
  const api::types::v1::PackageManager1GetRepoInfoResultRepoInfo &infos)
{
    std::cout << nlohmann::json(infos).dump() << std::endl;
}

void JSONPrinter::printLayerInfo(const api::types::v1::LayerInfo &info)
{
    std::cout << nlohmann::json(info).dump() << std::endl;
}

} // namespace linglong::cli
