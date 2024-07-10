/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_RUNTIME_MODULE_POLICY_H_
#define LINGLONG_RUNTIME_MODULE_POLICY_H_

#include <map>
#include <string>

namespace linglong::runtime {

class ModulePolicy
{
public:
    ModulePolicy() = default;
    ModulePolicy(const ModulePolicy &) = delete;
    ModulePolicy(ModulePolicy &&) = delete;
    ModulePolicy &operator=(const ModulePolicy &) = delete;
    ModulePolicy &operator=(ModulePolicy &&) = delete;
    virtual ~ModulePolicy() = default;

    virtual void appendModulesTo(std::map<std::string, bool> &modules) const noexcept = 0;
};

} // namespace linglong::runtime

#endif
