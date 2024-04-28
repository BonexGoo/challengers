#include <boss.hpp>
#include "challengers.hpp"

#include <resource.hpp>
#include <service/boss_zaywidget.hpp>
#include <format/boss_bmp.hpp>
#include <format/boss_flv.hpp>

ZAY_DECLARE_VIEW_CLASS("challengersView", challengersData)

extern sint32 gProgramWidth;
extern sint32 gProgramHeight;
extern String gFirstWidget;
extern sint32 gFirstPosX;
extern sint32 gFirstPosY;
extern sint32 gFirstScale;
extern sint32 gPythonAppID;
extern sint32 gPythonPort;
extern h_view gWindowView;

ZAY_VIEW_API OnCommand(CommandType type, id_share in, id_cloned_share* out)
{
    if(type == CT_Create)
    {
        // DOM로드
        const uint64 CurMsec = Platform::Utility::CurrentTimeMsec();
        m->CheckDOM(CurMsec);

        // 시작위젯처리
        if(0 < gFirstWidget.Length())
            m->mReservedEnterWidget = m->FindWidget(gFirstWidget);
    }
    if(type == CT_Size)
    {
        sint32s WH(in);
        gProgramWidth = WH[0];
        #if BOSS_WINDOWS
            gProgramHeight = WH[1] - 1;
        #else
            gProgramHeight = WH[1];
        #endif
        ZayWidgetDOM::SetValue("program.width", String::FromInteger(gProgramWidth));
        ZayWidgetDOM::SetValue("program.height", String::FromInteger(gProgramHeight));
    }
    else if(type == CT_Activate && !boolo(in).ConstValue())
        m->clearCapture();
    else if(type == CT_Tick)
    {
        // 예약된 위젯호출 또는 DOM확인
        const uint64 CurMsec = Platform::Utility::CurrentTimeMsec();
        if(m->mReservedEnterWidget != -1)
        {
            m->EnterWidget(m->mReservedEnterWidget);
            m->mReservedEnterWidget = -1;
        }
        else if(m->mReservedChangeWidget != -1)
        {
            m->ChangeWidget(m->mReservedChangeWidget);
            m->mReservedChangeWidget = -1;
        }
        else if(m->CheckDOM(CurMsec))
            m->invalidate();

        // 파이썬연결(최초한번)
        if(gPythonPort != 0)
        {
            id_socket NewSocket = Platform::Socket::OpenForWS(false);
            if(!Platform::Socket::Connect(NewSocket, "127.0.0.1", gPythonPort))
                Platform::Socket::Close(NewSocket);
            else
            {
                m->mPython = NewSocket;
                m->PythonSend(String::Format("init,%d", gPythonAppID));
            }
            gPythonPort = 0;
        }

        // Update처리
        if(m->mUpdateMsec != 0)
        {
            if(m->mUpdateMsec < CurMsec)
                m->mUpdateMsec = 0;
            m->invalidate(2);
        }

        // HtmlView처리
        if(const sint32 HtmlCount = m->mHtmlViewes.Count())
        {
            for(sint32 i = HtmlCount - 1; 0 <= i; --i)
            {
                chararray ViewID;
                if(auto CurHtml = m->mHtmlViewes.AccessByOrder(i, &ViewID))
                if(CurHtml->mLiveMsec < CurMsec)
                {
                    m->mHtmlViewes.Remove(&ViewID[0]);
                    ZayWidgetDOM::SetValue("program.html", String::FromInteger(m->mHtmlViewes.Count()));
                }
            }
            m->invalidate(2);
        }

        // 위젯 틱실행
        if(m->mWidgetMain)
        {
            if(m->mWidgetMain->TickOnce())
                m->invalidate();
        }
        else if(m->mWidgetIntro.TickOnce())
            m->invalidate();
    }
}

ZAY_VIEW_API OnNotify(NotifyType type, chars topic, id_share in, id_cloned_share* out)
{
    if(type == NT_Normal)
    {
        if(!String::Compare(topic, "Update"))
        {
            const uint64 Msec = (in)? Platform::Utility::CurrentTimeMsec() + sint32o(in).ConstValue() : 1;
            if(m->mUpdateMsec < Msec)
                m->mUpdateMsec = Msec;
        }
        else if(!String::Compare(topic, "Maximize"))
        {
            if(m->IsFullScreen())
                m->SetNormalWindow();
            else m->SetFullScreen();
        }
        else if(!String::Compare(topic, "ClearCapture"))
        {
            m->clearCapture();
        }
    }
    else if(type == NT_KeyPress)
    {
        const sint32 KeyCode = sint32o(in).ConstValue();
        switch(KeyCode)
        {
        #if BOSS_ANDROID
            case 0: // Android BackButton
                if(m->mWidgetMain)
                {
                    m->ExitWidget();
                    m->invalidate();
                }
                else Platform::Utility::ExitProgram();
                break;
        #endif

        case 27: // Escape
            if(m->mWidgetMain)
            {
                m->ExitWidget();
                m->invalidate();
            }
            break;
        }
        // 파이썬 키처리
        if(m->mPython != nullptr)
        {
            const String KeyText = m->FindPythonKey(KeyCode);
            if(0 < KeyText.Length())
                m->PythonSend(String::Format("call,KeyPress_%s", (chars) KeyText));
        }
    }
    else if(type == NT_KeyRelease)
    {
        const sint32 KeyCode = sint32o(in).ConstValue();
        // 파이썬 키처리
        if(m->mPython != nullptr)
        {
            const String KeyText = m->FindPythonKey(KeyCode);
            if(0 < KeyText.Length())
                m->PythonSend(String::Format("call,KeyRelease_%s", (chars) KeyText));
        }
    }
    else if(type == NT_SocketReceive)
    {
        if(topic == "message")
            m->TryPythonRecvOnce();
        else if(topic == "disconnected")
        {
            // 파이썬 접속상태확인
            if(m->mPython && !Platform::Socket::IsConnected(m->mPython))
            {
                Platform::Socket::Close(m->mPython);
                m->mPython = nullptr;
                Platform::Utility::ExitProgram();
            }
        }
    }
    else if(type == NT_ZayWidget)
    {
        if(!String::Compare(topic, "SetCursor"))
        {
            auto Cursor = CursorRole(sint32o(in).Value());
            m->SetCursor(Cursor);
        }
    }
    else if(type == NT_ZayAtlas)
    {
        if(!String::Compare(topic, "AtlasUpdated"))
        {
            const String AtlasJson = R::PrintUpdatedAtlas();
            m->mWidgetIntro.UpdateAtlas(AtlasJson);
            if(m->mWidgetMain)
                m->mWidgetMain->UpdateAtlas(AtlasJson);
        }
    }
}

