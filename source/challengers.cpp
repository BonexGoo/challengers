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
        else if(0 < m->mDirectlyWidget.Length())
            m->mReservedEnterWidget = m->FindWidget(m->mDirectlyWidget);
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

        // 예약된 밸류처리
        for(sint32 i = m->mReservedValues.Count() - 1; 0 <= i; --i)
        {
            chararray GetKey;
            if(auto CurValues = m->mReservedValues.AccessByOrder(i, &GetKey))
            {
                for(sint32 j = CurValues->Count() - 1; 0 <= j; --j)
                {
                    auto& CurValue = (*CurValues)[j];
                    if(CurValue.mFlushMsec <= CurMsec)
                    {
                        m->UpdateDomCore(&GetKey[0], CurValue.mValue, -1);
                        CurValues->SubtractionSection(j);
                        m->invalidate();
                    }
                }
                if(CurValues->Count() == 0)
                    m->mReservedValues.Remove(&GetKey[0]);
            }
        }

        // 예약된 사운드처리
        for(sint32 i = m->mReservedSounds.Count() - 1; 0 <= i; --i)
        {
            chararray GetFileName;
            if(auto CurSounds = m->mReservedSounds.AccessByOrder(i, &GetFileName))
            {
                for(sint32 j = CurSounds->Count() - 1; 0 <= j; --j)
                {
                    auto& CurSound = (*CurSounds)[j];
                    if(CurSound.mFlushMsec <= CurMsec)
                    {
                        m->PlaySoundCore(&GetFileName[0], CurSound.mValue, -1);
                        CurSounds->SubtractionSection(j);
                        m->invalidate();
                    }
                }
                if(CurSounds->Count() == 0)
                    m->mReservedSounds.Remove(&GetFileName[0]);
            }
        }

        // 예약된 시리얼처리
        for(sint32 i = m->mReservedSerials.Count() - 1; 0 <= i; --i)
        {
            chararray GetPacket;
            if(auto CurSerials = m->mReservedSerials.AccessByOrder(i, &GetPacket))
            {
                for(sint32 j = CurSerials->Count() - 1; 0 <= j; --j)
                {
                    auto& CurSerial = (*CurSerials)[j];
                    if(CurSerial.mFlushMsec <= CurMsec)
                    {
                        m->SendSerialCore(CurSerial.mValue, &GetPacket[0], -1);
                        CurSerials->SubtractionSection(j);
                        m->invalidate();
                    }
                }
                if(CurSerials->Count() == 0)
                    m->mReservedSerials.Remove(&GetPacket[0]);
            }
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

        if(m->mClient)
        {
            // 통신연결확인
            if(!m->mClientConnected && Platform::Socket::IsConnected(m->mClient))
            {
                m->mClientConnected = true;
                ZayWidgetDOM::SetValue("program.connect", "'client'");
                m->invalidate();
            }
            // 지연된 송신처리
            if(m->mClientConnected)
            {
                for(sint32 i = 0, iend = m->mClientSendTasks.Count(); i < iend; ++i)
                    Platform::Socket::Send(m->mClient, (bytes)(chars) m->mClientSendTasks[i],
                        m->mClientSendTasks[i].Length() + 1, 3000, true);
                m->mClientSendTasks.Clear();
            }
        }

        // 시리얼수신
        id_serial Serials[2] = {m->mSerialPass1, m->mSerialPass2};
        chars Devices[2] = {"pass1", "pass2"};
        for(sint32 i = 0; i < 2; ++i)
        {
            if(Serials[i] && Platform::Serial::ReadReady(Serials[i]))
            if(sint32 Count = Platform::Serial::ReadAvailable(Serials[i]))
            {
                if(m->mUseSizeField && 4 <= Count)
                {
                    sint32 PacketSize = 0;
                    Platform::Serial::Read(Serials[i], "#4:s4", &PacketSize);
                    Count -= 4;
                    // 오류상황처리
                    if(Count != PacketSize || 4096 < PacketSize)
                    while(0 < Count--)
                        Platform::Serial::Read(Serials[i], "#1:skip");
                }
                if(0 < Count)
                {
                    String NewPacket;
                    for(sint32 j = 0; j < Count; ++j)
                    {
                        char Letter = 0;
                        Platform::Serial::Read(Serials[i], "#1:s1", &Letter);
                        NewPacket += Letter;
                    }
                    m->RecvSerial(Devices[i], NewPacket, -1);
                    m->invalidate();
                }
            }
        }

        // 녹화
        if(m->mRecFrame != -1)
        if(auto CurImage = Platform::Utility::GetScreenshotImage(m->mRecRect))
        {
            id_bitmap NewBitmap = Platform::Utility::ImageToBitmap(CurImage, orientationtype_normal0);
            Bmp::ToFile(NewBitmap, String::Format("%s/frame%04d.bmp", (chars) m->mRecFolder, m->mRecFrame));
            Bmp::Remove(NewBitmap);
            m->mRecFrame++;
            m->invalidate(2);
        }

        // 인코딩
        if(m->mEncFrame != -1)
        {
            if(m->mEncFrame == m->mEncCount)
            {
                if(auto NewFile = Platform::File::OpenForWrite(String::Format("%s.flv", (chars) m->mRecFolder), true))
                {
                    sint32 VideoLength = 0;
                    bytes Video = Flv::GetBits(m->mEncFLV, &VideoLength);
                    Platform::File::Write(NewFile, Video, VideoLength);
                    Platform::File::Close(NewFile);
                }
                m->mEncFrame = -1;
                Flv::Remove(m->mEncFLV);
                m->mEncFLV = nullptr;
                AddOn::H264::Release(m->mEncH264);
                m->mEncH264 = nullptr;
                ZayWidgetDOM::SetValue("program.recstep", "'x'");
            }
            else
            {
                if(auto NewBitmap = Bmp::FromFile(String::Format("%s/frame%04d.bmp", (chars) m->mRecFolder, m->mEncFrame)))
                {
                    AddOn::H264::EncodeOnce(m->mEncH264, (uint32*) Bmp::GetBits(NewBitmap), m->mEncFLV, m->mEncTimeMs);
                    Bmp::Remove(NewBitmap);
                    m->mEncTimeMs += 1000 / 60;
                    ZayWidgetDOM::SetValue("program.recpercent", String::FromInteger(100 * m->mEncFrame / m->mEncCount));
                }
                m->mEncFrame++;
            }
            m->invalidate(2);
        }

        // 위젯 틱실행
        if(m->mWidgetMain)
        {
            if(m->mWidgetMain->TickOnce())
                m->invalidate();
            if(m->mWidgetSub && m->mWidgetSub->TickOnce())
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
    }
    else if(type == NT_SocketReceive)
    {
        m->TryServerRecvOnce();
        if(topic == "message")
        {
            m->TryClientRecvOnce();
            m->TryPythonRecvOnce();
        }
        else if(topic == "disconnected")
        {
            // 파이썬 접속상태확인
            if(m->mPython && !Platform::Socket::IsConnected(m->mPython))
            {
                Platform::Socket::Close(m->mPython);
                m->mPython = nullptr;
                m->ExitAll();
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
            if(m->mWidgetSub)
                m->mWidgetSub->UpdateAtlas(AtlasJson);
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
                if(m->mWidgetSub)
                ZAY_RECT(panel, m->mWidgetSubRect)
                    m->mWidgetSub->Render(panel);
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

        /*// 다이렉트핀 버튼
        if(gFirstWidget.Length() == 0)
        ZAY_XYWH_UI(panel, 70, 10, 60, 60, "ui_intro_skip",
            ZAY_GESTURE_T(t)
            {
                if(t == GT_InReleased)
                {
                    if(0 < m->mDirectlyWidget.Length())
                        m->mDirectlyWidget.Empty();
                    else m->mDirectlyWidget = "x";
                    m->invalidate();
                }
            })
        {
            const bool Hovered = ((panel.state("ui_intro_skip") & (PS_Focused | PS_Dropping)) == PS_Focused);
            const bool Grabbed = ((panel.state("ui_intro_skip") & (PS_Pressed | PS_Dragging)) != 0);
            if(Grabbed) panel.icon(R("btn_pin_a_p"), UIA_CenterMiddle);
            else if(0 < m->mDirectlyWidget.Length()) panel.icon(R("btn_pin_a_s"), UIA_CenterMiddle);
            else if(Hovered) panel.icon(R("btn_pin_a_h"), UIA_CenterMiddle);
            else panel.icon(R("btn_pin_a_n"), UIA_CenterMiddle);
        }*/

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
    ZayWidgetDOM::SetValue("program.recname", "'x'");
    ZayWidgetDOM::SetValue("program.recstep", "'x'"); // x, saving, savecheck, encoding
    ZayWidgetDOM::SetValue("program.recpercent", "0");
    mDirectlyWidget = String::FromAsset("directinfo.txt");

    // 인트로위젯
    InitWidget(mWidgetIntro, "intro");
    mWidgetIntro.Reload("widget/intro.json");
    mWidgetIntro.UpdateAtlas(R::PrintUpdatedAtlas(true));

    // 저장된 서버IP
    auto LastServerIP = String::FromAsset("serverip.txt");
    if(0 < LastServerIP.Length())
        ZayWidgetDOM::SetComment("serverip", LastServerIP);
}

challengersData::~challengersData()
{
    delete mWidgetMain;
    delete mWidgetSub;
    for(sint32 i = 0; i < 4; ++i)
        Platform::Sound::Close(mSounds[i]);
    mDirectlyWidget.ToAsset("directinfo.txt", true);
    Flv::Remove(mEncFLV);
    AddOn::H264::Release(mEncH264);

    // 서버IP의 저장
    auto LastServerIP = ZayWidgetDOM::GetComment("serverip");
    LastServerIP.ToAsset("serverip.txt", true);

    Platform::Server::Release(mServer);
    Platform::Socket::Close(mClient);
    Platform::Socket::Close(mPython);
    Platform::Serial::Close(mSerialPass1);
    Platform::Serial::Close(mSerialPass2);
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

bool challengersData::RenderLogView(ZayPanel& panel, sint32 maxcount)
{
    // 길이조절
    if(maxcount < mDebugLogs.Count())
        mDebugLogs.SubtractionSection(0, mDebugLogs.Count() - maxcount);

    // 역순출력
    const sint32 FontHeight = Platform::Graphics::GetStringHeight() + 10;
    for(sint32 i = 0, iend = mDebugLogs.Count(); i < iend; ++i)
        ZAY_XYWH(panel, 0, FontHeight * i, panel.w(), FontHeight)
            panel.text(mDebugLogs[iend - 1 - i], UIFA_RightTop, UIFE_Left);
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
                UpdateDomCore("d." + DomName, String::FromInteger(RoundValue), -1);
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
    ReloadDOMCore(Data);
}

void challengersData::ReloadDOMCore(const Context& data)
{
    mReservedValues.Reset();
    mReservedSounds.Reset();
    mReservedSerials.Reset();
    mStackedValues.Reset();
    ZayWidgetDOM::RemoveVariables("data.");
    ZayWidgetDOM::SetJson(data, "data.");
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
    const String CurName = ZayWidgetDOM::GetValue(CurHeader + "name").ToText();
    const String CurPath = ZayWidgetDOM::GetValue(CurHeader + "path").ToText();
    const sint32 CurWidth = ZayWidgetDOM::GetValue(CurHeader + "width").ToInteger();
    const sint32 CurHeight = ZayWidgetDOM::GetValue(CurHeader + "height").ToInteger();
    const sint32 CurScale = (0 < gFirstScale)? gFirstScale
        : ZayWidgetDOM::GetValue(CurHeader + "scale").ToInteger();
    const bool CurFlexible = (ZayWidgetDOM::GetValue(CurHeader + "flexible").ToInteger() != 0);

    // 서버/클라이언트구성
    const sint32 ServerIdx = ZayWidgetDOM::GetValue("data.server.idx").ToInteger();
    const String ServerHost = ZayWidgetDOM::GetValue("data.server.host").ToText();
    const sint32 ServerPort = ZayWidgetDOM::GetValue("data.server.port").ToInteger();
    const String ServerIP = ZayWidgetDOM::GetComment("serverip");
    if(index == ServerIdx)
    {
        // 클라이언트였다면
        if(mClient)
        {
            Platform::Socket::Close(mClient);
            mClient = nullptr;
            mClientConnected = false;
            mClientQueue.Clear();
            mClientSendTasks.Clear();
        }
        if(!mServer)
        {
            mServer = Platform::Server::CreateWS("Challengers", false);
            if(Platform::Server::Listen(mServer, ServerPort))
                ZayWidgetDOM::SetValue("program.connect", "'server(0)'");
            else
            {
                Platform::Server::Release(mServer);
                mServer = nullptr;
                mServerPeers.Clear();
                ZayWidgetDOM::SetValue("program.connect", "'x'");
            }
        }
    }
    else
    {
        // 서버였다면
        if(mServer)
        {
            Platform::Server::Release(mServer);
            mServer = nullptr;
            mServerPeers.Clear();
        }
        if(!mClient)
        {
            mClient = Platform::Socket::OpenForWS(false);
            if(ServerIP.Length() == 0)
                Platform::Socket::ConnectAsync(mClient, ServerHost, ServerPort);
            else Platform::Socket::ConnectAsync(mClient, ServerIP, ServerPort);
        }
    }

    // 시리얼구성
    const sint32 SerialIdx = ZayWidgetDOM::GetValue("data.serial.idx").ToInteger();
    const bool SerialSizeField = (ZayWidgetDOM::GetValue("data.serial.sizefield").ToInteger() != 0);
    if(index == SerialIdx)
    {
        id_serial* Serials[2] = {&mSerialPass1, &mSerialPass2};
        chars Devices[2] = {"pass1", "pass2"};
        for(sint32 i = 0; i < 2; ++i)
        {
            if(!*Serials[i])
            {
                const String SerialName = ZayWidgetDOM::GetValue(String::Format("data.serial.%s.name", Devices[i])).ToText();
                const sint32 SerialBaudRate = ZayWidgetDOM::GetValue(String::Format("data.serial.%s.baudrate", Devices[i])).ToInteger();
                *Serials[i] = Platform::Serial::Open(SerialName, SerialBaudRate);
                mDebugLogs.AtAdding() = String::Format("[%s_init] %s(%d) is %s",
                     Devices[i], (chars) SerialName, SerialBaudRate, (*Serials[i])? "connected" : "disconnected");
            }
        }
        mUseSizeField = SerialSizeField;
    }
    else
    {
        if(mSerialPass1)
        {
            Platform::Serial::Close(mSerialPass1);
            mSerialPass1 = nullptr;
        }
        if(mSerialPass1)
        {
            Platform::Serial::Close(mSerialPass1);
            mSerialPass1 = nullptr;
        }
    }

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

    // 다이렉트위젯저장
    if(gFirstWidget.Length() == 0 && 0 < mDirectlyWidget.Length())
        mDirectlyWidget = CurName;

    // 서브위젯처리
    const String SubName = ZayWidgetDOM::GetValue(CurHeader + "sub.name").ToText();
    if(0 < SubName.Length())
    {
        const String SubPath = ZayWidgetDOM::GetValue(CurHeader + "sub.path").ToText();
        mWidgetSub = new ZayWidget();
        InitWidget(*mWidgetSub, SubName);
        mWidgetSub->Reload(SubPath);
        mWidgetSub->UpdateAtlas(R::PrintUpdatedAtlas(true));
        // 영역수집
        const sint32 SubX = ZayWidgetDOM::GetValue(CurHeader + "sub.x").ToInteger();
        const sint32 SubY = ZayWidgetDOM::GetValue(CurHeader + "sub.y").ToInteger();
        const sint32 SubWidth = ZayWidgetDOM::GetValue(CurHeader + "sub.width").ToInteger();
        const sint32 SubHeight = ZayWidgetDOM::GetValue(CurHeader + "sub.height").ToInteger();
        mWidgetSubRect.l = SubX;
        mWidgetSubRect.t = SubY;
        mWidgetSubRect.r = SubX + SubWidth;
        mWidgetSubRect.b = SubY + SubHeight;
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
        // 스크립트실행
        .AddGlue("call_script", ZAY_DECLARE_GLUE(params, this)
        {
            if(1 <= params.ParamCount())
            {
                Strings ArgCollector;
                for(sint32 i = 1, iend = params.ParamCount(); i < iend; ++i)
                    ArgCollector.AtAdding() = params.Param(i).ToText();
                const String FileName = params.Param(0).ToText();
                CallScript(FileName, ArgCollector);
                invalidate();
            }
        })
        // DOM초기화
        .AddGlue("reset_dom", ZAY_DECLARE_GLUE(params, this)
        {
            ResetDom(-1);
            invalidate();
        })
        // DOM변경
        .AddGlue("update_dom", ZAY_DECLARE_GLUE(params, this)
        {
            if(2 <= params.ParamCount())
            {
                const String Key = params.Param(0).ToText();
                const String Value = params.Param(1).ToText();
                const sint32 DelayMsec = (2 < params.ParamCount())? params.Param(2).ToInteger() : 0;
                UpdateDom(Key, Value, DelayMsec, -1);
                invalidate();
            }
        })
        // DOM복원
        .AddGlue("restore_dom", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Key = params.Param(0).ToText();
                RestoreDom(Key, -1);
                invalidate();
            }
        })
        // DOM연결
        .AddGlue("synchronize_dom", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 2)
            {
                const String SaveName = params.Param(0).ToText();
                const String DomNameHeader = params.Param(1).ToText();
                if(!String::Compare(DomNameHeader, "d.", 2))
                {
                    auto& NewSyncDom = mSyncDoms.AtAdding();
                    NewSyncDom.mAssetName = "syncdom_" + SaveName + ".json";
                    NewSyncDom.mDomNameHeader = DomNameHeader.Offset(2);
                    const String JsonText = String::FromAsset(NewSyncDom.mAssetName);
                    if(0 < JsonText.Length())
                    {
                        const Context Json(ST_Json, SO_OnlyReference, JsonText);
                        ZayWidgetDOM::SetJson(Json, NewSyncDom.mDomNameHeader);
                        invalidate();
                    }
                }
            }
        })
        // 예약된 명령을 취소
        .AddGlue("cancel_reserve", ZAY_DECLARE_GLUE(params, this)
        {
            CancelReserve(-1);
            invalidate();
        })
        // 사운드재생
        .AddGlue("play_sound", ZAY_DECLARE_GLUE(params, this)
        {
            if(0 < params.ParamCount())
            {
                const String FileName = params.Param(0).ToText();
                const sint32 DelayMsec = (1 < params.ParamCount())? params.Param(1).ToInteger() : 0;
                const String Speaker = (2 < params.ParamCount())? params.Param(2).ToText() : String();
                PlaySound(FileName, Speaker, DelayMsec, -1);
                invalidate();
            }
        })
        // 사운드중지
        .AddGlue("stop_sound", ZAY_DECLARE_GLUE(params, this)
        {
            const String Speaker = (0 < params.ParamCount())? params.Param(0).ToText() : String();
            StopSound(Speaker, -1);
            invalidate();
        })
        // 무음 토글버튼
        .AddGlue("turn_mute", ZAY_DECLARE_GLUE(params, this)
        {
            const sint32 NewMute = ZayWidgetDOM::GetValue("program.mute").ToInteger() ^ 1;
            ZayWidgetDOM::SetValue("program.mute", String::FromInteger(NewMute));
            if(NewMute) StopSoundFlush("");
            invalidate();
        })
        // Pass1에 패킷전송
        .AddGlue("send_pass1", ZAY_DECLARE_GLUE(params, this)
        {
            if(0 < params.ParamCount())
            {
                const String Packet = params.Param(0).ToText();
                const sint32 DelayMsec = (1 < params.ParamCount())? params.Param(1).ToInteger() : 0;
                SendSerial("pass1", Packet, DelayMsec, -1);
                invalidate();
            }
        })
        // Pass1과의 테스트용 패킷수신
        .AddGlue("test_pass1", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Packet = params.Param(0).ToText();
                RecvSerial("pass1", Packet, -1);
                invalidate();
            }
        })
        // Pass2에 패킷전송
        .AddGlue("send_pass2", ZAY_DECLARE_GLUE(params, this)
        {
            if(0 < params.ParamCount())
            {
                const String Packet = params.Param(0).ToText();
                const sint32 DelayMsec = (1 < params.ParamCount())? params.Param(1).ToInteger() : 0;
                SendSerial("pass2", Packet, DelayMsec, -1);
                invalidate();
            }
        })
        // Pass2와의 테스트용 패킷수신
        .AddGlue("test_pass2", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String Packet = params.Param(0).ToText();
                RecvSerial("pass2", Packet, -1);
                invalidate();
            }
        })
        // HTML뷰에 명령전달
        .AddGlue("send_view", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 2)
            {
                const String ViewID = params.Param(0).ToText();
                const String Command = params.Param(1).ToText();
                SendView(ViewID, Command, -1);
                invalidate();
            }
        })
        // 게이트호출
        .AddGlue("call_gate", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 1)
            {
                const String GateName = params.Param(0).ToText();
                CallGate(GateName, -1);
                invalidate();
            }
        })
        // 전체종료
        .AddGlue("exit_all", ZAY_DECLARE_GLUE(params, this)
        {
            ExitAll();
            invalidate();
        })
        // 녹화시작
        .AddGlue("rec_start", ZAY_DECLARE_GLUE(params, this)
        {
            if(params.ParamCount() == 5)
            {
                const String RecName = params.Param(0).ToText();
                const sint32 RecX = params.Param(1).ToInteger();
                const sint32 RecY = params.Param(2).ToInteger();
                const sint32 RecWidth = params.Param(3).ToInteger();
                const sint32 RecHeight = params.Param(4).ToInteger();
                auto WindowRect = Platform::GetWindowRect();
                mRecFrame = 0;
                mRecFolder = Platform::File::RootForAssetsRem() + "rec/" + RecName;
                mRecRect.l = WindowRect.l + RecX;
                mRecRect.t = WindowRect.t + RecY;
                mRecRect.r = mRecRect.l + RecWidth;
                mRecRect.b = mRecRect.t + RecHeight;
                ZayWidgetDOM::SetValue("program.recname", "'" + RecName + "'");
                ZayWidgetDOM::SetValue("program.recstep", "'saving'");
                invalidate();
            }
        })
        // 녹화끝
        .AddGlue("rec_stop", ZAY_DECLARE_GLUE(params, this)
        {
            mEncCount = mRecFrame;
            mRecFrame = -1;
            ZayWidgetDOM::SetValue("program.recstep", "'savecheck'");
        })
        // 인코딩
        .AddGlue("encoding", ZAY_DECLARE_GLUE(params, this)
        {
            mEncFrame = 0;
            mEncTimeMs = 0;
            mEncFLV = Flv::Create(mRecRect.r - mRecRect.l, mRecRect.b - mRecRect.t);
            mEncH264 = AddOn::H264::CreateEncoder(mRecRect.r - mRecRect.l, mRecRect.b - mRecRect.t, false);
            ZayWidgetDOM::SetValue("program.recstep", "'encoding'");
            ZayWidgetDOM::SetValue("program.recpercent", "0");
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
            jump(!Type.Compare("log_view") && params.ParamCount() == 2)
            {
                const sint32 MaxCount = params.Param(1).ToInteger();
                HasRender = RenderLogView(panel, MaxCount);
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
    delete mWidgetSub;
    mWidgetSub = nullptr;
    SetTitle(nullptr);
    ZayWidgetDOM::SetValue("program.scale", "100");
    ZayWidgetDOM::SetValue("program.flexible", "1");
    if(!IsFullScreen())
        Platform::SetWindowSize(mSavedLobbySize.w, mSavedLobbySize.h);
    FlushSyncDom();
}

void challengersData::CallScript(chars filename, const Strings& args) const
{
    // [arg1 >> @stage] 'x' // stage변수를 만들어 arg1을 넣으며 arg1이 없을 경우 'x'를 넣는다 - 변수문
    // [update_dom] 'd.aaa.' + @stage + '.bbb', 100 // update_dom글루함수를 2개 인자와 함께 실행 - 실행문
    Strings VarCollector;
    Map<String> ValueMap; // [VarName]
    const Strings ScriptLines = String::Split(String::FromAsset(filename), '\n');
    for(sint32 i = 0, iend = ScriptLines.Count(); i < iend; ++i)
    {
        const String& CurLine = ScriptLines[i];
        const sint32 CommentPos = CurLine.Find(0, "//");
        const String Code = ((CommentPos == -1)? CurLine : CurLine.Left(CommentPos)).Trim(); // 주석을 뺀 실코드
        if(0 < Code.Length() && Code[0] == '[')
        {
            const sint32 HeaderEnd = Code.Find(1, ']');
            if(HeaderEnd != -1)
            {
                const String GlueName = Code.Middle(1, HeaderEnd - 1).Trim();
                const Strings Params = ZaySonUtility::GetCommaStrings(Code.Offset(HeaderEnd + 1));
                const sint32 ShiftPos = GlueName.Find(0, ">>");
                // 변수문
                if(ShiftPos != -1)
                {
                    const String ArgName = GlueName.Left(ShiftPos).Trim();
                    const String VarName = GlueName.Offset(ShiftPos + 2).Trim();
                    if(0 < VarName.Length())
                    {
                        VarCollector.AtAdding() = VarName;
                        // args에서 초기화
                        if(!ArgName.Left(3).Compare("arg"))
                        {
                            const sint32 ArgIndex = Parser::GetInt(ArgName.Offset(3)) - 1;
                            if(0 <= ArgIndex && ArgIndex < args.Count())
                                ValueMap(VarName) = args[ArgIndex];
                        }
                        // 초기화 실패시 Params[0]을 대입
                        if(!ValueMap.Access(VarName) && 0 < Params.Count())
                            ValueMap(VarName) = Params[0].Trim();
                    }
                }
                // 실행문
                else if(mWidgetMain)
                {
                    Strings ParamCollector;
                    for(sint32 j = 0, jend = Params.Count(); j < jend; ++j)
                    {
                        String CurParam = Params[j];
                        // 변수치환
                        for(sint32 k = 0, kend = VarCollector.Count(); k < kend; ++k)
                            CurParam.Replace(VarCollector[k], ValueMap(VarCollector[k]));
                        // 연산처리
                        ParamCollector.AtAdding() = Solver().Link("d").Parse(CurParam).ExecuteOnly().ToText();
                    }
                    mWidgetMain->GlueCall(GlueName, ParamCollector);
                }
            }
        }
    }
}

void challengersData::ResetDom(sint32 excluded_peerid)
{
    ReloadDOM();

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("ResetedDom");
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("ResetDom");
        ClientSend(NewPacket);
    }
}

void challengersData::UpdateDom(chars key, chars value, sint32 delay_msec, sint32 excluded_peerid)
{
    if(0 < delay_msec)
    {
        hook(mReservedValues(key))
        {
            auto& NewValue = fish.AtAdding();
            NewValue.mValue = value;
            NewValue.mFlushMsec = Platform::Utility::CurrentTimeMsec() + delay_msec;
        }
    }
    else
    {
        mReservedValues.Remove(key);
        UpdateDomCore(key, value, excluded_peerid);
    }
}

void challengersData::UpdateDomCore(chars key, chars value, sint32 excluded_peerid)
{
    UpdateDomFlush(key, value);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("UpdatedDom");
        NewPacket.At("key").Set(key);
        NewPacket.At("value").Set(value);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("UpdateDom");
        NewPacket.At("key").Set(key);
        NewPacket.At("value").Set(value);
        ClientSend(NewPacket);
    }
}

