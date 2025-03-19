from sqlalchemy import and_, or_
from models import Player, Team, State, Color, get_db_session

def query1(use_mpg=0, min_mpg=0, max_mpg=0,
           use_ppg=0, min_ppg=0, max_ppg=0,
           use_rpg=0, min_rpg=0, max_rpg=0,
           use_apg=0, min_apg=0, max_apg=0,
           use_spg=0, min_spg=0.0, max_spg=0.0,
           use_bpg=0, min_bpg=0.0, max_bpg=0.0):
    """
    Show all attributes of each player with average statistics that fall between
    the min and max for each enabled statistic
    """
    session = get_db_session()
    query = session.query(Player)
    
    # Build query with filters
    filters = []
    if use_mpg:
        filters.append(and_(Player.mpg >= min_mpg, Player.mpg <= max_mpg))
    if use_ppg:
        filters.append(and_(Player.ppg >= min_ppg, Player.ppg <= max_ppg))
    if use_rpg:
        filters.append(and_(Player.rpg >= min_rpg, Player.rpg <= max_rpg))
    if use_apg:
        filters.append(and_(Player.apg >= min_apg, Player.apg <= max_apg))
    if use_spg:
        filters.append(and_(Player.spg >= min_spg, Player.spg <= max_spg))
    if use_bpg:
        filters.append(and_(Player.bpg >= min_bpg, Player.bpg <= max_bpg))
    
    if filters:
        query = query.filter(*filters)
    
    # Execute query and print results
    players = query.all()
    print("PLAYER_ID TEAM_ID UNIFORM_NUM FIRST_NAME LAST_NAME MPG PPG RPG APG SPG BPG")
    for p in players:
        print(f"{p.player_id} {p.team_id} {p.uniform_num} {p.first_name} {p.last_name} "
              f"{p.mpg} {p.ppg} {p.rpg} {p.apg} {p.spg:.1f} {p.bpg:.1f}")
    
    session.close()
    return players

def query2(team_color):
    """Show the name of each team with the indicated uniform color"""
    session = get_db_session()
    
    teams = session.query(Team.name)\
        .join(Color, Team.color_id == Color.color_id)\
        .filter(Color.name == team_color)\
        .all()
    
    print("NAME")
    for team in teams:
        print(team.name)
    
    session.close()
    return teams

def query3(team_name):
    """
    Show the first and last name of each player that plays for the indicated team,
    ordered from highest to lowest ppg
    """
    session = get_db_session()
    
    players = session.query(Player.first_name, Player.last_name)\
        .join(Team, Player.team_id == Team.team_id)\
        .filter(Team.name == team_name)\
        .order_by(Player.ppg.desc())\
        .all()
    
    print("FIRST_NAME LAST_NAME")
    for player in players:
        print(f"{player.first_name} {player.last_name}")
    
    session.close()
    return players

def query4(team_state, team_color):
    """
    Show uniform number, first name and last name of each player that plays in
    the indicated state and wears the indicated uniform color
    """
    session = get_db_session()
    
    players = session.query(Player.uniform_num, Player.first_name, Player.last_name)\
        .join(Team, Player.team_id == Team.team_id)\
        .join(State, Team.state_id == State.state_id)\
        .join(Color, Team.color_id == Color.color_id)\
        .filter(State.name == team_state, Color.name == team_color)\
        .all()
    
    print("UNIFORM_NUM FIRST_NAME LAST_NAME")
    for player in players:
        print(f"{player.uniform_num} {player.first_name} {player.last_name}")
    
    session.close()
    return players

def query5(num_wins):
    """
    Show first name and last name of each player, and team name and number of wins
    for each team that has won more than the indicated number of games
    """
    session = get_db_session()
    
    results = session.query(Player.first_name, Player.last_name, Team.name, Team.wins)\
        .join(Team, Player.team_id == Team.team_id)\
        .filter(Team.wins > num_wins)\
        .all()
    
    print("FIRST_NAME LAST_NAME NAME WINS")
    for result in results:
        print(f"{result.first_name} {result.last_name} {result.name} {result.wins}")
    
    session.close()
    return results