ZAY_VIEW_API OnGesture(GestureType type, sint32 x, sint32 y)
{
    static point64 OldCursorPos;
    static rect128 OldWindowRect;
    static uint64 ReleaseMsec = 0;

    if(type == GT_Moving || type == GT_MovingIdle)
        m->SetCursor(CR_Arrow);
    else if(type == GT_Pressed)
    {
        Platform::Utility::GetCursorPos(OldCursorPos);
        OldWindowRect = Platform::GetWindowRect();
        m->clearCapture();
    }
    else if(type == GT_InDragging || type == GT_OutDragging)
    {
        if(!m->IsFullScreen())
        {
            point64 CurCursorPos;
            Platform::Utility::GetCursorPos(CurCursorPos);
            Platform::SetWindowRect(
                OldWindowRect.l + CurCursorPos.x - OldCursorPos.x,
                OldWindowRect.t + CurCursorPos.y - OldCursorPos.y,
                OldWindowRect.r - OldWindowRect.l, OldWindowRect.b - OldWindowRect.t);
            m->mIsWindowMoving = true;
        }
        ReleaseMsec = 0;
        m->invalidate();
    }
    else if(type == GT_InReleased || type == GT_OutReleased || type == GT_CancelReleased)
    {
        const uint64 CurReleaseMsec = Platform::Utility::CurrentTimeMsec();
        const bool HasDoubleClick = (CurReleaseMsec < ReleaseMsec + 300);
        if(HasDoubleClick)
        {
            if(m->IsFullScreen())
                m->SetNormalWindow();
            else m->SetFullScreen();
        }
        else ReleaseMsec = CurReleaseMsec;
        m->mIsWindowMoving = false;
    }
}

ZAY_VIEW_API OnRender(ZayPanel& panel)
{
    ZAY_XYWH(panel, 0, 0, gProgramWidth, gProgramHeight)
    {
        if(m->mWidgetMain)
        {
            #if !BOSS_ANDROID
                auto Scale = ZayWidgetDOM::GetValue("program.scale").ToInteger();
                ZAY_ZOOM(panel, Scale / 100.0)
            #endif
            {
                m->mWidgetMain->Render(panel);
            }

            // 윈도우가 움직일때 좌표알림
            if(m->mIsWindowMoving)
            {
                auto WindowRect = Platform::GetWindowRect();
                ZAY_RGB(panel, 0, 0, 0)
                ZAY_ZOOM_CLEAR(panel)
                    panel.text(10, 10, String::Format("(%dx%d)", WindowRect.l, WindowRect.t), UIFA_LeftTop);
            }
        }
        // 위젯포커스가 없을 때
        else m->mWidgetIntro.Render(panel);

        // 아웃라인
        if(!m->IsFullScreen())
            m->RenderWindowOutline(panel);
    }
}

challengersData::challengersData()
{
    String DateText = __DATE__;
    DateText.Replace("Jan", "01"); DateText.Replace("Feb", "02"); DateText.Replace("Mar", "03");
    DateText.Replace("Apr", "04"); DateText.Replace("May", "05"); DateText.Replace("Jun", "06");
    DateText.Replace("Jul", "07"); DateText.Replace("Aug", "08"); DateText.Replace("Sep", "09");
    DateText.Replace("Oct", "10"); DateText.Replace("Nov", "11"); DateText.Replace("Dec", "12");
    const String Day = String::Format("%02d", Parser::GetInt(DateText.Middle(2, DateText.Length() - 6).Trim()));
    DateText = DateText.Right(4) + "/" + DateText.Left(2) + "/" + Day;
    ZayWidgetDOM::SetValue("program.update", "'" + DateText + "'");

    SetTitle(nullptr);
    ZayWidgetDOM::SetValue("program.width", String::FromInteger(gProgramWidth));
    ZayWidgetDOM::SetValue("program.height", String::FromInteger(gProgramHeight));
    ZayWidgetDOM::SetValue("program.scale", "100");
    ZayWidgetDOM::SetValue("program.flexible", "1");
    ZayWidgetDOM::SetValue("program.connect", "'x'");
    ZayWidgetDOM::SetValue("program.html", "0");
    ZayWidgetDOM::SetValue("program.mute", "0");
    ZayWidgetDOM::SetValue("program.pass1.recv", "''");
    ZayWidgetDOM::SetValue("program.pass2.recv", "''");

    // 인트로위젯
    InitWidget(mWidgetIntro, "intro");
    mWidgetIntro.Reload("widget/intro.json");
    mWidgetIntro.UpdateAtlas(R::PrintUpdatedAtlas(true));
}

challengersData::~challengersData()
{
    delete mWidgetMain;
    for(sint32 i = 0; i < 4; ++i)
        Platform::Sound::Close(mSounds[i]);
    Platform::Socket::Close(mPython);
}

