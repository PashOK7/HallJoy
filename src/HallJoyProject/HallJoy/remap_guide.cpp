// remap_guide.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include "remap_guide.h"

using namespace Gdiplus;

// ============================================================================
// TEMP TUNING AREA (правь всё здесь, пересобирай, смотри визуально)
// ----------------------------------------------------------------------------
// Ниже — “пульт управления” иконкой Guide/Home.
// Все параметры максимально вынесены наружу: цвета, альфы, геометрия,
// смещения, скругления, толщины линий, коэффициенты размеров.
// ============================================================================
namespace RemapGuideCfg
{
    // ------------------------------------------------------------------------
    // [Глобальные / Общие]
    // ------------------------------------------------------------------------

    // Множитель для padRatio, который передаётся снаружи (из RemapIcons_DrawGlyphAA).
    // Если хочешь “общим тумблером” сделать домик крупнее/меньше относительно
    // квадрата иконки — меняй это (1.0 = как сейчас).
    static constexpr float kUserPadMul = 1.0f;

    // Минимальный/максимальный padRatio после умножения на kUserPadMul.
    // padRatio задаёт отступ круга (и всего содержимого) от границ квадрата.
    // БОЛЬШЕ padRatio => всё внутри меньше (больше “воздуха”).
    // МЕНЬШЕ padRatio => всё внутри больше (может уткнуться в края).
    static constexpr float kPadRatioMin = 0.06f;
    static constexpr float kPadRatioMax = 0.22f;

    // Смещение домика относительно центра круга по X (в долях ширины круга).
    // +X = вправо, -X = влево.
    // Полезно, если визуально домик кажется “не по центру”.
    static constexpr float kHouseOffsetX_R = 0.00f;

    // Смещение домика относительно центра круга по Y (в долях высоты круга).
    // +Y = вниз, -Y = вверх.
    // Обычно домик хочется немного ниже центра — это можно делать тут,
    // либо через kBodyYFromCenter_R (они суммируются по смыслу).
    static constexpr float kHouseOffsetY_R = -0.15f;

    // ------------------------------------------------------------------------
    // [Круг-кнопка: фон]
    // ------------------------------------------------------------------------

    // Цвет заливки фона кнопки в режиме brightFill=true (например при “bright” отрисовке).
    // Это НЕ цвет домика, это “пластик/кнопка” вокруг домика.
    static constexpr COLORREF kBaseFill_Bright = RGB(124, 132, 141);

    // Цвет заливки фона кнопки в режиме brightFill=false.
    static constexpr COLORREF kBaseFill_Dim = RGB(124, 132, 141);

    // Альфа (прозрачность) заливки фона: 255 = полностью непрозрачно.
    // Можешь уменьшать, если надо “легче/воздушнее”, но обычно держат 255.
    static constexpr BYTE     kBaseFill_A = 255;

    // ------------------------------------------------------------------------
    // [Круг-кнопка: обводка]
    // ------------------------------------------------------------------------

    // Цвет обводки круга (контур внешней кнопки).
    static constexpr COLORREF kBaseBorder_C = RGB(100, 106, 114);

    // Альфа обводки круга.
    // 235 означает “почти непрозрачно”, можно поднять до 255.
    static constexpr BYTE     kBaseBorder_A = 235;

    // Толщина обводки круга считается как max(kBaseBorderW_MinPx, round(s * kBaseBorderW_R))
    // где s = размер квадрата иконки (min(w,h)).
    // kBaseBorderW_R — относительная толщина (доля от размера).
    // Больше => толще контур, визуально “тяжелее”.
    static constexpr float kBaseBorderW_R = 0.06f;

    // Минимальная толщина обводки в пикселях (страховка на маленьких размерах).
    static constexpr float kBaseBorderW_MinPx = 1.0f;

    // ------------------------------------------------------------------------
    // [Домик: цвета/контур]
    // ------------------------------------------------------------------------

    // Цвет заливки домика при brightFill=true.
    static constexpr COLORREF kHomeFill_Bright = RGB(255, 212, 92);

    // Цвет заливки домика при brightFill=false (обычно “основной”).
    static constexpr COLORREF kHomeFill_Dim = RGB(255, 212, 92);

    // Альфа заливки домика.
    static constexpr BYTE     kHomeFill_A = 255;

    // Цвет контура домика (обводка крыши и корпуса).
    static constexpr COLORREF kHomeOutline_C = RGB(60, 60, 60);

    // Альфа контура домика.
    static constexpr BYTE     kHomeOutline_A = 240;

    // Толщина контура домика: max(kHomeStrokeW_MinPx, circle.Width * kHomeStrokeW_R)
    // где circle.Width — диаметр внутреннего круга.
    // Больше => контур домика толще, “мультяшнее/жирнее”.
    static constexpr float kHomeStrokeW_R = 0.055f;

    // Минимальная толщина контура домика в пикселях (для мелких иконок).
    static constexpr float kHomeStrokeW_MinPx = 1.2f;

