# Experimental direct-IP mode. Existing builds retain their default behavior.
option(KISAK_DIRECT_CONNECT "Build experimental direct-IP gameplay without Steam auth" OFF)
if(KISAK_DIRECT_CONNECT)
    target_compile_definitions(${BIN_NAME} PRIVATE OPENBLOPS_NO_STEAM_AUTH)
endif()
