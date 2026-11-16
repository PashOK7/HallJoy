#include "ui_activity.h"
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include "midi_settings_page.h"
#include "midi_output.h"
#include "backend.h"
#include "keyboard_ui.h"
#include "custom_page_surface.h"
#include "custom_page_controls.h"
#include "ui_theme.h"
#include "win_util.h"

namespace {
using namespace Gdiplus;
using namespace halljoy::midi;
constexpr COLORREF purple = RGB(177, 146, 241), blue = RGB(100, 190, 228);
enum Action { None, Reset, ShowFullCurve, Start, Stop, Expression, Advanced, Noise,
    SpeedField, VelocityField, AxisField, Point1, Point5 = Point1 + 4, Curve1, Curve5 = Curve1 + 4 };
struct State {
    CustomPageSurface surface;
    CustomPageScrollController scroll;
    std::shared_ptr<const Profile> displayed;
    MidiTelemetry live{};
    std::shared_ptr<const Profile> dragBase;
    std::optional<Profile> draft;
    unsigned depth = 0;
    int hot = None, pressed = None, drag = None;
    bool advanced = false, edited = false;
    POINT dragOrigin{};
    unsigned selectedPoint = 0;
    int field = None;
    HWND editor = nullptr;
    std::shared_ptr<const Profile> editBase;
    std::wstring feedback;
};
int S(HWND w, int value) { return WinUtil_ScalePx(w, value); }
Color C(COLORREF color, BYTE alpha = 255) { return Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color)); }
std::wstring Percent(int milli) {
    return std::to_wstring(milli / 10) + (milli % 10 ? L"." + std::to_wstring(milli % 10) : L"") + L"%";
}
struct Layout {
    HWND w;
    int left, right;
    explicit Layout(HWND window) : w(window) {
        RECT client{}; GetClientRect(w, &client);
        left = S(w, 16); right = std::max(left + S(w, 280), std::min<int>(client.right - S(w, 28), S(w, 946)));
    }
    RECT Row(int top, int bottom) const { return {left, S(w, top), right, S(w, bottom)}; }
    RECT Plot() const { return {left + S(w, 46), S(w, 80), right - S(w, 24), S(w, 306)}; }
    RECT Travel() const { return {left + S(w, 18), S(w, 516), right - S(w, 18), S(w, 544)}; }
    RECT Rect(int action) const {
        if (action >= Point1 && action <= Point5) {
            const int index = action - Point1;
            const int x = left + S(w, 14 + index * 45);
            return {x, S(w, 388), x + S(w, 40), S(w, 413)};
        }
        if (action >= SpeedField && action <= AxisField) {
            const int gap = S(w, 8), index = action - SpeedField;
            const int width = (right - left - S(w, 28) - 2 * gap) / 3;
            const int x = left + S(w, 14) + index * (width + gap);
            return {x, S(w, 343), x + width, S(w, 373)};
        }
        if (action == Reset) return {right - S(w, 94), S(w, 6), right, S(w, 34)};
        if (action == ShowFullCurve) return {right - S(w, 132), S(w, 418), right - S(w, 14), S(w, 445)};
        if (action == Expression) return {left + S(w, 14), S(w, 590), right - S(w, 14), S(w, 620)};
        if (action == Advanced) return Row(642, 672);
        if (action == Noise) return {left + S(w, 132), S(w, 706), right - S(w, 18), S(w, 736)};
        return {};
    }
    int Height(bool advanced) const { return S(w, advanced ? 789 : 690); }
};
void Text(HDC dc, RECT r, const std::wstring& text, bool muted = false, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS) {
    const auto previous = SelectObject(dc, GetStockObject(SYSTEM_FONT));
    CustomPage_DrawText(dc, text, r, muted ? UiTheme::Color_TextMuted() : UiTheme::Color_Text(), format);
    SelectObject(dc, previous);
}
void DrawNumberField(HWND w, Graphics& g, HDC dc, RECT field,
                     const std::wstring& text, bool hot, bool editing) {
    CustomPage_DrawRoundRect(g, field, UiTheme::Color_PanelBg(),
        editing ? blue : hot ? UiTheme::Color_TextMuted() : UiTheme::Color_Border(), REAL(S(w, 3)));
    InflateRect(&field, -S(w, 9), 0);
    if (!editing) Text(dc, field, text);
}
void Card(Graphics& g, RECT r) { CustomPage_DrawRoundRect(g, r, UiTheme::Color_ControlBg(), UiTheme::Color_Border(), 10); }
void Save(HWND w, State& st) {
    if (st.edited) { st.edited = false; KeyboardUI_SaveBindingsAfterUserChange(w); }
}
void Finish(HWND w, State& st, bool commit = false) {
    if (st.draft) {
        const auto current = Get();
        if (commit && st.dragBase == current && !SamePerformance(*current, *st.draft)) {
            Apply(*st.draft);
            st.edited = true;
        }
        st.draft.reset(); st.dragBase.reset();
        CustomPageSurface_MarkDirty(w, &st.surface);
    }
    st.drag = None; st.pressed = None;
    if (GetCapture() == w) ReleaseCapture();
    Save(w, st);
}
template<class Change> void ChangeValue(HWND w, State& st, Change change) {
    const auto current = Get();
    const auto& before = st.draft ? *st.draft : *current;
    Profile value = before; change(value);
    if (!ValidTouch(value) || (SamePerformance(value, before) && value.velocityAxisMaximum==before.velocityAxisMaximum)) return;
    if (st.draft) *st.draft = std::move(value);
    else { Apply(value); st.edited = true; }
    CustomPageSurface_MarkDirty(w, &st.surface);
}
std::wstring SpeedText(unsigned milli) {
    std::wstring text=std::to_wstring(milli/1000);
    if (const unsigned fraction=milli%1000) {
        auto decimals=std::to_wstring(1000+fraction).substr(1);
        while(decimals.back()==L'0')decimals.pop_back();
        text+=L"."+decimals;
    }
    return text;
}
bool ParseSpeed(const std::wstring& text,unsigned& milli) {
    unsigned whole=0, fraction=0, digits=0;
    bool decimal=false, hasDigit=false;
    for(wchar_t c:text) {
        if(c==L'.' && !decimal){decimal=true;continue;}
        if(c<L'0' || c>L'9')return false;
        hasDigit=true;
        if(decimal) {
            if(++digits>3)return false;
            fraction=fraction*10+(c-L'0');
        }else{
            whole=whole*10+(c-L'0');
            if(whole>1000000)return false;
        }
    }
    while(digits++<3)fraction*=10;
    if(!hasDigit || (whole==1000000 && fraction))return false;
    milli=whole*1000+fraction;
    return true;
}
REAL SpeedX(const RECT& plot,unsigned speed,const Profile& config) {
    return REAL(plot.left+(plot.right-plot.left)*double(speed)/CurveAxisMaximum(config));
}
PointF CurvePoint(const RECT& plot, unsigned point, const Profile& config) {
    return {SpeedX(plot,CurveSpeed(config,point),config),
        REAL(plot.bottom-(plot.bottom-plot.top)*CurveLevel(config,point)/1000.0)};
}
std::wstring FieldText(const Profile& config,unsigned point,int field) {
    if(field==VelocityField)return std::to_wstring((CurveLevel(config,point)*127+500)/1000);
    return SpeedText(field==AxisField?CurveAxisMaximum(config):CurveSpeed(config,point));
}
bool SetPoint(Profile& value,unsigned point,unsigned speed,int level,bool clamp) {
    const unsigned left=point?value.velocitySpeed[point-1]+1:0;
    const unsigned right=point<4?value.velocitySpeed[point+1]-1:1000000000u;
    const int bottom=point?value.velocityCurve[point-1]:0;
    const int top=point<4?value.velocityCurve[point+1]:1000;
    if(!clamp && (speed<left || speed>right || level<bottom || level>top))return false;
    value.velocitySpeed[point]=std::clamp(speed,left,right);
    value.velocityCurve[point]=std::clamp(level,bottom,top);
    return true;
}
constexpr UINT NumberFieldBlur = WM_APP + 394;
LRESULT CALLBACK NumberFieldProc(HWND edit, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR);