void challengersData::SetCursor(CursorRole role)
{
    if(mNowCursor != role)
    {
        mNowCursor = role;
        Platform::Utility::SetCursor(role);
        if(mNowCursor != CR_SizeVer && mNowCursor != CR_SizeHor && mNowCursor != CR_SizeBDiag && mNowCursor != CR_SizeFDiag && mNowCursor != CR_SizeAll)
        {
            mNcLeftBorder = false;
            mNcTopBorder = false;
            mNcRightBorder = false;
            mNcBottomBorder = false;
        }
    }
}

sint32 challengersData::MoveNcLeft(const rect128& rect, sint32 addx)
{
    const sint32 NewLeft = rect.l + addx;
    return rect.r - Math::Max(mMinWindowWidth, rect.r - NewLeft);
}

sint32 challengersData::MoveNcTop(const rect128& rect, sint32 addy)
{
    const sint32 NewTop = rect.t + addy;
    return rect.b - Math::Max(mMinWindowHeight, rect.b - NewTop);
}

sint32 challengersData::MoveNcRight(const rect128& rect, sint32 addx)
{
    const sint32 NewRight = rect.r + addx;
    return rect.l + Math::Max(mMinWindowWidth, NewRight - rect.l);
}

sint32 challengersData::MoveNcBottom(const rect128& rect, sint32 addy)
{
    const sint32 NewBottom = rect.b + addy;
    return rect.t + Math::Max(mMinWindowHeight, NewBottom - rect.t);
}

void challengersData::RenderWindowOutline(ZayPanel& panel)
{
    ZAY_INNER(panel, 1)
    ZAY_RGBA(panel, 0, 0, 0, 32)
        panel.rect(1);

    if(ZayWidgetDOM::GetValue("program.flexible").ToInteger() == 0)
        return;

    // 리사이징바
    ZAY_RGBA(panel, 0, 0, 0, 64 + 128 * Math::Abs(((frame() * 2) % 100) - 50) / 50)
    {
        if(mNcLeftBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, i, 0, 1, panel.h())
                panel.fill();
            invalidate(2);
        }
        if(mNcTopBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, 0, i, panel.w(), 1)
                panel.fill();
            invalidate(2);
        }
        if(mNcRightBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, panel.w() - 1 - i, 0, 1, panel.h())
                panel.fill();
            invalidate(2);
        }
        if(mNcBottomBorder)
        {
            for(sint32 i = 0; i < 5; ++i)
            ZAY_RGBA(panel, 128, 128, 128, 128 - i * 25)
            ZAY_XYWH(panel, 0, panel.h() - 1 - i, panel.w(), 1)
                panel.fill();
            invalidate(2);
        }
    }

    // 윈도우 리사이징 모듈
    #define RESIZING_MODULE(C, L, T, R, B, BORDER) do {\
        static point64 OldMousePos; \
        static rect128 OldWindowRect; \
        if(t == GT_Moving) \
        { \
            SetCursor(C); \
            mNcLeftBorder = false; \
            mNcTopBorder = false; \
            mNcRightBorder = false; \
            mNcBottomBorder = false; \
            BORDER = true; \
        } \
        else if(t == GT_MovingLosed) \
        { \
            SetCursor(CR_Arrow); \
        } \
        else if(t == GT_Pressed) \
        { \
            Platform::Utility::GetCursorPos(OldMousePos); \
            OldWindowRect = Platform::GetWindowRect(); \
        } \
        else if(t == GT_InDragging || t == GT_OutDragging || t == GT_InReleased || t == GT_OutReleased || t == GT_CancelReleased) \
        { \
            point64 NewMousePos; \
            Platform::Utility::GetCursorPos(NewMousePos); \
            const rect128 NewWindowRect = {L, T, R, B}; \
            Platform::SetWindowRect(NewWindowRect.l, NewWindowRect.t, \
                NewWindowRect.r - NewWindowRect.l, NewWindowRect.b - NewWindowRect.t); \
        }} while(false);

    // 윈도우 리사이징 꼭지점
    const sint32 SizeBorder = 10;
    ZAY_LTRB_UI(panel, 0, 0, SizeBorder, SizeBorder, "NcLeftTop",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeFDiag,
                MoveNcLeft(OldWindowRect, NewMousePos.x - OldMousePos.x),
                MoveNcTop(OldWindowRect, NewMousePos.y - OldMousePos.y),
                OldWindowRect.r,
                OldWindowRect.b,
                mNcLeftBorder = mNcTopBorder);
        });
    ZAY_LTRB_UI(panel, panel.w() - SizeBorder, 0, panel.w(), SizeBorder, "NcRightTop",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeBDiag,
                OldWindowRect.l,
                MoveNcTop(OldWindowRect, NewMousePos.y - OldMousePos.y),
                MoveNcRight(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.b,
                mNcRightBorder = mNcTopBorder);
        });
    ZAY_LTRB_UI(panel, 0, panel.h() - SizeBorder, SizeBorder, panel.h(), "NcLeftBottom",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeBDiag,
                MoveNcLeft(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.t,
                OldWindowRect.r,
                MoveNcBottom(OldWindowRect, NewMousePos.y - OldMousePos.y),
                mNcLeftBorder = mNcBottomBorder);
        });
    ZAY_LTRB_UI(panel, panel.w() - SizeBorder, panel.h() - SizeBorder, panel.w(), panel.h(), "NcRightBottom",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeFDiag,
                OldWindowRect.l,
                OldWindowRect.t,
                MoveNcRight(OldWindowRect, NewMousePos.x - OldMousePos.x),
                MoveNcBottom(OldWindowRect, NewMousePos.y - OldMousePos.y),
                mNcRightBorder = mNcBottomBorder);
        });

    // 윈도우 리사이징 모서리
    ZAY_LTRB_UI(panel, 0, SizeBorder, SizeBorder, panel.h() - SizeBorder, "NcLeft",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeHor,
                MoveNcLeft(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.t,
                OldWindowRect.r,
                OldWindowRect.b,
                mNcLeftBorder);
        });
    ZAY_LTRB_UI(panel, SizeBorder, 0, panel.w() - SizeBorder, SizeBorder, "NcTop",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeVer,
                OldWindowRect.l,
                MoveNcTop(OldWindowRect, NewMousePos.y - OldMousePos.y),
                OldWindowRect.r,
                OldWindowRect.b,
                mNcTopBorder);
        });
    ZAY_LTRB_UI(panel, panel.w() - SizeBorder, SizeBorder, panel.w(), panel.h() - SizeBorder, "NcRight",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeHor,
                OldWindowRect.l,
                OldWindowRect.t,
                MoveNcRight(OldWindowRect, NewMousePos.x - OldMousePos.x),
                OldWindowRect.b,
                mNcRightBorder);
        });
    ZAY_LTRB_UI(panel, SizeBorder, panel.h() - SizeBorder, panel.w() - SizeBorder, panel.h(), "NcBottom",
        ZAY_GESTURE_T(t, this)
        {
            RESIZING_MODULE(CR_SizeVer,
                OldWindowRect.l,
                OldWindowRect.t,
                OldWindowRect.r,
                MoveNcBottom(OldWindowRect, NewMousePos.y - OldMousePos.y),
                mNcBottomBorder);
        });
}

