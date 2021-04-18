|#9 Sub: |#2|{msg.message_area, "<45"} |#9Msg#: |[59;1H|#9[|#2|{msg.message_number, ">5"}|#9 of |#2|{msg.total_messages, ">5"}|#9]         Info: |#1|{if "msg.flags.local == true", "|#9[|#5LOC|#9] ", ""}|{if "msg.flags.wwivnet == true", "|#9[|#5WWIVNET|#9] ", ""}|{if "msg.flags.ftn == true", "|#9[|#5FTN|#9] ", ""}
|#9From: |#1|{msg.from, "<45"} |#9Date: |#1|{msg.date}
|#9  To: |#1|{msg.to, "<45"} |#9Subj: |#2|{msg.title}
|#9 Sys: |#1|{msg.sys, "<45"} |#9Loc: |[59;4H|{msg.loc}
|#0|{set num_header_lines 4}