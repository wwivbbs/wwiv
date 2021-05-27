#ifndef INCLUDED_FOSSIL_H
#define INCLUDED_FOSSIL_H

#include "dostypes.h"

#define ENABLE_LOG
#define FOSSIL_BUFFER_SIZE 4000

class FossilOptions {
public:
  FossilOptions();
  ~FossilOptions();

  int comport;
  int node_number;
  int open_timeout_seconds;
};

bool enable_fossil(const FossilOptions* options);
bool disable_fossil();

#endif // INCLUDED_FOSSIL_H