bool challengersData::RenderHtmlView(ZayPanel& panel, chars viewid, chars htmlname)
{
    auto& OneHtmlView = mHtmlViewes(viewid);
    const float OneZoom = panel.zoom().scale;
    const sint32 HtmlWidth = panel.w() / OneZoom;
    const sint32 HtmlHeight = panel.h() / OneZoom;

    // Html이 다를 경우
    if(!!OneHtmlView.mName.Compare(htmlname))
    {
        OneHtmlView.mName = htmlname;
        // 소멸처리
        if(!OneHtmlView.mName.Compare("x"))
        {
            if(OneHtmlView.mHtml.get())
            {
                OneHtmlView.mLoading = -1;
                if(Platform::Graphics::BeginGL())
                {
                    Platform::Web::Release(OneHtmlView.mHtml);
                    Platform::Graphics::EndGL();
                }
                OneHtmlView.mWidth = 0;
                OneHtmlView.mHeight = 0;
                OneHtmlView.mLiveMsec = 0;
            }
            return true;
        }

        // 로딩처리
        OneHtmlView.mLoading = 30;
        if(Platform::Graphics::BeginGL())
        {
            if(!OneHtmlView.mHtml.get())
            {
                OneHtmlView.mHtml = Platform::Web::Create(Platform::File::RootForAssets() + "html/" + OneHtmlView.mName,
                    HtmlWidth, HtmlHeight, false);
                OneHtmlView.mWidth = HtmlWidth;
                OneHtmlView.mHeight = HtmlHeight;
                ZayWidgetDOM::SetValue("program.html", String::FromInteger(mHtmlViewes.Count()));
            }
            else Platform::Web::Reload(OneHtmlView.mHtml, Platform::File::RootForAssets() + "html/" + OneHtmlView.mName);
            Platform::Graphics::EndGL();
        }
    }
    else if(!OneHtmlView.mName.Compare("x"))
        return true;

    // 화면크기변경
    if(OneHtmlView.mWidth != HtmlWidth || OneHtmlView.mHeight != HtmlHeight)
    {
        if(Platform::Graphics::BeginGL())
        {
            Platform::Web::Resize(OneHtmlView.mHtml, HtmlWidth, HtmlHeight);
            Platform::Graphics::EndGL();
        }
        OneHtmlView.mWidth = HtmlWidth;
        OneHtmlView.mHeight = HtmlHeight;
    }

    // 클릭처리
    if(0 <= OneHtmlView.mLoading)
    {
        float Rate = 0;
        if(!Platform::Web::NowLoading(OneHtmlView.mHtml, &Rate))
        if(Rate == 1 && Platform::Graphics::BeginGL())
        {
            if(0 == OneHtmlView.mLoading--)
            {
                Platform::Web::SendTouchEvent(OneHtmlView.mHtml, TT_Press, HtmlWidth / 2, HtmlHeight / 2);
                Platform::Web::SendTouchEvent(OneHtmlView.mHtml, TT_Release, HtmlWidth / 2, HtmlHeight / 2);
            }
            Platform::Graphics::EndGL();
        }
    }

    // 화면출력
    if(OneHtmlView.mLoading == -1 && Platform::Graphics::BeginGL())
    {
        auto LastTexture = Platform::Web::GetPageTexture(OneHtmlView.mHtml);
        const auto DstPos = panel.toview(0, 0);
        Platform::Graphics::DrawTextureToFBO(LastTexture, 0, 0, HtmlWidth, HtmlHeight,
            orientationtype_fliped180, false, DstPos.x * OneZoom, DstPos.y * OneZoom, panel.w() * OneZoom, panel.h() * OneZoom);
        Platform::Graphics::EndGL();
    }
    OneHtmlView.mLiveMsec = Platform::Utility::CurrentTimeMsec() + 3000;
    return true;
}

bool challengersData::RenderSlider(ZayPanel& panel, chars domname, sint32 maxvalue, bool flip)
{
    String DomName = domname;
    String UIName = String::Format("ui_slider_%s", domname);
    ZAY_INNER_UI(panel, 0, UIName,
        ZAY_GESTURE_VNTXY(v, n, t, x, y, this, DomName, maxvalue, flip)
        {
            static float LimitValue;
            if(t == GT_Pressed || t == GT_InDragging || t == GT_OutDragging)
            {
                auto CurRect = v->rect(n);
                const float RateValue = ((flip)? CurRect.r - x : x - CurRect.l) / float(CurRect.r - CurRect.l);
                LimitValue = Math::ClampF(maxvalue * RateValue, 0, maxvalue);
                ZayWidgetDOM::SetValue(DomName, String::FromFloat(LimitValue));
                invalidate();
            }
            else if(t == GT_InReleased || t == GT_OutReleased)
            {
                const sint32 RoundValue = Math::Math::Round(LimitValue);
                UpdateDom("d." + DomName, String::FromInteger(RoundValue));
                invalidate();
            }
        });
    return true;
}

