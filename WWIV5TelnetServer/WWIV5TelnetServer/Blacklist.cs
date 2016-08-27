using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WWIV5TelnetServer
{
  class Blacklist
  {
    private List<string> rbl_servers_;
    private HashSet<string> goodips_ = new HashSet<string>();
    private HashSet<string> badips_ = new HashSet<string>();
    private string goodip_file_;
    private string badip_file_;
    public Blacklist(string badip_file, string goodip_file, List<string> rbl_servers)
    {
      badip_file_ = badip_file;
      goodip_file_ = goodip_file;
      rbl_servers_ = new List<String>();
      rbl_servers_.AddRange(rbl_servers);

      if (File.Exists(goodip_file))
      {
        string[] lines = System.IO.File.ReadAllLines(goodip_file);
        foreach (string line in lines)
        {
          goodips_.Add(line);
        }
      }
      if (File.Exists(badip_file))
      {
        string[] lines = System.IO.File.ReadAllLines(badip_file);
        foreach (string line in lines)
        {
          badips_.Add(line);
        }
      }

    }

    public bool IsBlackListed(String ipaddress)
    {
      // Check whitelist first.
      if (goodips_.Contains(ipaddress))
      {
        // We're whitelisted.
        return false;
      }

      // Check blacklist next.
      if (badips_.Contains(ipaddress))
      {
        // Blacklisted IP, immediately return.
        return true;
      }

      if (rbl_servers_.Count == 0)
      {
        // We have no RBL to check, return false.
        return false;
      }
      VerifyIP IP = new VerifyIP(ipaddress, rbl_servers_.ToArray());
      if (IP.IPAddr.Valid)
      {
        if (IP.BlackList.IsListed)
        {
          Debug.Write(IP.BlackList.VerifiedOnServer);
          return true;
        }
      }
      return false;
    }

    public bool BlacklistIP(string ip_address)
    {
      badips_.Add(ip_address);
      using (System.IO.StreamWriter file = new System.IO.StreamWriter(badip_file_))
      {
        foreach (string s in badips_)
        {
          file.WriteLine(s);
        }
      }
      return true;
    }

  }
}
