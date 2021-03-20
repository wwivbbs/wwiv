#include "bbs/qwk/qwk_struct.h"

#include <iostream>

int main(char** argv) {
  std::cout << "qwk_record=" << sizeof(wwiv::bbs::qwk::qwk_record) << std::endl;
  std::cout << "qwk_index=" << sizeof(wwiv::bbs::qwk::qwk_index) << std::endl;
}

