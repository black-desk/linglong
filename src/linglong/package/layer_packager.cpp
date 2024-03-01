/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/util/file.h"
#include "linglong/util/runner.h"
#include "linglong/utils/command/env.h"

#include <QDataStream>

namespace linglong::package {

LayerPackager::LayerPackager(const QString &workDir)
    : workDir(QStringList{ workDir, QUuid::createUuid().toString(QUuid::Id128) }.join("-"))
{
    util::ensureDir(this->workDir);
}

LayerPackager::~LayerPackager()
{
    util::removeDir(this->workDir);
}

utils::error::Result<QSharedPointer<LayerFile>>
LayerPackager::pack(const LayerDir &dir, const QString &layerFilePath) const
{
    LINGLONG_TRACE("pack layer");

    QFile layer(layerFilePath);
    if (!layer.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return LINGLONG_ERR(layer);
    }

    if (layer.write(magicNumber) < 0) {
        return LINGLONG_ERR(layer);
    }

    auto info = dir.info();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    auto json = nlohmann::json(*info);
    auto data = QByteArray::fromStdString(json.dump());

    QByteArray dataSizeBytes;

    QDataStream dataSizeStream(&dataSizeBytes, QIODevice::WriteOnly);
    dataSizeStream.setVersion(QDataStream::Qt_5_10);
    dataSizeStream << quint32(data.size());

    Q_ASSERT(dataSizeStream.status() == QDataStream::Status::Ok);

    if (layer.write(dataSizeBytes) < 0) {
        return LINGLONG_ERR(layer);
    }

    if (layer.write(data) < 0) {
        return LINGLONG_ERR(layer);
    }

    layer.close();

    // compress data with erofs
    const auto compressedFilePath =
      QStringList{ this->workDir, "tmp.erofs" }.join(QDir::separator());

    auto ret =
      utils::command::Exec("mkfs.erofs", { "-zlz4hc,9", compressedFilePath, dir.absolutePath() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    ret = utils::command::Exec(
      "sh",
      { "-c", QString("cat %1 >> %2").arg(compressedFilePath, layerFilePath) });
    if (!ret) {
        LINGLONG_ERR(ret);
    }

    auto result = LayerFile::New(layerFilePath);
    Q_ASSERT(result.has_value());

    return result;
}

utils::error::Result<QSharedPointer<LayerDir>> LayerPackager::unpack(LayerFile &file,
                                                                     const QString &destnation)
{
    LINGLONG_TRACE("unpack layer file");

    auto unpackDir = QStringList{ this->workDir, "unpack" }.join(QDir::separator());
    util::ensureDir(unpackDir);

    QFileInfo fileInfo(file);

    auto offset = file.binaryDataOffset();
    if (!offset) {
        return LINGLONG_ERR(offset);
    }

    auto ret =
      util::Exec("erofsfuse",
                 { QString("--offset=%1").arg(*offset), fileInfo.absoluteFilePath(), unpackDir });
    if (ret) {
        return LINGLONG_ERR("call erofsfuse failed: " + ret.message(), ret.code());
    }

    util::copyDir(unpackDir, destnation);

    ret = util::Exec("umount", { unpackDir });
    if (ret) {
        return LINGLONG_ERR("call umount failed: " + ret.message(), ret.code());
    }

    QSharedPointer<LayerDir> layerDir(new LayerDir(destnation));

    return layerDir;
}

} // namespace linglong::package
