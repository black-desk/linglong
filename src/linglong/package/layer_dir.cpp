/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_dir.h"

#include "linglong/utils/serialize/json.h"

namespace linglong::package {

LayerDir::~LayerDir()
{
    if (cleanup) {
        this->removeRecursively();
    }
}

void LayerDir::setCleanStatus(bool status)
{
    this->cleanup = status;
}

utils::error::Result<api::types::v1::PackageInfo> LayerDir::info() const
{
    LINGLONG_TRACE("get layer info form dir");

    auto infoFile = QFile(this->filePath("info.json"));
    if (!infoFile.open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR(infoFile);
    }

    auto info = utils::serialize::LoadJSON<api::types::v1::PackageInfo>(infoFile.readAll());
    if (!info) {
        return LINGLONG_ERR(info);
    }

    return info;
}

} // namespace linglong::package
