|#9 Sub:                                     Msg#:
|#9From:                                     Date:
|#9  To:
|#9Subj:
|#9 Sys:                                      Loc:
|#9Info:
|[7;1H|#2|{msg.message_area}|[49;1H|#9[|#2|{msg.message_number}|#9 of |#2|{msg.total_messages}|#9]
|[7;2H|#1|{msg.from}|[49;2H|#1|{msg.date}
|[7;3H|#1|{msg.to}
|[7;4H|#2|{msg.title}
|[7;5H|#1|{msg.sys}|[49;5H|{msg.loc}
|[7;6H|#1|{if "msg.flags.local == true", "|#9[|#5LOC|#9] ", ""}|{if "msg.flags.wwivnet == true", "|#9[|#5WWIVNET|#9] ", ""}|{if "msg.flags.ftn == true", "|#9[|#5FTN|#9] ", ""}
|#0|{set num_header_lines 6}