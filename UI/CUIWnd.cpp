//CUIWnd.cpp

#include "CUIWnd.h"

#include "../client/human/HumanClientApp.h"
#include "ClientUI.h"
#include "CUIControls.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../util/Directories.h"

#include <GG/GUI.h>
#include <GG/DrawUtil.h>

/** \mainpage FreeOrion User Interface

    \section s_overview Overview
    The User Interface module contains all classes pertaining to
    user interactivity.  It consists of the ClientUI class which acts
    as the driver for all of the others.  This module operates as an
    extension to the GG Graphical User Interface Library written by Zach Laine.

    \section s_interface_classes Interface Classes
    <ul>
    <li>ClientUI - the main driver class of the module.
    <li>CUIWnd - parent class of all non-modal interface windows.
    <li>IntroScreen - a combination main menu/splash screen.  The first thing the user sees.
    <li>ServerConnectWnd - a modal window that allows the user to find and choose a game server.
    <li>GalaxySetupWnd - a modal window that allows the user to setup the galaxy size and shape.
    </ul>
    
    \section s_utility_classes Utility Classes
    <ul>
    <li>StringTable - a construct allowing language-independent string storage and retrieval.
    <li>ToolWnd - a GG::Control-derived class that provides balloon-style help
    <li>ToolContainer - a manager construct that drives the functionality of all ToolWnd objects.
    </ul>
    
*/

namespace {
    bool PlaySounds()
    {
        return GetOptionsDB().Get<bool>("UI.sound.enabled");
    }
    const std::string& SoundDir()
    {
        static std::string retval;
        if (retval == "") {
            retval = GetOptionsDB().Get<std::string>("settings-dir");
            if (!retval.empty() && retval[retval.size() - 1] != '/')
                retval += '/';
            retval += "data/sound/";
        }
        return retval;
    }
    void PlayMinimizeSound()
    {
#ifndef FREEORION_BUILD_UTIL
        if (PlaySounds()) HumanClientApp::GetApp()->PlaySound(SoundDir() + GetOptionsDB().Get<std::string>("UI.sound.window-maximize"));
#endif
    }
    void PlayMaximizeSound()
    {
#ifndef FREEORION_BUILD_UTIL
        if (PlaySounds()) HumanClientApp::GetApp()->PlaySound(SoundDir() + GetOptionsDB().Get<std::string>("UI.sound.window-minimize"));
#endif
    }
    void PlayCloseSound()
    {
#ifndef FREEORION_BUILD_UTIL
        if (PlaySounds()) HumanClientApp::GetApp()->PlaySound(SoundDir() + GetOptionsDB().Get<std::string>("UI.sound.window-close"));
#endif
    }

    const double BUTTON_DIMMING_SCALE_FACTOR = 0.75;

}

////////////////////////////////////////////////
// CUI_MinRestoreButton
////////////////////////////////////////////////
CUI_MinRestoreButton::CUI_MinRestoreButton(int x, int y) : 
    GG::Button(x, y, 7, 7, "", boost::shared_ptr<GG::Font>(), ClientUI::WndInnerBorderColor()),
    m_mode(MIN_BUTTON)
{
    GG::Connect(ClickedSignal, &CUI_MinRestoreButton::Toggle, this);
}

void CUI_MinRestoreButton::Render()
{
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();
    GG::Clr color_to_use = ClientUI::WndInnerBorderColor();
    if (State() != BN_ROLLOVER)
        AdjustBrightness(color_to_use, BUTTON_DIMMING_SCALE_FACTOR);
    if (m_mode == MIN_BUTTON) {
        // draw a dash to signify the minimize command
        int middle_y = (lr.y + ul.y) / 2;
        glDisable(GL_TEXTURE_2D);
        glColor4ubv(color_to_use.v);
        glBegin(GL_LINES);
        glVertex2i(ul.x, middle_y);
        glVertex2i(lr.x, middle_y);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    } else {
        // draw a square to signify the restore command
        GG::FlatRectangle(ul.x, ul.y, lr.x, lr.y, GG::CLR_ZERO, ClientUI::WndInnerBorderColor(), 1);
    }
}

void CUI_MinRestoreButton::Toggle()
{
    if (m_mode == MIN_BUTTON) {
        PlayMinimizeSound();
        m_mode = RESTORE_BUTTON;
    } else {
        PlayMaximizeSound();
        m_mode = MIN_BUTTON;
    }
}


////////////////////////////////////////////////
// CUI_CloseButton
////////////////////////////////////////////////
CUI_CloseButton::CUI_CloseButton(int x, int y) : 
    GG::Button(x, y, 7, 7, "", boost::shared_ptr<GG::Font>(), ClientUI::WndInnerBorderColor())
{
    GG::Connect(ClickedSignal, &PlayCloseSound, -1);
}

