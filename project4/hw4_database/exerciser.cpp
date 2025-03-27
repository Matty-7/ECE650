#include "exerciser.h"
#include "query_funcs.h"

void exercise(connection *C)
{
  // Query1: 
  // test players with RPG between 6-10
  query1(C, 0, 0, 0, 0, 0, 0, 1, 6, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  // test players with APG > 5 and SPG > 1.5
  query1(C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 5, 10, 1, 1.5, 3.0, 0, 0, 0);
  // test players with PPG>18, RPG>8
  query1(C, 0, 0, 0, 1, 18, 25, 1, 8, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // Query2: 
  query2(C, "Red");
  query2(C, "DarkBlue");
  query2(C, "Maroon");

  // Query3: 
  query3(C, "Miami");
  query3(C, "Virginia");
  query3(C, "Louisville");

  // Query4: 
  query4(C, "FL", "Orange");   
  query4(C, "VA", "Blue");     
  query4(C, "NC", "Red");      

  // Query5: 
  // teams with more than 8 wins
  query5(C, 8);   
  // teams with more than 12 wins
  query5(C, 12);  
}
