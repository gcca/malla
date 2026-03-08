#pragma once

#include "dbconn.hpp"
#include <libpq-fe.h>
#include <memory>
#include <string>
#include <vector>

namespace malla {

class PsqlDBConn : public DBConn {
public:
  explicit PsqlDBConn(const connection_params &params);
  ~PsqlDBConn() final;

  bool Ok() const final;
  std::vector<std::string> ListDatabases() const final;
  std::vector<std::string> ListTables() const final;
  std::vector<std::pair<std::string, std::string>>
  ListSchema(const std::string &table) const final;
  std::vector<std::vector<std::string>>
  ListFilteredRows(const std::string &table,
                   const std::vector<std::string> &columns, int max_rows = 25,
                   const std::string &where_clause = "") const final;

  std::vector<std::string>
  ListRowIds(const std::string &table, int max_rows = 25,
             const std::string &where_clause = "") const final;
  std::string GetPrimaryKeyColumn(const std::string &table) const final;

  bool UpdateCell(const std::string &table, const std::string &row_id,
                  const std::string &column,
                  const std::string &new_value) final;

private:
  PGconn *conn_;
};

} // namespace malla
