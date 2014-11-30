#ifndef __INCLUDED_NETWORKB_BINKP_COMMANDS_H__
#define  __INCLUDED_NETWORKB_BINKP_COMMANDS_H__
#pragma once

#include <string>

namespace wwiv {
namespace net {

class BinkpCommands {
public:
static const int M_NUL  = 0;
static const int M_ADR  = 1;
static const int M_PWD  = 2;
static const int M_FILE = 3;
static const int M_OK   = 4;
static const int M_EOB  = 5;
static const int M_GOT  = 6;
static const int M_ERR  = 7;
static const int M_BSY  = 8;
static const int M_GET  = 9;
static const int M_SKIP = 10;

static std::string command_id_to_name(int command_id);

private:
BinkpCommands() {}
~BinkpCommands() {}
};

}  // namespace net
} // namespace wwiv

#endif  // __INCLUDED_NETWORKB_BINKP_COMMANDS_H__