void EndField(HWND w, State& st, bool commit) {
    if (!st.editor) return;
    wchar_t input[12]{};
    GetWindowTextW(st.editor, input, static_cast<int>(std::size(input)));
    const auto editor = st.editor;
    const int field = st.field;
    st.editor = nullptr;
    st.field = None;
    RemoveWindowSubclass(editor, NumberFieldProc, 1);
    DestroyWindow(editor);
    const auto base=std::move(st.editBase);
    if(commit && base==Get()) {
        auto value=*base;
        if(field!=AxisField)MakeEditableCurve(value);
        unsigned amount=0;
        bool valid=ParseSpeed(input,amount);
        if(field==AxisField) {
            valid=valid && amount>0;
            if(valid)value.velocityAxisMaximum=amount;
        }else if(field==SpeedField) {
            valid=valid && SetPoint(value,st.selectedPoint,amount,value.velocityCurve[st.selectedPoint],false);
        }else{
            valid=valid && amount<=127000 && amount%1000==0 &&
                SetPoint(value,st.selectedPoint,value.velocitySpeed[st.selectedPoint],int((amount/1000*1000+63)/127),false);
        }
        if(valid && (FieldText(*base,st.selectedPoint,field)!=FieldText(value,st.selectedPoint,field))) {
            Apply(value);st.edited=true;Save(w,st);
        }
        st.feedback=valid?L"":field==AxisField?L"Enter an axis limit greater than zero.":L"Enter a value within the neighbouring points.";
    }
    CustomPageSurface_MarkDirty(w,&st.surface);
}
LRESULT CALLBACK NumberFieldProc(HWND edit, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    const auto page = GetParent(edit);
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE)) return 0;
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(page, GWLP_USERDATA));
        if (state) EndField(page, *state, wp == VK_RETURN);
        return 0;
    }
    if (msg == WM_KILLFOCUS) PostMessageW(page, NumberFieldBlur, 0, reinterpret_cast<LPARAM>(edit));
    return DefSubclassProc(edit, msg, wp, lp);
}

void BeginField(HWND w, State& st, int field, POINT pointer) {
    const auto profile = Get();
    auto rect = CustomPageSurface_ContentToClient(&st.surface, Layout(w).Rect(field));
    InflateRect(&rect, -S(w, 7), -S(w, 6));
    const auto editor = CreateWindowExW(0, L"EDIT", FieldText(*profile, st.selectedPoint, field).c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        w, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!editor) return;
    if (!SetWindowSubclass(editor, NumberFieldProc, 1, 0)) {
        DestroyWindow(editor);
        return;
    }
    st.editor = editor;
    st.field = field;
    st.editBase = profile;
    st.feedback.clear();
    SendMessageW(editor, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(SYSTEM_FONT)), FALSE);
    SendMessageW(editor, EM_SETLIMITTEXT, 11, 0);
    SetFocus(editor);
    MapWindowPoints(w, editor, &pointer, 1);
    RECT client{};
    GetClientRect(editor, &client);
    pointer.x = std::clamp(pointer.x, 0L, std::max(0L, client.right - 1));
    pointer.y = std::clamp(pointer.y, 0L, std::max(0L, client.bottom - 1));
    SendMessageW(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pointer.x, pointer.y));
    CustomPageSurface_MarkDirty(w, &st.surface);
}

