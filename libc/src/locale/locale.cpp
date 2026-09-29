//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of static locale instances and locale category data.
///
//===----------------------------------------------------------------------===//

#include "src/locale/locale.h"
#include "hdr/locale_macros.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/locale/locale_data.h"
#ifdef LIBC_COPT_LOCALE_SUPPORT_ENVIRON
#include "src/stdlib/environ_internal.h"
#endif

namespace LIBC_NAMESPACE_DECL {

__locale_t c_locale(&C_CTYPE_DATA, &C_NUMERIC_DATA, &C_TIME_DATA, nullptr,
                    &C_MONETARY_DATA, &C_MESSAGES_DATA);

__locale_t utf8_locale(&UTF8_CTYPE_DATA, &C_NUMERIC_DATA, &C_TIME_DATA, nullptr,
                       &C_MONETARY_DATA, &C_MESSAGES_DATA);

#ifdef LIBC_CONF_ENABLE_RUNTIME_LOCALE
static locale_t global_locale =
    DEFAULT_LOCALE_IS_UTF8 ? &utf8_locale : &c_locale;

static LIBC_THREAD_LOCAL locale_t thread_locale = nullptr;
#endif

locale_t get_global_locale() {
#ifdef LIBC_CONF_ENABLE_RUNTIME_LOCALE
  return global_locale;
#else
  return DEFAULT_LOCALE_IS_UTF8 ? &utf8_locale : &c_locale;
#endif
}

void set_global_locale([[maybe_unused]] locale_t loc) {
#ifdef LIBC_CONF_ENABLE_RUNTIME_LOCALE
  global_locale = loc;
#endif
}

locale_t get_thread_locale() {
#ifdef LIBC_CONF_ENABLE_RUNTIME_LOCALE
  return thread_locale;
#else
  return nullptr;
#endif
}

void set_thread_locale([[maybe_unused]] locale_t loc) {
#ifdef LIBC_CONF_ENABLE_RUNTIME_LOCALE
  thread_locale = loc;
#endif
}

static locale_t match_locale_name(cpp::string_view name) {
  if (!is_supported_base_locale(name))
    return nullptr;
  if (is_c_locale_name(name))
    return &c_locale;
  if (is_utf8_locale_name(name))
    return &utf8_locale;
  return nullptr;
}

#ifdef LIBC_COPT_LOCALE_SUPPORT_ENVIRON
static locale_t resolve_from_env(int category_mask) {
  locale_t default_loc = DEFAULT_LOCALE_IS_UTF8 ? &utf8_locale : &c_locale;
  auto &env = internal::EnvironmentManager::get_instance();
  const char *lc_all = env.get("LC_ALL");
  if (lc_all && lc_all[0] != '\0')
    return match_locale_name(lc_all);

  struct CategoryEnv {
    int category;
    const char *name;
  };
  constexpr CategoryEnv CATEGORY_ENVS[] = {
      {LC_CTYPE, "LC_CTYPE"},       {LC_NUMERIC, "LC_NUMERIC"},
      {LC_TIME, "LC_TIME"},         {LC_COLLATE, "LC_COLLATE"},
      {LC_MONETARY, "LC_MONETARY"}, {LC_MESSAGES, "LC_MESSAGES"},
  };

  const char *lang = env.get("LANG");
  locale_t result = default_loc;
  int mask = (category_mask != 0) ? category_mask : LC_ALL_MASK;
  for (const CategoryEnv &entry : CATEGORY_ENVS) {
    if (!(mask & (1 << entry.category)))
      continue;
    const char *val = env.get(entry.name);
    if (!val || val[0] == '\0')
      val = lang;
    locale_t cat_loc =
        (val && val[0] != '\0') ? match_locale_name(val) : default_loc;
    if (!cat_loc)
      return nullptr;
    if (entry.category == LC_CTYPE || mask == (1 << entry.category))
      result = cat_loc;
  }
  return result;
}
#endif

locale_t resolve_locale(int category_mask, const char *locale_name) {
  if (!locale_name || (category_mask & ~LC_ALL_MASK) != 0)
    return nullptr;

  cpp::string_view name(locale_name);
  if (name.empty()) {
#ifdef LIBC_COPT_LOCALE_SUPPORT_ENVIRON
    return resolve_from_env(category_mask);
#else
    return DEFAULT_LOCALE_IS_UTF8 ? &utf8_locale : &c_locale;
#endif
  }

  locale_t loc = match_locale_name(name);
  if constexpr (!ENABLE_RUNTIME_LOCALE) {
    if (loc == &utf8_locale && !DEFAULT_LOCALE_IS_UTF8)
      return nullptr;
  }
  return loc;
}

} // namespace LIBC_NAMESPACE_DECL
