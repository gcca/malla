#include "malla.hpp"
#include "components/columns_dropdown.hpp"
#include "components/data_table.hpp"
#include "components/searchable_dropdown.hpp"
#include "components/where_input.hpp"
#include <boost/program_options.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <iostream>
#include <sstream>
#include <string>

namespace malla {

po_options ParseOptions(int argc, char *argv[]) {
  namespace po = boost::program_options;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "produce help message")(
      "kind,k", po::value<std::string>()->default_value("psql"),
      "database kind (psql)")(
      "host,H", po::value<std::string>()->default_value("localhost"),
      "host")("user,U", po::value<std::string>()->default_value(""), "user")(
      "password,p", po::value<std::string>()->default_value(""), "password")(
      "port,P", po::value<unsigned>()->default_value(5432u), "port")(
      "name,N", po::value<std::string>()->default_value(""), "database name")(
      "table,T", po::value<std::string>()->default_value(""), "initial table");

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);

  po_options opts;
  opts.help = vm.count("help") != 0;
  std::ostringstream oss;
  oss << desc;
  opts.help_text = oss.str();
  opts.host = vm["host"].as<std::string>();
  opts.port = vm["port"].as<unsigned>();
  opts.dbname = vm["name"].as<std::string>();
  opts.table = vm["table"].as<std::string>();
  opts.user = vm["user"].as<std::string>();
  opts.password = vm["password"].as<std::string>();
  std::string kind_str = vm["kind"].as<std::string>();
  if (kind_str == "psql")
    opts.dbkind = DBKind::PSQL;
  return opts;
}

} // namespace malla

