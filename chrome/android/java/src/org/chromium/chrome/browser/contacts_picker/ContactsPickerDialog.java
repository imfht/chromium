// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.support.v7.app.AlertDialog;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.ui.ContactsPickerListener;

/**
 * UI for the contacts picker that shows on the Android platform as a result of
 * &lt;input type=file accept=contacts &gt; form element.
 */
public class ContactsPickerDialog extends AlertDialog {
    // The category we're showing contacts for.
    private PickerCategoryView mCategoryView;

    /**
     * The ContactsPickerDialog constructor.
     * @param context The context to use.
     * @param listener The listener object that gets notified when an action is taken.
     * @param allowMultiple Whether the contacts picker should allow multiple items to be selected.
     * @param includeNames Whether the contacts data returned includes names.
     * @param includeEmails Whether the contacts data returned includes emails.
     * @param includeTel Whether the contacts data returned includes telephone numbers.
     */
    public ContactsPickerDialog(Context context, ContactsPickerListener listener,
            boolean allowMultiple, boolean includeNames, boolean includeEmails,
            boolean includeTel) {
        super(context, R.style.FullscreenWhite);

        // Initialize the main content view.
        mCategoryView = new PickerCategoryView(
                context, allowMultiple, includeNames, includeEmails, includeTel);
        mCategoryView.initialize(this, listener);
        setView(mCategoryView);
    }

    @VisibleForTesting
    public PickerCategoryView getCategoryViewForTesting() {
        return mCategoryView;
    }
}
