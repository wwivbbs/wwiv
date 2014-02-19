using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using WWIV5TelnetServer;
using System.Collections.Generic;

namespace UnitTestProject1
{
    [TestClass]
    public class CommandLineBuilderTest
    {
        private CommandLineBuilder builder = new CommandLineBuilder("-XT -N@N -H@H", @"C:\bin");
        private Dictionary<string, string> dict = new Dictionary<string, string>() {
                { "N", "1" },
                { "H", "1234" },
            };

        [TestMethod]
        public void TestSunnyCase_FullCommandLine()
        {
            Assert.AreEqual(@"C:\bin -XT -N1 -H1234", builder.CreateFullCommandLine(dict));
        }

        [TestMethod]
        public void TestSunnyCase_Arguments()
        {
            Assert.AreEqual(@"-XT -N1 -H1234", builder.CreateArguments(dict));
            Assert.AreEqual(@"-XT -N2 -H2345", builder.CreateArguments(2, 2345));
        }

        [TestMethod]
        public void TestGetExecutable()
        {
            Assert.AreEqual(@"C:\bin", builder.Executable);
        }
    }
}