void challengersData::UpdateDomFlush(chars key, chars value)
{
    if(key[0] == 'd' && key[1] == '.')
    {
        mStackedValues(key).AtAdding() = ZayWidgetDOM::GetValue(&key[2]).ToText();
        ZayWidgetDOM::SetValue(&key[2], value);
    }
}

void challengersData::RestoreDom(chars key, sint32 excluded_peerid)
{
    RestoreDomFlush(key);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("RestoredDom");
        NewPacket.At("key").Set(key);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("RestoreDom");
        NewPacket.At("key").Set(key);
        ClientSend(NewPacket);
    }
}

void challengersData::RestoreDomFlush(chars key)
{
    if(key[0] == 'd' && key[1] == '.')
    hook(mStackedValues(key))
    if(0 < fish.Count())
    {
        ZayWidgetDOM::SetValue(&key[2], fish[-1]);
        fish.SubtractionOne();
    }
}

void challengersData::CancelReserve(sint32 excluded_peerid)
{
    CancelReserveFlush();

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("CanceledReserve");
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("CancelReserve");
        ClientSend(NewPacket);
    }
}

void challengersData::CancelReserveFlush()
{
    mReservedValues.Reset();
    mReservedSounds.Reset();
    mReservedSerials.Reset();
}

void challengersData::PlaySound(chars filename, chars speaker, sint32 delay_msec, sint32 excluded_peerid)
{
    if(0 < delay_msec)
    {
        hook(mReservedSounds(filename))
        {
            auto& NewValue = fish.AtAdding();
            NewValue.mValue = speaker;
            NewValue.mFlushMsec = Platform::Utility::CurrentTimeMsec() + delay_msec;
        }
    }
    else PlaySoundCore(filename, speaker, excluded_peerid);
}

