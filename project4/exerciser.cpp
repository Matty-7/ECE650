#include "exerciser.h"
#include "query_funcs.h"

// This function calls queries for testing.
// You can modify or add more calls as needed.
void exercise(connection *C)
{
  // Query1 examples:
  query1(C, 1, 35, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  query1(C, 1, 35, 40, 1, 15, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  // Query2 examples:
  query2(C, "Green");
  query2(C, "Orange");

  // Query3 examples:
  query3(C, "Duke");
  query3(C, "UNC");

  // Query4 examples:
  query4(C, "MA", "Maroon");
  query4(C, "NC", "DarkBlue");

  // Query5 examples:
  query5(C, 13);
  query5(C, 10);

  // You may also test add_player, add_team, etc. here if desired.
}
