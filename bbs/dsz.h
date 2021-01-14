/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_DSZ_H__
#define __INCLUDED_BBS_DSZ_H__

#include <functional>
#include <string>
#include <tuple>

enum class dsz_logline_t {
  upload,
  download,
  error
};


typedef std::function<void(dsz_logline_t, std::string, int)> dsz_logline_callback_fn;

std::tuple<dsz_logline_t, std::string, int> handle_dszline(const std::string& l);
bool ProcessDSZLogFile(const std::string& path, dsz_logline_callback_fn cb);

#endif  // __INCLUDED_BBS_DSZ_H__
