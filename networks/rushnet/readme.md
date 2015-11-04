# BinkP Test Net
***

##Warning
This documents expalains future looking funciontality of WWIV and WWIVNet. It is not
currently available for use by the communtiy as a whole.

##Overview
These files represent the net config files for Rushnet, which is a 
[BinkP](https://en.wikipedia.org/wiki/Binkp) based net. The current WWIVnet uses a 
centralized SMTP and POP server to post and retrieve the email and SUB messages. That 
implementation lacks redundancy and some levels of security. The goal is to replace that 
system with a a BinkP based network will be more resilient to indidiual node failure and 
enhance the security of the network.

##What Work & Limitations

* Only email works over BinkP. There are no subs available. You can only email other 
nodes on the BinkP net.
* Currently the BinkP net is not truly point to point, it still realies on going through
a single central node.

##Setup
* Unless you're asked to be part of the BinkP net, you don't bother with this.
* All files mentioned refer to the files in the rushnet folder on github unless otherwise stated.

###Add A Node to the Net Configuration Files
* Get a node assignment from RushFan. At this time you will need to provide him a password as well.
* Use Github to create a branch. You can call it "AddingNode####". You will be editing the files 
in the rushnet folder. The same one this ReaMe is in.
* Add your node to BBSLIST.NET, simply insert a row in the proper number order it should look 
similiar to this:  
```@509  *204-000-INET #33600 !   [     ]  "Cloud City BBS"```
Just makes sure your @Node, Fake Phone Number and Name are unique. Leave the rest alone.
* In BINKP.NET add your @node and the DNS name:port for your system. BinkP uses port 24554 
by default. If you're not using another port you can just list your @node and DNS Name. Here 
are examples with and without a custom port:
```
@5 bbsdoors.com:24555
@509 wwiv.cloudcitybbs.com
```
* Edit CONNECT.NET and add a row indicating your Node conencts to @1 at a cost of 0.00 like so:
```@509 1=0.00```
* Also add to the first @1 line to indicate node 1 connects to you as well, like so:
```@1 2=0.00 5=0.00 509=0.00 1000=0.00 9999=0.00```
* You should now create a pull request to merge the files to main. Everyone else on the net 
will need to pick up the new files once you are on the net.

###Configure Rushnet on your BBS
* Start INIT.EXE and add a new Network (N) to your board. It will be a WWIVNet called RushNet. 
Your node is as you configured it in the Net files and your file location can be whatever you 
like, but \wwiv\nets\rushnet is recommneded. You will end up with something like this:
```
┌─────────────────────────────────────────── Network Configuration ┐
│ Net Type  : WWIVnet                                              │
│ Net Name  : RushNet                                              │
│ Node #    : 509                                                  │
│ Directory : NETS\RUSHNET\                                        │
└──────────────────────────────────────────────────────────────────┘
```
* Copy all the files you just edited on GitHub to \wwiv\nets\rushnet on your local hard drive
* Run \WWIV\NETWORK3.EXE Y .1 to test your configuration. Just as when you joined WWIVNet this will 
verify your config and email your #1 account on the local BBS.  If you have no error you're 
ready to go.  
**Note:** This assume Rushnet is your second net configured in INIT. If its the first use .0,
the third .2, etc..
* Make sure PORT 24554 (or the port you setup) is open inbound on your firewall.

###Running your BINKP Server
* Run \wwiv\networkb.exe --receive (in a loop)
* If you changed the port number (from the default of 24554) then add --port=####

Example:
```cmd
    rem NetworkB controller
    :start
    C:
    cd \wwiv
    networkb --receive
    goto start
```
