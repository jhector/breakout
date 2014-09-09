#!/usr/bin/env katana
$e=load "prison"
$ehframe=dwarfscript compile "prison.dws"
replace section $e ".eh_frame" $ehframe[0]
replace section $e ".eh_frame_hdr" $ehframe[1]
replace section $e ".gcc_except_table" $ehframe[2]
save $e "prison_mod"
!chmod +x prison_mod
