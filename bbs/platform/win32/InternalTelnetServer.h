/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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

#if !defined (__INCLUDED_INTERNALTELNETSERVER_H__)
#define __INCLUDED_INTERNALTELNETSERVER_H__


#if defined( _WIN32 )
extern "C"
{
    #include <winsock2.h>
}
#endif // _WIN32

class Runnable;

class WInternalTelnetServer
{
private:
    Runnable* m_pRunnable;
    SOCKET hSocketHandle;

    void CreateListener();
public:
    WInternalTelnetServer( Runnable* pRunnable );
    virtual ~WInternalTelnetServer();

    void RunTelnetServer();
};


#endif  // #if !defined (__INCLUDED_INTERNALTELNETSERVER_H__)
