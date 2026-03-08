#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace malla {

enum class DBKind { PSQL };

struct connection_params {
  std::string host = "localhost";
  unsigned port = 5432;
  std::string dbname = "postgres";
  std::string user;
  std::string password;
};

class DBConn {
public:
  virtual ~DBConn() = default;

  DBConn(const DBConn &) = delete;
  DBConn(DBConn &&) = delete;
  DBConn &operator=(const DBConn &) = delete;
  DBConn &operator=(DBConn &&) = delete;

  virtual bool Ok() const = 0;
  virtual std::vector<std::string> ListDatabases() const = 0;
  virtual std::vector<std::string> ListTables() const = 0;
  virtual std::vector<std::pair<std::string, std::string>>
  ListSchema(const std::string &table) const = 0;
  virtual std::vector<std::vector<std::string>>
  ListFilteredRows(const std::string &table,
                   const std::vector<std::string> &columns, int max_rows = 25,
                   const std::string &where_clause = "") const = 0;

  virtual std::vector<std::string>
  ListRowIds(const std::string &table, int max_rows = 25,
             const std::string &where_clause = "") const = 0;
  virtual std::string GetPrimaryKeyColumn(const std::string &table) const = 0;

  virtual bool UpdateCell(const std::string &table, const std::string &row_id,
                          const std::string &column,
                          const std::string &new_value) = 0;

  static std::unique_ptr<DBConn> Make(DBKind kind,
                                      const connection_params &params);

protected:
  DBConn() = default;
};

void ShowDatabasesAndTables(const connection_params &params,
                            std::ostream &out = std::cout,
                            DBKind kind = DBKind::PSQL);

} // namespace malla
