// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_dialog_test', function() {
  /** @enum {string} */
  const TestNames = {
    PrinterList: 'PrinterList',
    ShowProvisionalDialog: 'ShowProvisionalDialog',
    ReloadPrinterList: 'ReloadPrinterList',
  };

  const suiteName = 'DestinationDialogTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationDialogElement} */
    let dialog = null;

    /** @type {?print_preview.DestinationStore} */
    let destinationStore = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @type {!Array<!print_preview.Destination>} */
    let destinations = [];

    /** @type {!Array<!print_preview.LocalDestinationInfo>} */
    let localDestinations = [];

    /** @type {!Array<!print_preview.RecentDestination>} */
    let recentDestinations = [];

    /** @override */
    suiteSetup(function() {
      print_preview_test_utils.setupTestListenerElement();
    });

    /** @override */
    setup(function() {
      // Create data classes
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      destinationStore = print_preview_test_utils.createDestinationStore();
      destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
      recentDestinations =
          [print_preview.makeRecentDestination(destinations[4])];
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          recentDestinations /* recentDestinations */);
      nativeLayer.setLocalDestinations(localDestinations);

      // Set up dialog
      dialog = document.createElement('print-preview-destination-dialog');
      dialog.activeUser = '';
      dialog.users = [];
      dialog.destinationStore = destinationStore;
      dialog.invitationStore = new print_preview.InvitationStore();
      document.body.appendChild(dialog);
      return nativeLayer.whenCalled('getPrinterCapabilities')
          .then(function() {
            destinationStore.startLoadAllDestinations();
            dialog.show();
            return nativeLayer.whenCalled('getPrinters');
          })
          .then(function() {
            Polymer.dom.flush();
          });
    });

    // Test that destinations are correctly displayed in the lists.
    test(assert(TestNames.PrinterList), function() {
      const list = dialog.$$('print-preview-destination-list');

      const printerItems = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item');

      const getDisplayedName = item => item.$$('.name').textContent;
      // 5 printers + Save as PDF
      assertEquals(6, printerItems.length);
      // Save as PDF shows up first.
      assertEquals(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          getDisplayedName(printerItems[0]));
      assertEquals(
          'rgb(32, 33, 36)',
          window.getComputedStyle(printerItems[0].$$('.name')).color);
      // FooName will be second since it was updated by the capabilities fetch.
      assertEquals('FooName', getDisplayedName(printerItems[1]));
      Array.from(printerItems).slice(2).forEach((item, index) => {
        assertEquals(destinations[index].displayName, getDisplayedName(item));
      });
    });

    // Test that clicking a provisional destination shows the provisional
    // destinations dialog, and that the escape key closes only the provisional
    // dialog when it is open, not the destinations dialog.
    test(assert(TestNames.ShowProvisionalDialog), function() {
      const provisionalDialog =
          dialog.$$('print-preview-provisional-destination-resolver');
      const provisionalDestination = {
        extensionId: 'ABC123',
        extensionName: 'ABC Printing',
        id: 'XYZDevice',
        name: 'XYZ',
        provisional: true,
      };

      // Set the extension destinations and force the destination store to
      // reload printers.
      nativeLayer.setExtensionDestinations([provisionalDestination]);
      destinationStore.onDestinationsReload();

      return nativeLayer.whenCalled('getPrinters')
          .then(function() {
            Polymer.dom.flush();
            assertFalse(provisionalDialog.$.dialog.open);
            const list = dialog.$$('print-preview-destination-list');
            const printerItems = list.shadowRoot.querySelectorAll(
                'print-preview-destination-list-item');

            // Should have 5 local destinations, Save as PDF + extension
            // destination.
            assertEquals(7, printerItems.length);
            const provisionalItem =
                Array.from(printerItems).find(printerItem => {
                  return printerItem.destination.id ===
                      provisionalDestination.id;
                });

            // Click the provisional destination to select it.
            provisionalItem.click();
            Polymer.dom.flush();
            assertTrue(provisionalDialog.$.dialog.open);

            // Send escape key on provisionalDialog. Destinations dialog should
            // not close.
            const whenClosed =
                test_util.eventToPromise('close', provisionalDialog);
            MockInteractions.keyEventOn(
                provisionalDialog, 'keydown', 19, [], 'Escape');
            Polymer.dom.flush();
            return whenClosed;
          })
          .then(function() {
            assertFalse(provisionalDialog.$.dialog.open);
            assertTrue(dialog.$.dialog.open);
          });
    });

    // Test that destinations are correctly cleared when the destination store
    // reloads the printer list.
    test(assert(TestNames.ReloadPrinterList), function() {
      const list = dialog.$$('print-preview-destination-list');

      const printerItems = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item');

      assertEquals(6, printerItems.length);
      const oldPdfDestination = printerItems[0].destination;
      assertEquals(
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
          oldPdfDestination.id);
      const whenReset = test_util.eventToPromise(
          print_preview.DestinationStore.EventType.DESTINATIONS_RESET,
          destinationStore);
      cr.webUIListenerCallback('reload-printer-list');
      return whenReset.then(() => {
        Polymer.dom.flush();
        const printerItems = dialog.$.printList.shadowRoot.querySelectorAll(
            'print-preview-destination-list-item');
        assertEquals(6, printerItems.length);
        const newPdfDestination = printerItems[0].destination;
        assertEquals(
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
            newPdfDestination.id);
        assertNotEquals(oldPdfDestination, newPdfDestination);
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