void CUI_CloseButton::Render()
{
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();
    GG::Clr color_to_use = ClientUI::WndInnerBorderColor();
    if (State() != BN_ROLLOVER)
        AdjustBrightness(color_to_use, BUTTON_DIMMING_SCALE_FACTOR);
    glDisable(GL_TEXTURE_2D);
    glColor4ubv(color_to_use.v);
    // this is slightly less efficient than using GL_LINES, but the lines are rasterized differently on different 
    // OpengGL implementaions, so we do it this way to produce the "x" we want
    glBegin(GL_POINTS);
    for (int i = 0; i < GG::Wnd::Width(); ++i) {
        glVertex2d(ul.x + i, ul.y + i + 0.5);
    }
    for (int i = 0; i < GG::Wnd::Width(); ++i) {
        if (i != GG::Wnd::Width() / 2)
            glVertex2d(lr.x - i - 1, ul.y + i + 0.5);
    }
    glEnd();
    glEnable(GL_TEXTURE_2D);
}


////////////////////////////////////////////////
// CUIWnd
////////////////////////////////////////////////
CUIWnd::CUIWnd(const std::string& t, int x, int y, int w, int h, Uint32 flags) : 
    GG::Wnd(x, y, w, h, flags & ~GG::RESIZABLE),
    m_resizable (flags & GG::RESIZABLE),
    m_closable(flags & CLOSABLE),
    m_minimizable(flags & MINIMIZABLE),
    m_minimized(false),
    m_drag_offset(-1, -1),
    m_close_button(0),
    m_minimize_button(0)
{
    // set window text
    SetText(t);
    // call to CUIWnd::MinimizedLength() because MinimizedLength is virtual
    SetMinSize(GG::Pt(CUIWnd::MinimizedLength(), BORDER_TOP + INNER_BORDER_ANGLE_OFFSET + BORDER_BOTTOM + 50));
    InitButtons();
    EnableChildClipping(true);
}

CUIWnd::~CUIWnd()
{}

void CUIWnd::SizeMove(const GG::Pt& ul, const GG::Pt& lr)
{
    Wnd::SizeMove(ul, lr);
    GG::Pt button_ul = GG::Pt(Width() - BUTTON_RIGHT_OFFSET, BUTTON_TOP_OFFSET) + UpperLeft() - ClientUpperLeft();
    if (m_close_button)
        m_close_button->MoveTo(GG::Pt(button_ul.x, button_ul.y));
    if (m_minimize_button)
        m_minimize_button->MoveTo(GG::Pt(button_ul.x - (m_close_button ? BUTTON_RIGHT_OFFSET : 0), button_ul.y));
}

void CUIWnd::Render()
{
    GG::Pt ul = UpperLeft();
    GG::Pt lr = LowerRight();
    GG::Pt cl_ul = ClientUpperLeft();
    GG::Pt cl_lr = ClientLowerRight();

    if (!m_minimized) {
        // use GL to draw the lines
        glDisable(GL_TEXTURE_2D);
        GLint initial_modes[2];
        glGetIntegerv(GL_POLYGON_MODE, initial_modes);

        // draw background
        glPolygonMode(GL_BACK, GL_FILL);
        glBegin(GL_POLYGON);
            glColor4ubv(ClientUI::WndColor().v);
            glVertex2i(ul.x, ul.y);
            glVertex2i(lr.x, ul.y);
            glVertex2i(lr.x, lr.y - OUTER_EDGE_ANGLE_OFFSET);
            glVertex2i(lr.x - OUTER_EDGE_ANGLE_OFFSET, lr.y);
            glVertex2i(ul.x, lr.y);
            glVertex2i(ul.x, ul.y);
        glEnd();

        // draw outer border on pixel inside of the outer edge of the window
        glPolygonMode(GL_BACK, GL_LINE);
        glBegin(GL_POLYGON);
            glColor4ubv(ClientUI::WndOuterBorderColor().v);
            glVertex2i(ul.x, ul.y);
            glVertex2i(lr.x, ul.y);
            glVertex2i(lr.x, lr.y - OUTER_EDGE_ANGLE_OFFSET);
            glVertex2i(lr.x - OUTER_EDGE_ANGLE_OFFSET, lr.y);
            glVertex2i(ul.x, lr.y);
            glVertex2i(ul.x, ul.y);
        glEnd();

        // reset this to whatever it was initially
        glPolygonMode(GL_BACK, initial_modes[1]);

        // draw inner border, including extra resize-tab lines
        glBegin(GL_LINE_STRIP);
            glColor4ubv(ClientUI::WndInnerBorderColor().v);
            glVertex2i(cl_ul.x, cl_ul.y);
            glVertex2i(cl_lr.x, cl_ul.y);
            glVertex2i(cl_lr.x, cl_lr.y - INNER_BORDER_ANGLE_OFFSET);
            glVertex2i(cl_lr.x - INNER_BORDER_ANGLE_OFFSET, cl_lr.y);
            glVertex2i(cl_ul.x, cl_lr.y);
            glVertex2i(cl_ul.x, cl_ul.y);
        glEnd();
        glBegin(GL_LINES);
            // draw the extra lines of the resize tab
            if (m_resizable) {
                glColor4ubv(ClientUI::WndInnerBorderColor().v);
            } else {
                glColor4ubv(GG::DisabledColor(ClientUI::WndInnerBorderColor()).v);
            }
            glVertex2i(cl_lr.x, cl_lr.y - RESIZE_HASHMARK1_OFFSET);
            glVertex2i(cl_lr.x - RESIZE_HASHMARK1_OFFSET, cl_lr.y);
            
            glVertex2i(cl_lr.x, cl_lr.y - RESIZE_HASHMARK2_OFFSET);
            glVertex2i(cl_lr.x - RESIZE_HASHMARK2_OFFSET, cl_lr.y);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    } else {
        GG::FlatRectangle(ul.x, ul.y, lr.x, lr.y, ClientUI::WndColor(), ClientUI::WndOuterBorderColor(), 1);
    }

    glColor4ubv(ClientUI::TextColor().v);
    boost::shared_ptr<GG::Font> font = GG::GUI::GetGUI()->GetFont(ClientUI::TitleFont(), ClientUI::TitlePts());
    font->RenderText(ul.x + BORDER_LEFT, ul.y, WindowText());
}

