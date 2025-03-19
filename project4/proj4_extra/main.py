from models import get_db_session
from orm_queries import query1, query2, query3, query4, query5

def main():
    print("SQLAlchemy ORM implementation for Project 4 Extra Credit")
    
    session = get_db_session()
    
    try:
        # Test the queries
        print("\n--- Testing Query 1: Players with MPG between 35 and 40 ---")
        query1(use_mpg=1, min_mpg=35, max_mpg=40)
        
        print("\n--- Testing Query 2: Teams with Green color ---")
        query2("Green")
        
        print("\n--- Testing Query 3: Players from Duke, ordered by PPG ---")
        query3("Duke")
        
        print("\n--- Testing Query 4: Players from NC with DarkBlue color ---")
        query4("NC", "DarkBlue")
        
        print("\n--- Testing Query 5: Players from teams with more than 10 wins ---")
        query5(10)
        
        print("\nAll queries completed successfully!")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        session.close()

if __name__ == "__main__":
    main()
