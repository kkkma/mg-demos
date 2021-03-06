///////////////////////////////////////////////////////////////////////////////
//
//                          IMPORTANT NOTICE
//
// The following open source license statement does not apply to any
// entity in the Exception List published by FMSoft.
//
// For more information, please visit:
//
// https://www.fmsoft.cn/exception-list
//
//////////////////////////////////////////////////////////////////////////////
/*
** This file is a part of mg-demos package.
**
** Copyright (C) 2010 ~ 2019 FMSoft (http://www.fmsoft.cn).
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*! ============================================================================
 * @file ActivityStack.cc
 * @Synopsis
 * @author DongKai
 * @version 1.0
 *  Company: Beijing FMSoft Technology Co., Ltd.
 */

#include <string>
#include <vector>

#include "global.h"
#include "Activity.hh"
#include "ActivityStack.hh"

#define SWITCH_EFFECT_DURATION  220

ActivityStack* ActivityStack::s_single = NULL;

using namespace std;

// get the single instance of ActivityStack
ActivityStack* ActivityStack::singleton()
{
    if (NULL == s_single) {
        s_single = new ActivityStack();
    }
    return s_single;
}

// get the top activity in stack
Activity* ActivityStack::top() const
{
    if (m_activities.empty()) {
        return NULL;
    } else {
        return m_activities.back().second;
    }
}

const char* ActivityStack::currentAppName() const
{
    if (m_activities.empty()) {
        return NULL;
    }
    else {
        return m_activities.back().first.c_str();
    }

}

// Create a new activity and bring it to the top of the stack,
// using appName to specify which Activity will be created,
// using intentPtr to specify your intend (optionally).
bool ActivityStack::push(const char* appName, Intent* intentPtr)
{
    bool ret = false;

    int temp_status = m_status;
    if (READY == m_status) {
        m_status = PUSH;
        ret = (innerPush(appName, intentPtr) != NULL);
        m_status = READY;
        if (ret == FALSE)
            m_status = temp_status;
    }
    else {
        _ERR_PRINTF ("ActivityStack::push: I'm called when m_status is not READY: %d\n", m_status);
    }

    return ret;
}

// Remove the top activity from the stack, and send MSG_CLOSE message to
// the window of the activity, instead of destroying it directly.
bool ActivityStack::pop (DWORD res)
{
    bool ret = false;

    if (READY == m_status) {
        m_status = POP;
        ret = innerPop (res);
        m_status = READY;
    }

    return ret;
}

// Back to previous view
bool ActivityStack::back()
{
    if (NULL == top()) {
        assert (0);
        return false;
    }

    if (top()->onBack() == 0) {
        return pop();
    }
    return true;
}

// Return to home
bool ActivityStack::home()
{
    if (NULL == top()) {
        assert (0);
        return false;
    }
    if (0 == top()->onHome()) {
        switchTo("LauncherActivity");
        return true;
    }
    return false;
}

void ActivityStack::switchTo(const char* appName, Intent* intentPtr, DWORD res)
{
    Activity* next = searchActivityByName(appName);

    if (NULL == next) {
        push(appName, intentPtr);
    }
    else {
        if (READY == m_status) {
            m_status = POP;
            popTo(next, intentPtr, res);
            m_status = READY;
        }
    }
}



// private:

Activity* ActivityStack::innerPush(const char* appName, Intent* intentPtr)
{
    Activity* next = ActivityFactory::singleton()->create(appName);
    Activity* prev = top();

    if (next == NULL) {
        _ERR_PRINTF ("ActivityStack::innerPush: failed to create app: %s\n", appName);
        return NULL;
    }

    // push the new activity into stack
    m_activities.push_back(ActivityEntry(appName, next));

    _DBG_PRINTF (">>>>>>>> push activity: %s %p\n", appName, top());

    // send intend to the new activity
    SendMessage(next->hwnd(), MSG_USER_APP_DATA, (DWORD)intentPtr, 0);

    next->hide();

    next->onStart();

    if (prev) {
        _doSwitchEffect(prev, next, TRUE);

        // suspend the previous activity
        prev->onPause();
    }

    // show it up
    next->show();

    if (prev) {
        prev->hide();
    }

    SendNotifyMessage (next->hwnd(), MSG_USER_APP_READY, ACTIVITY_PUSHED, 0);
    return next;
}

bool ActivityStack::innerPop (DWORD res)
{
    Activity *prev = NULL;
    Activity *next = NULL;

    if (m_activities.empty()) {
        assert (0);
        return false;
    }

    prev = top();
    next = _underTop();

    prev->onStop();

    if (next) {
        _doSwitchEffect(prev, next, FALSE);
        next->onResume();
        next->show();
        SendNotifyMessage (next->hwnd(), MSG_USER_APP_READY, ACTIVITY_POPPED, res);
    }

    _DBG_PRINTF ("<<<<<<<< pop activity: %s %p\n", m_activities.back().first.c_str(), top());

    m_activities.pop_back();
    SendNotifyMessage (prev->hwnd(), MSG_CLOSE, 0, 0);
    return true;
}