bool challengersData::RenderLoader(ZayPanel& panel, float percent, sint32 gap)
{
    const sint32 TopRight = (panel.w() - panel.w() / 2) * Math::ClampF(percent / 12.5, 0, 1);
    ZAY_XYWH(panel, panel.w() / 2, 0, TopRight, gap)
        panel.fill();

    const sint32 Right = (panel.h() - gap) * Math::ClampF((percent - 12.5) / 25.0, 0, 1);
    ZAY_XYWH(panel, panel.w() - gap, gap, gap, Right)
        panel.fill();

    const sint32 Bottom = (panel.w() - gap) * Math::ClampF((percent - 37.5) / 25.0, 0, 1);
    ZAY_XYWH(panel, panel.w() - gap - Bottom, panel.h() - gap, Bottom, gap)
        panel.fill();

    const sint32 Left = (panel.h() - gap) * Math::ClampF((percent - 62.5) / 25.0, 0, 1);
    ZAY_XYWH(panel, 0, panel.h() - gap - Left, gap, Left)
        panel.fill();

    const sint32 TopLeft = (panel.w() / 2 - gap) * Math::ClampF((percent - 87.5) / 12.5, 0, 1);
    ZAY_XYWH(panel, gap, 0, TopLeft, gap)
        panel.fill();
    return true;
}

bool challengersData::IsFullScreen()
{
    return mIsFullScreen;
}

void challengersData::SetFullScreen()
{
    if(!mIsFullScreen)
    {
        mIsFullScreen = true;
        mSavedNormalRect = Platform::GetWindowRect();

        point64 CursorPos;
        Platform::Utility::GetCursorPos(CursorPos);
        sint32 ScreenID = Platform::Utility::GetScreenID(CursorPos);
        rect128 FullScreenRect;
        Platform::Utility::GetScreenRect(FullScreenRect, ScreenID, false);
        Platform::SetWindowRect(FullScreenRect.l, FullScreenRect.t,
            FullScreenRect.r - FullScreenRect.l, FullScreenRect.b - FullScreenRect.t + 1);
    }
}

void challengersData::SetNormalWindow()
{
    if(mIsFullScreen)
    {
        mIsFullScreen = false;
        Platform::SetWindowRect(mSavedNormalRect.l, mSavedNormalRect.t,
            mSavedNormalRect.r - mSavedNormalRect.l, mSavedNormalRect.b - mSavedNormalRect.t);
    }
}

bool challengersData::CheckDOM(uint64 msec)
{
    static uint64 LastUpdateCheckTime = 0;
    if(LastUpdateCheckTime + 100 < msec)
    {
        LastUpdateCheckTime = msec;
        uint64 DataModifyTime = 0;
        if(Asset::Exist("widget/data.json", nullptr, nullptr, nullptr, nullptr, &DataModifyTime))
        {
            if(mLastModifyTime != DataModifyTime)
            {
                mLastModifyTime = DataModifyTime;
                ReloadDOM();
                return true;
            }
        }
    }
    return false;
}

void challengersData::ReloadDOM()
{
    String DataString = String::FromAsset("widget/data.json");
    Context Data(ST_Json, SO_OnlyReference, DataString, DataString.Length());

    ZayWidgetDOM::RemoveVariables("data.");
    ZayWidgetDOM::SetJson(Data, "data.");
    ZayWidgetDOM::RemoveVariables("option.");

    // 폰트확보
    const sint32 FontCount = ZayWidgetDOM::GetValue("data.fonts.count").ToInteger();
    for(sint32 i = 0; i < FontCount; ++i)
    {
        const String CurHeader = String::Format("data.fonts.%d.", i);
        const String CurName = ZayWidgetDOM::GetValue(CurHeader + "name").ToText();
        const String CurPath = ZayWidgetDOM::GetValue(CurHeader + "path").ToText();
        if(CurName.Length() == 0 && 0 < CurPath.Length())
        {
            buffer NewFontData = Asset::ToBuffer(CurPath);
            auto NewFontName = Platform::Utility::CreateSystemFont((bytes) NewFontData, Buffer::CountOf(NewFontData));
            Buffer::Free(NewFontData);
            ZayWidgetDOM::SetValue(CurHeader + "name", "'" + NewFontName + "'");
        }
    }
}

void challengersData::SetTitle(chars name)
{
    Platform::SetWindowName((name)? name : "Challengers");
    ZayWidgetDOM::SetValue("program.title", (name)? String::Format("'%s'", name) : String("'unknown'"));
}

sint32 challengersData::FindWidget(chars name) const
{
    const sint32 WidgetCount = ZayWidgetDOM::GetValue("data.widgets.count").ToInteger();
    for(sint32 i = 0; i < WidgetCount; ++i)
    {
        const String CurHeader = String::Format("data.widgets.%d.", i);
        const String CurName = ZayWidgetDOM::GetValue(CurHeader + "name").ToText();
        if(!CurName.Compare(name))
            return i;
    }
    return -1;
}

