#pragma once

#include "value.h"

#include <string_view>

namespace wwivbasic {
class Module;
}

namespace wwivbasic::stdlib {

// Registers the my_basic-compatible collection functions (LIST, PUSH, POP,
// BACK, INSERT, REMOVE, CLEAR, EXISTS, INDEX_OF, GET, SET, CLONE, SORT,
// ITERATOR, MOVE_NEXT) in the given module. Indices are 0-based.
void register_collections(Module& m);

// Returns item `index` of `list`. Throws BasicError, naming `fn`, if `list`
// is not a list or the index is out of range. Used by GET and by l(i).
Value list_get(const Value& list, const Value& index, std::string_view fn);

} // namespace wwivbasic::stdlib