void challengersData::PlaySoundCore(chars filename, chars speaker, sint32 excluded_peerid)
{
    PlaySoundFlush(filename, speaker);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("PlayedSound");
        NewPacket.At("filename").Set(filename);
        NewPacket.At("speaker").Set(speaker);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("PlaySound");
        NewPacket.At("filename").Set(filename);
        NewPacket.At("speaker").Set(speaker);
        ClientSend(NewPacket);
    }
}

void challengersData::PlaySoundFlush(chars filename, chars speaker)
{
    const sint32 Mute = ZayWidgetDOM::GetValue("program.mute").ToInteger();
    if(Mute == 0)
    {
        const String Title = ZayWidgetDOM::GetValue("program.title").ToText();
        if(!Title.Compare(speaker) || (speaker[0] == '\0' && mServer))
        {
            Platform::Sound::Close(mSounds[mSoundFocus]);
            mSounds[mSoundFocus] = Platform::Sound::OpenForFile(Platform::File::RootForAssets() + filename);
            Platform::Sound::SetVolume(mSounds[mSoundFocus], 1);
            Platform::Sound::Play(mSounds[mSoundFocus]);
            mSoundFocus = (mSoundFocus + 1) % 4;
        }
    }
}

void challengersData::StopSound(chars speaker, sint32 excluded_peerid)
{
    StopSoundFlush(speaker);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("StoppedSound");
        NewPacket.At("speaker").Set(speaker);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("StopSound");
        NewPacket.At("speaker").Set(speaker);
        ClientSend(NewPacket);
    }
}

