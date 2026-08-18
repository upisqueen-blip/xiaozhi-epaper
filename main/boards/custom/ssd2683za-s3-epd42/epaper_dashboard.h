#pragma once

#include <lvgl.h>

#include <string>

class EpaperDashboard {
public:
    enum class Page { kHome, kCalendar, kSchedule, kAlbum, kQuota, kCount };

    EpaperDashboard();
    ~EpaperDashboard();

    void Show(Page page);
    void Show(const std::string& page_name);
    void Next();
    void HideForVoice();
    void RestoreAfterVoice();
    void SetSchedule(const std::string& schedule);
    void SetAlbumCaption(const std::string& caption);
    void SetQuota(double balance, double used, int requests, long long tokens);
    std::string CurrentPageName() const;

private:
    Page page_ = Page::kHome;
    lv_obj_t* overlay_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* body_ = nullptr;
    lv_obj_t* footer_ = nullptr;
    bool visible_ = false;
    bool hidden_for_voice_ = false;

    void EnsureUi();
    void Render();
    std::string RenderHome() const;
    std::string RenderCalendar() const;
    std::string RenderSchedule() const;
    std::string RenderAlbum() const;
    std::string RenderQuota() const;
    static const char* PageName(Page page);
};
