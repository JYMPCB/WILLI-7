SET(SOURCES screens/ui_INICIO.c
    screens/ui_HOME.c
    screens/ui_CONFIG.c
    screens/ui_PROGRAMADO.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_logoj2f_png.c
    images/ui_img_rgb_png.c
    images/ui_img_gauge_velocidad_png.c
    images/ui_img_speed_indicator_png.c
    images/ui_img_icn_wifi_png.c
    images/ui_img_icn_settings_png.c
    images/ui_img_icn_reinit_png.c
    images/ui_img_icn_run_png.c
    images/ui_img_icn_clock_png.c
    images/ui_img_icn_update_png.c
    images/ui_img_icn_x2_png.c
    images/ui_img_txt_conectar_png.c
    images/ui_img_txt_desconectar_png.c
    images/ui_img_icn_actualizar_png.c
    images/ui_img_icn_series_png.c
    images/ui_img_icn_minus_png.c
    images/ui_img_icn_plus_png.c
    images/ui_img_icn_pasadas_png.c
    images/ui_img_icn_modo_png.c
    fonts/ui_font_Big.c
    fonts/ui_font_Big_2.c
    fonts/ui_font_Medium.c
    fonts/ui_font_Medium_26.c
    fonts/ui_font_Number.c
    fonts/ui_font_Small.c
    fonts/ui_font_Units.c)

add_library(ui ${SOURCES})
