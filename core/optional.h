/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2018, WWIV Software Services                */
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
/**************************************************************************/
#ifndef __INCLUDED_CORE_OPTIONAL_H__
#define __INCLUDED_CORE_OPTIONAL_H__


namespace wwiv {
namespace core {

#ifndef __has_include
#error "__has_include must be supported"
#endif  // __has_include

#if __has_include(<optional>)
#include <optional>
using std::bad_optional_access;
using std::in_place;
using std::in_place_t;
using std::make_optional;
using std::nullopt;
using std::nullopt_t;
using std::optional;
#elif __has_include(<experimental/optional>)
#include <experimental/optional>
using std::experimental::bad_optional_access;
using std::experimental::in_place;
using std::experimental::in_place_t;
using std::experimental::make_optional;
using std::experimental::nullopt;
using std::experimental::nullopt_t;
using std::experimental::optional;
#else
#error "Either <optional> or <experimental/optional> must exist"
#endif

}
}

#endif  // __INCLUDED_CORE_OPTIONAL_H__