void challengersData::EnterWidget(sint32 index)
{
    const String CurHeader = String::Format("data.widgets.%d.", index);
    const String CurType = ZayWidgetDOM::GetValue(CurHeader + "type").ToText();
    const String CurName = ZayWidgetDOM::GetValue(CurHeader + "name").ToText();
    const String CurPath = ZayWidgetDOM::GetValue(CurHeader + "path").ToText();
    const sint32 CurWidth = ZayWidgetDOM::GetValue(CurHeader + "width").ToInteger();
    const sint32 CurHeight = ZayWidgetDOM::GetValue(CurHeader + "height").ToInteger();
    const sint32 CurScale = (0 < gFirstScale)? gFirstScale
        : ZayWidgetDOM::GetValue(CurHeader + "scale").ToInteger();
    const bool CurFlexible = (ZayWidgetDOM::GetValue(CurHeader + "flexible").ToInteger() != 0);

    // 아틀라스 동적로딩
    const sint32 CurAtlasCount = ZayWidgetDOM::GetValue(CurHeader + "atlas.count").ToInteger();
    if(0 < CurAtlasCount)
    {
        const String AtlasInfoString = String::FromAsset("atlasinfo.json");
        Context AtlasInfo(ST_Json, SO_OnlyReference, AtlasInfoString, AtlasInfoString.Length());
        R::SaveAtlas(AtlasInfo); // 최신정보의 병합
        for(sint32 i = 0; i < CurAtlasCount; ++i)
        {
            const String AtlasFileName = ZayWidgetDOM::GetValue(CurHeader + String::Format("atlas.%d", i)).ToText();
            R::AddAtlas("ui_atlaskey.png", AtlasFileName, AtlasInfo);
        }
        if(R::IsAtlasUpdated())
            R::RebuildAll();
    }

    // 위젯생성
    mWidgetMain = new ZayWidget();
    InitWidget(*mWidgetMain, CurName);
    mWidgetMain->Reload(CurPath);
    mWidgetMain->UpdateAtlas(R::PrintUpdatedAtlas(true));
    SetTitle(CurName);
    ZayWidgetDOM::SetValue("program.scale", String::FromInteger(CurScale));
    ZayWidgetDOM::SetValue("program.flexible", (CurFlexible)? "1" : "0");
    if(!IsFullScreen())
    {
        const auto WindowRect = Platform::GetWindowRect();
        mSavedLobbySize.w = WindowRect.r - WindowRect.l;
        mSavedLobbySize.h = WindowRect.b - WindowRect.t;
        Platform::SetWindowSize(CurWidth * CurScale / 100, CurHeight * CurScale / 100 + 1);
    }
}