void challengersData::StopSoundFlush(chars speaker)
{
    const String Title = ZayWidgetDOM::GetValue("program.title").ToText();
    if(!Title.Compare(speaker) || speaker[0] == '\0')
    {
        mReservedSounds.Reset();
        for(sint32 i = 0; i < 4; ++i)
        {
            Platform::Sound::Close(mSounds[i]);
            mSounds[i] = nullptr;
        }
        mSoundFocus = 0;
    }
}

void challengersData::SendSerial(chars device, chars packet, sint32 delay_msec, sint32 excluded_peerid)
{
    if(0 < delay_msec)
    {
        hook(mReservedSerials(packet))
        {
            auto& NewValue = fish.AtAdding();
            NewValue.mValue = device;
            NewValue.mFlushMsec = Platform::Utility::CurrentTimeMsec() + delay_msec;
        }
    }
    else SendSerialCore(device, packet, excluded_peerid);
}

void challengersData::SendSerialCore(chars device, chars packet, sint32 excluded_peerid)
{
    SendSerialFlush(device, packet);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("SendedSerial");
        NewPacket.At("device").Set(device);
        NewPacket.At("packet").Set(packet);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("SendSerial");
        NewPacket.At("device").Set(device);
        NewPacket.At("packet").Set(packet);
        ClientSend(NewPacket);
    }
}

