#include "epaper_dashboard.h"

#include <array>
#include <cstdio>
#include <ctime>

#include "esp_lvgl_port.h"
#include "settings.h"

namespace {
constexpr std::array<const char*, 7> kWeekdays = {"一", "二", "三", "四", "五", "六", "日"};

bool IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DaysInMonth(int year, int month) {
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 ? days[1] + (IsLeapYear(year) ? 1 : 0) : days[month - 1];
}

std::tm LocalTime() {
    const std::time_t now = std::time(nullptr);
    std::tm value = {};
    localtime_r(&now, &value);
    return value;
}
}  // namespace

EpaperDashboard::EpaperDashboard() = default;

EpaperDashboard::~EpaperDashboard() {
    if (overlay_ && lvgl_port_lock(1000)) {
        lv_obj_delete(overlay_);
        lvgl_port_unlock();
    }
}

const char* EpaperDashboard::PageName(Page page) {
    switch (page) {
        case Page::kHome: return "首页";
        case Page::kCalendar: return "日历";
        case Page::kSchedule: return "课程表";
        case Page::kAlbum: return "相册";
        case Page::kQuota: return "额度看板";
        default: return "首页";
    }
}

std::string EpaperDashboard::CurrentPageName() const { return PageName(page_); }

void EpaperDashboard::EnsureUi() {
    if (overlay_) return;
    overlay_ = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(overlay_);
    lv_obj_set_size(overlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(overlay_, 12, 0);

    title_ = lv_label_create(overlay_);
    lv_obj_set_width(title_, LV_PCT(100));
    lv_obj_set_style_text_color(title_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 0);

    body_ = lv_label_create(overlay_);
    lv_obj_set_size(body_, LV_PCT(100), 228);
    lv_label_set_long_mode(body_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(body_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_line_space(body_, 5, 0);
    lv_obj_align(body_, LV_ALIGN_TOP_LEFT, 0, 34);

    footer_ = lv_label_create(overlay_);
    lv_obj_set_width(footer_, LV_PCT(100));
    lv_obj_set_style_text_color(footer_, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_align(footer_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(footer_, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void EpaperDashboard::Show(Page page) {
    page_ = page;
    visible_ = true;
    hidden_for_voice_ = false;
    if (!lvgl_port_lock(3000)) return;
    EnsureUi();
    lv_obj_remove_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    Render();
    lvgl_port_unlock();
}

void EpaperDashboard::Show(const std::string& name) {
    if (name == "calendar" || name == "日历") Show(Page::kCalendar);
    else if (name == "schedule" || name == "timetable" || name == "课程表") Show(Page::kSchedule);
    else if (name == "album" || name == "相册") Show(Page::kAlbum);
    else if (name == "quota" || name == "额度") Show(Page::kQuota);
    else Show(Page::kHome);
}

void EpaperDashboard::Next() {
    auto next = (static_cast<int>(page_) + 1) % static_cast<int>(Page::kCount);
    Show(static_cast<Page>(next));
}

void EpaperDashboard::HideForVoice() {
    if (!visible_ || !overlay_) return;
    hidden_for_voice_ = true;
    if (lvgl_port_lock(3000)) {
        lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
        lvgl_port_unlock();
    }
}

void EpaperDashboard::RestoreAfterVoice() {
    if (!visible_ || !hidden_for_voice_) return;
    hidden_for_voice_ = false;
    Show(page_);
}

void EpaperDashboard::SetSchedule(const std::string& schedule) {
    Settings settings("epd_dash", true);
    settings.SetString("schedule", schedule);
    if (page_ == Page::kSchedule && visible_) Show(page_);
}

void EpaperDashboard::SetAlbumCaption(const std::string& caption) {
    Settings settings("epd_dash", true);
    settings.SetString("album_text", caption);
    if (page_ == Page::kAlbum && visible_) Show(page_);
}

void EpaperDashboard::SetQuota(double balance, double used, int requests, long long tokens) {
    Settings settings("epd_dash", true);
    settings.SetString("quota_balance", std::to_string(balance));
    settings.SetString("quota_used", std::to_string(used));
    settings.SetInt("quota_requests", requests);
    settings.SetString("quota_tokens", std::to_string(tokens));
    if (page_ == Page::kQuota && visible_) Show(page_);
}

std::string EpaperDashboard::RenderHome() const {
    auto now = LocalTime();
    char text[384];
    std::snprintf(text, sizeof(text),
                  "小悟智慧墨水屏\n\n%04d-%02d-%02d  %02d:%02d\n\n"
                  "日历 · 课程表 · 相册 · 额度\n\n"
                  "双击按键：切换信息页\n单击按键：开始语音交互",
                  now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
                  now.tm_hour, now.tm_min);
    return text;
}

std::string EpaperDashboard::RenderCalendar() const {
    auto now = LocalTime();
    const int year = now.tm_year + 1900;
    const int month = now.tm_mon + 1;
    std::tm first = now;
    first.tm_mday = 1;
    std::mktime(&first);
    const int monday_offset = (first.tm_wday + 6) % 7;
    std::string out;
    char header[48];
    std::snprintf(header, sizeof(header), "%04d 年 %02d 月\n", year, month);
    out += header;
    for (auto day : kWeekdays) { out += day; out += "   "; }
    out += "\n";
    for (int i = 0; i < monday_offset; ++i) out += "     ";
    for (int day = 1; day <= DaysInMonth(year, month); ++day) {
        char cell[12];
        std::snprintf(cell, sizeof(cell), day == now.tm_mday ? "[%2d] " : " %2d  ", day);
        out += cell;
        if ((monday_offset + day) % 7 == 0) out += "\n";
    }
    return out;
}

std::string EpaperDashboard::RenderSchedule() const {
    Settings settings("epd_dash", false);
    auto schedule = settings.GetString("schedule", "08:30  语文\n10:00  数学\n14:00  英语\n16:00  自习");
    return "今日课程\n\n" + schedule;
}

std::string EpaperDashboard::RenderAlbum() const {
    Settings settings("epd_dash", false);
    auto caption = settings.GetString("album_text", "还没有保存的照片");
    return "电子相册\n\n[ 图片展示区域 ]\n\n" + caption +
           "\n\n语音收到的图片仍可通过小智预览";
}

std::string EpaperDashboard::RenderQuota() const {
    Settings settings("epd_dash", false);
    const auto balance = settings.GetString("quota_balance", "--");
    const auto used = settings.GetString("quota_used", "--");
    const auto requests = settings.GetInt("quota_requests", 0);
    const auto tokens = settings.GetString("quota_tokens", "0");
    char text[320];
    std::snprintf(text, sizeof(text),
                  "通用 API 额度\n\n余额     %s\n已使用   %s\n请求数   %ld\n令牌数   %s\n\n"
                  "数据通过设备工具更新，不保存账号密码",
                  balance.c_str(), used.c_str(), static_cast<long>(requests), tokens.c_str());
    return text;
}

void EpaperDashboard::Render() {
    std::string body;
    switch (page_) {
        case Page::kCalendar: body = RenderCalendar(); break;
        case Page::kSchedule: body = RenderSchedule(); break;
        case Page::kAlbum: body = RenderAlbum(); break;
        case Page::kQuota: body = RenderQuota(); break;
        default: body = RenderHome(); break;
    }
    lv_label_set_text(title_, PageName(page_));
    lv_label_set_text(body_, body.c_str());
    lv_label_set_text(footer_, "小悟 · 400×300 四色电子纸");
}
