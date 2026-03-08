#include "pq.hpp"
#include <libpq-fe.h>
#include <string>
#include <vector>

namespace malla {

PsqlDBConn::PsqlDBConn(const connection_params &params) : conn_(nullptr) {
  std::string port_str = std::to_string(params.port);
  const char *keys[] = {"host", "port", "dbname", "user", "password", nullptr};
  const char *values[] = {params.host.c_str(),     port_str.c_str(),
                          params.dbname.c_str(),   params.user.c_str(),
                          params.password.c_str(), nullptr};
  conn_ = PQconnectdbParams(keys, values, 0);
}

PsqlDBConn::~PsqlDBConn() {
  if (conn_) {
    PQfinish(conn_);
    conn_ = nullptr;
  }
}

bool PsqlDBConn::Ok() const {
  return conn_ && PQstatus(conn_) == CONNECTION_OK;
}

std::vector<std::string> PsqlDBConn::ListDatabases() const {
  std::vector<std::string> databases;
  if (!Ok())
    return databases;

  const char *query =
      "SELECT datname FROM pg_database WHERE datistemplate = false ORDER BY "
      "datname";
  PGresult *res = PQexec(conn_, query);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    return databases;
  }

  int nrows = PQntuples(res);
  databases.reserve(nrows);
  for (int i = 0; i < nrows; ++i) {
    databases.push_back(PQgetvalue(res, i, 0));
  }
  PQclear(res);
  return databases;
}

std::vector<std::string> PsqlDBConn::ListTables() const {
  std::vector<std::string> tables;
  if (!Ok())
    return tables;

  const char *query = "SELECT table_name FROM information_schema.tables "
                      "WHERE table_schema = 'public' ORDER BY table_name";
  PGresult *res = PQexec(conn_, query);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    return tables;
  }

  int nrows = PQntuples(res);
  tables.reserve(nrows);
  for (int i = 0; i < nrows; ++i) {
    tables.push_back(PQgetvalue(res, i, 0));
  }
  PQclear(res);
  return tables;
}

std::vector<std::pair<std::string, std::string>>
PsqlDBConn::ListSchema(const std::string &table) const {
  std::vector<std::pair<std::string, std::string>> columns;
  if (!Ok())
    return columns;

  const char *query =
      "SELECT column_name, data_type FROM information_schema.columns "
      "WHERE table_schema = 'public' AND table_name = $1 ORDER BY "
      "ordinal_position";
  const char *values[] = {table.c_str()};
  PGresult *res =
      PQexecParams(conn_, query, 1, nullptr, values, nullptr, nullptr, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    return columns;
  }

  int nrows = PQntuples(res);
  columns.reserve(nrows);
  for (int i = 0; i < nrows; ++i) {
    columns.emplace_back(PQgetvalue(res, i, 0), PQgetvalue(res, i, 1));
  }
  PQclear(res);
  return columns;
}

static std::string escape_identifier(const std::string &s) {
  std::string r = "\"";
  for (char c : s) {
    if (c == '"')
      r += "\"\"";
    else
      r += c;
  }
  return r + "\"";
}

std::vector<std::vector<std::string>> PsqlDBConn::ListFilteredRows(
    const std::string &table, const std::vector<std::string> &columns,
    int max_rows, const std::string &where_clause) const {
  std::vector<std::vector<std::string>> rows;
  if (!Ok() || columns.empty())
    return rows;

  int limit = max_rows <= 0 ? 25 : (max_rows > 500 ? 500 : max_rows);

  std::string cols;
  for (size_t i = 0; i < columns.size(); ++i) {
    if (i > 0)
      cols += ", ";
    cols += escape_identifier(columns[i]);
  }
  std::string query = "SELECT " + cols + " FROM " + escape_identifier(table);
  if (!where_clause.empty())
    query += " WHERE " + where_clause;
  query += " LIMIT " + std::to_string(limit);
  PGresult *res = PQexec(conn_, query.c_str());
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    return rows;
  }

  int nrows = PQntuples(res);
  int ncols = PQnfields(res);
  rows.reserve(nrows);
  for (int i = 0; i < nrows; ++i) {
    std::vector<std::string> row;
    row.reserve(ncols);
    for (int j = 0; j < ncols; ++j) {
      const char *v = PQgetvalue(res, i, j);
      row.push_back(v ? v : "");
    }
    rows.push_back(std::move(row));
  }
  PQclear(res);
  return rows;
}

