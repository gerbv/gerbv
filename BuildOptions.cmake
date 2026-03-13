
option(DEBUG_PRINTOUT "Print out debug messages as gerbv process files" FALSE)

# Default border coefficient for export
if(NOT DEFINED GERBV_DEFAULT_BORDER_COEFF)
    set(GERBV_DEFAULT_BORDER_COEFF 0.05)
endif()

# Default unit to display in statusbar
# Possible values are "GERBV_MILS", "GERBV_MMS" or "GERBV_INS"
if(NOT DEFINED GERBV_DEFAULT_UNIT)
    set(GERBV_DEFAULT_UNIT "GERBV_MILS")
endif()
