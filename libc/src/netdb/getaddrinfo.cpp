//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of getaddrinfo.
///
//===----------------------------------------------------------------------===//

#include "src/netdb/getaddrinfo.h"

#include "hdr/netdb_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_addrinfo.h"
#include "hdr/types/struct_servent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/netdb/addrinfo.h"
#include "src/__support/netdb/services.h"

namespace LIBC_NAMESPACE_DECL {

static int add_named_service(cpp::string_view serv, const char *proto,
                             int socktype, int protocol,
                             cpp::span<netdb::ServiceSpec> out, size_t &count,
                             int &sys_err) {
  constexpr size_t SERV_BUF_SIZE = 1024;
  struct servent entry{};
  struct servent *found = nullptr;
  char buffer[SERV_BUF_SIZE];

  const int RC = netdb::lookup_serv_by_name(serv, proto, &entry, buffer, &found,
                                            netdb::get_services_path());
  if (RC != 0) {
    sys_err = RC;
    return RC;
  }
  if (found != nullptr)
    out[count++] = {socktype, protocol, static_cast<in_port_t>(entry.s_port)};
  return 0;
}

LLVM_LIBC_FUNCTION(int, getaddrinfo,
                   (const char *__restrict nodename,
                    const char *__restrict servname,
                    const struct addrinfo *__restrict hints,
                    struct addrinfo **__restrict res)) {
  LIBC_CRASH_ON_NULLPTR(res);

  int sys_err = 0;
  const int RC = netdb::lookup_addrinfo(nodename, servname, hints,
                                        &add_named_service, res, sys_err);
  if (RC == EAI_SYSTEM)
    libc_errno = sys_err;
  return RC;
}

} // namespace LIBC_NAMESPACE_DECL
