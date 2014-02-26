/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                 Copyright (C) 2014, WWIV Software Services             */
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
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace WWIV5TelnetServer
{
    public class CommandLineBuilder
    {
        private string templateString;
        private string executable;
        public CommandLineBuilder(string templateString, string executable)
        {
            this.templateString = templateString;
            this.executable = executable;
        }

        public string CreateFullCommandLine(IDictionary<string, string> values)
        {
            return executable + " " + CreateArguments(values);
        }

        public string CreateFullCommandLine(int node, int handle)
        {
            return executable + " " + CreateTelnetArguments(node, handle);
        }

        public string CreateArguments(IDictionary<string, string> values)
        {
            Regex re = new Regex(@"\@(\w+)", RegexOptions.Compiled);
            string arguments = re.Replace(templateString, match => values[match.Groups[1].Value]);
            return arguments;
        }

        public string CreateTelnetArguments(int node, int handle)
        {
            IDictionary<string, string> values = new Dictionary<string, string>();
            values.Add("N", Convert.ToString(node));
            values.Add("H", Convert.ToString(handle));
            Regex re = new Regex(@"\@(\w+)", RegexOptions.Compiled);
            string arguments = re.Replace(templateString, match => values[match.Groups[1].Value]);
            return arguments;
        }

        public string Executable
        {
            get
            {
                return executable;
            }
        }

    }
}
