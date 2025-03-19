from sqlalchemy import Column, Integer, String, Float, ForeignKey, create_engine
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship, sessionmaker

Base = declarative_base()

class State(Base):
    __tablename__ = 'state'
    
    state_id = Column(Integer, primary_key=True)
    name = Column(String(256))
    
    teams = relationship("Team", back_populates="state")
    
    def __repr__(self):
        return f"<State(name={self.name})>"

class Color(Base):
    __tablename__ = 'color'
    
    color_id = Column(Integer, primary_key=True)
    name = Column(String(256))
    
    teams = relationship("Team", back_populates="color")
    
    def __repr__(self):
        return f"<Color(name={self.name})>"

class Team(Base):
    __tablename__ = 'team'
    
    team_id = Column(Integer, primary_key=True)
    name = Column(String(256))
    state_id = Column(Integer, ForeignKey('state.state_id', ondelete='SET NULL', onupdate='CASCADE'))
    color_id = Column(Integer, ForeignKey('color.color_id', ondelete='SET NULL', onupdate='CASCADE'))
    wins = Column(Integer)
    losses = Column(Integer)
    
    state = relationship("State", back_populates="teams")
    color = relationship("Color", back_populates="teams")
    players = relationship("Player", back_populates="team")
    
    def __repr__(self):
        return f"<Team(name={self.name}, wins={self.wins}, losses={self.losses})>"

class Player(Base):
    __tablename__ = 'player'
    
    player_id = Column(Integer, primary_key=True)
    team_id = Column(Integer, ForeignKey('team.team_id', ondelete='SET NULL', onupdate='CASCADE'))
    uniform_num = Column(Integer)
    first_name = Column(String(256))
    last_name = Column(String(256))
    mpg = Column(Integer)
    ppg = Column(Integer)
    rpg = Column(Integer)
    apg = Column(Integer)
    spg = Column(Float)
    bpg = Column(Float)
    
    team = relationship("Team", back_populates="players")
    
    def __repr__(self):
        return f"<Player(name={self.first_name} {self.last_name}, ppg={self.ppg})>"

# Database connection function
def get_db_session():
    engine = create_engine('postgresql://postgres:passw0rd@localhost/ACC_BBALL')
    Session = sessionmaker(bind=engine)
    return Session()
