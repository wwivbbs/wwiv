/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

// Version String
const char *wwiv_version = "v5.00";

// Build Information
const char *beta_version = " (Build 57)";

// this is used to set the status.wwiv_version
// leaving at 431 until we break file compatability
unsigned int wwiv_num_version = 431;

// Data/time of this build
const char *wwiv_date = __DATE__ ", " __TIME__;

/////////////////////////////////////////////////////////////////////////////
//
// Version number used by the auto-update support
// M  = major version number
// 11 = 1st position
// 22 = 2nd position
// 33 = build number
// Example: 5.01 build 20 = 500010020
//
//                              M11223333
const long lWWIVVersionNumber = 500000057;
