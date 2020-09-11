/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2020, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#ifndef __INCLUDED_CORE_EVENTBUS_H__
#define __INCLUDED_CORE_EVENTBUS_H__

#include "core/log.h"
#include "core/callable/callable.hpp"
#include <any>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>


namespace wwiv::core {

class Event {
public:
  explicit Event(std::string id) : id_(std::move(id_)) {}
  Event(std::string id, std::string data) : id_(std::move(id_)) {}

  const std::string& id() { return id_; }
  const std::string& data() { return data_; }

private:
  const std::string id_;
  const std::string data_;
};

class EventBus final {
public:
  EventBus() = default;
  ~EventBus() = default;

  template<typename T, typename H> void add_handler(H handler) {
    static_assert(!std::is_reference<T>::value, "add_handler: Handler param must not be reference");
    const std::string name = typeid(T).name();
    if constexpr (callable_traits<H>::argc == 0) {
      handlers_.emplace(name, [handler](std::any) { handler(); });
    } else {
      handlers_.emplace(name,
                        [f = std::forward<H>(handler)](auto value) { f(std::any_cast<T>(value)); });
    }
  }

  template <typename T, typename M, typename I> void add_handler(M method, I instance) {
    const std::string name = typeid(T).name();
    std::function<void(T)> f = std::bind(method, instance, std::placeholders::_1);
    handlers_.emplace(name, [f](auto value) { f(std::any_cast<T>(value)); });
  }

  template <typename T> void invoke() { invoke(T{}); }

  template <typename T> void invoke(const T& event_type) {
    const std::string name = typeid(T).name();
    auto range = handlers_.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
      try {
        it->second(std::make_any<T>(event_type));
      } catch (const std::bad_cast& e) {
        LOG(INFO) << e.what();
      }
    }
  }


  std::unordered_multimap<std::string, std::function<void(std::any)>> handlers_;
};

EventBus& bus();

} // namespace wwiv::core

#endif // __INCLUDED_CORE_EVENTBUS_H__
