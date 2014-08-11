using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace WWIV5TelnetServer
{
    class Launcher
    {
        string executable;
        string homeDirectory;
        string argumentsTemplate;
        Action<string> logger;

        public Launcher(string executable, string homeDirectory, string argumentsTemplate, Action<string> logger)
        {
            this.executable = executable;
            this.homeDirectory = homeDirectory;
            this.argumentsTemplate = argumentsTemplate;
            this.logger = logger;
        }

        private const int SW_MINIMIZED = 2;
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1060:MovePInvokesToNativeMethodsClass"), DllImport("user32.dll")]
        private static extern int ShowWindowAsync(IntPtr hWnd, int nCmdShow);

        public Process launchLocalNode(int node) {
            return launchNode("-N@N -M", node, -1, false);
        }
        
        public Process launchTelnetNode(int node, Int32 socketHandle)
        {
            return launchNode(argumentsTemplate, node, socketHandle, Properties.Settings.Default.launchMinimized);
        }

        Process launchNode(String template, int node, Int32 socketHandle, bool minimize)
        {
            CommandLineBuilder cmdlineBuilder = new CommandLineBuilder(template, executable);

            Process p = new Process();
            p.EnableRaisingEvents = false;
            p.StartInfo.FileName = executable;
            p.StartInfo.Arguments = cmdlineBuilder.CreateTelnetArguments(node, socketHandle);
            p.StartInfo.WorkingDirectory = homeDirectory;
            p.StartInfo.UseShellExecute = false;
            logger("Launching binary: " + cmdlineBuilder.CreateFullCommandLine(node, socketHandle));
            p.Start();
            Console.WriteLine("binary launched.");

            if (minimize)
            {
                for (int i = 0; i < 25; i++)
                {
                    // The process is launched asynchronously, so wait for up to a second
                    // for the main window handle to be created and set on the process class.
                    if (p.MainWindowHandle.ToInt32() != 0)
                    {
                        break;
                    }
                    Thread.Sleep(100);
                }
                logger("Trying to minimize process on handle:" + p.MainWindowHandle);
                ShowWindowAsync(p.MainWindowHandle, SW_MINIMIZED);
            }
            return p;
        }

    }
}
