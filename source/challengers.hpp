#pragma once
#include <service/boss_zay.hpp>
#include <service/boss_zaywidget.hpp>

class challengersData : public ZayObject
{
public:
    challengersData();
    ~challengersData();

public:
    void SetCursor(CursorRole role);
    sint32 MoveNcLeft(const rect128& rect, sint32 addx);
    sint32 MoveNcTop(const rect128& rect, sint32 addy);
    sint32 MoveNcRight(const rect128& rect, sint32 addx);
    sint32 MoveNcBottom(const rect128& rect, sint32 addy);
    void RenderWindowOutline(ZayPanel& panel);
    bool IsFullScreen();
    void SetFullScreen();
    void SetNormalWindow();
    bool CheckDOM(uint64 msec);
    void ReloadDOM();
    void SetTitle(chars name);
    sint32 FindWidget(chars name) const;
    void EnterWidget(sint32 index);
    void InitWidget(ZayWidget& widget, chars name);
    void ChangeWidget(sint32 index);
    void ExitWidget();
    void UpdateDom(chars key, chars value);
    void PlaySound(chars filename);
    void StopSound();
    void CallGate(chars gatename);

public: // 파이썬
    void TryPythonRecvOnce();
    void OnPython_set(const Strings& params);
    void OnPython_get(const Strings& params);
    void OnPython_call(const Strings& params);
    void PythonSend(const String& comma_params);
    String FindPythonKey(sint32 keycode);

public: // 윈도우
    static const sint32 mMinWindowWidth = 400;
    static const sint32 mMinWindowHeight = 400;
    CursorRole mNowCursor {CR_Arrow};
    bool mNcLeftBorder {false};
    bool mNcTopBorder {false};
    bool mNcRightBorder {false};
    bool mNcBottomBorder {false};
    bool mIsFullScreen {false};
    bool mIsWindowMoving {false};
    rect128 mSavedNormalRect {0, 0, 0, 0};
    size64 mSavedLobbySize {0, 0};

public: // 위젯
    uint64 mUpdateMsec {0};
    uint64 mLastModifyTime {0};
    ZayWidget mWidgetIntro;
    ZayWidget* mWidgetMain {nullptr};
    sint32 mReservedEnterWidget {-1};
    sint32 mReservedChangeWidget {-1};
    id_sound mSounds[4] {nullptr, nullptr, nullptr, nullptr};
    sint32 mSoundFocus {0};

public: // 파이썬통신
    id_socket mPython {nullptr};
    uint08s mPythonQueue;
};
