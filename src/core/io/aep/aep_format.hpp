/*
 * SPDX-FileCopyrightText: 2019-2023 Mattia Basaglia <dev@dragon.best>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "io/base.hpp"
#include "io/io_registry.hpp"

namespace glaxnimate::io::avd {


class AepFormat : public ImportExport
{
    Q_OBJECT

public:
    QString slug() const override { return "aep"; }
    QString name() const override { return tr("Adobe After Effects Project"); }
    QStringList extensions() const override { return {"aep"}; }
    bool can_save() const override { return false; }

protected:
    bool on_open(QIODevice& file, const QString&, model::Document* document, const QVariantMap& options) override;

private:
    static Autoreg<AepFormat> autoreg;
};


class AepxFormat : public ImportExport
{
    Q_OBJECT

public:
    QString slug() const override { return "aepx"; }
    QString name() const override { return tr("Adobe After Effects Project XML"); }
    QStringList extensions() const override { return {"aepx"}; }
    bool can_save() const override { return false; }

protected:
    bool on_open(QIODevice& file, const QString&, model::Document* document, const QVariantMap& options) override;

private:
    static Autoreg<AepFormat> autoreg;
};

} // namespace glaxnimate::io::avd