    // ------------------------------------------------------------------------
    // [Домик: геометрия — основные размеры]
    // ------------------------------------------------------------------------

    // Ширина корпуса домика относительно диаметра круга.
    // Больше => домик шире (может упереться в круг).
    static constexpr float kHouseW_R = 0.54f;

    // Высота корпуса домика относительно диаметра круга.
    // Больше => домик выше (корпус “вытянут”).
    static constexpr float kHouseH_R = 0.34f;

    // Высота крыши (треугольника) относительно диаметра круга.
    // Больше => крыша выше/острее.
    static constexpr float kRoofH_R = 0.26f;

    // Смещение корпуса домика вниз от центра (относительно диаметра круга).
    // + больше => домик ниже (обычно это “красивее”, т.к. крыша визуально тяжёлая).
    static constexpr float kBodyYFromCenter_R = 0.05f;

    // Вынос крыши по бокам относительно ширины корпуса.
    // 0.10 = крыша шире корпуса на 10% с каждой стороны.
    // Больше => крыша шире и “нависает”.
    static constexpr float kRoofOverhang_R = 0.10f;

    // ------------------------------------------------------------------------
    // [Домик: скругления корпуса]
    // ------------------------------------------------------------------------

    // Радиус скругления углов корпуса как доля от высоты корпуса.
    // Например 0.18 => 18% от высоты корпуса.
    // Больше => более “пухлый” корпус.
    static constexpr float kBodyRadius_R = 0.18f;

    // Минимальный радиус скругления (пиксели) — чтобы не пропадало на маленьких размерах.
    static constexpr float kBodyRadius_MinPx = 2.0f;

    // Максимальный радиус скругления (пиксели) — чтобы корпус не стал “капсулой”.
    static constexpr float kBodyRadius_MaxPx = 8.0f;

    // ------------------------------------------------------------------------
    // [Дверь]
    // ------------------------------------------------------------------------

    // Рисовать дверь или нет.
    // Если дверь мешает читаемости на мелких размерах — выключи (false).
    static constexpr bool     kDrawDoor = true;

    // Ширина двери относительно ширины корпуса домика.
    static constexpr float    kDoorW_R = 0.25f;

    // Высота двери относительно высоты корпуса домика.
    static constexpr float    kDoorH_R = 0.6f;

    // Цвет заливки двери (обычно тёмный “вырез”).
    static constexpr COLORREF kDoorFill_C = RGB(25, 25, 25);

    // Альфа двери.
    static constexpr BYTE     kDoorFill_A = 255;

    // ------------------------------------------------------------------------
    // [Труба (chimney)]
    // ------------------------------------------------------------------------

    // Рисовать трубу или нет.
    static constexpr bool kDrawChimney = false;

    // Позиция трубы по X: body.X + houseW * kChimneyX_R
    // Больше => труба правее.
    static constexpr float kChimneyX_R = 0.12f;

    // Позиция трубы по Y: body.Y - roofH * kChimneyY_OverRoofH_R
    // Больше => труба выше (сильнее “вылезает” над крышей).
    static constexpr float kChimneyY_OverRoofH_R = 0.62f;

    // Ширина трубы относительно houseW.
    static constexpr float kChimneyW_R = 0.2f;

    // Высота трубы относительно roofH.
    static constexpr float kChimneyH_OverRoofH_R = 0.75f;

    // ------------------------------------------------------------------------
    // [Качество рендера]
    // ------------------------------------------------------------------------

    // HighQuality включает более дорогие режимы сглаживания/композитинга.
    // На иконках это обычно стоит оставлять true.
    static constexpr bool kHighQuality = true;
}

