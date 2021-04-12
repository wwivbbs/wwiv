|[2J|#4 Your Preferences|[K|#0

|#11|#9) Screen size       : |#2|{user.screenwidth, ">3"} |#9X |#2|{user.screenlines, ">3"}        |#12|#9) ANSI              : |#2|{user.ansistr}
|#13|#9) Pause on screen   : |#2|{if "user.pause == true", "On", "Off"}
|#14|#9) Mailbox           : |#2|{user.mailbox_state}
|#15|#9) Configured Q-scan                    |#16|#9) Change password
|#17|#9) Update macros                        |#18|#9) Change colors
|#19|#9) Message editor    : |#2|{user.editorname, "<16"} |#1A|#9) Extended colors   : |#2|{yesno "user.extcolors == true"}
|#1B|#9) Optional lines    : |#2|{user.optional_lines}                |#1C|#9) Conferencing      : |#2|{yesno "user.conferencing == true"}
|#1D|#9) Show Hidden Lines : |#2|{yesno "user.show_controlcodes == true", "<16"} |#1G|#9) Message Reader    : |#2|{if "user.fs_reader == true", "Full-Screen", "Traditional"}
|#1I|#9) Internet Address  : |#2|{user.email}
|#1K|#9) Configure Menus
|#1M|#9) Allow user msgs   : |#2|{yesno "user.ignore_msgs == false", "<16"} |#1N|#9) Configure QWK
|#1S|#9) Cls Between Msgs? : |#2|{yesno "user.clear_screen == true", "<16"} |#1T|#9) 12hr or 24hr clock: |#2|{if "user.twentyfour_clock == true", "24hr", "12hr"}
|#1U|#9) Use Msg AutoQuote : |#2|{yesno "user.auto_quote == true", "<16"} |#W|#9) WWIV reg num      : |#2|{user.regnum}
|#1Q|#9) Quit to main menu