int main(int argc, char *argv[]) {
  auto opts = malla::ParseOptions(argc, argv);
  if (opts.help) {
    std::cout << opts.help_text << std::endl;
    return 1;
  }

  malla::connection_params params;
  params.host = opts.host;
  params.port = opts.port;
  params.dbname = opts.dbname;
  params.user = opts.user;
  params.password = opts.password;

  malla::connection_params base = params;
  base.dbname = "template1";
  auto conn = malla::DBConn::Make(opts.dbkind, base);
  if (!conn || !conn->Ok()) {
    std::cerr << "Connection failed" << std::endl;
    return 1;
  }

  auto databases = conn->ListDatabases();
  if (databases.empty()) {
    databases.push_back("(no databases)");
  }

  int selected_db = 0;
  int selected_table = 0;
  std::vector<std::string> tables;
  std::vector<std::pair<std::string, std::string>> columns;
  std::vector<bool> selected_columns;
  std::vector<std::string> table_headers;
  std::vector<std::vector<std::string>> table_rows;
  std::vector<std::string> row_ids;
  std::string pk_col;
  std::vector<std::string> pk_values;
  std::string current_table;
  std::string where_text;

  auto load_columns = [&] {
    columns.clear();
    selected_columns.clear();
    pk_col.clear();
    if (selected_table >= 0 && selected_table < int(tables.size())) {
      std::string table = tables[selected_table];
      if (table != "(no tables)") {
        malla::connection_params db_params = params;
        db_params.dbname = databases[selected_db];
        auto db_conn = malla::DBConn::Make(opts.dbkind, db_params);
        if (db_conn && db_conn->Ok()) {
          columns = db_conn->ListSchema(table);
          selected_columns.resize(columns.size(), true);
          pk_col = db_conn->GetPrimaryKeyColumn(table);
        }
      }
    }
  };

  auto load_tables = [&] {
    malla::connection_params db_params = params;
    db_params.dbname = databases[selected_db];
    auto db_conn = malla::DBConn::Make(opts.dbkind, db_params);
    tables.clear();
    if (db_conn && db_conn->Ok()) {
      tables = db_conn->ListTables();
    }
    if (tables.empty()) {
      tables.push_back("(no tables)");
    }
    selected_table = 0;
    load_columns();
  };

  auto load_rows = [&] {
    table_headers.clear();
    table_rows.clear();
    for (size_t i = 0; i < columns.size() && i < selected_columns.size(); ++i) {
      if (selected_columns[i])
        table_headers.push_back(columns[i].first);
    }
    if (table_headers.empty())
      return;
    if (selected_table < 0 || selected_table >= int(tables.size()))
      return;
    std::string table = tables[selected_table];
    if (table == "(no tables)")
      return;
    malla::connection_params db_params = params;
    db_params.dbname = databases[selected_db];
    auto db_conn = malla::DBConn::Make(opts.dbkind, db_params);
    if (db_conn && db_conn->Ok()) {
      table_rows =
          db_conn->ListFilteredRows(table, table_headers, 25, where_text);
      row_ids = db_conn->ListRowIds(table, 25, where_text);
      if (!pk_col.empty()) {
        auto pk_rows = db_conn->ListFilteredRows(table, {pk_col}, 25);
        pk_values.clear();
        for (const auto &r : pk_rows)
          pk_values.push_back(r.empty() ? "" : r[0]);
      }
      current_table = table;
    }
  };

  if (!opts.dbname.empty()) {
    for (int i = 0; i < int(databases.size()); ++i) {
      if (databases[i] == opts.dbname) {
        selected_db = i;
        break;
      }
    }
  }

  load_tables();

  if (!opts.table.empty()) {
    for (int i = 0; i < int(tables.size()); ++i) {
      if (tables[i] == opts.table) {
        selected_table = i;
        load_columns();
        break;
      }
    }
  }

  load_rows();

  int last_selected_db = selected_db;
  int last_selected_table = selected_table;
  auto db_dropdown = malla::SearchableDropdown(&databases, &selected_db);
  auto table_dropdown = malla::SearchableDropdown(&tables, &selected_table);
  auto columns_dropdown = malla::ColumnsDropdown(&columns, &selected_columns);
  auto where_input = malla::WhereInput(&where_text, [&] { load_rows(); });

  auto on_update = [&](const std::string &tbl, const std::string &rid,
                       const std::string &col, const std::string &val) -> bool {
    malla::connection_params db_params = params;
    db_params.dbname = databases[selected_db];
    auto uc = malla::DBConn::Make(opts.dbkind, db_params);
    if (!uc || !uc->Ok())
      return false;
    bool ok = uc->UpdateCell(tbl, rid, col, val);
    if (ok)
      load_rows();
    return ok;
  };

  auto content = malla::DataTable(&table_headers, &table_rows, &current_table,
                                  &row_ids, &pk_col, &pk_values, on_update);

  auto header = ftxui::Container::Horizontal(
      {db_dropdown, table_dropdown, columns_dropdown, where_input});
  auto header_fixed = ftxui::Renderer(
      header, [&db_dropdown, &table_dropdown, &columns_dropdown, &where_input] {
        return ftxui::hbox({
            ftxui::hbox({
                db_dropdown->Render() | ftxui::flex,
                table_dropdown->Render() | ftxui::flex,
                columns_dropdown->Render() | ftxui::flex,
            }) | ftxui::flex,
            where_input->Render() | ftxui::flex,
        });
      });
  auto main_layout = ftxui::Container::Vertical({header_fixed, content});
  auto layout =
      main_layout | ftxui::CatchEvent([&](ftxui::Event event) {
        if (selected_db != last_selected_db) {
          last_selected_db = selected_db;
          load_tables();
          load_rows();
        } else if (selected_table != last_selected_table) {
          last_selected_table = selected_table;
          load_columns();
          load_rows();
        } else {
          std::vector<std::string> current_headers;
          for (size_t i = 0; i < columns.size() && i < selected_columns.size();
               ++i) {
            if (selected_columns[i])
              current_headers.push_back(columns[i].first);
          }
          if (current_headers != table_headers) {
            load_rows();
          }
        }
        return false;
      });

  auto screen = ftxui::ScreenInteractive::FitComponent();
  layout |= ftxui::CatchEvent([&screen](ftxui::Event event) {
    if (event == ftxui::Event::Character('q') ||
        event == ftxui::Event::Character('Q')) {
      screen.Exit();
      return true;
    }
    return false;
  });

  struct TabToggle : ftxui::ComponentBase {
    ftxui::Component db_;
    ftxui::Component table_;
    ftxui::Component cols_;
    ftxui::Component where_;
    ftxui::Component content_;
    ftxui::Component last_header_;
    TabToggle(ftxui::Component child, ftxui::Component db,
              ftxui::Component table, ftxui::Component cols,
              ftxui::Component where, ftxui::Component content)
        : db_(db), table_(table), cols_(cols), where_(where), content_(content),
          last_header_(db) {
      Add(std::move(child));
    }
    bool OnEvent(ftxui::Event event) override {
      if (db_->Focused())
        last_header_ = db_;
      else if (table_->Focused())
        last_header_ = table_;
      else if (cols_->Focused())
        last_header_ = cols_;
      else if (where_->Focused())
        last_header_ = where_;
      if (event == ftxui::Event::Tab || event == ftxui::Event::TabReverse) {
        if (content_->Focused())
          last_header_->TakeFocus();
        else
          content_->TakeFocus();
        return true;
      }
      return ftxui::ComponentBase::OnEvent(event);
    }
  };
  auto root = ftxui::Make<TabToggle>(layout, db_dropdown, table_dropdown,
                                     columns_dropdown, where_input, content);

  std::cout << "\033[2J\033[H" << std::flush;
  screen.Loop(root);
  return 0;
}
