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

#if !defined (__INCLUDED_WCOMM_H__)
#define __INCLUDED_WCOMM_H__


/**
 * Base Communication Class.
 */
class WComm
{
private:
    // used by the GetLastErrorText() method
    static char m_szErrorText[8192];

protected:
    // Com Port Number.  Only used by Serial IO derivatives
    // of this class
    int m_ComPort;
    bool m_bBinaryMode;

    static const char* GetLastErrorText();

public:
    WComm() { m_ComPort = 0; m_bBinaryMode = false; }
    virtual ~WComm() { shutdown(); }

    virtual unsigned int open() = 0;
    virtual bool setup(char parity, int wordlen, int stopbits, unsigned long baud) = 0;
    virtual void close( bool bIsTemporary = false ) = 0;
    virtual unsigned int putW(unsigned char ch) = 0;
    virtual unsigned char getW() = 0;
    virtual bool dtr(bool raise) = 0;
    virtual void flushOut() = 0;
    virtual void purgeOut() = 0;
    virtual void purgeIn() = 0;
    virtual unsigned int put(unsigned char ch) = 0;
    virtual char peek() = 0;
    virtual unsigned int read(char *buffer, unsigned int count) = 0;
    virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation = false) = 0;
    virtual bool carrier() = 0;
    virtual bool incoming() = 0;
    virtual bool startup();
    virtual bool shutdown();
	virtual void StopThreads() = 0;
	virtual void StartThreads() = 0;


    // Get/Set Com Port Number
    int  GetComPort();
    void SetComPort(int nNewPort);

    void SetBinaryMode( bool b ) { m_bBinaryMode = b; }
    bool GetBinaryMode() { return m_bBinaryMode; }

};



#endif  // #if !defined (__INCLUDED_WCOMM_H__)

