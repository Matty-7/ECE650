#ifndef _EXERCISER_
#define _EXERCISER_

#include <pqxx/pqxx>

using namespace pqxx;

// Called after all tables are created and data loaded.
void exercise(connection *C);

#endif //_EXERCISER_
