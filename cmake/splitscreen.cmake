# Experimental local guest, two-camera and controller build for PC and Switch.
# Regular builds retain their single-client behavior.
option(KISAK_EXPERIMENTAL_SPLITSCREEN "Build the experimental local split-screen prototype" OFF)
if(KISAK_EXPERIMENTAL_SPLITSCREEN)
    if(NOT KISAK_DIRECT_CONNECT)
        message(FATAL_ERROR "Split-screen requires KISAK_DIRECT_CONNECT=ON")
    endif()
    target_sources(${BIN_NAME} PRIVATE
        "${SRC_DIR}/client/splitscreen_input.cpp"
        "${SRC_DIR}/client/splitscreen_menu.cpp"
        "${SRC_DIR}/client/splitscreen_session.cpp"
        "${SRC_DIR}/ui/ui_splitscreen.cpp"
        "${SRC_DIR}/aim_assist/aim_splitscreen.cpp"
        "${SRC_DIR}/client/splitscreen_presentation.cpp"
        "${SRC_DIR}/client/splitscreen_prediction.cpp"
        "${SRC_DIR}/cgame/cg_splitscreen_hud.cpp"
        "${SRC_DIR}/cgame/cg_splitscreen_hud_mp.cpp"
        "${SRC_DIR}/EffectsCore/fx_shared_guest.cpp")
    target_compile_definitions(${BIN_NAME} PRIVATE KISAK_EXPERIMENTAL_SPLITSCREEN MAX_LOCAL_CLIENTS=2)
endif()
