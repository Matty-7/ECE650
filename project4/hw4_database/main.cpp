#include <iostream>
#include <pqxx/pqxx>
#include <fstream>
#include <sstream>
#include <string>

#include "exerciser.h"
#include "query_funcs.h"

using namespace std;
using namespace pqxx;

// Helper: Run a single SQL statement
static void runSQL(connection *C, const string &sql) {
  work txn(*C);
  txn.exec(sql);
  txn.commit();
}

// Drop table if exists
static void dropTable(connection *C, const string &tableName) {
  ostringstream oss;
  oss << "DROP TABLE IF EXISTS " << tableName << " CASCADE;";
  runSQL(C, oss.str());
}

// Create four tables
static void createTables(connection *C) {
  // STATE
  runSQL(C,
    "CREATE TABLE STATE("
    " STATE_ID SERIAL,"
    " NAME VARCHAR(256),"
    " PRIMARY KEY (STATE_ID)"
    ");"
  );
  // COLOR
  runSQL(C,
    "CREATE TABLE COLOR("
    " COLOR_ID SERIAL,"
    " NAME VARCHAR(256),"
    " PRIMARY KEY (COLOR_ID)"
    ");"
  );
  // TEAM
  runSQL(C,
    "CREATE TABLE TEAM("
    " TEAM_ID SERIAL,"
    " NAME VARCHAR(256),"
    " STATE_ID INT,"
    " COLOR_ID INT,"
    " WINS INT,"
    " LOSSES INT,"
    " PRIMARY KEY (TEAM_ID),"
    " FOREIGN KEY (STATE_ID) REFERENCES STATE(STATE_ID)"
    "   ON DELETE SET NULL ON UPDATE CASCADE,"
    " FOREIGN KEY (COLOR_ID) REFERENCES COLOR(COLOR_ID)"
    "   ON DELETE SET NULL ON UPDATE CASCADE"
    ");"
  );
  // PLAYER
  runSQL(C,
    "CREATE TABLE PLAYER("
    " PLAYER_ID SERIAL,"
    " TEAM_ID INT,"
    " UNIFORM_NUM INT,"
    " FIRST_NAME VARCHAR(256),"
    " LAST_NAME VARCHAR(256),"
    " MPG INT,"
    " PPG INT,"
    " RPG INT,"
    " APG INT,"
    " SPG DOUBLE PRECISION,"
    " BPG DOUBLE PRECISION,"
    " PRIMARY KEY (PLAYER_ID),"
    " FOREIGN KEY (TEAM_ID) REFERENCES TEAM(TEAM_ID)"
    "   ON DELETE SET NULL ON UPDATE CASCADE"
    ");"
  );
}

// Insert data from color.txt
static void loadColor(connection *C, const string &fname) {
  ifstream fin(fname);
  if (!fin) {
    cerr << "Unable to open " << fname << endl;
    return;
  }
  string line;
  while (getline(fin, line)) {
    // Format: color_id color_name
    // "1 LightBlue"
    stringstream ss(line);
    int cid; 
    string cname;
    ss >> cid >> cname;
    add_color(C, cname);
  }
  fin.close();
}

// Insert data from state.txt
static void loadState(connection *C, const string &fname) {
  ifstream fin(fname);
  if (!fin) {
    cerr << "Unable to open " << fname << endl;
    return;
  }
  string line;
  while (getline(fin, line)) {
    // Format: state_id state_name
    stringstream ss(line);
    int stid; 
    string stname;
    ss >> stid >> stname;
    add_state(C, stname);
  }
  fin.close();
}

// Insert data from team.txt
static void loadTeam(connection *C, const string &fname) {
  ifstream fin(fname);
  if (!fin) {
    cerr << "Unable to open " << fname << endl;
    return;
  }
  string line;
  while (getline(fin, line)) {
    // Format: team_id name state_id color_id wins losses
    stringstream ss(line);
    int tid, sid, cid, w, l;
    string tname;
    ss >> tid >> tname >> sid >> cid >> w >> l;
    add_team(C, tname, sid, cid, w, l);
  }
  fin.close();
}

// Insert data from player.txt
static void loadPlayer(connection *C, const string &fname) {
  ifstream fin(fname);
  if (!fin) {
    cerr << "Unable to open " << fname << endl;
    return;
  }
  string line;
  while (getline(fin, line)) {
    // Format: 
    // player_id team_id uniform_num first_name last_name mpg ppg rpg apg spg bpg
    stringstream ss(line);
    int pid, tid, unum, mpg, ppg, rpg, apg;
    double spg, bpg;
    string fname_, lname_;
    ss >> pid >> tid >> unum >> fname_ >> lname_
       >> mpg >> ppg >> rpg >> apg >> spg >> bpg;
    add_player(C, tid, unum, fname_, lname_, mpg, ppg, rpg, apg, spg, bpg);
  }
  fin.close();
}

int main(int argc, char *argv[]) {
  connection *C = nullptr;
  try {
    // Connect to ACC_BBALL
    C = new connection("dbname=ACC_BBALL user=postgres password=passw0rd");
    if (C->is_open()) {
      cout << "Opened database successfully: " << C->dbname() << endl;
    } else {
      cerr << "Can't open database ACC_BBALL" << endl;
      return 1;
    }

    // Drop existing tables if any
    dropTable(C, "PLAYER");
    dropTable(C, "TEAM");
    dropTable(C, "STATE");
    dropTable(C, "COLOR");

    // Create fresh tables
    createTables(C);

    // Load data from text files
    loadState(C,  "state.txt");
    loadColor(C,  "color.txt");
    loadTeam(C,   "team.txt");
    loadPlayer(C, "player.txt");

    // Run queries
    exercise(C);
    
    delete C;  // close the connection
    return 0;
  } catch (const std::exception &e) {
    cerr << e.what() << endl;
    if (C) {
      delete C;
    }
    return 1;
  }
}