void challengersData::SendSerialFlush(chars device, chars packet)
{
    id_serial OneSerial = nullptr;
    if(!String::Compare(device, "pass1")) OneSerial = mSerialPass1;
    else if(!String::Compare(device, "pass2")) OneSerial = mSerialPass2;

    if(OneSerial)
    {
        if(mUseSizeField)
        {
            const sint32 PacketSize = boss_strlen(packet);
            Platform::Serial::Write(OneSerial, "#4:s4", PacketSize);
        }
        for(sint32 i = 0; packet[i]; ++i)
            Platform::Serial::Write(OneSerial, "#1", packet[i] & 0xFF);
        Platform::Serial::WriteFlush(OneSerial);
        mDebugLogs.AtAdding() = String::Format("[%s_send] %s", device, packet);
    }
    else mDebugLogs.AtAdding() = String::Format("[%s_send+] %s", device, packet);
}

void challengersData::RecvSerial(chars device, chars packet, sint32 excluded_peerid)
{
    RecvSerialFlush(device, packet);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("RecvedSerial");
        NewPacket.At("device").Set(device);
        NewPacket.At("packet").Set(packet);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("RecvSerial");
        NewPacket.At("device").Set(device);
        NewPacket.At("packet").Set(packet);
        ClientSend(NewPacket);
    }
}

