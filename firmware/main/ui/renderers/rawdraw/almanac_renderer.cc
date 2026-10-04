/**
 * @file almanac_renderer.cc
 * @brief Almanac page renderer - displays lunar date, solar terms, and almanac info
 *
 * Uses Calendar::ToLunarDate() for lunar calendar conversion (2000-2050).
 * Shows: today's Gregorian date, lunar date, solar term, weekday, and
 * traditional almanac info (yiji - auspicious/inauspicious activities).
 */

#include "almanac_renderer.h"
#include "rawdraw/rawdraw.h"
#include "rawdraw/style.h"
#include "rawdraw/layout_utils.h"  // FIX: 使用 InkCenteredTextTopYInBox 替代 line_height 居中
#include "rawdraw/components/calendar.h"
#include "rawdraw/theme.h"
#include <cstring>
#include <ctime>
#include <cstdio>

// External font references
extern const lv_font_t SourceHanSansSC_Regular_slim;
extern const lv_font_t SourceHanSansSC_Medium_slim;
extern const lv_font_t weather_icons_48;

// Weekday characters (matches calendar.cc)
static const char* kWeekdayFull[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// Lunar month/day names (same as calendar.cc)
static const char* kLunarMonths[] = {
    "M1", "M2", "M3", "M4", "M5", "M6",
    "M7", "M8", "M9", "M10", "M11", "M12"
};
static const char* kLunarDays[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
    "11", "12", "13", "14", "15", "16", "17", "18", "19", "20",
    "21", "22", "23", "24", "25", "26", "27", "28", "29", "30"
};

// Tian Gan / Di Zhi (used via Calendar::GetLunarYearName)
// static const char* kTianGan[] = ...;  // Not used directly, Calendar handles it
// static const char* kDiZhi[] = ...;

// Solar terms (same as calendar.cc)
struct SolarTermEntry {
    int month;
    int day;
    const char* name;
};
static const SolarTermEntry kSolarTerms[] = {
    { 1,  5, "Minor Cold" }, { 1, 20, "Major Cold" },
    { 2,  4, "Spring" }, { 2, 19, "Rain Water" },
    { 3,  5, "Insects" }, { 3, 20, "Equinox" },
    { 4,  4, "Qingming" }, { 4, 20, "Grain Rain" },
    { 5,  5, "Summer" }, { 5, 21, "Grain Buds" },
    { 6,  5, "Grain Ear" }, { 6, 21, "Solstice" },
    { 7,  7, "Minor Heat" }, { 7, 23, "Major Heat" },
    { 8,  7, "Autumn" }, { 8, 23, "End Heat" },
    { 9,  7, "White Dew" }, { 9, 23, "Equinox" },
    {10,  8, "Cold Dew" }, {10, 23, "Frost" },
    {11,  7, "Winter" }, {11, 22, "Minor Snow" },
    {12,  7, "Major Snow" }, {12, 22, "Solstice" },
};

static const char* GetSolarTerm(int month, int day) {
    for (size_t i = 0; i < sizeof(kSolarTerms) / sizeof(kSolarTerms[0]); i++) {
        if (kSolarTerms[i].month == month && kSolarTerms[i].day == day) {
            return kSolarTerms[i].name;
        }
    }
    return nullptr;
}

// Simplified yiji (宜忌) based on lunar day patterns
// This is a traditional approximation, not a full almanac calculation
static const char* kYiTable[][4] = {
    {"Offerings", "Prayer", "Travel", "Groundwork"},
    {"Wedding", "Betrothal", "Contracts", "Travel"},
    {"Open shop", "Trade", "Deeds", "Income"},
    {"Break ground", "Exhumation", "Burial", "Tomb repair"},
    {"Building", "Groundwork", "Lay base", "Foundations"},
    {"Set up bed", "Open shop", "Trade", "Deeds"},
    {"Offerings", "Bathing", "Cleaning", "Building"},
    {"Prayer", "Pray for kids", "Travel", "Cleansing"},
    {"Wedding", "Offerings", "Prayer", "Travel"},
    {"Open shop", "Deeds", "Trade", "Income"},
};
static const char* kJiTable[][3] = {
    {"Break ground", "Burial", "Exhumation"},
    {"Open storage", "Spending", "Harvest"},
    {"Lawsuits", "Arguments", "Gossip"},
    {"Wedding", "Travel", "Prayer"},
    {"Set up bed", "Moving", "Move in"},
    {"Offerings", "Building", "Groundwork"},
    {"Open shop", "Income", "Trade"},
    {"Travel", "Cleansing", "Demolition"},
    {"Break ground", "Exhumation", "Burial"},
    {"Betrothal", "Contracts", "Wedding"},
};

namespace rawdraw {

AlmanacRenderer::AlmanacRenderer()
    : font_(&SourceHanSansSC_Regular_slim)
    , title_font_(&SourceHanSansSC_Medium_slim)
    , icon_font_(&weather_icons_48) {
}

AlmanacRenderer::~AlmanacRenderer() = default;

void AlmanacRenderer::Init(int width, int height) {
    width_ = width;
    height_ = height;
    needs_full_refresh_ = true;
    RefreshData();
}

void AlmanacRenderer::RefreshData() {
    time_t now = time(nullptr);
    localtime_r(&now, &tm_);

    year_ = tm_.tm_year + 1900;
    month_ = tm_.tm_mon + 1;
    day_ = tm_.tm_mday;
    weekday_ = tm_.tm_wday;  // 0=Sun

    // Lunar date via Calendar algorithm
    lunar_ = Calendar::ToLunarDate(year_, month_, day_);
    lunar_year_name_ = Calendar::GetLunarYearName(year_);

    // Solar term
    solar_term_ = GetSolarTerm(month_, day_);

    // Yiji (宜忌) - simplified based on lunar day
    int yi_idx = (lunar_.lunar_day - 1) % 10;
    int ji_idx = (lunar_.lunar_day) % 10;
    yi_ = kYiTable[yi_idx];
    ji_ = kJiTable[ji_idx];
}

void AlmanacRenderer::Render(uint8_t* fb, int width, int height) {
    if (!fb) return;

    const int content_top = Style::kStatusBarHeight + kTitleBarH + Style::kSpacingXS;
    int y = content_top + Style::kSpacingMD;
    const auto& theme = ThemeManager::Get();
    const Color text = theme.ColorFor(ThemeToken::TextPrimary);
    const Color secondary = theme.ColorFor(ThemeToken::TextSecondary);
    const Color accent = theme.ColorFor(ThemeToken::Accent);
    const Color danger = theme.ColorFor(ThemeToken::Danger);
    const Color border = theme.ColorFor(ThemeToken::Border);

    // === Title bar ===
    DrawTitleBar(fb, width);

    // === Large lunar year name + date ===
    // e.g. "丙午年 三月初八"
    char lunar_full[32];
    if (lunar_.lunar_month > 0 && lunar_.lunar_day > 0) {
        snprintf(lunar_full, sizeof(lunar_full), "%s year %s %s",
                 lunar_year_name_, GetLunarMonthName(lunar_.lunar_month),
                 GetLunarDayName(lunar_.lunar_day));
    } else {
        snprintf(lunar_full, sizeof(lunar_full), "%s year", lunar_year_name_);
    }

    // Draw centered
    int lunar_w = MeasureTextWidth(lunar_full, title_font_);
    int lunar_x = (width - lunar_w) / 2;
    DrawText(fb, width, lunar_x, y, lunar_full, title_font_, accent);
    y += title_font_->line_height + Style::kSpacingMD;

    // === Gregorian date ===
    char greg_buf[64];
    snprintf(greg_buf, sizeof(greg_buf), "%d-%02d-%02d %s",
             year_, month_, day_, kWeekdayFull[weekday_]);
    int greg_w = MeasureTextWidth(greg_buf, font_);
    int greg_x = (width - greg_w) / 2;
    DrawText(fb, width, greg_x, y, greg_buf, font_, secondary);
    y += font_->line_height + Style::kSpacingMD;

    // === Solar term (if today) ===
    if (solar_term_) {
        char st_buf[32];
        snprintf(st_buf, sizeof(st_buf), "[%s]", solar_term_);
        int st_w = MeasureTextWidth(st_buf, title_font_);
        int st_x = (width - st_w) / 2;
        DrawText(fb, width, st_x, y, st_buf, title_font_, accent);
        y += title_font_->line_height + Style::kSpacingMD;
    }

    // === Divider ===
    DrawHLine(fb, width, y, Style::kSpacingLG, width - Style::kSpacingLG, border);
    y += Style::kSpacingSM;

    // === 宜 (auspicious) section ===
    DrawText(fb, width, Style::kSpacingLG, y, "Do", title_font_, accent);
    int yi_label_w = MeasureTextWidth("Do", title_font_);
    int yi_start = Style::kSpacingLG + yi_label_w + Style::kSpacingSM;
    int yi_y = y;
    // English words vary in width, so flow items by measured width
    int yi_x = yi_start;
    for (int i = 0; i < 4; i++) {
        const int item_w = MeasureTextWidth(yi_[i], font_);
        if (yi_x + item_w > width - Style::kSpacingLG) break;
        DrawText(fb, width, yi_x, yi_y, yi_[i], font_, text);
        yi_x += item_w + Style::kSpacingMD;
    }
    y += font_->line_height + Style::kSpacingMD;

    // === 忌 (inauspicious) section ===
    DrawText(fb, width, Style::kSpacingLG, y, "Avoid", title_font_, danger);
    int ji_label_w = MeasureTextWidth("Avoid", title_font_);
    int ji_start = Style::kSpacingLG + ji_label_w + Style::kSpacingSM;
    int ji_y = y;
    // English words vary in width, so flow items by measured width
    int ji_x = ji_start;
    for (int i = 0; i < 3; i++) {
        const int item_w = MeasureTextWidth(ji_[i], font_);
        if (ji_x + item_w > width - Style::kSpacingLG) break;
        DrawText(fb, width, ji_x, ji_y, ji_[i], font_, text);
        ji_x += item_w + Style::kSpacingMD;
    }

    needs_full_refresh_ = false;
}

void AlmanacRenderer::DrawTitleBar(uint8_t* fb, int width) {
    const auto& theme = ThemeManager::Get();
    const PaintStyle bar_style = theme.Style(ThemeToken::BackgroundSecondary);
    const Color text = theme.ColorFor(ThemeToken::TextPrimary);
    const Color border = theme.ColorFor(ThemeToken::Border);
    const int title_y_start = Style::kStatusBarHeight;
    const int title_bar_h = kTitleBarH;

    // Background
    DrawStyledRect(fb, width, {0, title_y_start, width, title_bar_h}, bar_style);

    // Top divider (2px)
    DrawHLine(fb, width, title_y_start, 0, width, border);
    DrawHLine(fb, width, title_y_start + 1, 0, width, border);

    // Bottom divider (2px)
    const int line_y = title_y_start + title_bar_h - 2;
    DrawHLine(fb, width, line_y, 0, width, border);
    DrawHLine(fb, width, line_y + 1, 0, width, border);

    // FIX: 改用 InkCenteredTextTopYInBox，避免 line_height 居中导致中文偏上
    // 参见 wiki/projects/notellm-baseline-alignment.md
    int title_text_y = InkCenteredTextTopYInBox(font_, "Almanac", title_y_start, title_bar_h, 1);
    DrawText(fb, width, Style::kSpacingLG, title_text_y, "Almanac", font_, text);
}

const char* AlmanacRenderer::GetLunarMonthName(int month) {
    if (month < 1 || month > 12) return "";
    return kLunarMonths[month - 1];
}

const char* AlmanacRenderer::GetLunarDayName(int day) {
    if (day < 1 || day > 30) return "";
    return kLunarDays[day - 1];
}

bool AlmanacRenderer::HandleInput(const ButtonEvent& event) {
    switch (event.type) {
        case ButtonEvent::kUpClick:
        case ButtonEvent::kDownClick:
            // Navigate months (UP=prev, DOWN=next)
            if (event.type == ButtonEvent::kUpClick) {
                month_--;
                if (month_ < 1) { month_ = 12; year_--; }
            } else {
                month_++;
                if (month_ > 12) { month_ = 1; year_++; }
            }
            lunar_ = Calendar::ToLunarDate(year_, month_, day_);
            solar_term_ = GetSolarTerm(month_, day_);
            needs_full_refresh_ = true;
            return true;

        case ButtonEvent::kBootLongPress:
            // Jump to today
            RefreshData();
            needs_full_refresh_ = true;
            return true;

        default:
            break;
    }
    return false;
}

}  // namespace rawdraw