void challengersData::InitWidget(ZayWidget& widget, chars name)
{
    widget.Init(name, nullptr,
        [](chars name)->const Image* {return &((const Image&) R(name));})
        // 특정시간동안 지속적인 화면업데이트(60fps)
        .AddGlue("update", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const uint64 UpdateMsec = Platform::Utility::CurrentTimeMsec() + params.Param(0).ToFloat() * 1000;
                if(mUpdateMsec < UpdateMsec)
                    mUpdateMsec = UpdateMsec;
            }
        })
        // 로그출력
        .AddGlue("log", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Text = params.Param(0).ToText();
                if(mWidgetMain)
                    mWidgetMain->SendLog(Text);
            }
        })
        // 이벤트
        .AddGlue("event", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Topic = params.Param(0).ToText();
                Platform::BroadcastNotify(Topic, nullptr);
            }
        })
        // 파이썬실행
        .AddGlue("python", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Func = params.Param(0).ToText();
                if(mPython != nullptr)
                    PythonSend(String::Format("call,%s", (chars) Func));
            }
        })
        // 옵션준비
        .AddGlue("option_ready", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const sint32 Index = params.Param(0).ToInteger();
                if(Index < ZayWidgetDOM::GetValue("data.widgets.count").ToInteger())
                {
                    const String Header = String::Format("data.widgets.%d", Index);
                    ZayWidgetDOM::SetComment("option.edit0", ZayWidgetDOM::GetValue(Header + ".name").ToText());
                    ZayWidgetDOM::SetComment("option.edit1", ZayWidgetDOM::GetValue(Header + ".width").ToText());
                    ZayWidgetDOM::SetComment("option.edit2", ZayWidgetDOM::GetValue(Header + ".height").ToText());
                    ZayWidgetDOM::SetValue("option.flexible", ZayWidgetDOM::GetValue(Header + ".flexible").ToText());
                    ZayWidgetDOM::SetValue("option.scale", ZayWidgetDOM::GetValue(Header + ".scale").ToText());
                }
                else
                {
                    ZayWidgetDOM::SetComment("option.edit0", "나의 앱");
                    ZayWidgetDOM::SetComment("option.edit1", "800");
                    ZayWidgetDOM::SetComment("option.edit2", "600");
                    ZayWidgetDOM::SetValue("option.flexible", "0");
                    ZayWidgetDOM::SetValue("option.scale", "100");
                }
            }
        })
        // 옵션승인
        .AddGlue("option_confirm", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const sint32 Index = params.Param(0).ToInteger();
                const String DataString = String::FromAsset("widget/data.json");
                Context DataJson(ST_Json, SO_OnlyReference, DataString, DataString.Length());

                const String Name = ZayWidgetDOM::GetComment("option.edit0");
                const sint32 Width = Math::Clamp(Parser::GetInt(ZayWidgetDOM::GetComment("option.edit1")), 100, 3840);
                const sint32 Height = Math::Clamp(Parser::GetInt(ZayWidgetDOM::GetComment("option.edit2")), 100, 2160);
                const String Path = String::Format("widget/%s.json", (chars) Name);
                // Name과 동일한 위젯이 없을 경우 sample을 복사
                if(!Asset::Exist(Path))
                {
                    const String Sample = String::FromAsset("widget/sample.json");
                    Sample.ToAsset(Path, true);
                }

                hook(DataJson.At("widgets").At(Index))
                {
                    fish.At("name").Set(Name);
                    fish.At("width").Set(String::FromInteger(Width));
                    fish.At("height").Set(String::FromInteger(Height));
                    fish.At("flexible").Set(ZayWidgetDOM::GetValue("option.flexible").ToText());
                    fish.At("scale").Set(ZayWidgetDOM::GetValue("option.scale").ToText());
                    fish.At("path").Set(Path);
                }
                DataJson.SaveJson().ToAsset("widget/data.json", true);
            }
        })
        // 옵션삭제
        .AddGlue("option_remove", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const sint32 Index = params.Param(0).ToInteger();
                const String DataString = String::FromAsset("widget/data.json");
                Context DataJson(ST_Json, SO_OnlyReference, DataString, DataString.Length());

                DataJson.At("widgets").Remove(Index);
                DataJson.SaveJson().ToAsset("widget/data.json", true);
            }
        })
        // 옵션-플렉시블토글
        .AddGlue("option_flexible_turn", ZAY_DECLARE_GLUE(params, this)
        {
            const sint32 NextTrun = 1 - ZayWidgetDOM::GetValue("option.flexible").ToInteger();
            ZayWidgetDOM::SetValue("option.flexible", String::FromInteger(NextTrun));
        })
        // 옵션-스케일업
        .AddGlue("option_scale_up", ZAY_DECLARE_GLUE(params, this)
        {
            const sint32 NextScale = Math::Min(ZayWidgetDOM::GetValue("option.scale").ToInteger() + 10, 500);
            ZayWidgetDOM::SetValue("option.scale", String::FromInteger(NextScale));
        })
        // 옵션-스케일다운
        .AddGlue("option_scale_down", ZAY_DECLARE_GLUE(params, this)
        {
            const sint32 NextScale = Math::Max(10, ZayWidgetDOM::GetValue("option.scale").ToInteger() - 10);
            ZayWidgetDOM::SetValue("option.scale", String::FromInteger(NextScale));
        })
        // 위젯선택
        .AddGlue("enter", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const sint32 Index = params.Param(0).ToInteger();
                mReservedEnterWidget = Index;
                invalidate();
            }
        })
        // 위젯변경
        .AddGlue("change", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Name = params.Param(0).ToText();
                mReservedChangeWidget = FindWidget(Name);
                invalidate();
            }
        })
        // DOM초기화
        .AddGlue("reset_dom", ZAY_DECLARE_GLUE(params, this)
        {
            ReloadDOM();
            invalidate();
        })
        // DOM변경
        .AddGlue("update_dom", ZAY_DECLARE_GLUE(params, this)
        {
            if(1 < params.ParamCount())
            {
                const String Key = params.Param(0).ToText();
                const String Value = params.Param(1).ToText();
                UpdateDom(Key, Value);
                invalidate();
            }
        })
        // 사운드재생
        .AddGlue("play_sound", ZAY_DECLARE_GLUE(params, this)
        {
            if(0 < params.ParamCount())
            {
                const String FileName = params.Param(0).ToText();
                PlaySound(FileName);
                invalidate();
            }
        })
        // 사운드중지
        .AddGlue("stop_sound", ZAY_DECLARE_GLUE(params, this)
        {
            StopSound();
            invalidate();
        })
        // 무음 토글버튼
        .AddGlue("turn_mute", ZAY_DECLARE_GLUE(params, this)
        {
            const sint32 NewMute = ZayWidgetDOM::GetValue("program.mute").ToInteger() ^ 1;
            ZayWidgetDOM::SetValue("program.mute", String::FromInteger(NewMute));
            if(NewMute) StopSound();
            invalidate();
        })
        // HTML뷰에 명령전달
        .AddGlue("send_view", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 2)
            {
                const String ViewID = params.Param(0).ToText();
                const String Command = params.Param(1).ToText();
                SendView(ViewID, Command);
                invalidate();
            }
        })
        // 게이트호출
        .AddGlue("call_gate", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String GateName = params.Param(0).ToText();
                CallGate(GateName);
                invalidate();
            }
        })
        // user_content
        .AddComponent(ZayExtend::ComponentType::ContentWithParameter, "user_content", ZAY_DECLARE_COMPONENT(panel, params, this)
        {
            if(params.ParamCount() < 1)
                return panel._push_pass();
            const String Type = params.Param(0).ToText();
            bool HasRender = false;

            branch;
            jump(!Type.Compare("html_view") && params.ParamCount() == 3)
            {
                const String ViewID = params.Param(1).ToText();
                const String HtmlName = params.Param(2).ToText();
                HasRender = RenderHtmlView(panel, ViewID, HtmlName);
            }
            jump(!Type.Compare("slider") && params.ParamCount() == 4)
            {
                const String DomName = params.Param(1).ToText();
                const sint32 MaxValue = params.Param(2).ToInteger();
                const bool Flip = (params.Param(3).ToInteger() != 0);
                HasRender = RenderSlider(panel, DomName, MaxValue, Flip);
            }
            jump(!Type.Compare("loader") && params.ParamCount() == 3)
            {
                const float Percent = params.Param(1).ToFloat();
                const sint32 Gap = params.Param(2).ToInteger();
                HasRender = RenderLoader(panel, Percent, Gap);
            }

            // 그외 처리
            if(!HasRender)
            ZAY_INNER_SCISSOR(panel, 0)
            {
                ZAY_RGBA(panel, 255, 0, 0, 128)
                    panel.fill();
                for(sint32 i = 0; i < 5; ++i)
                {
                    ZAY_INNER(panel, 1 + i)
                    ZAY_RGBA(panel, 255, 0, 0, 128 - 16 * i)
                        panel.rect(1);
                }
                ZAY_FONT(panel, 1.2 * Math::MinF(Math::MinF(panel.w(), panel.h()) / 40, 1))
                ZAY_RGB(panel, 255, 0, 0)
                    panel.text(Type, UIFA_CenterMiddle, UIFE_Right);
            }
            return panel._push_pass();
        });
}

