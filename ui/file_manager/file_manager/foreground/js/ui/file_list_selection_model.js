// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class FileListSelectionModel extends cr.ui.ListSelectionModel {
  /** @param {number=} opt_length The number items in the selection. */
  constructor(opt_length) {
    super(opt_length);

    /** @private {boolean} */
    this.isCheckSelectMode_ = false;

    this.addEventListener('change', this.onChangeEvent_.bind(this));
  }

  /**
   * Updates the check-select mode.
   * @param {boolean} enabled True if check-select mode should be enabled.
   */
  setCheckSelectMode(enabled) {
    this.isCheckSelectMode_ = enabled;
  }

  /**
   * Gets the check-select mode.
   * @return {boolean} True if check-select mode is enabled.
   */
  getCheckSelectMode() {
    return this.isCheckSelectMode_;
  }

  /**
   * @override
   * Changes to single-select mode if all selected files get deleted.
   */
  adjustToReordering(permutation) {
    // Look at the old state.
    var oldSelectedItemsCount = this.selectedIndexes.length;
    var oldLeadIndex = this.leadIndex;
    var newSelectedItemsCount =
        this.selectedIndexes.filter(i => permutation[i] != -1).length;
    // Call the superclass function.
    super.adjustToReordering(permutation);
    // Leave check-select mode if all items have been deleted.
    if (oldSelectedItemsCount && !newSelectedItemsCount && this.length_ &&
        oldLeadIndex != -1) {
      this.isCheckSelectMode_ = false;
    }
  }

  /**
   * Handles change event to update isCheckSelectMode_ BEFORE the change event
   * is dispatched to other listeners.
   * @param {!Event} event Event object of 'change' event.
   * @private
   */
  onChangeEvent_(event) {
    // When the number of selected item is not one, update che check-select
    // mode. When the number of selected item is one, the mode depends on the
    // last keyboard/mouse operation. In this case, the mode is controlled from
    // outside. See filelist.handlePointerDownUp and filelist.handleKeyDown.
    var selectedIndexes = this.selectedIndexes;
    if (selectedIndexes.length === 0) {
      this.isCheckSelectMode_ = false;
    } else if (selectedIndexes.length >= 2) {
      this.isCheckSelectMode_ = true;
    }
  }
}

class FileListSingleSelectionModel extends cr.ui.ListSingleSelectionModel {
  /**
   * Updates the check-select mode.
   * @param {boolean} enabled True if check-select mode should be enabled.
   */
  setCheckSelectMode(enabled) {
    // Do nothing, as check-select mode is invalid in single selection model.
  }

  /**
   * Gets the check-select mode.
   * @return {boolean} True if check-select mode is enabled.
   */
  getCheckSelectMode() {
    return false;
  }
}
