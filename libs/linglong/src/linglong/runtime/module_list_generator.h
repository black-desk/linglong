/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_RUNTIME_MODULE_LIST_GENERATOR_H_
#define LINGLONG_RUNTIME_MODULE_LIST_GENERATOR_H_

#include "linglong/runtime/module_policy.h"

#include <map>
#include <string>
#include <vector>

namespace linglong::runtime {

// This class generate the automatically enabled module list;
class ModuleListGenerator
{
public:
    explicit ModuleListGenerator(const std::vector<std::reference_wrapper<ModulePolicy>> &policies);

    std::map<std::string, bool> gen() noexcept;

private:
    std::vector<std::reference_wrapper<ModulePolicy>> policies;
};

} // namespace linglong::runtime
#endif