// ============================================================================
// Helpers
// ============================================================================
static inline Color GpFromColorRef(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void AddRoundRectPath(GraphicsPath& path, const RectF& r, float radius)
{
    float rad = std::max(0.0f, radius);
    float d = rad * 2.0f;
    if (d > r.Width)  d = r.Width;
    if (d > r.Height) d = r.Height;

    RectF arc(r.X, r.Y, d, d);

    path.Reset();
    path.AddArc(arc, 180, 90);
    arc.X = r.GetRight() - d;  path.AddArc(arc, 270, 90);
    arc.Y = r.GetBottom() - d; path.AddArc(arc, 0, 90);
    arc.X = r.X;               path.AddArc(arc, 90, 90);
    path.CloseFigure();
}

// ============================================================================
// Public
// ============================================================================
void RemapGuide_DrawGlyphAA(HDC hdc, const RECT& rc, bool brightFill, float padRatio, COLORREF homeAccentOverride)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 2 || h <= 2) return;

    Graphics g(hdc);

    if (RemapGuideCfg::kHighQuality)
    {
        g.SetSmoothingMode(SmoothingModeHighQuality);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetCompositingQuality(CompositingQualityHighQuality);
    }
    else
    {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetCompositingQuality(CompositingQualityHighQuality);
    }

    float s = (float)std::min(w, h);
    float x0 = (float)rc.left + (float)(w - (int)s) * 0.5f;
    float y0 = (float)rc.top + (float)(h - (int)s) * 0.5f;

    RectF tile(x0, y0, s, s);

    // padding (отступ от края квадрата до круга)
    float pr = padRatio * RemapGuideCfg::kUserPadMul;
    pr = std::clamp(pr, RemapGuideCfg::kPadRatioMin, RemapGuideCfg::kPadRatioMax);
    float pad = std::max(1.0f, std::round(s * pr));

    RectF circle = tile;
    circle.Inflate(-pad, -pad);

    // -------------------- Base circle --------------------
    COLORREF baseFill = brightFill ? RemapGuideCfg::kBaseFill_Bright : RemapGuideCfg::kBaseFill_Dim;
    SolidBrush brBase(GpFromColorRef(baseFill, RemapGuideCfg::kBaseFill_A));
    g.FillEllipse(&brBase, circle);

    float baseBw = std::max(RemapGuideCfg::kBaseBorderW_MinPx, std::round(s * RemapGuideCfg::kBaseBorderW_R));
    Pen penBase(GpFromColorRef(RemapGuideCfg::kBaseBorder_C, RemapGuideCfg::kBaseBorder_A), baseBw);
    penBase.SetLineJoin(LineJoinRound);
    g.DrawEllipse(&penBase, circle);

    // -------------------- House colors --------------------
    COLORREF homeFill = brightFill ? RemapGuideCfg::kHomeFill_Bright : RemapGuideCfg::kHomeFill_Dim;
    if (homeAccentOverride != CLR_INVALID) homeFill = homeAccentOverride;

    SolidBrush brHouse(GpFromColorRef(homeFill, RemapGuideCfg::kHomeFill_A));

    float houseStroke = std::max(RemapGuideCfg::kHomeStrokeW_MinPx, circle.Width * RemapGuideCfg::kHomeStrokeW_R);
    Pen penHouse(GpFromColorRef(RemapGuideCfg::kHomeOutline_C, RemapGuideCfg::kHomeOutline_A), houseStroke);
    penHouse.SetLineJoin(LineJoinRound);

    // -------------------- House geometry --------------------
    float cx = circle.X + circle.Width * 0.5f;
    float cy = circle.Y + circle.Height * 0.5f;

    cx += circle.Width * RemapGuideCfg::kHouseOffsetX_R;
    cy += circle.Height * RemapGuideCfg::kHouseOffsetY_R;

    float houseW = circle.Width * RemapGuideCfg::kHouseW_R;
    float houseH = circle.Height * RemapGuideCfg::kHouseH_R;
    float roofH = circle.Height * RemapGuideCfg::kRoofH_R;

    float bodyX = cx - houseW * 0.5f;
    float bodyY = cy + circle.Height * RemapGuideCfg::kBodyYFromCenter_R;
    RectF body(bodyX, bodyY, houseW, houseH);

    // Roof triangle
    float roofOver = houseW * RemapGuideCfg::kRoofOverhang_R;
    PointF roofPts[3] =
    {
        PointF(body.X - roofOver, body.Y),
        PointF(cx, body.Y - roofH),
        PointF(body.GetRight() + roofOver, body.Y),
    };

    GraphicsPath roofPath;
    roofPath.AddPolygon(roofPts, 3);

    g.FillPath(&brHouse, &roofPath);
    g.DrawPath(&penHouse, &roofPath);

    // Body rounded rect
    {
        float rad = body.Height * RemapGuideCfg::kBodyRadius_R;
        rad = std::clamp(rad, RemapGuideCfg::kBodyRadius_MinPx, RemapGuideCfg::kBodyRadius_MaxPx);

        GraphicsPath p;
        AddRoundRectPath(p, body, rad);
        g.FillPath(&brHouse, &p);
        g.DrawPath(&penHouse, &p);
    }

    // Door
    if (RemapGuideCfg::kDrawDoor)
    {
        float doorW = houseW * RemapGuideCfg::kDoorW_R;
        float doorH = houseH * RemapGuideCfg::kDoorH_R;
        RectF door(cx - doorW * 0.5f, body.GetBottom() - doorH, doorW, doorH);

        SolidBrush brDoor(GpFromColorRef(RemapGuideCfg::kDoorFill_C, RemapGuideCfg::kDoorFill_A));
        g.FillRectangle(&brDoor, door);
    }

    // Chimney
    if (RemapGuideCfg::kDrawChimney)
    {
        float chimX = body.X + houseW * RemapGuideCfg::kChimneyX_R;
        float chimY = body.Y - roofH * RemapGuideCfg::kChimneyY_OverRoofH_R;
        float chimW = houseW * RemapGuideCfg::kChimneyW_R;
        float chimH = roofH * RemapGuideCfg::kChimneyH_OverRoofH_R;

        RectF chim(chimX, chimY, chimW, chimH);
        g.FillRectangle(&brHouse, chim);
        g.DrawRectangle(&penHouse, chim);
    }
}