void challengersData::RecvSerialFlush(chars device, chars packet)
{
    if(mWidgetMain)
    {
        ZayWidgetDOM::SetValue(String::Format("program.%s.recv", device), String::Format("'%s'", packet));
        mWidgetMain->JumpCall(String::Format("RecvSerial_%s", device));
    }
    mDebugLogs.AtAdding() = String::Format("[%s_recv] %s", device, packet);
}

void challengersData::SendView(chars viewid, chars command, sint32 excluded_peerid)
{
    SendViewFlush(viewid, command);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("SendedView");
        NewPacket.At("viewid").Set(viewid);
        NewPacket.At("command").Set(command);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("SendView");
        NewPacket.At("viewid").Set(viewid);
        NewPacket.At("command").Set(command);
        ClientSend(NewPacket);
    }
}

void challengersData::SendViewFlush(chars viewid, chars command)
{
    if(auto OneHtmlView = mHtmlViewes.Access(viewid))
    if(OneHtmlView->mHtml.get())
        Platform::Web::CallJSFunction(OneHtmlView->mHtml, command);
}

void challengersData::CallGate(chars gatename, sint32 excluded_peerid)
{
    CallGateFlush(gatename);

    if(mServer)
    {
        Context NewPacket;
        NewPacket.At("type").Set("CalledGate");
        NewPacket.At("gatename").Set(gatename);
        ServerSend(NewPacket, excluded_peerid);
    }
    else if(mClient)
    {
        Context NewPacket;
        NewPacket.At("type").Set("CallGate");
        NewPacket.At("gatename").Set(gatename);
        ClientSend(NewPacket);
    }
}