void challengersData::ChangeWidget(sint32 index)
{
    const String CurHeader = String::Format("data.widgets.%d.", index);
    const String CurName = ZayWidgetDOM::GetValue(CurHeader + "name").ToText();
    const String CurPath = ZayWidgetDOM::GetValue(CurHeader + "path").ToText();

    // 위젯교체
    delete mWidgetMain;
    mWidgetMain = new ZayWidget();
    InitWidget(*mWidgetMain, CurName);
    mWidgetMain->Reload(CurPath);
    mWidgetMain->UpdateAtlas(R::PrintUpdatedAtlas(true));
    SetTitle(CurName);
}

void challengersData::ExitWidget()
{
    delete mWidgetMain;
    mWidgetMain = nullptr;
    SetTitle(nullptr);
    ZayWidgetDOM::SetValue("program.scale", "100");
    ZayWidgetDOM::SetValue("program.flexible", "1");
    if(!IsFullScreen())
        Platform::SetWindowSize(mSavedLobbySize.w, mSavedLobbySize.h);
}

void challengersData::UpdateDom(chars key, chars value)
{
    if(key[0] == 'd' && key[1] == '.')
        ZayWidgetDOM::SetValue(&key[2], value);
}

void challengersData::PlaySound(chars filename)
{
    const sint32 Mute = ZayWidgetDOM::GetValue("program.mute").ToInteger();
    if(Mute == 0)
    {
        Platform::Sound::Close(mSounds[mSoundFocus]);
        mSounds[mSoundFocus] = Platform::Sound::OpenForFile(Platform::File::RootForAssets() + filename);
        Platform::Sound::SetVolume(mSounds[mSoundFocus], 1);
        Platform::Sound::Play(mSounds[mSoundFocus]);
        mSoundFocus = (mSoundFocus + 1) % 4;
    }
}

void challengersData::StopSound()
{
    for(sint32 i = 0; i < 4; ++i)
    {
        Platform::Sound::Close(mSounds[i]);
        mSounds[i] = nullptr;
    }
    mSoundFocus = 0;
}

void challengersData::SendView(chars viewid, chars command)
{
    if(auto OneHtmlView = mHtmlViewes.Access(viewid))
    if(OneHtmlView->mHtml.get())
        Platform::Web::CallJSFunction(OneHtmlView->mHtml, command);
}

void challengersData::CallGate(chars gatename)
{
    if(mWidgetMain)
        mWidgetMain->JumpCall(gatename);
}

void challengersData::TryPythonRecvOnce()
{
    while(0 < Platform::Socket::RecvAvailable(mPython))
    {
        uint08 RecvTemp[4096];
        const sint32 RecvSize = Platform::Socket::Recv(mPython, RecvTemp, 4096);
        if(0 < RecvSize)
            Memory::Copy(mPythonQueue.AtDumpingAdded(RecvSize), RecvTemp, RecvSize);
        else if(RecvSize < 0)
            return;

        sint32 QueueEndPos = 0;
        for(sint32 iend = mPythonQueue.Count(), i = iend - RecvSize; i < iend; ++i)
        {
            if(mPythonQueue[i] == '#')
            {
                // 스트링 읽기
                const String Packet((chars) &mPythonQueue[QueueEndPos], i - QueueEndPos);
                QueueEndPos = i + 1;
                if(0 < Packet.Length())
                {
                    const Strings Params = String::Split(Packet);
                    if(0 < Params.Count())
                    {
                        const String Type = Params[0];
                        branch;
                        jump(!Type.Compare("set")) OnPython_set(Params);
                        jump(!Type.Compare("get")) OnPython_get(Params);
                        jump(!Type.Compare("call")) OnPython_call(Params);
                    }
                }
                invalidate();
            }
        }
        if(0 < QueueEndPos)
            mPythonQueue.SubtractionSection(0, QueueEndPos);
    }
}

void challengersData::OnPython_set(const Strings& params)
{
    if(2 < params.Count())
    {
        UpdateDom(params[1], params[2]);
        invalidate();
    }
}

void challengersData::OnPython_get(const Strings& params)
{
    if(1 < params.Count())
    {
        const chars Key = (chars) params[1];
        if(Key[0] == 'd' && Key[1] == '.')
        {
            const String Value = ZayWidgetDOM::GetValue(&Key[2]).ToText();
            PythonSend(String::Format("put,%s", (chars) Value));
        }
    }
}

void challengersData::OnPython_call(const Strings& params)
{
    if(1 < params.Count())
    {
        if(mWidgetMain)
            mWidgetMain->JumpCall(params[1]);
    }
}

void challengersData::PythonSend(const String& comma_params)
{
    Platform::Socket::Send(mPython, (bytes)(chars) comma_params,
        comma_params.Length(), 3000, true);
}

String challengersData::FindPythonKey(sint32 keycode)
{
    String KeyText;
    switch(keycode)
    {
    case 8: KeyText = "Backspace"; break;
    case 13: KeyText = "Enter"; break;
    case 16: KeyText = "Shift"; break;
    case 17: KeyText = "Ctrl"; break;
    case 18: KeyText = "Alt"; break;
    case 21: KeyText = "Hangul"; break;
    case 25: KeyText = "Hanja"; break;
    case 32: KeyText = "Space"; break;
    case 37: KeyText = "Left"; break;
    case 38: KeyText = "Up"; break;
    case 39: KeyText = "Right"; break;
    case 40: KeyText = "Down"; break;
    case 48: case 49: case 50: case 51: case 52: case 53: case 54: case 55: case 56: case 57:
        KeyText = '0' + (keycode - 48); break;
    case 65: case 66: case 67: case 68: case 69: case 70: case 71: case 72: case 73: case 74:
    case 75: case 76: case 77: case 78: case 79: case 80: case 81: case 82: case 83: case 84:
    case 85: case 86: case 87: case 88: case 89: case 90:
        KeyText = 'A' + (keycode - 65); break;
    case 112: case 113: case 114: case 115: case 116: case 117:
    case 118: case 119: case 120: case 121: case 122: case 123:
        KeyText = String::Format("F%d", 1 + (keycode - 112)); break;
    }
    return KeyText;
}
