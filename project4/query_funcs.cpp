#include "query_funcs.h"
#include <iostream>
#include <iomanip>
#include <sstream>

using std::cout;
using std::endl;
using std::fixed;
using std::setprecision;
using std::stringstream;

// Add a player
void add_player(connection *C, int team_id, int jersey_num,
                string first_name, string last_name,
                int mpg, int ppg, int rpg, int apg,
                double spg, double bpg) {
  work W(*C);
  stringstream ss;
  ss << "INSERT INTO PLAYER (TEAM_ID, UNIFORM_NUM, FIRST_NAME, LAST_NAME, "
     << "MPG, PPG, RPG, APG, SPG, BPG) VALUES ("
     << team_id << ", " << jersey_num << ", "
     << W.quote(first_name) << ", " << W.quote(last_name) << ", "
     << mpg << ", " << ppg << ", " << rpg << ", " << apg << ", "
     << spg << ", " << bpg << ");";
  W.exec(ss.str());
  W.commit();
}

// Add a team
void add_team(connection *C, string name, int state_id,
              int color_id, int wins, int losses) {
  work W(*C);
  stringstream ss;
  ss << "INSERT INTO TEAM (NAME, STATE_ID, COLOR_ID, WINS, LOSSES) "
     << "VALUES (" << W.quote(name) << ", "
     << state_id << ", " << color_id << ", "
     << wins << ", " << losses << ");";
  W.exec(ss.str());
  W.commit();
}

// Add a state
void add_state(connection *C, string name) {
  work W(*C);
  string sql = "INSERT INTO STATE (NAME) VALUES (" + W.quote(name) + ");";
  W.exec(sql);
  W.commit();
}

// Add a color
void add_color(connection *C, string name) {
  work W(*C);
  string sql = "INSERT INTO COLOR (NAME) VALUES (" + W.quote(name) + ");";
  W.exec(sql);
  W.commit();
}

/*
  query1: Show all attributes of each player whose stats are in the given ranges.
  If use_ param is 0 => ignore that attribute. If use_ param is 1 => use the range.
*/
void query1(connection *C,
            int use_mpg, int min_mpg, int max_mpg,
            int use_ppg, int min_ppg, int max_ppg,
            int use_rpg, int min_rpg, int max_rpg,
            int use_apg, int min_apg, int max_apg,
            int use_spg, double min_spg, double max_spg,
            int use_bpg, double min_bpg, double max_bpg) {
  stringstream sql;
  sql << "SELECT * FROM PLAYER";

  bool first_clause = true;
  auto add_cond = [&](const string &cond) {
    if (first_clause) {
      sql << " WHERE " << cond;
      first_clause = false;
    } else {
      sql << " AND " << cond;
    }
  };

  if (use_mpg) add_cond("(MPG >= " + std::to_string(min_mpg) + " AND MPG <= " + std::to_string(max_mpg) + ")");
  if (use_ppg) add_cond("(PPG >= " + std::to_string(min_ppg) + " AND PPG <= " + std::to_string(max_ppg) + ")");
  if (use_rpg) add_cond("(RPG >= " + std::to_string(min_rpg) + " AND RPG <= " + std::to_string(max_rpg) + ")");
  if (use_apg) add_cond("(APG >= " + std::to_string(min_apg) + " AND APG <= " + std::to_string(max_apg) + ")");
  if (use_spg) {
    std::ostringstream spg_ss;
    spg_ss << "(SPG >= " << min_spg << " AND SPG <= " << max_spg << ")";
    add_cond(spg_ss.str());
  }
  if (use_bpg) {
    std::ostringstream bpg_ss;
    bpg_ss << "(BPG >= " << min_bpg << " AND BPG <= " << max_bpg << ")";
    add_cond(bpg_ss.str());
  }

  sql << ";";

  nontransaction N(*C);
  result R(N.exec(sql.str()));

  cout << "PLAYER_ID TEAM_ID UNIFORM_NUM FIRST_NAME LAST_NAME MPG PPG RPG APG SPG BPG\n";
  for (auto row : R) {
    cout << row[0].as<int>() << " "
         << row[1].as<int>() << " "
         << row[2].as<int>() << " "
         << row[3].as<string>() << " "
         << row[4].as<string>() << " "
         << row[5].as<int>() << " "
         << row[6].as<int>() << " "
         << row[7].as<int>() << " "
         << row[8].as<int>() << " "
         << fixed << setprecision(1) << row[9].as<double>() << " "
         << row[10].as<double>() << endl;
  }
}

// query2: Show the name of each team with the given color.
void query2(connection *C, string team_color) {
  work W(*C);
  stringstream ss;
  ss << "SELECT T.NAME FROM TEAM T JOIN COLOR C ON (T.COLOR_ID = C.COLOR_ID) "
     << "WHERE C.NAME = " << W.quote(team_color) << ";";
  W.commit();

  nontransaction N(*C);
  result R(N.exec(ss.str()));

  cout << "NAME\n";
  for (auto row : R) {
    cout << row[0].as<string>() << endl;
  }
}

// query3: Show the first/last name of each player on the given team, sorted by PPG desc.
void query3(connection *C, string team_name) {
  work W(*C);
  stringstream ss;
  ss << "SELECT P.FIRST_NAME, P.LAST_NAME "
     << "FROM PLAYER P JOIN TEAM T ON (P.TEAM_ID = T.TEAM_ID) "
     << "WHERE T.NAME = " << W.quote(team_name)
     << " ORDER BY P.PPG DESC;";
  W.commit();

  nontransaction N(*C);
  result R(N.exec(ss.str()));

  cout << "FIRST_NAME LAST_NAME\n";
  for (auto row : R) {
    cout << row[0].as<string>() << " " << row[1].as<string>() << endl;
  }
}

// query4: Show uniform#, first/last name of players who play in a given state and wear a given color.
void query4(connection *C, string team_state, string team_color) {
  work W(*C);
  stringstream ss;
  ss << "SELECT P.UNIFORM_NUM, P.FIRST_NAME, P.LAST_NAME "
     << "FROM PLAYER P "
     << "JOIN TEAM T ON P.TEAM_ID = T.TEAM_ID "
     << "JOIN STATE S ON T.STATE_ID = S.STATE_ID "
     << "JOIN COLOR C ON T.COLOR_ID = C.COLOR_ID "
     << "WHERE S.NAME = " << W.quote(team_state)
     << " AND C.NAME = " << W.quote(team_color) << ";";
  W.commit();

  nontransaction N(*C);
  result R(N.exec(ss.str()));

  cout << "UNIFORM_NUM FIRST_NAME LAST_NAME\n";
  for (auto row : R) {
    cout << row[0].as<int>() << " "
         << row[1].as<string>() << " "
         << row[2].as<string>() << endl;
  }
}

// query5: Show first/last name, team name, and wins for teams with wins > num_wins.
void query5(connection *C, int num_wins) {
  work W(*C);
  stringstream ss;
  ss << "SELECT P.FIRST_NAME, P.LAST_NAME, T.NAME, T.WINS "
     << "FROM PLAYER P JOIN TEAM T ON (P.TEAM_ID = T.TEAM_ID) "
     << "WHERE T.WINS > " << num_wins << ";";
  W.commit();

  nontransaction N(*C);
  result R(N.exec(ss.str()));

  cout << "FIRST_NAME LAST_NAME NAME WINS\n";
  for (auto row : R) {
    cout << row[0].as<string>() << " "
         << row[1].as<string>() << " "
         << row[2].as<string>() << " "
         << row[3].as<int>() << endl;
  }
}
