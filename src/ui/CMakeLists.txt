SET(SOURCES screens/ui_INICIO.c
    screens/ui_HOME.c
    screens/ui_CONFIG.c
    screens/ui_PROGRAMADO.c
    screens/ui_PROGRAMADO_2.c
    screens/ui_PROGRAMADO_3.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_logoj2f_png.c
    images/ui_img_bg1_png.c
    images/ui_img_gauge_velocidad_png.c
    images/ui_img_speed_indicator_png.c
    images/ui_img_settings_up_png.c
    images/ui_img_icn_wifi_png.c
    images/ui_img_icn_settings_png.c
    images/ui_img_settings_down_png.c
    images/ui_img_icn_run_png.c
    images/ui_img_icn_clock_png.c
    images/ui_img_icn_x2_png.c
    images/ui_img_icn_arrow_png.c
    images/ui_img_icn_arrow_left_png.c
    images/ui_img_icn_ok_png.c
    fonts/ui_font_Big.c
    fonts/ui_font_Big_2.c
    fonts/ui_font_Medium.c
    fonts/ui_font_Medium_26.c
    fonts/ui_font_Number.c
    fonts/ui_font_Small.c)

add_library(ui ${SOURCES})
