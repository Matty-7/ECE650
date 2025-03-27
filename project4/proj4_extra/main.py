from models import get_db_session
from orm_queries import query1, query2, query3, query4, query5

def main():
    print("SQLAlchemy ORM implementation for Project 4 Extra Credit")
    
    session = get_db_session()
    
    try:
        # Query1: 
        print("\n--- Testing Query 1: Players with RPG between 6-10 ---")
        query1(use_rpg=1, min_rpg=6, max_rpg=10)
        
        print("\n--- Testing Query 1: Players with APG > 5 and SPG > 1.5 ---")
        query1(use_apg=1, min_apg=5, max_apg=10, use_spg=1, min_spg=1.5, max_spg=3.0)
        
        print("\n--- Testing Query 1: Players with PPG>18, RPG>8 ---")
        query1(use_ppg=1, min_ppg=18, max_ppg=25, use_rpg=1, min_rpg=8, max_rpg=15)
        
        # Query2: 
        print("\n--- Testing Query 2: Teams with Red color ---")
        query2("Red")
        
        print("\n--- Testing Query 2: Teams with DarkBlue color ---")
        query2("DarkBlue")
        
        print("\n--- Testing Query 2: Teams with Maroon color ---")
        query2("Maroon")
        
        # Query3: 
        print("\n--- Testing Query 3: Players from Miami, ordered by PPG ---")
        query3("Miami")
        
        print("\n--- Testing Query 3: Players from Virginia, ordered by PPG ---")
        query3("Virginia")
        
        print("\n--- Testing Query 3: Players from Louisville, ordered by PPG ---")
        query3("Louisville")
        
        # Query4: 
        print("\n--- Testing Query 4: Players from FL with Orange color ---")
        query4("FL", "Orange")
        
        print("\n--- Testing Query 4: Players from VA with Blue color ---")
        query4("VA", "Blue")
        
        print("\n--- Testing Query 4: Players from NC with Red color ---")
        query4("NC", "Red")
        
        # Query5: 
        print("\n--- Testing Query 5: Players from teams with more than 8 wins ---")
        query5(8)
        
        print("\n--- Testing Query 5: Players from teams with more than 12 wins ---")
        query5(12)
        
        print("\nAll queries completed successfully!")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        session.close()

if __name__ == "__main__":
    main()
