#ifndef INCLUDED_FOSSIL_H
#define INCLUDED_FOSSIL_H

#define ENABLE_LOG
#define FOSSIL_BUFFER_SIZE 4000

void enable_fossil(int nodenum, int comport);
void disable_fossil();

#endif // INCLUDED_FOSSIL_H
