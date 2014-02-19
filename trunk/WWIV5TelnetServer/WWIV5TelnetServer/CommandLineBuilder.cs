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
            return executable + " " + CreateArguments(node, handle);
        }

        public string CreateArguments(IDictionary<string, string> values)
        {
            Regex re = new Regex(@"\@(\w+)", RegexOptions.Compiled);
            string arguments = re.Replace(templateString, match => values[match.Groups[1].Value]);
            return arguments;
        }

        public string CreateArguments(int node, int handle)
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
