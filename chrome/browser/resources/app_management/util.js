// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the App Management page.
 */

cr.define('app_management.util', function() {
  /**
   * @return {!AppManagementPageState}
   */
  function createEmptyState() {
    return {
      apps: {},
      currentPage: {
        pageType: PageType.MAIN,
        selectedAppId: null,
      }
    };
  }

  /**
   * @param {!Array<App>} apps
   * @return {!AppManagementPageState}
   */
  function createInitialState(apps) {
    const initialState = createEmptyState();

    for (const app of apps) {
      initialState.apps[app.id] = app;
    }

    return initialState;
  }

  /**
   * @param {number} permissionId
   * @param {!PermissionValueType} valueType
   * @param {number} value
   * @return {!Permission}
   */
  function createPermission(permissionId, valueType, value) {
    return {
      permissionId: permissionId,
      valueType: valueType,
      value: value,
    };
  }

  /**
   * @param {App} app
   * @return {string}
   */
  function getAppIcon(app) {
    return `chrome://app-icon/${app.id}/128`;
  }

  /**
   * If there is no selected app, returns undefined so that Polymer bindings
   * won't be updated.
   * @param {AppManagementPageState} state
   * @return {App|undefined}
   */
  function getSelectedApp(state) {
    const selectedAppId = state.currentPage.selectedAppId;
    if (selectedAppId) {
      return state.apps[selectedAppId];
    }
  }

  return {
    createEmptyState: createEmptyState,
    createInitialState: createInitialState,
    createPermission: createPermission,
    getAppIcon: getAppIcon,
    getSelectedApp: getSelectedApp,
  };
});