// Do the same thing as pop(), but supply a parameter "which" to specify
// which activity you want to show up, and all activities above it will be poped.
// Notice that, if you call this function with parameter NULL, the stack will be cleared.
// return how much activities has been poped up
int ActivityStack::popTo(Activity* which, Intent* intentPtr, DWORD res)
{
    int count = 0;

    Activity *prev = top();
    Activity *next = which;

    assert (prev && next);

    if (next && next != prev) {
        prev->onStop();

        SendMessage(next->hwnd(), MSG_USER_APP_DATA, (DWORD)intentPtr, 0);

        _doSwitchEffect(prev, next, FALSE);
        next->onResume();
        next->show();
        SendNotifyMessage (next->hwnd(), MSG_USER_APP_READY, ACTIVITY_POPPED, res);
    }

    while (!m_activities.empty() && top() != which) {
        _DBG_PRINTF ("<<<<<<<< pop activity: %s %p\n", m_activities.back().first.c_str(), top());

        prev = top();
        m_activities.pop_back();
        next = top();

        SendNotifyMessage(prev->hwnd(), MSG_CLOSE, 0, 0);

        ++ count;
    }

    return count;
}


Activity* ActivityStack::_underTop() const
{
    int sz = m_activities.size();

    if (sz >= 2)
        return m_activities[m_activities.size() - 2].second;
    else
        return NULL;
}

Activity* ActivityStack::searchActivityByName(std::string appName)
{
    ActivityCollection::iterator it;
    for (it = m_activities.begin(); it != m_activities.end(); ++it) {
        if (appName == it->first) {
            return it->second;
        }
    }
    return NULL;
}

Activity* ActivityStack::getActivity (const char* appName)
{
    std::string name (appName);
    return searchActivityByName (name);
}

void ActivityStack::_doSwitchEffect(Activity* prev, Activity* next, BOOL switchTo)
{
    const char* effectorName = NULL;
    if (prev == NULL || next == NULL)
        return;

    if (! prev->needSwitchEffect() || ! next->needSwitchEffect())
        return;

    if (prev->style() == Activity::STYLE_PUSH && next->style() == Activity::STYLE_PUSH) {
        effectorName = MGEFF_MINOR_push;
    } else if (next->style() == Activity::STYLE_ZOOM){
        effectorName = MGEFF_MINOR_zoom;
    } else {
        effectorName = MGEFF_MINOR_alpha;
    }

    {
        HDC sink_dc = HDC_INVALID;
        HDC dst_hdc = HDC_INVALID;
        HDC src_hdc = HDC_INVALID;

        MGEFF_EFFECTOR effector = NULL;
        MGEFF_SOURCE source1, source2;
        MGEFF_SINK sink;
        MGEFF_ANIMATION handle;

        src_hdc = prev->snapshot();
        dst_hdc = next->snapshot();

        sink_dc = GetClientDC(prev->hwnd());

        assert(HDC_INVALID != sink_dc);

        source1 = mGEffCreateSource(src_hdc);
        source2 = mGEffCreateSource(dst_hdc);
        sink = mGEffCreateHDCSink(sink_dc);
        //sink = mGEffCreateHDCSink(HDC_SCREEN);

        effector = mGEffEffectorCreate(mGEffStr2Key(effectorName));
        assert(effector);

        mGEffEffectorAppendSource(effector, source1);
        mGEffEffectorAppendSource(effector, source2);
        mGEffSetBufferSink(sink, src_hdc);
        mGEffEffectorSetSink (effector, sink);

        if (prev->style() == Activity::STYLE_PUSH && next->style() == Activity::STYLE_PUSH) {
            LeafProperty direction = MGEFF_DIRECTION_RIGHT2LEFT;

            if (switchTo) {
                direction = MGEFF_DIRECTION_RIGHT2LEFT;
                if (next->m_pushIndex < prev->m_pushIndex)
                    direction = MGEFF_DIRECTION_LEFT2RIGHT;
            } else {
                direction = MGEFF_DIRECTION_LEFT2RIGHT;
                if (next->m_pushIndex > prev->m_pushIndex)
                    direction = MGEFF_DIRECTION_RIGHT2LEFT;
            }

            mGEffEffectorSetProperty (
                effector, (EffAnimProperty)MGEFF_PROPERTY_DIRECTION, direction);
        }
        else if (prev->style() == Activity::STYLE_ZOOM && next->style() == Activity::STYLE_ZOOM){
            if (switchTo) {
                mGEffEffectorSetProperty (effector, (EffAnimProperty)MGEFF_PROPERTY_ZOOM, MGEFF_ZOOMIN);
            }
            else {
                mGEffEffectorSetProperty (effector, (EffAnimProperty)MGEFF_PROPERTY_ZOOM, MGEFF_ZOOMOUT);
            }

     }

        handle = mGEffAnimationCreateWithEffector(effector);
        assert(handle);

        mGEffAnimationSetCurve(handle, InQuad);
        mGEffAnimationSetDuration(handle, SWITCH_EFFECT_DURATION);
        mGEffAnimationSyncRun(handle);
        mGEffAnimationDelete(handle);

        mGEffEffectorDelete(effector);
        ReleaseDC(sink_dc);
        DeleteMemDC(dst_hdc);
        DeleteMemDC(src_hdc);
    }
}

// constructor & desctructor, for internal use only
ActivityStack::ActivityStack() : m_status(ActivityStack::READY)
{
}

ActivityStack::~ActivityStack()
{
}

