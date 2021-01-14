/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2021, WWIV Software Services            */
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
#ifndef __INCLUDED_EXTERNAL_EDIT_WWIv_H__
#define __INCLUDED_EXTERNAL_EDIT_WWIv_H__

#include "bbs/external_edit.h"
#include "common/message_editor_data.h"
#include <string>
#include <filesystem>
#include "sdk/vardec.h"

class ExternalWWIVMessageEditor : public ExternalMessageEditor {
public:
  ExternalWWIVMessageEditor(const editorrec& editor, wwiv::common::MessageEditorData& data, int maxli,
                            int* setanon, const std::filesystem::path& temp_directory)
      : ExternalMessageEditor(editor, data, maxli, setanon, temp_directory) {}
  virtual ~ExternalWWIVMessageEditor();
  void CleanupControlFiles() override;
  bool Before() override;
  bool After() override;
  std::string editor_filename() const override;
};


#endif // __INCLUDED_EXTERNAL_EDIT_WWIv_H__