void CUIWnd::LButtonDown(const GG::Pt& pt, Uint32 keys)
{
    if (!m_minimized && m_resizable) {
        GG::Pt cl_lr = LowerRight() - GG::Pt(BORDER_RIGHT, BORDER_BOTTOM);
        GG::Pt dist_from_lr = cl_lr - pt;
        if (dist_from_lr.x + dist_from_lr.y <= INNER_BORDER_ANGLE_OFFSET) {
            m_drag_offset = pt - LowerRight();
        }
    }
}

void CUIWnd::LDrag(const GG::Pt& pt, const GG::Pt& move, Uint32 keys)
{
    if (m_drag_offset != GG::Pt(-1, -1)) { // resize-dragging
        Resize((pt - m_drag_offset) - UpperLeft());
    } else { // normal-dragging
        GG::Pt ul = UpperLeft(), lr = LowerRight();
        if ((0 <= ul.x + move.x) && (lr.x + move.x < GG::GUI::GetGUI()->AppWidth()) &&
            (0 <= ul.y + move.y) && (lr.y + move.y < GG::GUI::GetGUI()->AppHeight()))
            GG::Wnd::LDrag(pt, move, keys);
    }
}

void CUIWnd::LButtonUp(const GG::Pt& pt, Uint32 keys)
{
    m_drag_offset = GG::Pt(-1, -1);
}

GG::Pt CUIWnd::ClientUpperLeft() const
{
    return m_minimized ? UpperLeft() : UpperLeft() + GG::Pt(BORDER_LEFT, BORDER_TOP);
}

GG::Pt CUIWnd::ClientLowerRight() const
{
    return m_minimized ? LowerRight() : LowerRight() - GG::Pt(BORDER_RIGHT, BORDER_BOTTOM);
}

bool CUIWnd::InWindow(const GG::Pt& pt) const
{
    GG::Pt lr = LowerRight();
    GG::Pt dist_from_lr = lr - pt;
    bool inside_lower_right_corner = OUTER_EDGE_ANGLE_OFFSET < dist_from_lr.x + dist_from_lr.y;
    return (UpperLeft() <= pt && pt < LowerRight() && inside_lower_right_corner);
}

void CUIWnd::InitButtons()
{
    // create the close button
    GG::Pt button_ul = GG::Pt(Width() - BUTTON_RIGHT_OFFSET, BUTTON_TOP_OFFSET) + UpperLeft() - ClientUpperLeft();
    if (m_closable) {
        m_close_button = new CUI_CloseButton(button_ul.x, button_ul.y);
        GG::Connect(m_close_button->ClickedSignal, &CUIWnd::CloseClicked, this);
        AttachChild(m_close_button);
    }

    // create the minimize button
    if (m_minimizable) {
        m_minimize_button = new CUI_MinRestoreButton(button_ul.x - (m_close_button ? BUTTON_RIGHT_OFFSET : 0), button_ul.y);
        GG::Connect(m_minimize_button->ClickedSignal, &CUIWnd::MinimizeClicked, this);
        AttachChild(m_minimize_button);      
    }    
}

int CUIWnd::MinimizedLength() const 
{
    return MINIMIZED_WND_LENGTH;
}

int CUIWnd::LeftBorder() const
{
    return BORDER_LEFT;
}

int CUIWnd::TopBorder() const
{
    return BORDER_TOP;
}

