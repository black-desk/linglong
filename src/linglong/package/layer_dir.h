/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_DIR_H_
#define LINGLONG_PACKAGE_LAYER_DIR_H_

#include "linglong/api/types/v1/PackageInfo.hpp"
#include "linglong/utils/error/error.h"

#include <QDir>

namespace linglong::package {

class LayerDir : public QDir
{
public:
    LayerDir(const LayerDir &) = delete;
    LayerDir(LayerDir &&) = delete;
    LayerDir &operator=(const LayerDir &) = delete;
    LayerDir &operator=(LayerDir &&) = delete;

    using QDir::QDir;
    ~LayerDir();
    utils::error::Result<api::types::v1::PackageInfo> info() const;
    utils::error::Result<QByteArray> rawInfo() const;

    // NOTE: Maybe should be removed. and use QTemporaryDir
    void setCleanStatus(bool status);

private:
    bool cleanup = true;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_DIR_H_ */
