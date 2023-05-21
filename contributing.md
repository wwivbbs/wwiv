# Contributing

Thank you for your interest in contributing to WWIV. 
<strong>We love pull requests from everyone. Here's how you can help!</strong>

## GitHub Issue Tracker

* WWIV uses the [Github issue tracker](https://github.com/wwivbbs/wwiv/issues) as the main venue for
bug reports, feature requests, enhancement requests, and pull requests.
* Please do not use the issue tracker for general support, please drop in on 
IRC and feel free to ask questions.  Bear in mind that most of us are idling
on IRC using an IRC bouncer, so expect high latency on replies.

## IRC

The dev team and fellow SysOps hangs out on IRC at:

* Server: irc.wwivbbs.org:6667
* Channel: #WWIV
* <a href="irc://irc.wwivbbs.org:6667/wwiv">Click to Join IRC</a>
* For IRC logs and stats check out [Venom's Lair IRC Page](http://venomslair.com/irc.php)

## Issues and Labels

* WWIV uses labels to track platform, feature, enhancement, and type of bug. 
* Here are some labels:
...

[WWIV Releases and Versioning](http://wwivbbs.readthedocs.org/en/latest/Development/wwiv_releases_and_versioning/)

## Bug Reports

We love good bug reports!  A good bug report is:
* Specific: Ideally somebody can fix the bug without needing to figure our where to look.  
* Actionable: Knowing what was expected vs. what actually happened is critical.
* Not a duplicate: Please search the existing issues to see if this has already been reported
* Versioned: Please indicate what version of the software you are using (a build number of jenkins, or sha of the repository if you built it from git.
* Environmental: Please indicate the OS and version you are using.  If you have multiple OSes available knowing if this is specific to one operating system or version of operating system is helpful.

Yes, this is a lot to ask for, but the more specific bug reports are, the more quickly someone can grab it and fix it.  As much (or as little) information you have helps, but the more the merrier and the more likely it'll get quickly fixed.

## Pull Requests

We love Pull Requests! Good pull requests for features, enhancements and bug reports are the key to any open source project. Ideally they will also solve or implement one feature, enhancement request or bug report so it is easy to merge, and in the case of problems, revert. 

<strong>Please talk to us first</strong> before you spend a nontrivial amount of time on a new coding project.  We're happy to help offer suggestions, help vet design ideas, and also warn about gotchas in the code before you wander down a path that can cause problems that can keep a PR from being merged.  Please drop by IRC and ask away.

Here's the best way to work with the WWIV git repository:

1. [Fork](https://help.github.com/articles/fork-a-repo/), then clone your fork, and configure the remotes:
    
    ```bash
    # Create a directory for your fork's clone.
    mkdir git
    chdir git
    # Clone your fork into the current directory (git).
    # Use your GitHub username instead of <em>YOUR-USERNAME</em>
    git clone https://github.com/<em>YOUR-USERNAME</em>/wwiv.git
    # Change directory into the wwiv directory of your clone/
    chdir wwiv
    # Add the remotes for the upstream repository (wwivbbs/wwiv)
    git remote add upstream https://github.com/wwivbbs/wwiv.git
    ```
    
2. If you have done step 1 a while ago, pull from the upstream repository to update your clone with the latest from wwivbbs/wwiv.
    ```bash
    # make sure your branch is back onto the "main" branch
    git checkout main
    # pull (this is a fetch + merge) in the changes from the wwivbbs/wwiv repository.
    git pull --recurse upstream main
    # push the changes from wwivbbs/wwiv to your fork on github.
    git push
    ```
    
3. Create a new branch off of main for your feature, enhancement, or bug fix and let GitHub know about your branch.

    ```bash
    git checkout -b <MY-BRANCH-NAME>
    git push origin <MY-BRANCH-NAME>
    ```    

4. Make your changes by editing the files and committing changes to your local repository.  Please use good commit messages that explain the changes and also reference github issues as necessary.

5. Merge any new changes from the wwivbbs/wwiv repository into your development branch

    ```bash
    git pull --recurse upstream main
    ```    
    
6. Push your changes from your local machine to your fork on github.

    ```bash
    git push origin <MY-BRANCH-NAME>
    ```
    
7. Open a [Pull Request](https://help.github.com/articles/using-pull-requests/) with a clear and concise
   title and description of the changes.  Please reference any issues on GitHub as needed. 
   [pr]: https://github.com/wwivbbs/wwiv/compare

8. At this point you're waiting on us. We will endeavour to approve requests or comment within a week.
Please remember this is a just hobby. :-) We may suggest
some changes or improvements or alternatives to your suggestions or approve them outright.

9. Some things that will increase the chance that your pull request is accepted:

* Drop in on IRC and talk about your changes.
* Fix an open issue.
* Write a [good commit message][commit].
* Explain how you tested the changes, any known limitations on Windows version or Linux.

[commit]: http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html
