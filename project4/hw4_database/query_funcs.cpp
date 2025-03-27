#include "query_funcs.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <pqxx/pqxx>

// Helper template for update queries
template<typename BuildQueryFunc>
void execute_update(connection *c, BuildQueryFunc build_query) {
  work w(*c);
  std::string sql = build_query(w);
  w.exec(sql);
  w.commit();
}

// Helper template for select queries
template<typename BuildQueryFunc>
pqxx::result execute_select(connection *c, BuildQueryFunc build_query) {
  work w(*c);
  std::string sql = build_query(w);
  w.commit();
  pqxx::nontransaction n(*c);
  return n.exec(sql);
}

void add_player(connection *c, int team_id, int jersey_num,
                std::string first_name, std::string last_name,
                int mpg, int ppg, int rpg, int apg,
                double spg, double bpg) {
  execute_update(c, [&](work &w) -> std::string {
    std::stringstream ss;
    ss << "INSERT INTO PLAYER (TEAM_ID, UNIFORM_NUM, FIRST_NAME, LAST_NAME, "
       << "MPG, PPG, RPG, APG, SPG, BPG) VALUES ("
       << team_id << ", " << jersey_num << ", " << w.quote(first_name) << ", " 
       << w.quote(last_name) << ", " << mpg << ", " << ppg << ", " 
       << rpg << ", " << apg << ", " << spg << ", " << bpg << ");";
    return ss.str();
  });
}

void add_team(connection *c, std::string name, int state_id,
              int color_id, int wins, int losses) {
  execute_update(c, [&](work &w) -> std::string {
    std::stringstream ss;
    ss << "INSERT INTO TEAM (NAME, STATE_ID, COLOR_ID, WINS, LOSSES) VALUES ("
       << w.quote(name) << ", " << state_id << ", " << color_id << ", " 
       << wins << ", " << losses << ");";
    return ss.str();
  });
}

void add_state(connection *c, std::string name) {
  execute_update(c, [&](work &w) -> std::string {
    return "INSERT INTO STATE (NAME) VALUES (" + w.quote(name) + ");";
  });
}

void add_color(connection *c, std::string name) {
  execute_update(c, [&](work &w) -> std::string {
    return "INSERT INTO COLOR (NAME) VALUES (" + w.quote(name) + ");";
  });
}

void query1(connection *c,
            int use_mpg, int min_mpg, int max_mpg,
            int use_ppg, int min_ppg, int max_ppg,
            int use_rpg, int min_rpg, int max_rpg,
            int use_apg, int min_apg, int max_apg,
            int use_spg, double min_spg, double max_spg,
            int use_bpg, double min_bpg, double max_bpg) {
  pqxx::result r = execute_select(c, [&](work &w) -> std::string {
    std::stringstream ss;
    ss << "SELECT * FROM PLAYER";
    bool first_clause = true;
    auto add_cond = [&](const std::string &cond) {
      if (first_clause) { 
        ss << " WHERE " << cond; 
        first_clause = false; 
      } else { 
        ss << " AND " << cond; 
      }
    };
    if (use_mpg) add_cond("MPG >= " + std::to_string(min_mpg) + " AND MPG <= " + std::to_string(max_mpg));
    if (use_ppg) add_cond("PPG >= " + std::to_string(min_ppg) + " AND PPG <= " + std::to_string(max_ppg));
    if (use_rpg) add_cond("RPG >= " + std::to_string(min_rpg) + " AND RPG <= " + std::to_string(max_rpg));
    if (use_apg) add_cond("APG >= " + std::to_string(min_apg) + " AND APG <= " + std::to_string(max_apg));
    if (use_spg) {
      std::ostringstream spg_ss;
      spg_ss << "SPG >= " << min_spg << " AND SPG <= " << max_spg;
      add_cond(spg_ss.str());
    }
    if (use_bpg) {
      std::ostringstream bpg_ss;
      bpg_ss << "BPG >= " << min_bpg << " AND BPG <= " << max_bpg;
      add_cond(bpg_ss.str());
    }
    ss << ";";
    return ss.str();
  });
  
  std::cout << "PLAYER_ID TEAM_ID UNIFORM_NUM FIRST_NAME LAST_NAME MPG PPG RPG APG SPG BPG\n";
  for (auto row : r) {
    std::cout << row[0].as<int>() << " " << row[1].as<int>() << " " << row[2].as<int>() << " "
              << row[3].as<std::string>() << " " << row[4].as<std::string>() << " " 
              << row[5].as<int>() << " " << row[6].as<int>() << " " << row[7].as<int>() << " "
              << row[8].as<int>() << " " << std::fixed << std::setprecision(1) 
              << row[9].as<double>() << " " << row[10].as<double>() << "\n";
  }
}

void query2(connection *c, std::string team_color) {
  pqxx::result r = execute_select(c, [&](work &w) -> std::string {
    std::stringstream ss;
    ss << "SELECT T.NAME FROM TEAM T JOIN COLOR C ON (T.COLOR_ID = C.COLOR_ID) "
       << "WHERE C.NAME = " << w.quote(team_color) << ";";
    return ss.str();
  });
  
  std::cout << "NAME\n";
  for (auto row : r)
    std::cout << row[0].as<std::string>() << "\n";
}

void query3(connection *c, std::string team_name) {
  pqxx::result r = execute_select(c, [&](work &w) -> std::string {
    std::stringstream ss;
    ss << "SELECT P.FIRST_NAME, P.LAST_NAME FROM PLAYER P JOIN TEAM T ON (P.TEAM_ID = T.TEAM_ID) "
       << "WHERE T.NAME = " << w.quote(team_name) << " ORDER BY P.PPG DESC;";
    return ss.str();
  });
  
  std::cout << "FIRST_NAME LAST_NAME\n";
  for (auto row : r)
    std::cout << row[0].as<std::string>() << " " << row[1].as<std::string>() << "\n";
}

void query4(connection *c, std::string team_state, std::string team_color) {
  pqxx::result r = execute_select(c, [&](work &w) -> std::string {
    std::stringstream ss;
    ss << "SELECT P.UNIFORM_NUM, P.FIRST_NAME, P.LAST_NAME FROM PLAYER P "
       << "JOIN TEAM T ON P.TEAM_ID = T.TEAM_ID "
       << "JOIN STATE S ON T.STATE_ID = S.STATE_ID "
       << "JOIN COLOR C ON T.COLOR_ID = C.COLOR_ID "
       << "WHERE S.NAME = " << w.quote(team_state)
       << " AND C.NAME = " << w.quote(team_color) << ";";
    return ss.str();
  });
  
  std::cout << "UNIFORM_NUM FIRST_NAME LAST_NAME\n";
  for (auto row : r)
    std::cout << row[0].as<int>() << " " << row[1].as<std::string>() << " " 
              << row[2].as<std::string>() << "\n";
}

void query5(connection *c, int num_wins) {
  pqxx::result r = execute_select(c, [&](work &w) -> std::string {
    std::stringstream ss;
    ss << "SELECT P.FIRST_NAME, P.LAST_NAME, T.NAME, T.WINS FROM PLAYER P JOIN TEAM T ON (P.TEAM_ID = T.TEAM_ID) "
       << "WHERE T.WINS > " << num_wins << ";";
    return ss.str();
  });
  
  std::cout << "FIRST_NAME LAST_NAME NAME WINS\n";
  for (auto row : r)
    std::cout << row[0].as<std::string>() << " " << row[1].as<std::string>() << " " 
              << row[2].as<std::string>() << " " << row[3].as<int>() << "\n";
}
