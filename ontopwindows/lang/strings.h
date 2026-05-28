#pragma once

struct Strings {
    const wchar_t* main_title;
    const wchar_t* main_select_btn;
    const wchar_t* main_menu_btn;

    const wchar_t* menu_check_updates;
    const wchar_t* menu_about;

    const wchar_t* update_check_fail;
    const wchar_t* update_error_title;
    const wchar_t* update_version_fail;
    const wchar_t* update_available;
    const wchar_t* update_current;
    const wchar_t* update_download_prompt;
    const wchar_t* update_title;
    const wchar_t* update_up_to_date;

    const wchar_t* about_title;
    const wchar_t* about_version;
    const wchar_t* about_developer;
    const wchar_t* about_telegram;
    const wchar_t* about_license;

    const wchar_t* settings_title;
    const wchar_t* settings_slow_resize;
    const wchar_t* settings_fast_resize;
    const wchar_t* settings_click_through;
    const wchar_t* settings_crop;
    const wchar_t* settings_auto_update;
    const wchar_t* settings_on;
    const wchar_t* settings_off;
    const wchar_t* settings_ok;
    const wchar_t* settings_cancel;
    const wchar_t* settings_language;
    const wchar_t* lang_russian;
    const wchar_t* lang_english;
    const wchar_t* lang_spanish;
    const wchar_t* lang_ukrainian;
    const wchar_t* lang_french;
    const wchar_t* lang_german;
    const wchar_t* lang_polish;

    const wchar_t* select_title;
    const wchar_t* select_btn;
    const wchar_t* select_cancel;
    const wchar_t* select_window_title;

    const wchar_t* clone_title;
    const wchar_t* ctx_reset_window;
    const wchar_t* ctx_toggle_border;
    const wchar_t* ctx_return_main;
    const wchar_t* ctx_close;

    const wchar_t* crop_instruction;
};

extern const Strings* g_str;