void Render(HWND w, HDC dc, const RECT&, void* context) {
    auto& st = *static_cast<State*>(context);
    const auto current = Get(); const auto* config = st.draft ? &*st.draft : current.get();
    const Layout l(w);
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    Text(dc, l.Row(6, 34), L"MIDI velocity curve");
    CustomPage_DrawButton(g, dc, l.Rect(Reset), L"Reset curve", st.hot==Reset, st.pressed==Reset, true);
    Card(g, l.Row(43, 452));
    const RECT plot=l.Plot();
    const auto axis=CurveAxisMaximum(*config);
    const auto endSpeed = CurveSpeed(*config, 4);
    const bool curveClipped = endSpeed > axis;
    SolidBrush background(C(UiTheme::Color_PanelBg()));
    g.FillRectangle(&background, Rect(plot.left,plot.top,plot.right-plot.left,plot.bottom-plot.top));
    const auto first=CurvePoint(plot,0,*config),last=CurvePoint(plot,4,*config);
    SolidBrush zone(C(blue,25));
    g.FillRectangle(&zone, REAL(plot.left), REAL(plot.top),
        std::min(first.X, REAL(plot.right)) - plot.left, REAL(plot.bottom - plot.top));
    if (endSpeed < axis)
        g.FillRectangle(&zone, last.X, REAL(plot.top), REAL(plot.right) - last.X, REAL(plot.bottom - plot.top));
    Pen grid(C(UiTheme::Color_Border()),1);
    for(int i=0;i<=4;++i) {
        const int x=plot.left+(plot.right-plot.left)*i/4;
        const int y=plot.bottom-(plot.bottom-plot.top)*i/4;
        g.DrawLine(&grid,x,plot.top,x,plot.bottom);g.DrawLine(&grid,plot.left,y,plot.right,y);
        Text(dc,{l.left+S(w,3),y-S(w,9),plot.left-S(w,6),y+S(w,9)},
            std::to_wstring((127*i+2)/4),true,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        Text(dc,{x-S(w,34),S(w,311),x+S(w,34),S(w,330)},
            SpeedText(static_cast<unsigned>(std::uint64_t(axis)*i/4)),true,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }
    auto axisLabel=l.Row(49,71);axisLabel.left+=S(w,14);
    Text(dc,axisLabel,L"Velocity  /  Speed (key travels/s)",true);
    PointF points[8] = {{REAL(plot.left), REAL(plot.bottom)}};
    unsigned count = 1;
    for (unsigned i = 0; i < 5 && CurveSpeed(*config, i) <= axis; ++i) {
        const auto point = CurvePoint(plot, i, *config);
        if (i == 0) points[count++] = {point.X, REAL(plot.bottom)};
        points[count++] = point;
    }
    // Clip at the right edge by evaluating the response at that speed.
    if (endSpeed != axis) {
        const auto edgeY = !curveClipped ? last.Y
            : REAL(plot.bottom - (plot.bottom - plot.top) * VelocityForSpeed(axis, *config) / 127.0);
        points[count++] = {REAL(plot.right), edgeY};
    }
    GraphicsPath area;
    area.AddLines(points, count);
    area.AddLine(points[count - 1], PointF(REAL(plot.right), REAL(plot.bottom)));
    area.CloseFigure();
    SolidBrush fill(C(purple, 28));
    g.FillPath(&fill, &area);
    Pen curve(C(purple), REAL(S(w, 2)));
    g.DrawLines(&curve, points, count);
    for(unsigned i=0;i<5;++i) {
        if (CurveSpeed(*config, i) > axis) break;
        const auto point = CurvePoint(plot, i, *config);
        const bool selected=i==st.selectedPoint;
        const int radius=S(w,selected?7:5);
        SolidBrush handle(C(selected?UiTheme::Color_Text():purple));
        Pen border(C(UiTheme::Color_PanelBg()),REAL(S(w,2)));
        g.FillEllipse(&handle,point.X-radius,point.Y-radius,REAL(2*radius),REAL(2*radius));
        g.DrawEllipse(&border,point.X-radius,point.Y-radius,REAL(2*radius),REAL(2*radius));
    }
    for(const int field:{SpeedField,VelocityField,AxisField}) {
        static const wchar_t* labels[]={L"Point speed",L"Point velocity",L"X-axis limit"};
        RECT r=l.Rect(field),label=r;label.top-=S(w,16);label.bottom=r.top;
        Text(dc,label,labels[field-SpeedField],true);
        const bool editing = st.field == field;
        DrawNumberField(w, g, dc, r, FieldText(*config, st.selectedPoint, field), st.hot == field, editing);
    }
    for(int i=0;i<5;++i) {
        const auto action=Point1+i;
        CustomPage_DrawButton(g,dc,l.Rect(action),i==0?L"Start":i==4?L"End":std::to_wstring(i+1),
            st.hot==action || st.selectedPoint==unsigned(i),st.pressed==action,true);
    }
    auto helpText=l.Row(418,445);helpText.left+=S(w,14);helpText.right-=S(w,14);
    if (curveClipped) {
        const auto show = l.Rect(ShowFullCurve);
        helpText.right = show.left - S(w, 8);
        CustomPage_DrawButton(g, dc, show, L"Show full curve", st.hot == ShowFullCurve, st.pressed == ShowFullCurve, true);
    }
    const auto fieldHelp = st.field ? L"Enter to apply  ·  Esc to cancel"
        : !st.feedback.empty() ? st.feedback.c_str()
        : curveClipped ? L"Points off-screen" : L"Before Start: silent  ·  After End: capped at End velocity";
    Text(dc, helpText, fieldHelp, true);
    Card(g, l.Row(465, 630));
    RECT heading = l.Row(471, 491); heading.left += S(w, 14);
    Text(dc, heading, L"When a note starts and stops");
    RECT help = l.Row(493, 511); help.left += S(w, 14);
    Text(dc, help, L"Drag the two markers. Lift the key past the blue marker to play it again.", true);
    CustomPage_DrawCheckbox(g, dc, w, l.Rect(Expression), L"Depth expression (aftertouch)", config->depthPressure);
    CustomPage_DrawButton(g, dc, l.Rect(Advanced), st.advanced ? L"Hide advanced settings" : L"Advanced settings...", st.hot == Advanced, st.pressed == Advanced, true);
    if (st.advanced) {
        Card(g, l.Row(684, 773));
        RECT noiseLabel{l.left + S(w, 14), S(w, 706), l.left + S(w, 128), S(w, 736)};
        Text(dc, noiseLabel, L"Ignore tiny motion");
        CustomPage_DrawSlider(g, w, l.Rect(Noise), 0, std::min(50, config->attackMilli - config->releaseMilli - 1), config->noiseMilli);
        RECT details=l.Row(742,765);details.left+=S(w,14);
        Text(dc,details,L"Motion filter "+Percent(config->noiseMilli),true);
    }
}
void Overlay(HWND w, HDC dc, void* context) {
    auto& st = *static_cast<State*>(context); const Layout l(w);
    const auto current = Get(); const auto* config = st.draft ? &*st.draft : current.get();
    const auto LiveText = [&](RECT r, const std::wstring& text, bool muted = false) {
        Text(dc, CustomPageSurface_ContentToClient(&st.surface, r), text, muted);
    };
    RECT visible{}; GetClientRect(w, &visible); OffsetRect(&visible, 0, st.surface.scrollY);
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.TranslateTransform(0, REAL(-st.surface.scrollY));
    const RECT plot = l.Plot();
    if (visible.top < S(w,452) && visible.bottom > plot.top && st.live.touch.attacks) {
        const auto speed=st.live.touch.speedMilli;
        const auto velocity=st.live.touch.velocity;
        const bool onScale = speed <= CurveAxisMaximum(*config);
        if (onScale) {
            const REAL x = SpeedX(plot, speed, *config);
            const REAL y = REAL(plot.bottom - (plot.bottom - plot.top) * velocity / 127.0);
            SolidBrush dot(C(blue)); Pen guide(C(blue,120),1);
            guide.SetDashStyle(DashStyleDash); g.DrawLine(&guide,x,REAL(plot.bottom),x,y);
            g.FillEllipse(&dot,x-S(w,3),y-S(w,3),REAL(S(w,6)),REAL(S(w,6)));
        }
        auto result=l.Row(49,71);result.left=plot.left;result.right=plot.right;
        auto text=SpeedText(speed)+L" travels/s  ·  Velocity "+std::to_wstring(velocity);
        if (!onScale) text += L"  ·  Off scale";
        if(st.live.touch.quality>=TouchQuality::SparseInterval)text+=L"  ·  Limited samples";
        Text(dc,CustomPageSurface_ContentToClient(&st.surface,result),text,true,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }
    if (visible.top < S(w, 586) && visible.bottom > S(w, 503)) {
        const auto bar = l.Travel();
        SolidBrush rest(C(UiTheme::Color_PanelBg())), depth(C(purple, 105));
        g.FillRectangle(&rest, Rect(bar.left, bar.top, bar.right - bar.left, bar.bottom - bar.top));
        g.FillRectangle(&depth, Rect(bar.left, bar.top, (bar.right - bar.left) * int(st.depth) / 1000, bar.bottom - bar.top));
        for (const auto action : {Stop, Start}) {
            const int amount = action == Start ? config->attackMilli : config->releaseMilli;
            const int x = bar.left + (bar.right - bar.left) * amount / 1000;
            const auto color = action == Start ? purple : blue;
            Pen pen(C(color), REAL(S(w, 2))); g.DrawLine(&pen, x, bar.top - S(w, 6), x, bar.bottom + S(w, 6));
            const int y = action == Start ? bar.top - S(w, 4) : bar.bottom + S(w, 4), r = S(w, 5);
            SolidBrush knob(C(color)); g.FillEllipse(&knob, x - r, y - r, 2 * r, 2 * r);
        }
        RECT labels = l.Row(558, 582); labels.left += S(w, 14);
        LiveText(labels, L"Start " + Percent(config->attackMilli) + L"    Stop " + Percent(config->releaseMilli) + L"    Deepest bound key " + Percent(st.depth), true);
    }
}
int Hit(HWND w, State& st, POINT p) {
    const Layout l(w); const auto config = Get();
    const auto plot = l.Plot();
    int closest=None;
    double best=double(S(w,12))*S(w,12);
    for(unsigned i=0;i<5;++i) {
        if (CurveSpeed(*config, i) > CurveAxisMaximum(*config)) break;
        const auto point=CurvePoint(plot,i,*config);
        const double dx=p.x-point.X,dy=p.y-point.Y,distance=dx*dx+dy*dy;
        if(distance<best || (distance==best && i==st.selectedPoint)){best=distance;closest=Curve1+i;}
    }
    if(closest)return closest;
    const auto bar = l.Travel();
    if (p.x >= bar.left - S(w, 12) && p.x <= bar.right + S(w, 12) && p.y >= bar.top - S(w, 14) && p.y <= bar.bottom + S(w, 14)) {
        const int start = bar.left + (bar.right - bar.left) * config->attackMilli / 1000;
        const int stop = bar.left + (bar.right - bar.left) * config->releaseMilli / 1000;
        // Upper/lower handles remain individually reachable at close thresholds.
        return std::abs(p.x - start) == std::abs(p.x - stop) ? (p.y < (bar.top + bar.bottom) / 2 ? Start : Stop) : std::abs(p.x - start) < std::abs(p.x - stop) ? Start : Stop;
    }
    for (int a : std::array<int, 13>{Reset, ShowFullCurve, Expression, Advanced, Noise, SpeedField, VelocityField,
                                    AxisField, Point1, Point1 + 1, Point1 + 2, Point1 + 3, Point5}) {
        if (a == Noise && !st.advanced) continue;
        if (a == ShowFullCurve && CurveSpeed(*config, 4) <= CurveAxisMaximum(*config)) continue;
        RECT r = l.Rect(a); if (PtInRect(&r, p)) return a;
    }
    return None;
}
void Drag(HWND w, State& st, POINT p) {
    const Layout l(w);
    ChangeValue(w, st, [&](Profile& value) {
        if (st.drag >= Curve1 && st.drag <= Curve5) {
            const unsigned i=st.drag-Curve1;
            const auto plot=l.Plot();
            const auto axis=CurveAxisMaximum(value);
            const auto startSpeed=CurveSpeed(*st.dragBase,i);
            const auto startLevel=CurveLevel(*st.dragBase,i);
            const auto dx=std::llround(double(p.x-st.dragOrigin.x)*axis/(plot.right-plot.left));
            const auto dy=std::lround(1000.0*(st.dragOrigin.y-p.y)/(plot.bottom-plot.top));
            MakeEditableCurve(value);
            SetPoint(value,i,static_cast<unsigned>(std::clamp<long long>(startSpeed+dx,0,axis)),startLevel+int(dy),true);
            if(value.velocitySpeed[i]==startSpeed && value.velocityCurve[i]==startLevel)value=*st.dragBase;
        }
        else if (st.drag == Noise) value.noiseMilli = CustomPage_SliderValueFromPoint(w, l.Rect(Noise), 0, std::min(50, value.attackMilli - value.releaseMilli - 1), p.x);
        else {
            const auto bar = l.Travel(); const int milli = int(std::lround(1000.0 * (p.x - bar.left) / std::max(1L, bar.right - bar.left)));
            if (st.drag == Start) {
                const bool legacy=value.touchVersion==1;
                value.attackMilli=std::clamp(milli,std::max(legacy?100:1,value.releaseMilli+value.noiseMilli+1),legacy?800:1000);
            } else if (st.drag == Stop) value.releaseMilli = std::clamp(milli, 0, value.attackMilli - value.noiseMilli - 1);
        }
    });
}
void Activate(HWND w, State& st, int action) {
    st.feedback.clear();
    if (action == Advanced) { st.advanced = !st.advanced; CustomPageSurface_SetContentHeight(w, &st.surface, Layout(w).Height(st.advanced)); }
    else if(action>=Point1 && action<=Point5)st.selectedPoint=action-Point1;
    else ChangeValue(w,st,[&](Profile& value) {
        if(action==Expression)value.depthPressure=!value.depthPressure;
        if (action == ShowFullCurve) value.velocityAxisMaximum = CurveSpeed(value, 4);
        if(action==Reset) {
            const Profile factory;
            value.touchVersion=factory.touchVersion;
            value.velocitySpeed=factory.velocitySpeed;
            value.velocityCurve=factory.velocityCurve;
            value.velocityAxisMaximum=factory.velocityAxisMaximum;
        }
    });
    Save(w, st); CustomPageSurface_MarkDirty(w, &st.surface);
}
LRESULT CALLBACK Proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (msg == WM_CREATE) {
        st = new State; SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->displayed = Get(); CustomPageSurface_SetContentHeight(w, &st->surface, Layout(w).Height(false)); return 0;
    }
    if (!st) return DefWindowProcW(w, msg, wp, lp);
    if ((msg == WM_SHOWWINDOW && !wp) || msg == WM_CANCELMODE) {
        EndField(w, *st, true);
        Finish(w, *st);
    }
    if (msg == WM_MOUSEWHEEL || msg == WM_LBUTTONDOWN) EndField(w, *st, true);
    if (msg == WM_MOUSEWHEEL && st->drag != None) Finish(w, *st);
    const auto scroll = CustomPageSurface_HandleScrollMessage(w, &st->surface, &st->scroll, msg, wp, lp, 44);
    if (scroll != CustomPageScrollResult::NotHandled) return 0;
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT:
        CustomPageSurface_Paint(w, msg, wp, &st->surface, Render, st, st->scroll.draggingThumb, Overlay, st); return 0;
    case WM_SIZE:
        EndField(w, *st, true);
        Finish(w, *st); CustomPageSurface_SetContentHeight(w, &st->surface, Layout(w).Height(st->advanced));
        CustomPageSurface_MarkDirty(w, &st->surface); return 0;
    case WM_SHOWWINDOW:
        if (wp) { st->displayed = Get(); MidiSettingsPage_Tick(w); CustomPageSurface_MarkDirty(w, &st->surface); } return 0;
    case NumberFieldBlur:
        if (st->editor && reinterpret_cast<HWND>(lp) == st->editor && GetFocus() != st->editor)
            EndField(w, *st, true);
        return 0;
    case WM_CTLCOLOREDIT:
        if (reinterpret_cast<HWND>(lp) == st->editor) {
            const auto dc = reinterpret_cast<HDC>(wp);
            SetTextColor(dc, UiTheme::Color_Text());
            SetBkColor(dc, UiTheme::Color_PanelBg());
            return reinterpret_cast<LRESULT>(UiTheme::Brush_PanelBg());
        }
        break;
    case WM_APP_MIDI_SETTINGS_CHANGED:
        if(st->field && st->editBase!=Get())EndField(w,*st,false);
        if (st->draft && st->dragBase != Get()) Finish(w, *st);
        if (st->displayed != Get()) { st->displayed = Get(); CustomPageSurface_MarkDirty(w, &st->surface); }
        return 0;
    case WM_MOUSEMOVE: {
        POINT p = CustomPageSurface_ClientToContent(&st->surface, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
        if (st->drag != None) Drag(w, *st, p);
        else { const int hot = Hit(w, *st, p); if (hot != st->hot) { st->hot = hot; CustomPageSurface_MarkDirty(w, &st->surface); } }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, w, 0}; TrackMouseEvent(&track); return 0;
    }
    case WM_MOUSELEAVE: st->hot = None; CustomPageSurface_MarkDirty(w, &st->surface); return 0;
    case WM_LBUTTONDOWN: {
        POINT p = CustomPageSurface_ClientToContent(&st->surface, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
        const int action = Hit(w, *st, p);
        if (action >= SpeedField && action <= AxisField) {
            BeginField(w, *st, action, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            return 0;
        }
        if (!action) return 0;
        SetFocus(w); st->pressed = action; SetCapture(w);
        if (action == Start || action == Stop || action == Noise || action >= Curve1) {
            st->drag = action; st->feedback.clear();st->dragOrigin=p;
            if(action>=Curve1)st->selectedPoint=action-Curve1;
            st->dragBase = Get(); st->draft = *st->dragBase;
            if (action == Noise) Drag(w, *st, p);
        }
        CustomPageSurface_MarkDirty(w, &st->surface); return 0;
    }
    case WM_LBUTTONUP: {
        if (st->field && st->pressed == None) return 0;
        POINT p = CustomPageSurface_ClientToContent(&st->surface, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
        const int action = st->pressed; const bool click = st->drag == None && action && Hit(w, *st, p) == action;
        Finish(w, *st, true);
        if (click) Activate(w, *st, action);
        CustomPageSurface_MarkDirty(w, &st->surface); return 0;
    }
    case WM_CAPTURECHANGED: Finish(w, *st); return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT pointer{};
            int action = st->drag;
            if (action == None && GetCursorPos(&pointer) && ScreenToClient(w, &pointer))
                action = Hit(w, *st, CustomPageSurface_ClientToContent(&st->surface, pointer));
            if (action != None) {
                const auto cursor = action >= SpeedField && action <= AxisField ? IDC_IBEAM
                    : action >= Curve1 ? IDC_SIZEALL : IDC_HAND;
                SetCursor(LoadCursorW(nullptr, cursor));
                return TRUE;
            }
        }
        break;
    case WM_DESTROY:
        EndField(w,*st,false);Finish(w, *st); CustomPageSurface_Destroy(&st->surface); delete st; SetWindowLongPtrW(w, GWLP_USERDATA, 0); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}
}
HWND MidiSettingsPage_Create(HWND parent, HINSTANCE instance) {
    WNDCLASSW wc{}; wc.lpfnWndProc = Proc; wc.hInstance = instance;
    wc.lpszClassName = L"HallJoyMidiSettingsPage"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!GetClassInfoW(instance, wc.lpszClassName, &wc)) {
        wc.lpfnWndProc = Proc; wc.hInstance = instance; wc.lpszClassName = L"HallJoyMidiSettingsPage"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
    }
    return CreateWindowW(L"HallJoyMidiSettingsPage", L"", WS_CHILD | WS_CLIPCHILDREN, 0, 0, 100, 100, parent, nullptr, instance, nullptr);
}
void MidiSettingsPage_Tick(HWND page) {
    if (!HallJoyUiVisible(page) || !NeedsAnalog()) return;
    auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(page, GWLP_USERDATA)); if (!st) return;
    const auto config = Get();
    if (st->displayed != config) { st->displayed = config; CustomPageSurface_MarkDirty(page, &st->surface); }
    MidiTelemetry live; MidiOutput_GetTelemetry(live);
    unsigned depth = 0;
    for (std::uint16_t hid = 1; hid < config->keys.size(); ++hid)
        if (config->keys[hid]) depth = std::max(depth, unsigned(BackendUI_GetRawMilli(hid)));
    if (depth == st->depth && live.touch.attacks == st->live.touch.attacks && live.generatedActive == st->live.generatedActive) return;

    st->depth = depth; st->live = std::move(live);
    const Layout l(page);
    for (RECT r : {l.Row(49, 314), l.Row(503, 586)}) {
        r = CustomPageSurface_ContentToClient(&st->surface, r); InvalidateRect(page, &r, FALSE);
    }
}

#if defined(HALLJOY_ANALOG_SIMULATOR)
#include "test_thread_desktop.h"
bool MidiSettingsPage_TestInteractions() {
    return halljoy::test_desktop::RunOnPrivateDesktop(L"HJ-MidiSettings", [] {
        const auto original = Get();
        const Profile factory;
        Apply(factory);
        HWND owner = CreateWindowW(L"STATIC", L"", WS_OVERLAPPEDWINDOW, 0, 0, 700, 600, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND page = MidiSettingsPage_Create(owner, GetModuleHandleW(nullptr));
        if (!owner || !page) { if (owner) DestroyWindow(owner); Apply(*original); return false; }
        SetWindowPos(page, nullptr, 0, 0, 650, 520, SWP_NOZORDER);
        auto* st = reinterpret_cast<State*>(GetWindowLongPtrW(page, GWLP_USERDATA));
        const Layout l(page);
        auto mouse = [&](UINT msg, int x, int y) { SendMessageW(page, msg, 0, MAKELPARAM(x, y - st->surface.scrollY)); };
        const auto plot = l.Plot();
        auto click = [&](RECT control) {
            const int x = (control.left + control.right) / 2;
            const int y = (control.top + control.bottom) / 2;
            mouse(WM_LBUTTONDOWN, x, y);
            mouse(WM_LBUTTONUP, x, y);
        };
        auto allHandlesReachable = [&] {
            const auto current = Get();
            bool reachable = true;
            for (unsigned i = 0; i < 5; ++i) {
                const auto point = CurvePoint(plot, i, *current);
                mouse(WM_LBUTTONDOWN, int(point.X), int(point.Y));
                reachable &= st->drag == Curve1 + int(i) && st->selectedPoint == i;
                mouse(WM_LBUTTONUP, int(point.X), int(point.Y));
            }
            return reachable && current == Get();
        };
        bool ok = allHandlesReachable();
        Profile fixture; fixture.keys[4] = 61; fixture.port = L"Preserved exact route";
        fixture.velocitySpeed = {0, 25000, 50000, 75000, 100000};
        fixture.velocityAxisMaximum = 200000;
        Apply(fixture);
        auto middle = CurvePoint(plot, 2, *Get());
        mouse(WM_LBUTTONDOWN, int(middle.X), int(middle.Y));
        const int movedX=int(middle.X)+S(page,24);
        mouse(WM_MOUSEMOVE, movedX, plot.top-S(page,40));
        ok &= Get()->velocityCurve==fixture.velocityCurve && Get()->velocitySpeed==fixture.velocitySpeed &&
            st->draft && st->draft->velocitySpeed[2]>fixture.velocitySpeed[2] &&
            st->draft->velocityCurve[2]==fixture.velocityCurve[3];
        mouse(WM_LBUTTONUP, movedX, plot.top-S(page,40));
        ok &= Get()->velocityCurve[2]==fixture.velocityCurve[3] && Get()->velocitySpeed[2]>fixture.velocitySpeed[2] &&
            Get()->velocityAxisMaximum==fixture.velocityAxisMaximum && ValidTouch(*Get());
        for(unsigned i:{0u,1u,3u,4u})
            ok &= Get()->velocitySpeed[i]==fixture.velocitySpeed[i] && Get()->velocityCurve[i]==fixture.velocityCurve[i];
        const auto bar = l.Travel();
        const int start = bar.left + (bar.right - bar.left) * Get()->attackMilli / 1000;
        mouse(WM_LBUTTONDOWN, start, bar.top);
        mouse(WM_MOUSEMOVE, bar.left, bar.top); mouse(WM_LBUTTONUP, bar.left, bar.top);
        ok &= Get()->attackMilli==fixture.releaseMilli+fixture.noiseMilli+1 &&
            Get()->releaseMilli==fixture.releaseMilli && ValidTouch(*Get());
        RECT advanced = l.Rect(Advanced);
        mouse(WM_LBUTTONDOWN, advanced.left + 4, advanced.top + 4); mouse(WM_LBUTTONUP, advanced.left + 4, advanced.top + 4);
        ok &= st->advanced && st->surface.contentHeight == l.Height(true);
        // Scrolled client input still edits the intended content-space handle.
        CustomPageSurface_SetScrollY(page, &st->surface, S(page, 90));
        auto first = CurvePoint(plot, 1, *Get());
        mouse(WM_LBUTTONDOWN, int(first.X), int(first.Y));
        mouse(WM_MOUSEMOVE, int(first.X), plot.bottom); mouse(WM_LBUTTONUP, int(first.X), plot.bottom);
        ok &= Get()->velocityCurve[1]==0 && Get()->velocitySpeed[1]==fixture.velocitySpeed[1] && ValidTouch(*Get());
        auto firstEdge=CurvePoint(plot,0,*Get());
        mouse(WM_LBUTTONDOWN,int(firstEdge.X),int(firstEdge.Y));
        mouse(WM_MOUSEMOVE,int(firstEdge.X)+S(page,15),int(firstEdge.Y));
        mouse(WM_LBUTTONUP,int(firstEdge.X)+S(page,15),int(firstEdge.Y));
        ok &= Get()->velocitySpeed.front()>0;
        auto endEdge=CurvePoint(plot,4,*Get());
        mouse(WM_LBUTTONDOWN,int(endEdge.X),int(endEdge.Y));
        mouse(WM_MOUSEMOVE,int(endEdge.X)+S(page,18),int(endEdge.Y)+S(page,12));
        mouse(WM_LBUTTONUP,int(endEdge.X)+S(page,18),int(endEdge.Y)+S(page,12));
        ok &= Get()->velocitySpeed.back()>fixture.velocitySpeed.back() && Get()->velocityCurve.back()<1000;
        const auto curveBeforeAxis=*Get();
        const auto axisField = l.Rect(AxisField);
        const int fieldX = (axisField.left + axisField.right) / 2;
        const int fieldY = (axisField.top + axisField.bottom) / 2;
        mouse(WM_LBUTTONDOWN, fieldX, fieldY);
        ok &= st->field == AxisField && st->editor;
        if (st->editor) {
            const auto editor = st->editor;
            SendMessageW(editor, WM_LBUTTONUP, 0, 0);
            DWORD start = 0, end = 0;
            SendMessageW(editor, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
            ok &= start == end;
            SendMessageW(editor, EM_SETSEL, 1, 2);
            SendMessageW(editor, WM_CHAR, L'7', 0);
            wchar_t draft[12]{};
            GetWindowTextW(editor, draft, static_cast<int>(std::size(draft)));
            ok &= std::wstring(draft) == L"270" && Get()->velocityAxisMaximum == 200000;
            SendMessageW(editor, WM_KEYDOWN, VK_ESCAPE, 0);
        }
        ok &= !st->editor && Get()->velocityAxisMaximum == 200000;
        mouse(WM_LBUTTONDOWN, fieldX, fieldY);
        if (st->editor) {
            const auto editor = st->editor;
            SendMessageW(editor, WM_LBUTTONUP, 0, 0);
            SetWindowTextW(editor, L"70");
            SetFocus(page);
            SendMessageW(page, NumberFieldBlur, 0, reinterpret_cast<LPARAM>(editor));
        } else ok = false;
        ok &= !st->editor && Get()->velocityAxisMaximum == 70000 &&
            CurveAxisMaximum(*Get()) < CurveSpeed(*Get(), 4) && SamePerformance(*Get(), curveBeforeAxis);
        mouse(WM_LBUTTONDOWN, fieldX, fieldY);
        if (st->editor) {
            SendMessageW(st->editor, WM_LBUTTONUP, 0, 0);
            SetWindowTextW(st->editor, L"360");
            SendMessageW(st->editor, WM_KEYDOWN, VK_RETURN, 0);
        } else ok = false;
        ok &= !st->editor && Get()->velocityAxisMaximum == 360000 && SamePerformance(*Get(), curveBeforeAxis);
        Profile cropped = fixture;
        cropped.velocityAxisMaximum = 70000;
        Apply(cropped);
        const auto show = l.Rect(ShowFullCurve);
        const POINT showCenter{(show.left + show.right) / 2, (show.top + show.bottom) / 2};
        ok &= Hit(page, *st, showCenter) == ShowFullCurve;
        click(show);
        ok &= SamePerformance(*Get(), cropped) && Get()->velocityAxisMaximum == 100000 &&
            Hit(page, *st, showCenter) == None;
        CustomPageSurface_SetScrollY(page, &st->surface, 0);
        ok &= allHandlesReachable();
        ok &= Get()->port == fixture.port && Get()->keys == fixture.keys && Get()->enabled == fixture.enabled;
        ok &= Percent(50) == L"5%" && Percent(2) == L"0.2%";
        HDC dc = CreateCompatibleDC(nullptr); HBITMAP bitmap = CreateBitmap(650, 520, 1, 32, nullptr);
        const auto old = SelectObject(dc, bitmap);
        SendMessageW(page, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
        const auto builds = st->surface.cacheRebuilds;
        st->depth = 750; st->live.touch.attacks = 1; st->live.touch.speedMilli = 10000;
        st->live.touch.quality = TouchQuality::CapturedMotion;
        SendMessageW(page, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
        CustomPageSurface_SetScrollY(page, &st->surface, S(page, 140));
        SendMessageW(page, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
        ok &= st->surface.contentCache != nullptr && st->surface.cacheRebuilds == builds;
        SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
        CustomPageSurface_SetScrollY(page, &st->surface, 0);
        click(l.Rect(Reset));
        ok &= Get()->velocityAxisMaximum == 70000 && Get()->velocitySpeed == factory.velocitySpeed;
        ok &= allHandlesReachable();
        const auto end = CurvePoint(plot, 4, *Get());
        mouse(WM_LBUTTONDOWN, int(end.X), int(end.Y));
        mouse(WM_MOUSEMOVE, int(end.X) - S(page, 16), int(end.Y) + S(page, 14));
        mouse(WM_LBUTTONUP, int(end.X) - S(page, 16), int(end.Y) + S(page, 14));
        ok &= CurveSpeed(*Get(), 4) < 70000 && CurveLevel(*Get(), 4) < 1000 &&
            CurveAxisMaximum(*Get()) == 70000 && ValidTouch(*Get());
        DestroyWindow(owner); Apply(*original); return ok;
    });
}
#endif
