/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/language_module_policy.h"

#include <QDebug>

#include <cstdlib>

namespace linglong::runtime {

namespace {

const char *getLocale() noexcept
{
    // Refer to https://man7.org/linux/man-pages/man7/locale.7.html
    // (1)  If there is a non-null environment variable LC_ALL, the value of LC_ALL is used.
    const char *lang = nullptr;
    lang = getenv("LC_ALL");
    if (lang != nullptr) {
        return lang;
    }

    // (2)  If an environment variable with the same name as one of the categories above exists and
    // is non-null, its value is used for that category.

    const static std::vector<std::string> localeEnvs{
        "LC_ADDRESS",  "LC_COLLATE",  "LC_CTYPE",       "LC_IDENTIFICATION",
        "LC_MONETARY", "LC_MESSAGES", "LC_MEASUREMENT", "LC_NAME",
        "LC_NUMERIC",  "LC_PAPER",    "LC_TELEPHONE",   "LC_TIME",
    };

    bool warning = false;

    for (const auto &env : localeEnvs) {
        if (getenv(env.c_str()) == nullptr) {
            continue;
        }
        warning = true;
    }

    if (warning) {
        qWarning()
          << "Some non-LC_ALL LC_* environment variables is set while LC_ALL not set yet. This is "
             "not supported now and language module policy will fallback to use LANG.";
    }

    // (3)  If there is a non-null environment variable LANG, the value of LANG is used.
    lang = getenv("LANG");
    if (lang != nullptr) {
        return lang;
    }

    return "C";
}

std::string getLanguage(const char *locale) noexcept
{
    // Refer to https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html
    // `locale` format is [language[_territory][.codeset][@modifier]]
    // We want [language[_territory] part here.

    Q_ASSERT(locale != nullptr);

    std::string lang(locale);
    auto pos = lang.find('.');
    if (pos != std::string::npos) {
        lang = lang.substr(0, pos);
    }

    pos = lang.find('@');
    if (pos != std::string::npos) {
        lang = lang.substr(0, pos);
    }

    return lang;
}

} // namespace

void LanguageModulePolicy::appendModulesTo(std::map<std::string, bool> &modules) const noexcept
{
    auto lang = getLanguage(getLocale());
    Q_ASSERT(lang.empty() == false);

    modules.emplace("lang_" + lang, false);
}

} // namespace linglong::runtime
