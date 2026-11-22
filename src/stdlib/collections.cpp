#include "stdlib/collections.h"

#include "context.h"
#include "wwivbasic/value.h"
#include "fmt/format.h"

#include <algorithm>
#include <string>
#include <vector>

namespace wwivbasic::stdlib {

namespace {

using Args = std::vector<Value>;

void check_args(const char* fn, const Args& args, size_t min, size_t max) {
  if (args.size() < min || args.size() > max) {
    if (min == max) {
      throw BasicError(fmt::format("{}: expected {} argument(s), got {}", fn, min, args.size()));
    }
    throw BasicError(
        fmt::format("{}: expected {} to {} arguments, got {}", fn, min, max, args.size()));
  }
}

// Returns the index as a size_t, or throws if it is out of range for a list
// of size `size`. `allow_end` permits index == size (for INSERT).
size_t checked_index(std::string_view fn, const Value& v, size_t size, bool allow_end = false) {
  const auto i = v.toInt();
  const auto limit = allow_end ? size + 1 : size;
  if (i < 0 || static_cast<size_t>(i) >= limit) {
    throw BasicError(fmt::format("{}: index {} is out of range (list has {} items)", fn, i, size));
  }
  return static_cast<size_t>(i);
}

constexpr size_t kMany = static_cast<size_t>(-1);

} // namespace

Value list_get(const Value& list, const Value& index, std::string_view fn) {
  const auto& items = list.list(fn)->items;
  return items[checked_index(fn, index, items.size())];
}

void register_collections(Module& m) {
  // LIST(a, b, ...) creates a new list holding its arguments.
  m.native_functionl("LIST", [](const Args& args) {
    auto l = Value::new_list();
    l.list("LIST")->items = args;
    return l;
  });

  // PUSH(l, v, ...) appends to the list and returns it.
  m.native_functionl("PUSH", [](const Args& args) {
    check_args("PUSH", args, 2, kMany);
    auto list = args[0].list("PUSH");
    list->items.insert(list->items.end(), args.begin() + 1, args.end());
    return args[0];
  });

  // POP(l) removes and returns the last item.
  m.native_functionl("POP", [](const Args& args) {
    check_args("POP", args, 1, 1);
    auto list = args[0].list("POP");
    if (list->items.empty()) {
      throw BasicError("POP: the list is empty");
    }
    auto v = list->items.back();
    list->items.pop_back();
    return v;
  });

  // BACK(l) returns the last item without removing it.
  m.native_functionl("BACK", [](const Args& args) {
    check_args("BACK", args, 1, 1);
    auto list = args[0].list("BACK");
    if (list->items.empty()) {
      throw BasicError("BACK: the list is empty");
    }
    return list->items.back();
  });

  // INSERT(l, index, v) inserts before index and returns the list.
  m.native_functionl("INSERT", [](const Args& args) {
    check_args("INSERT", args, 3, 3);
    auto list = args[0].list("INSERT");
    const auto i = checked_index("INSERT", args[1], list->items.size(), true);
    list->items.insert(list->items.begin() + static_cast<std::ptrdiff_t>(i), args[2]);
    return args[0];
  });

  // REMOVE(l, index) removes the item at index and returns the list.
  m.native_functionl("REMOVE", [](const Args& args) {
    check_args("REMOVE", args, 2, 2);
    auto list = args[0].list("REMOVE");
    const auto i = checked_index("REMOVE", args[1], list->items.size());
    list->items.erase(list->items.begin() + static_cast<std::ptrdiff_t>(i));
    return args[0];
  });

  // CLEAR(l) removes every item and returns the list.
  m.native_functionl("CLEAR", [](const Args& args) {
    check_args("CLEAR", args, 1, 1);
    args[0].list("CLEAR")->items.clear();
    return args[0];
  });

  // EXISTS(l, v) is TRUE if v is in the list.
  m.native_functionl("EXISTS", [](const Args& args) {
    check_args("EXISTS", args, 2, 2);
    const auto& items = args[0].list("EXISTS")->items;
    return Value(std::find(items.begin(), items.end(), args[1]) != items.end());
  });

  // INDEX_OF(l, v) returns the index of v, or -1 if it is not in the list.
  m.native_functionl("INDEX_OF", [](const Args& args) {
    check_args("INDEX_OF", args, 2, 2);
    const auto& items = args[0].list("INDEX_OF")->items;
    const auto it = std::find(items.begin(), items.end(), args[1]);
    return Value(it == items.end() ? -1 : static_cast<int>(it - items.begin()));
  });

  // GET(l, index) returns an item; GET(iterator) returns the current item.
  m.native_functionl("GET", [](const Args& args) {
    check_args("GET", args, 1, 2);
    if (args.size() == 1) {
      auto it = args[0].iterator("GET");
      const auto& items = it->list->items;
      if (it->pos < 0 || static_cast<size_t>(it->pos) >= items.size()) {
        throw BasicError("GET: the iterator is not on an item (call MOVE_NEXT first)");
      }
      return items[static_cast<size_t>(it->pos)];
    }
    return list_get(args[0], args[1], "GET");
  });

  // SET(l, index, v) replaces an item and returns the list.
  m.native_functionl("SET", [](const Args& args) {
    check_args("SET", args, 3, 3);
    auto& items = args[0].list("SET")->items;
    items[checked_index("SET", args[1], items.size())] = args[2];
    return args[0];
  });

  // CLONE(l) returns a new list with the same items.
  m.native_functionl("CLONE", [](const Args& args) {
    check_args("CLONE", args, 1, 1);
    auto l = Value::new_list();
    l.list("CLONE")->items = args[0].list("CLONE")->items;
    return l;
  });

  // SORT(l) sorts the list in place and returns it. Numbers (and booleans)
  // sort before strings; numbers compare numerically, strings by character.
  m.native_functionl("SORT", [](const Args& args) {
    check_args("SORT", args, 1, 1);
    auto& items = args[0].list("SORT")->items;
    // 0 for numbers (and booleans), 1 for strings; lists can't be sorted.
    auto rank = [](const Value& v) {
      switch (v.value_type()) {
      case Value::Type::BOOLEAN:
      case Value::Type::INTEGER:
      case Value::Type::REAL:
        return 0;
      case Value::Type::STRING:
        return 1;
      case Value::Type::LIST:
      case Value::Type::ITERATOR:
        break;
      }
      throw BasicError(fmt::format("SORT: cannot sort a list containing a {}", v.type_name()));
    };
    for (const auto& v : items) {
      rank(v);
    }
    std::stable_sort(items.begin(), items.end(), [&](const Value& a, const Value& b) {
      const auto ra = rank(a);
      const auto rb = rank(b);
      if (ra != rb) {
        return ra < rb;
      }
      return ra == 0 ? a.toDouble() < b.toDouble() : a.toString() < b.toString();
    });
    return args[0];
  });

  // ITERATOR(l) returns an iterator positioned before the first item.
  m.native_functionl("ITERATOR", [](const Args& args) {
    check_args("ITERATOR", args, 1, 1);
    auto it = std::make_shared<IteratorData>();
    it->list = args[0].list("ITERATOR");
    return Value(it);
  });

  // MOVE_NEXT(it) advances the iterator; TRUE while it is on an item.
  m.native_functionl("MOVE_NEXT", [](const Args& args) {
    check_args("MOVE_NEXT", args, 1, 1);
    auto it = args[0].iterator("MOVE_NEXT");
    if (static_cast<size_t>(it->pos + 1) <= it->list->items.size()) {
      ++it->pos;
    }
    return Value(static_cast<size_t>(it->pos) < it->list->items.size());
  });
}

} // namespace wwivbasic::stdlib
