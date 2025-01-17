// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_LOCAL_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_LOCAL_COMMANDS_H_

#include "base/ios/block_types.h"

class GURL;

@protocol ClearBrowsingDataLocalCommands

// Opens URL in a new non-incognito tab and dismisses the clear browsing data
// view.
- (void)openURL:(const GURL&)URL;
// Notifies the coordinator that Clear Browsing Data should be dismissed.
- (void)dismissClearBrowsingDataWithCompletion:
    (ProceduralBlock)completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_LOCAL_COMMANDS_H_
