#pragma once

#include "storage/dbconn.hpp"
#include <string>

namespace malla {

struct po_options {
  bool help = false;
  std::string help_text;
  DBKind dbkind = DBKind::PSQL;
  std::string host;
  unsigned port = 5432;
  std::string dbname;
  std::string table;
  std::string user;
  std::string password;
};

po_options ParseOptions(int argc, char *argv[]);

} // namespace malla
