#include "dbconn.hpp"
#include "pq.hpp"

namespace malla {

std::unique_ptr<DBConn> DBConn::Make(DBKind kind,
                                     const connection_params &params) {
  if (kind == DBKind::PSQL) {
    return std::make_unique<PsqlDBConn>(params);
  }
  return nullptr;
}

} // namespace malla