std::vector<std::string>
PsqlDBConn::ListRowIds(const std::string &table, int max_rows,
                       const std::string &where_clause) const {
  std::vector<std::string> ids;
  if (!Ok())
    return ids;

  int limit = max_rows <= 0 ? 25 : (max_rows > 500 ? 500 : max_rows);
  std::string query = "SELECT ctid::text FROM " + escape_identifier(table);
  if (!where_clause.empty())
    query += " WHERE " + where_clause;
  query += " LIMIT " + std::to_string(limit);
  PGresult *res = PQexec(conn_, query.c_str());
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    return ids;
  }

  int nrows = PQntuples(res);
  ids.reserve(nrows);
  for (int i = 0; i < nrows; ++i) {
    const char *v = PQgetvalue(res, i, 0);
    ids.push_back(v ? v : "");
  }
  PQclear(res);
  return ids;
}

std::string PsqlDBConn::GetPrimaryKeyColumn(const std::string &table) const {
  if (!Ok())
    return "";
  const char *query =
      "SELECT kcu.column_name FROM information_schema.table_constraints tc "
      "JOIN information_schema.key_column_usage kcu "
      "ON tc.constraint_name = kcu.constraint_name "
      "AND tc.table_schema = kcu.table_schema "
      "WHERE tc.table_schema = 'public' AND tc.table_name = $1 "
      "AND tc.constraint_type = 'PRIMARY KEY' "
      "ORDER BY kcu.ordinal_position LIMIT 1";
  const char *values[] = {table.c_str()};
  PGresult *res =
      PQexecParams(conn_, query, 1, nullptr, values, nullptr, nullptr, 0);
  std::string col;
  if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0)
    col = PQgetvalue(res, 0, 0);
  PQclear(res);
  return col;
}

bool PsqlDBConn::UpdateCell(const std::string &table, const std::string &row_id,
                            const std::string &column,
                            const std::string &new_value) {
  if (!Ok())
    return false;

  std::string query = "UPDATE " + escape_identifier(table) + " SET " +
                      escape_identifier(column) + " = $1 WHERE ctid = $2::tid";
  const char *values[] = {new_value.c_str(), row_id.c_str()};
  PGresult *res = PQexecParams(conn_, query.c_str(), 2, nullptr, values,
                               nullptr, nullptr, 0);
  bool ok = PQresultStatus(res) == PGRES_COMMAND_OK;
  PQclear(res);
  return ok;
}

void ShowDatabasesAndTables(const connection_params &params, std::ostream &out,
                            DBKind kind) {
  connection_params base = params;
  base.dbname = "template1";
  auto base_conn = DBConn::Make(kind, base);
  if (!base_conn || !base_conn->Ok())
    return;

  auto databases = base_conn->ListDatabases();
  out << "Databases (" << databases.size() << "):" << std::endl;
  for (const auto &db : databases) {
    out << "  " << db << std::endl;
  }
  out << std::endl;
  if (params.dbname.empty())
    return;

  auto db_conn = DBConn::Make(kind, params);
  if (!db_conn || !db_conn->Ok())
    return;
  auto tables = db_conn->ListTables();
  out << "Tables (" << params.dbname << ", " << tables.size()
      << "):" << std::endl;
  for (const auto &table : tables) {
    out << "  " << table << std::endl;
  }
}

} // namespace malla