void challengersData::CallGateFlush(chars gatename)
{
    if(mWidgetMain)
        mWidgetMain->JumpCall(gatename);
    if(mWidgetSub)
        mWidgetSub->JumpCall(gatename);
}

void challengersData::ExitAll()
{
    if(mServer)
    {
        if(mServerPeers.Count() == 0)
        {
            FlushSyncDom();
            Platform::Utility::ExitProgram();
        }
        else
        {
            Context NewPacket;
            NewPacket.At("type").Set("ExitedAll");
            ServerSend(NewPacket, -1);
            mWasExitAll = true;
        }
    }
    else if(mClient)
    {
        if(!mClientConnected)
            Platform::Utility::ExitProgram();
        else
        {
            Context NewPacket;
            NewPacket.At("type").Set("ExitAll");
            ClientSend(NewPacket);
        }
    }
}

void challengersData::FlushSyncDom()
{
    for(sint32 i = mSyncDoms.Count() - 1; 0 <= i; --i)
    {
        auto& CurSyncDom = mSyncDoms[i];
        Context Json;
        ZayWidgetDOM::GetJson(Json, CurSyncDom.mDomNameHeader);
        Json.SaveJson().ToAsset(CurSyncDom.mAssetName, true);
    }
    mSyncDoms.Clear();
}

void challengersData::TryServerRecvOnce()
{
    while(Platform::Server::TryNextPacket(mServer))
    {
        const sint32 OnePeerID = Platform::Server::GetPacketPeerID(mServer);
        switch(Platform::Server::GetPacketType(mServer))
        {
        case packettype_entrance:
            mServerPeers.AtAdding() = OnePeerID;
            ZayWidgetDOM::SetValue("program.connect", String::Format("'server(%d)'", mServerPeers.Count()));
            break;
        case packettype_leaved:
        case packettype_kicked:
            for(sint32 i = mServerPeers.Count() - 1; 0 <= i; --i)
                if(mServerPeers[i] == OnePeerID)
                    mServerPeers.SubtractionSection(i);
            ZayWidgetDOM::SetValue("program.connect", String::Format("'server(%d)'", mServerPeers.Count()));
            // ExitAll처리
            if(mWasExitAll && mServerPeers.Count() == 0)
            {
                FlushSyncDom();
                Platform::Utility::ExitProgram();
            }
            break;
        case packettype_message:
            {
                sint32 BufferSize = 0;
                bytes Buffer = Platform::Server::GetPacketBuffer(mServer, &BufferSize);
                const String RecvText((chars) Buffer, BufferSize);
                const Context RecvJson(ST_Json, SO_OnlyReference, RecvText);
                const String Type = RecvJson("type").GetText();

                branch;
                jump(!Type.Compare("ResetDom")) OnServer_ResetDom(OnePeerID);
                jump(!Type.Compare("UpdateDom")) OnServer_UpdateDom(RecvJson, OnePeerID);
                jump(!Type.Compare("RestoreDom")) OnServer_RestoreDom(RecvJson, OnePeerID);
                jump(!Type.Compare("CancelReserve")) OnServer_CancelReserve(RecvJson, OnePeerID);
                jump(!Type.Compare("PlaySound")) OnServer_PlaySound(RecvJson, OnePeerID);
                jump(!Type.Compare("StopSound")) OnServer_StopSound(RecvJson, OnePeerID);
                jump(!Type.Compare("SendSerial")) OnServer_SendSerial(RecvJson, OnePeerID);
                jump(!Type.Compare("RecvSerial")) OnServer_RecvSerial(RecvJson, OnePeerID);
                jump(!Type.Compare("SendView")) OnServer_SendView(RecvJson, OnePeerID);
                jump(!Type.Compare("CallGate")) OnServer_CallGate(RecvJson, OnePeerID);
                jump(!Type.Compare("ExitAll")) OnServer_ExitAll(RecvJson);
            }
            break;
        }
        invalidate();
    }
}

void challengersData::OnServer_ResetDom(sint32 peerid)
{
    ResetDom(peerid);
}

void challengersData::OnServer_UpdateDom(const Context& json, sint32 peerid)
{
    const String Key = json("key").GetText();
    const String Value = json("value").GetText();
    UpdateDom(Key, Value, 0, peerid);
}

void challengersData::OnServer_RestoreDom(const Context& json, sint32 peerid)
{
    const String Key = json("key").GetText();
    RestoreDom(Key, peerid);
}

void challengersData::OnServer_CancelReserve(const Context& json, sint32 peerid)
{
    CancelReserve(peerid);
}

void challengersData::OnServer_PlaySound(const Context& json, sint32 peerid)
{
    const String FileName = json("filename").GetText();
    const String Speaker = json("speaker").GetText();
    PlaySound(FileName, Speaker, 0, peerid);
}

void challengersData::OnServer_StopSound(const Context& json, sint32 peerid)
{
    const String Speaker = json("speaker").GetText();
    StopSound(Speaker, peerid);
}

