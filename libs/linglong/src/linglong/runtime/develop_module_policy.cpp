/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/develop_module_policy.h"

namespace linglong::runtime {

void DevelopModulePolicy::appendModulesTo(std::map<std::string, bool> &modules) const noexcept
{
    modules.emplace("develop", true);
}

} // namespace linglong::runtime
