#
# Timefield
# ----------------------------------------------------------------------
# Implements a time entry field with adjustable built-in intelligence
# levels.
# 
# Author: Arnulf P. Wiedemann
# Copyright (c) 2008 for the reimplemented version
#
# see file license.terms in the top directory
#
# ----------------------------------------------------------------------
# This code is derived/reimplemented from the iwidgets package Timefield
# written by:
#   AUTHOR:  John A. Tucker          E-mail: jatucker@austin.dsccc.com
#   Copyright (c) 1995 DSC Technologies Corporation
# ----------------------------------------------------------------------
#
#   @(#) $Id: timefield.tcl,v 1.1.2.2 2009/01/05 23:52:43 wiede Exp $
# ======================================================================

#
# Use option database to override default resources of base classes.
#
option add *Timefield.justify center widgetDefault

namespace eval ::itcl::widgets {

#
# Provide a lowercased access method for the timefield class.
# 
proc ::itcl::widgets::timefield {pathName args} {
    uplevel ::itcl::widgets::Timefield $pathName $args
}

# ------------------------------------------------------------------
#                               TIMEFIELD
# ------------------------------------------------------------------
::itcl::extendedclass Timefield {
    inherit ::itcl::widgets::Labeledwidget 
    
    component time
    protected component dfchildsite

    option [list -childsitepos childSitePos Position] -default e -configuremethod configChildsitepos
    option [list -command command Command] -default {} -configuremethod configCommand
    option [list -seconds seconds Seconds] -default on
    option [list -format format Format] -default civilian -configuremethod configFormat
    option [list -iq iq Iq] -default high -configuremethod configIq
    option [list -gmt gmt GMT] -default no -configuremethod configGmt
    option [list -state state State] -default normal -configuremethod configState

    delegate option [list -textbackground textBackground Background] to time as -background
    protected variable _cfield hour
    protected variable _formatString "%r"
    protected variable _fields {}
    protected variable _numFields 4
    protected variable _forward {}
    protected variable _backward {}
    protected variable _timeVar ""

    protected common _militaryFields {hour minute second}
    protected common _civilianFields {hour minute second ampm}

    constructor {args} {}

    protected method _backwardCivilian {}
    protected method _backwardMilitary {}
    protected method _focusIn {}
    protected method _forwardCivilian {}
    protected method _forwardMilitary {}
    protected method _keyPress {char sym state}
    protected method _moveField {direction}
    protected method _setField {field}
    protected method _whichField {}
    protected method _toggleAmPm {}
    protected method configChildsitepos {option value}
    protected method configCommand {option value}
    protected method configFormat {option value}
    protected method configIq {option value}
    protected method configGmt {option value}
    protected method configState {option value}

    public method get {{format "-string"}}
    public method isvalid {}
    public method show {{time "now"}}

public method component {comp args} {
    uplevel 0 [set $comp] $args
}
}

# ------------------------------------------------------------------
#                        CONSTRUCTOR
# ------------------------------------------------------------------
::itcl::body Timefield::constructor {args} {
    $itcl_hull configure -borderwidth 0
    
    #
    # Create an entry field for entering the time.
    #
    setupcomponent time using entry $itcl_interior.time
    keepcomponentoption time -borderwidth -cursor -exportselection \
          -foreground -highlightcolor -highlightthickness \
          -insertbackground -justify -relief -textvariable
      
# FIXME      rename -font -textfont textFont Font
# FIXME      rename -highlightbackground -background background Background

    #
    # Create the child site widget.
    #
    setupcomponent dfchildsite using frame $itcl_interior.dfchildsite
    set itcl_interior $dfchildsite
    
    #
    # Add timefield event bindings for focus in and keypress events.
    #
    bind $time <FocusIn>   [itcl::code $this _focusIn]
    bind $time <KeyPress>  [itcl::code $this _keyPress %A %K %s]
    bind $time <1> "focus $time; break"

    #
    # Disable some mouse button event bindings:
    #   Button Motion
    #   Double-Clicks
    #   Triple-Clicks
    #   Button2
    #
    bind $time <Button1-Motion>	break
    bind $time <Button2-Motion>	break
    bind $time <Double-Button>	break
    bind $time <Triple-Button>	break
    bind $time <2>		break

    #
    # Initialize the widget based on the command line options.
    #
    if {[llength $args] > 0} {
        uplevel 0 configure $args
    }

    #
    # Initialize the time to the current time.
    #
    show
}

# ------------------------------------------------------------------
#                             OPTIONS
# ------------------------------------------------------------------

# ------------------------------------------------------------------
# OPTION: -childsitepos
#
# Specifies the position of the child site in the widget.  Valid
# locations are n, s, e, and w.
# ------------------------------------------------------------------
::itcl::body Timefield::configChildsitepos {option value} {
    set parent [winfo parent $time]
    switch $value {
    n {
        grid $dfchildsite -row 0 -column 0 -sticky ew
        grid $time -row 1 -column 0 -sticky nsew

        grid rowconfigure $parent 0 -weight 0
        grid rowconfigure $parent 1 -weight 1
        grid columnconfigure $parent 0 -weight 1
        grid columnconfigure $parent 1 -weight 0
      }
    e {
        grid $dfchildsite -row 0 -column 1 -sticky ns
        grid $time -row 0 -column 0 -sticky nsew

        grid rowconfigure $parent 0 -weight 1
        grid rowconfigure $parent 1 -weight 0
        grid columnconfigure $parent 0 -weight 1
        grid columnconfigure $parent 1 -weight 0
      }
    s {
        grid $dfchildsite -row 1 -column 0 -sticky ew
        grid $time -row 0 -column 0 -sticky nsew

        grid rowconfigure $parent 0 -weight 1
        grid rowconfigure $parent 1 -weight 0
        grid columnconfigure $parent 0 -weight 1
        grid columnconfigure $parent 1 -weight 0
      }
    w {
        grid $dfchildsite -row 0 -column 0 -sticky ns
        grid $time -row 0 -column 1 -sticky nsew

        grid rowconfigure $parent 0 -weight 1
        grid rowconfigure $parent 1 -weight 0
        grid columnconfigure $parent 0 -weight 0
        grid columnconfigure $parent 1 -weight 1
      }
    default {
        error "bad childsite option\
                \"$value\":\
                should be n, e, s, or w"
      }
    }
    set itcl_options($option) $value
}

# ------------------------------------------------------------------
# OPTION: -command
#
# Command invoked upon detection of return key press event.
# ------------------------------------------------------------------
::itcl::body Timefield::configCommand {option value} {
    set itcl_options($option) $value
}

# ------------------------------------------------------------------
# OPTION: -iq
#
# Specifies the level of intelligence to be shown in the actions
# taken by the time field during the processing of keypress events.
# Valid settings include high or low.  With a high iq,
# the time prevents the user from typing in an invalid time.  For 
# example, if the current time is 05/31/1997 and the user changes
# the hour to 04, then the minute will be instantly modified for them 
# to be 30.  In addition, leap seconds are fully taken into account.
# A setting of low iq instructs the widget to do no validity checking
# at all during time entry.  With a low iq level, it is assumed that
# the validity will be determined at a later time using the time's
# isvalid command.
# ------------------------------------------------------------------
::itcl::body Timefield::configIq {option value} {
    switch $value {
    high -
    low {
      }
    default {
        error "bad iq option \"$value\": should be high or low"
      }
    }
    set itcl_options($option) $value
}

# ------------------------------------------------------------------
# OPTION: -format
#
# Specifies the time format displayed in the entry widget.
# ------------------------------------------------------------------
::itcl::body Timefield::configFormat {option value} {
    switch $value {
    civilian {
        set _backward _backwardCivilian
        set _forward _forwardCivilian
        set _fields $_civilianFields
        set _numFields 4
        set _formatString "%r"
        $time config -width 11
      }
    military {
        set _backward _backwardMilitary
        set _forward _forwardMilitary
        set _fields $_militaryFields
        set _numFields 3
        set _formatString "%T"
        $time config -width 8
      }
    default {
        error "bad iq option \"$value\":\
             should be civilian or military"
      }
    }

    #
    # Update the current contents of the entry field to reflect
    # the configured format.
    #
    show $_timeVar
    set itcl_options($option) $value
}

# ------------------------------------------------------------------
# OPTION: -gmt
#
# This option is used for GMT time.  Must be a boolean value.
# ------------------------------------------------------------------
::itcl::body Timefield::configGmt {option value} {
    switch $value {
    0 -
    no -
    false -
    off {
      }
    1 -
    yes -
    true -
    on {
      }
    default {
      error "bad gmt option \"$value\": should be boolean"
      }
    }
    set itcl_options($option) $value
}

# ------------------------------------------------------------------
# OPTION: -state
#
# Disable the 
# ------------------------------------------------------------------
::itcl::body Timefield::configState {option value} {
    switch -- $value {
    normal {
        $time configure -state normal
      }
    disabled {
        focus $itcl_hull
        $time configure -state disabled
      }
    default {
        error "Invalid value for -state: $value.  Should be\
          \"normal\" or \"disabled\"."
      }
    }
    set itcl_options($option) $value
}


# ------------------------------------------------------------------
#                            METHODS
# ------------------------------------------------------------------

# ------------------------------------------------------------------
# PUBLIC METHOD: get ?format?
#
# Return the current contents of the timefield in one of two formats
# string or as an integer clock value using the -string and -clicks
# options respectively.  The default is by string.  Reference the 
# clock command for more information on obtaining times and their 
# formats.
# ------------------------------------------------------------------
::itcl::body Timefield::get {{format "-string"}} {
    set _timeVar [$time get]
    switch -- $format {
    "-string" {
        return $_timeVar
      }
    "-clicks" {
        return [::clock scan $_timeVar -gmt $itcl_options(-gmt)]
      }
    default {
        error "bad format option \"$format\":\
               should be -string or -clicks"
      }
    }
}

# ------------------------------------------------------------------
# PUBLIC METHOD: show time
#
# Changes the currently displayed time to be that of the time 
# argument.  The time may be specified either as a string or an
# integer clock value.  Reference the clock command for more 
# information on obtaining times and their formats.
# ------------------------------------------------------------------
::itcl::body Timefield::show {{mytime "now"}} {
    set icursor [$time index insert]
    if {$mytime eq {}} {
        set time "now"
    }

    switch -regexp -- $mytime {
    {^now$} {
        set seconds [::clock seconds]
      }
    {^[0-9]+$} {
        if { [catch {::clock format $mytime -gmt $itcl_options(-gmt)}] } {
            error "bad time: \"$mytime\", must be a valid time \
                 string, clock clicks value or the keyword now"
        }
        set seconds $time
      }
    default {
        if {[catch {
	        set seconds [::clock scan $mytime -gmt $itcl_options(-gmt)]}]} {
            error "bad time: \"$mytime\", must be a valid time \
                 string, clock clicks value or the keyword now"
        }
      }
    }

    set _timeVar [::clock format $seconds -format $_formatString \
        -gmt $itcl_options(-gmt)]
    $time delete 0 end
    $time insert end $_timeVar
    $time icursor $icursor
    return $_timeVar
}

# ------------------------------------------------------------------
# PUBLIC METHOD: isvalid
#
# Returns a boolean indication of the validity of the currently
# displayed time value.  For example, 09:59::59 is valid whereas
# 26:59:59 is invalid.
# ------------------------------------------------------------------
::itcl::body Timefield::isvalid {} {
    set _timeVar [$time get]
    return [expr {([catch {
            ::clock scan $_timeVar -gmt $itcl_options(-gmt)}] == 0)}]
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _focusIn
#
# This method is bound to the <FocusIn> event.  It resets the 
# insert cursor and field settings to be back to their last known
# positions.
# ------------------------------------------------------------------
::itcl::body Timefield::_focusIn {} {
    _setField $_cfield
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _keyPress 
#
# This method is the workhorse of the class.  It is bound to the
# <KeyPress> event and controls the processing of all key strokes.
# ------------------------------------------------------------------
::itcl::body Timefield::_keyPress {char sym state} {

    #
    #  Determine which field we are in currently.  This is needed
    # since the user may have moved to this position via a mouse
    # selection and so it would not be in the position we last 
    # knew it to be.
    #
    set _cfield [_whichField ]

    #
    # Set up a few basic variables we'll be needing throughout the
    # rest of the method such as the position of the insert cursor
    # and the currently displayed minute, hour, and second.
    #
    set inValid 0
    set icursor [$time index insert]
    set lastField [lindex $_fields end]
    set prevtime $_timeVar
    regexp {^([0-9])([0-9]):([0-9])([0-9]):([0-9])([0-9]).*$} \
          $_timeVar dummy \
          hour1 hour2 minute1 minute2 second1 second2
    set hour "$hour1$hour2"
    set minute "$minute1$minute2"
    set second "$second1$second2"
  
    #
    # Process numeric keystrokes.  This involes a fair amount of 
    # processing with step one being to check and make sure we
    # aren't attempting to insert more that 6 characters.  If
    # so ring the bell and break.
    #
    if {![catch {expr {int($char)}}]} {
        # If we are currently in the hour field then we process the
        # number entered based on the cursor position.  If we are at
        # at the first position and our iq is low, then accept any 
        # input.  
        #
        # if the current format is military, then
        # validate the hour field which can be [00 - 23]
        #
        switch $_cfield {
        hour {
            if {$itcl_options(-iq) == "low"} {
                $time delete $icursor
                $time insert $icursor $char

            } elseif {$itcl_options(-format) == "military"} {
                if {$icursor == 0}  {
                    #
                    # if the digit is less than 2, then 
                    # the second hour digit is valid for 0-9
                    #
                    if {$char < 2} {
                        $time delete 0 1
                        $time insert 0 $char

                        #
                        # if the digit is equal to 2, then 
                        # the second hour digit is valid for 0-3
                        #
                    } elseif {$char == 2} {
                        $time delete 0 1
                        $time insert 0 $char

                        if {$hour2 > 3} {
                            $time delete 1 2
                            $time insert 1 "0"
                            $time icursor 1
                        }

                        #
                        # if the digit is greater than 2, then 
                        # set the first hour digit to 0 and the
                        # second hour digit to the value.
                        #
                    } elseif {$char > 2}  {
                        $time delete 0 2
                        $time insert 0 "0$char"
                        set icursor 1
                    } else {
                        set inValid 1
                    }

                    #
                    # if the insertion cursor is for the second hour digit, then
                    # format is military, then it can only be valid if the first
                    # hour digit is less than 2 or the new digit is less than 4
                    #
                } else {
                    if {$hour1 < 2 || $char < 4} {
                        $time delete 1 2
                        $time insert 1 $char
                    } else {
                        set inValid 1
                    }
                }

                #
                # The format is civilian, so we need to
                # validate the hour field which can be [01 - 12]
                #
            } else {
                if {$icursor == 0}  {
                    #
                    # if the digit is 0, then 
                    #   the second hour digit is valid for 1-9
                    #   so just insert it.
                    #
                    if {$char == 0 && $hour2 != 0} {
                        $time delete 0 1
                        $time insert 0 $char
  
                        #
                        # if the digit is equal to 1, then 
                        #   the second hour digit is valid for 0-2
                        #
                    } elseif {$char == 1} {
                        $time delete 0 1
                        $time insert 0 $char

                        if {$hour2 > 2} {
                            $time delete 1 2
                            $time insert 1 0
                            set icursor 1
                        }

                        #
                        # if the digit is greater than 1, then 
                        #   set the first hour digit to 0 and the
                        #   second hour digit to the value.
                        #
                    } elseif {$char > 1}  {
                        $time delete 0 2
                        $time insert 0 "0$char"
                        set icursor 1

                    } else {
                        set inValid 1
                    }

                    #
                    # The insertion cursor is at the second hour digit, so
                    # it can only be valid if the firs thour digit is 0
                    # or the new digit is less than or equal to 2
                    #
                } else {
                    if {$hour1 == 0 || $char <= 2} {
                        $time delete 1 2
                        $time insert 1 $char
                    } else {
                        set inValid 1
                    }
                }
            }
            if {$inValid} {
                bell
            } elseif {$icursor == 1} {
                _setField minute
            }
          }
        minute {
            if {$itcl_options(-iq) == "low" || $char < 6 || $icursor == 4} {
                $time delete $icursor
                $time insert $icursor $char
            } elseif {$itcl_options(-iq) == "high"} {
                if {$char > 5} {
                    $time delete 3 5
                    $time insert 3 "0$char"
                    set icursor 4
                }
            }
            if {$icursor == 4} {
                _setField second
            }
          }
        second {
            if {$itcl_options(-iq) == "low" || $char < 6 || $icursor == 7} {
                $time delete $icursor
                $time insert $icursor $char
            } elseif {$itcl_options(-iq) == "high"} {
                if {$char > 5} {
                    $time delete 6 8
                    $(time insert 6 "0$char"
                    set icursor 7
                }
            }
            if {$icursor == 7} {
                _moveField forward
            }
          }
        }
        set _timeVar [$time get]
        return -code break
    }

    #
    # Process the plus and the up arrow keys.  They both yield the same
    # effect, they increment the minute by one.
    #
    switch $sym {
    p -
    P {
        if {$itcl_options(-format) == "civilian"} {
            $time delete 9 10
            $time insert 9 P
            _setField hour
        }
      }
    a -
    A {
        if {$itcl_options(-format) == "civilian"} {
            $time delete 9 10
            $time insert 9 A
            _setField hour
        }
      }
    plus -
    Up {
        if {$_cfield == "ampm"} {
            _toggleAmPm
        } else {
            set newclicks [::clock scan "$prevtime 1 $_cfield"]
            show [::clock format $newclicks -format $_formatString]
          }
      }
    minus -
    Down {
        #
        # Process the minus and the down arrow keys which decrement the value
        # of the field in which the cursor is currently positioned.
        #
        if {$_cfield == "ampm"} {
            _toggleAmPm
        } else {
            set newclicks [::clock scan "$prevtime 1 $_cfield ago"]
            show [::clock format $newclicks -format $_formatString]
        }
      }
    Tab {
        #
        # A tab key moves the "hour:minute:second" field forward by one unless
        # the current field is the second.  In that case we'll let tab
        # do what is supposed to and pass the focus onto the next widget.
        #
        if {$state == 0} {
            if {($itcl_options(-format) == "civilian" && \
	            $_cfield == $lastField)} {
              _setField hour
              return -code continue
            }
            _moveField forward

          #
          # A ctrl-tab key moves the hour:minute:second field backwards by one 
          # unless the current field is the hour.  In that case we'll let 
          # tab take the focus to a previous widget.
          #
        } elseif {$state == 4} {
            if {$_cfield == "hour"} {
                _setField hour
                return -code continue
            }
            _moveField backward
        }
      }
    Right {
        #
        # A right arrow key moves the insert cursor to the right one.
        #
        $_forward
      }
    Left -
    BackSpace -
    Delete {
        #
        # A left arrow, backspace, or delete key moves the insert cursor 
        # to the left one.  This is what you expect for the left arrow
        # and since the whole widget always operates in overstrike mode,
        # it makes the most sense for backspace and delete to do the same.
        #
        $_backward
      }
    Return {
        #
        # A Return key invokes the optionally specified command option.
        #
        uplevel #0 $itcl_options(-command)
      }
    default {
      }
    }
    return -code break
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _toggleAmPm
#
# Internal method which toggles the displayed time
# between "AM" and "PM" when format is "civilian".
# ------------------------------------------------------------------
::itcl::body Timefield::_toggleAmPm {} {
    set firstChar  [string index $_timeVar 9]
    $time delete 9 10
    $time insert 9 [expr {($firstChar == "A") ? "P" : "A"}]
    $time icursor 9
    set _timeVar [$time get]
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _setField field
#
# Adjusts the current field to be that of the argument, setting the
# insert cursor appropriately.
# ------------------------------------------------------------------
::itcl::body Timefield::_setField {field} {
    # Move the position of the cursor to the first character of the
    # field given by the argument:
    #
    # Field   First Character Index
    # -----   ---------------------
    # hour    0
    # minute  3
    # second  6
    # ampm    9
    #
    switch $field {
    hour {
        $time icursor 0
      }
    minute {
        $time icursor 3
      }
    second {
        $time icursor 6
      }
    ampm {
        if {$itcl_options(-format) == "military"} {
            error "bad field: \"$field\", must be hour, minute or second"
        }
        $time icursor 9
      }
    default {
        if {$itcl_options(-format) == "military"} {
            error "bad field: \"$field\", must be hour, minute or second"
        } else {
            error "bad field: \"$field\", must be hour, minute, second or ampm"
        }
      }
    }
    set _cfield $field
    return $_cfield
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _moveField
#
# Moves the cursor one field forward or backward.
# ------------------------------------------------------------------
::itcl::body Timefield::_moveField {direction} {
    # Since the value "_fields" list variable is always either value:
    #   military => {hour minute second}
    #   civilian => {hour minute second ampm}
    #
    # the index of the previous or next field index can be determined
    # by subtracting or adding 1 to current the index, respectively.
    # 
    set index [lsearch $_fields $_cfield]
    expr {($direction == "forward") ? [incr index] : [incr index -1]}
    if {$index == $_numFields} {
        set index 0
    } elseif {$index < 0} {
        set index [expr {$_numFields-1}]
    }
    _setField [lindex $_fields $index]
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _whichField
#
# Returns the current field that the cursor is positioned within.
# ------------------------------------------------------------------
::itcl::body Timefield::_whichField {} {
    # Return the current field based on the position of the cursor.
    #
    # Field   Index
    # -----   -----
    # hour    0,1
    # minute  3,4
    # second  6,7
    # ampm    9,10
    #
    set icursor [$time index insert]
    switch $icursor {
    0 -
    1 {
        set _cfield hour
      }
    3 -
    4 {
        set _cfield minute
      }
    6 -
    7 {
        set _cfield second
      }
    9 -
    10 {
        set _cfield ampm
      }
    }
    return $_cfield
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _forwardCivilian
#
# Internal method which moves the cursor forward by one character
# jumping over the slashes and wrapping.
# ------------------------------------------------------------------
::itcl::body Timefield::_forwardCivilian {} {
    #
    # If the insertion cursor is at the second digit
    # of either the hour, minute or second field, then
    # move the cursor to the first digit of the right-most field.
    #
    # else move the insertion cursor right one character
    #
    set icursor [$time index insert]
    switch $icursor {
    1 {
        _setField minute
      }
    4 {
        _setField second
      }
    7 {
        _setField ampm
      }
    9 - 10 {
        _setField hour
      }
    default {
        $time icursor [expr {$icursor+1}]
      }
    }
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _forwardMilitary
#
# Internal method which moves the cursor forward by one character
# jumping over the slashes and wrapping.
# ------------------------------------------------------------------
::itcl::body Timefield::_forwardMilitary {} {
    #
    # If the insertion cursor is at the second digit of either
    # the hour, minute or second field, then move the cursor to
    # the first digit of the right-most field.
    #
    # else move the insertion cursor right one character
    #
    set icursor [$time index insert]
    switch $icursor {
    1 {
        _setField minute
      }
    4 {
        _setField second
      }
    7 {
        _setField hour
      }
    default {
        $time icursor [expr {$icursor+1}]
      }
    }
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _backwardCivilian
#
# Internal method which moves the cursor backward by one character
# jumping over the ":" and wrapping.
# ------------------------------------------------------------------
::itcl::body Timefield::_backwardCivilian {} {
    #
    # If the insertion cursor is at the first character
    # of either the minute or second field or at the ampm
    # field, then move the cursor to the second character
    # of the left-most field.
    #
    # else if the insertion cursor is at the first digit of the
    # hour field, then move the cursor to the first character
    # of the ampm field.
    #
    # else move the insertion cursor left one character
    #
    set icursor [$time index insert]
    switch $icursor {
    9 {
        _setField second
        $time icursor 7
      }
    6 {
        _setField minute
        $time icursor 4
      }
    3 {
        _setField hour
        $time icursor 1
      }
    0 {
        _setField ampm
        $time icursor 9
      }
    default {
        $time icursor [expr {$icursor-1}]
      }
    }
}

# ------------------------------------------------------------------
# PROTECTED METHOD: _backwardMilitary
#
# Internal method which moves the cursor backward by one character
# jumping over the slashes and wrapping.
# ------------------------------------------------------------------
::itcl::body Timefield::_backwardMilitary {} {
    #
    # If the insertion cursor is at the first digit of either
    # the minute or second field, then move the cursor to the
    # second character of the left-most field.
    #
    # else if the insertion cursor is at the first digit of the
    # hour field, then move the cursor to the second digit
    # of the second field.
    #
    # else move the insertion cursor left one character
    #
    set icursor [$time index insert]
    switch $icursor {
    6 {
        _setField minute
        $time icursor 4
      }
    3 {
        _setField hour
        $time icursor 1
      }
    0 {
        _setField second
        $time icursor 7
      }
    default {
        $time icursor [expr {$icursor-1}]
      }
    }
}

} ; # end ::itcl::widgets