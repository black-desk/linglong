/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_RUNTIME_DEVELOP_MODULE_POLICY_H_
#define LINGLONG_RUNTIME_DEVELOP_MODULE_POLICY_H_

#include "linglong/runtime/module_policy.h"

namespace linglong::runtime {
class DevelopModulePolicy : public ModulePolicy
{
public:
    void appendModulesTo(std::map<std::string, bool> &modules) const noexcept override;
};

} // namespace linglong::runtime

#endif
