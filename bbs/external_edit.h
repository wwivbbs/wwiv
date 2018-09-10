/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2017, WWIV Software Services            */
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
#ifndef __INCLUDED_EXTERNAL_EDIT_H__
#define __INCLUDED_EXTERNAL_EDIT_H__

#include "bbs/message_editor_data.h"
#include <string>

#include "sdk/vardec.h"

class ExternalMessageEditor {
public:
  ExternalMessageEditor(const editorrec& editor, wwiv::bbs::MessageEditorData& data, int maxli, int* setanon,
                        const std::string& temp_directory)
      : editor_(editor), data_(data), maxli_(maxli), setanon_(setanon),
        temp_directory_(temp_directory){};
  virtual ~ExternalMessageEditor() = default;

  virtual bool Run();
  virtual void CleanupControlFiles() = 0;
  virtual bool Before() = 0;
  virtual bool After() = 0;
  virtual const std::string editor_filename() const = 0;

protected:
  const editorrec editor_;
  wwiv::bbs::MessageEditorData& data_;
  const int maxli_;
  int* setanon_;
  const std::string temp_directory_;
};

class ExternalQBBSMessageEditor : public ExternalMessageEditor {
public:
  ExternalQBBSMessageEditor(const editorrec& editor, wwiv::bbs::MessageEditorData& data, int maxli,
                            int* setanon, const std::string& temp_directory)
      : ExternalMessageEditor(editor, data, maxli, setanon, temp_directory) {}
  virtual ~ExternalQBBSMessageEditor();
  void CleanupControlFiles() override;
  bool Before() override;
  bool After() override;
  virtual const std::string editor_filename() const override;
};

class ExternalWWIVMessageEditor : public ExternalMessageEditor {
public:
  ExternalWWIVMessageEditor(const editorrec& editor, wwiv::bbs::MessageEditorData& data, int maxli,
                            int* setanon, const std::string& temp_directory)
      : ExternalMessageEditor(editor, data, maxli, setanon, temp_directory) {}
  virtual ~ExternalWWIVMessageEditor();
  void CleanupControlFiles() override;
  bool Before() override;
  bool After() override;
  virtual const std::string editor_filename() const override;
};

bool DoExternalMessageEditor(wwiv::bbs::MessageEditorData& data, int maxli, int* setanon);

bool external_text_edit(const std::string& edit_filename, const std::string& new_directory,
                        int numlines, int flags);

#endif // __INCLUDED_EXTERNAL_EDIT_H__
