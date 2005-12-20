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

#ifndef __INCLUDED_INI_H__
#define __INCLUDED_INI_H__

#include <string>

struct ini_flags_rec
{
    int			strnum;
    bool		sense;
    unsigned long value;
};

void ini_done();
bool ini_init(const char *pszFileName, const char *prim, const char *sec);
char *ini_get( const char *pszKey, int nNumericIndex, char *pszStringIndex );





class WIniFile
{
public:
    // Constructor/Destructor
    WIniFile(const char *pszFileName ) { m_strFileName = pszFileName; }
    virtual ~WIniFile() { Close(); }

    //
    // Member functions
    //
    bool Initialize( const char *prim, const char *sec = NULL ) { return ini_init( m_strFileName.c_str(), prim, sec ); }
    bool Close() { ini_done(); return true; }

    const char* GetValue( const char *pszKey ) const { return GetValue( pszKey, -1, NULL ); }
    const char* GetValue( const char *pszKey, int nNumericIndex ) const { return GetValue( pszKey, nNumericIndex, NULL ); }
    const char* GetValue( const char *pszKey, char *pszStringIndex ) const { return GetValue( pszKey, -1, pszStringIndex ); }

    const long GetNumericValue( const char *pszKey ) const { return atoi( GetValue( pszKey, -1, NULL ) ); }
    const long GetNumericValue( const char *pszKey, int nNumericIndex ) const { return atoi( GetValue( pszKey, nNumericIndex, NULL ) ); }
    const long GetNumericValue( const char *pszKey, char *pszStringIndex ) const { return atoi( GetValue( pszKey, -1, pszStringIndex ) ); }

    const long GetNumericValueWithDefault( const char *pszKey, int nDefaultValue ) const;
    const long GetNumericValueWithDefault( const char *pszKey, int nDefaultValue, int nNumericIndex ) const;
    const long GetNumericValueWithDefault( const char *pszKey, int nDefaultValue, char *pszStringIndex ) const;

    const bool GetBooleanValue( const char *pszKey ) const;
    const bool GetBooleanValue( const char *pszKey, int nNumericIndex ) const;
    const bool GetBooleanValue( const char *pszKey, char *pszStringIndex ) const;

protected:
    const char* GetValue( const char *pszKey, int nNumericIndex, char *pszStringIndex ) const { return ini_get( pszKey, nNumericIndex, pszStringIndex ); }
    static bool StringToBoolean( const char *p );

private:
    // Data
    std::string m_strFileName;

};


#endif	// __INCLUDED_INI_H__
