// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_UTIL_H_
#define ASH_APP_LIST_APP_LIST_UTIL_H_

#include "ash/app_list/app_list_export.h"
#include "ui/events/event.h"

namespace views {
class Textfield;
}

namespace app_list {

// Returns true if the key event is an unhandled left or right arrow (unmodified
// by ctrl, shift, or alt)
APP_LIST_EXPORT bool IsUnhandledLeftRightKeyEvent(const ui::KeyEvent& event);

// Returns true if the key event is an unhandled up or down arrow (unmodified by
// ctrl, shift, or alt)
APP_LIST_EXPORT bool IsUnhandledUpDownKeyEvent(const ui::KeyEvent& event);

// Returns true if the key event is an unhandled arrow key event of any type
// (unmodified by ctrl, shift, or alt)
APP_LIST_EXPORT bool IsUnhandledArrowKeyEvent(const ui::KeyEvent& event);

// Processes left/right key traversal for the given Textfield. Returns true
// if focus is moved.
APP_LIST_EXPORT bool ProcessLeftRightKeyTraversalForTextfield(
    views::Textfield* textfield,
    const ui::KeyEvent& key_event);

}  // namespace app_list

#endif  // ASH_APP_LIST_APP_LIST_UTIL_H_