int CUIWnd::RightBorder() const
{
    return BORDER_RIGHT;
}

int CUIWnd::BottomBorder() const
{
    return BORDER_BOTTOM;
}

int CUIWnd::InnerBorderAngleOffset() const
{
    return INNER_BORDER_ANGLE_OFFSET;
}

void CUIWnd::CloseClicked()
{
    m_done = true;
    if (Parent())
        Parent()->DetachChild(this);
    else
        GG::GUI::GetGUI()->Remove(this);
}

void CUIWnd::MinimizeClicked()
{
    if (!m_minimized) {
        m_minimized = true;
        m_original_size = Size();
        SetMinSize(GG::Pt(MinimizedLength(), BORDER_TOP));
        Resize(GG::Pt(MINIMIZED_WND_LENGTH, BORDER_TOP));
        GG::Pt button_ul = GG::Pt(Width() - BUTTON_RIGHT_OFFSET, BUTTON_TOP_OFFSET);
        if (m_close_button)
            m_close_button->MoveTo(GG::Pt(button_ul.x, button_ul.y));
        if (m_minimize_button)
            m_minimize_button->MoveTo(GG::Pt(button_ul.x - (m_close_button ? BUTTON_RIGHT_OFFSET : 0), button_ul.y));
        Hide();
        Show(false);
        if (m_close_button)
            m_close_button->Show();
        if (m_minimize_button)
            m_minimize_button->Show();
    } else {
        m_minimized = false;
        SetMinSize(GG::Pt(MinimizedLength(), BORDER_TOP + INNER_BORDER_ANGLE_OFFSET + BORDER_BOTTOM + 10));
        Resize(GG::Pt(m_original_size));
        GG::Pt button_ul = GG::Pt(Width() - BUTTON_RIGHT_OFFSET, BUTTON_TOP_OFFSET) + UpperLeft() - ClientUpperLeft();
        if (m_close_button)
            m_close_button->MoveTo(GG::Pt(button_ul.x, button_ul.y));
        if (m_minimize_button)
            m_minimize_button->MoveTo(GG::Pt(button_ul.x - (m_close_button ? BUTTON_RIGHT_OFFSET : 0), button_ul.y));
        Show();
    }
}

///////////////////////////////////////
// class CUIEditWnd
///////////////////////////////////////

CUIEditWnd::CUIEditWnd(int w, const std::string& prompt_text, const std::string& edit_text, Uint32 flags/* = Wnd::MODAL*/) : 
    CUIWnd(prompt_text, 0, 0, w, 1, flags)
{
    m_edit = new CUIEdit(LeftBorder() + 3, TopBorder() + 3, ClientWidth() - 2 * BUTTON_WIDTH - 2 * CONTROL_MARGIN - 6 - LeftBorder() - RightBorder(), edit_text);
    m_ok_bn = new CUIButton(m_edit->LowerRight().x + CONTROL_MARGIN, TopBorder() + 3, BUTTON_WIDTH, UserString("OK"));
    m_cancel_bn = new CUIButton(m_ok_bn->LowerRight().x + CONTROL_MARGIN, TopBorder() + 3, BUTTON_WIDTH, UserString("CANCEL"));
    m_ok_bn->OffsetMove(GG::Pt(0, (m_edit->Height() - m_ok_bn->Height()) / 2));
    m_cancel_bn->OffsetMove(GG::Pt(0, (m_edit->Height() - m_ok_bn->Height()) / 2));

    Resize(GG::Pt(w, std::max(m_edit->LowerRight().y, m_cancel_bn->LowerRight().y) + BottomBorder() + 3));
    MoveTo(GG::Pt((GG::GUI::GetGUI()->AppWidth() - w) / 2, (GG::GUI::GetGUI()->AppHeight() - Height()) / 2));

    AttachChild(m_edit);
    AttachChild(m_ok_bn);
    AttachChild(m_cancel_bn);

    GG::Connect(m_ok_bn->ClickedSignal, &CUIEditWnd::OkClicked, this);
    GG::Connect(m_cancel_bn->ClickedSignal, &CUIWnd::CloseClicked, static_cast<CUIWnd*>(this));

    m_edit->SelectAll();
}

void CUIEditWnd::ModalInit()
{
    GG::GUI::GetGUI()->SetFocusWnd(m_edit);
}

void CUIEditWnd::KeyPress(GG::Key key, Uint32 key_mods)
{
    switch (key) {
    case GG::GGK_RETURN: if (!m_ok_bn->Disabled()) OkClicked(); break;
    case GG::GGK_ESCAPE: CloseClicked(); break;
    default: break;
    }
}

const std::string& CUIEditWnd::Result() const 
{
    return m_result;
}

void CUIEditWnd::OkClicked() 
{
    m_result = m_edit->WindowText();
    CloseClicked();
}
