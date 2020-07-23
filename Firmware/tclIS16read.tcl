#!/usr/bin/tclsh

package require Tk

wm title . "IS16"
wm protocol . WM_DELETE_WINDOW {
    if {[tk_messageBox -message "Exit" -type yesno] eq "yes"} {
       doExit
    }
}

proc printName {name} {
	puts $name
	return
};

proc doExit {} {
	puts "exit"
	exit
	return
};

proc confColor {curobj curcolor} {
	$curobj configure -background $curcolor
	return
};

button .busb -width 20 -text {Enter port} -command {
    printName /dev/ttyUSB0
    .honk configure -state normal
    .woop configure -state normal
    .start configure -state normal
    .busb configure -state disabled
}
button .honk -width 20 -text {honk} -state disabled -command {printName alarm1}
button .woop -width 20 -text {woop} -state disabled -command {printName alarm2}

button .start -width 20 -foreground white -text {START} -state disabled -command {printName start}
button .ex -width 20 -foreground white -text {EXIT} -command {doExit}

confColor .busb yellow
confColor .ex red
confColor .start green

pack .busb
pack .honk
pack .woop
pack .start
pack .ex