void challengersData::OnServer_SendSerial(const Context& json, sint32 peerid)
{
    const String Device = json("device").GetText();
    const String Packet = json("packet").GetText();
    SendSerial(Device, Packet, 0, peerid);
}

void challengersData::OnServer_RecvSerial(const Context& json, sint32 peerid)
{
    const String Device = json("device").GetText();
    const String Packet = json("packet").GetText();
    RecvSerial(Device, Packet, peerid);
}

void challengersData::OnServer_SendView(const Context& json, sint32 peerid)
{
    const String ViewID = json("viewid").GetText();
    const String Command = json("command").GetText();
    SendView(ViewID, Command, peerid);
}

void challengersData::OnServer_CallGate(const Context& json, sint32 peerid)
{
    const String GateName = json("gatename").GetText();
    CallGate(GateName, peerid);
}

void challengersData::OnServer_ExitAll(const Context& json)
{
    ExitAll();
}

void challengersData::ServerSend(const Context& json, sint32 excluded_peerid)
{
    const String NewJson = json.SaveJson();
    for(sint32 i = 0, iend = mServerPeers.Count(); i < iend; ++i)
    {
        if(mServerPeers[i] == excluded_peerid) continue;
        Platform::Server::SendToPeer(mServer, mServerPeers[i],
            (const void*)(chars) NewJson, NewJson.Length() + 1, true);
    }
}

void challengersData::TryClientRecvOnce()
{
    while(0 < Platform::Socket::RecvAvailable(mClient))
    {
        uint08 RecvTemp[4096];
        const sint32 RecvSize = Platform::Socket::Recv(mClient, RecvTemp, 4096);
        if(0 < RecvSize)
            Memory::Copy(mClientQueue.AtDumpingAdded(RecvSize), RecvTemp, RecvSize);
        else if(RecvSize < 0)
            return;

        sint32 QueueEndPos = 0;
        for(sint32 iend = mClientQueue.Count(), i = iend - RecvSize; i < iend; ++i)
        {
            if(mClientQueue[i] == '\0')
            {
                // 스트링 읽기
                const String Packet((chars) &mClientQueue[QueueEndPos], i - QueueEndPos);
                QueueEndPos = i + 1;
                if(0 < Packet.Length())
                {
                    const Context RecvJson(ST_Json, SO_OnlyReference, Packet);
                    const String Type = RecvJson("type").GetText();

                    branch;
                    jump(!Type.Compare("ResetedDom")) OnClient_ResetedDom();
                    jump(!Type.Compare("UpdatedDom")) OnClient_UpdatedDom(RecvJson);
                    jump(!Type.Compare("RestoredDom")) OnClient_RestoredDom(RecvJson);
                    jump(!Type.Compare("CanceledReserve")) OnClient_CanceledReserve(RecvJson);
                    jump(!Type.Compare("PlayedSound")) OnClient_PlayedSound(RecvJson);
                    jump(!Type.Compare("StoppedSound")) OnClient_StoppedSound(RecvJson);
                    jump(!Type.Compare("SendedSerial")) OnClient_SendedSerial(RecvJson);
                    jump(!Type.Compare("RecvedSerial")) OnClient_RecvedSerial(RecvJson);
                    jump(!Type.Compare("SendedView")) OnClient_SendedView(RecvJson);
                    jump(!Type.Compare("CalledGate")) OnClient_CalledGate(RecvJson);
                    jump(!Type.Compare("ExitedAll")) OnClient_ExitedAll(RecvJson);
                }
                invalidate();
            }
        }
        if(0 < QueueEndPos)
            mClientQueue.SubtractionSection(0, QueueEndPos);
    }
}

void challengersData::OnClient_ResetedDom()
{
    ReloadDOM();
}

void challengersData::OnClient_UpdatedDom(const Context& json)
{
    const String Key = json("key").GetText();
    const String Value = json("value").GetText();
    UpdateDomFlush(Key, Value);
}

void challengersData::OnClient_RestoredDom(const Context& json)
{
    const String Key = json("key").GetText();
    RestoreDomFlush(Key);
}

void challengersData::OnClient_CanceledReserve(const Context& json)
{
    CancelReserveFlush();
}

void challengersData::OnClient_PlayedSound(const Context& json)
{
    const String FileName = json("filename").GetText();
    const String Speaker = json("speaker").GetText();
    PlaySoundFlush(FileName, Speaker);
}

void challengersData::OnClient_StoppedSound(const Context& json)
{
    const String Speaker = json("speaker").GetText();
    StopSoundFlush(Speaker);
}

void challengersData::OnClient_SendedSerial(const Context& json)
{
    const String Device = json("device").GetText();
    const String Packet = json("packet").GetText();
    SendSerialFlush(Device, Packet);
}

void challengersData::OnClient_RecvedSerial(const Context& json)
{
    const String Device = json("device").GetText();
    const String Packet = json("packet").GetText();
    RecvSerialFlush(Device, Packet);
}

void challengersData::OnClient_SendedView(const Context& json)
{
    const String ViewID = json("viewid").GetText();
    const String Command = json("command").GetText();
    SendViewFlush(ViewID, Command);
}

void challengersData::OnClient_CalledGate(const Context& json)
{
    const String GateName = json("gatename").GetText();
    CallGateFlush(GateName);
}

void challengersData::OnClient_ExitedAll(const Context& json)
{
    FlushSyncDom();
    Platform::Utility::ExitProgram();
}

void challengersData::ClientSend(const Context& json)
{
    const String NewJson = json.SaveJson();
    if(mClientConnected)
        Platform::Socket::Send(mClient, (bytes)(chars) NewJson,
            NewJson.Length() + 1, 3000, true);
    else mClientSendTasks.AtAdding() = NewJson;
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
        UpdateDom(params[1], params[2], 0, -1);
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
