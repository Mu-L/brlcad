#                   M A N . T C L
# BRL-CAD
#
# Copyright (c) 2004-2025 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#


encoding system utf-8

proc man {{cmdname {}}} {
    global mged_console_mode
    global tcl_platform
    if {![info exists mged_console_mode]} {
	error "Unable to determine MGED console mode."
    }
    if {$mged_console_mode == "gui"} {
	package require ManBrowser 1.0
	if {![winfo exists .mgedMan]} {
	    ManBrowser .mgedMan -useToC 1 -defaultDir n -parentName MGED
	}

	if {$cmdname != {} && ![.mgedMan select $cmdname]} {
	    error "couldn't find manual page \"$cmdname\""
	}
	.mgedMan activate
    }
    if {$mged_console_mode == "classic" || $mged_console_mode == "batch"} {
	if {$cmdname != {}} {
	    set exe_ext ""
	    if {$::tcl_platform(platform) == "windows"} {
		set exe_ext ".exe"
	    }
	    set cmd [list [file join [bu_dir bin] brlman$exe_ext]]
	    exec $cmd $cmdname
	}
    }
}

proc brlman {{cmdname {}}} {
    # simple (intentionally undocumented) pass-through alias
    man $cmdname
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
