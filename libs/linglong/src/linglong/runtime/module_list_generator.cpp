#include "linglong/runtime/module_list_generator.h"

#include <algorithm>

namespace linglong::runtime {

std::map<std::string, bool> ModuleListGenerator::gen() noexcept
{
    auto result = std::map<std::string, bool>{};

    std::for_each(this->policies.cbegin(),
                  this->policies.cend(),
                  [&result](const ModulePolicy &policy) noexcept {
                      policy.appendModulesTo(result);
                  });

    return result;
}

} // namespace linglong::runtime
