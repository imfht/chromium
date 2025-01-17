// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser.h"

#include <gtk/gtk.h>

#include <utility>

#include "base/bind.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/base/string_resources.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

class GtkFileChooserOnUiThread {
 public:
  GtkFileChooserOnUiThread(
      scoped_refptr<base::SequencedTaskRunner> source_task_runner,
      FileChooser::ResultCallback callback);

  ~GtkFileChooserOnUiThread();

  void Show();

 private:
  // Callback for when the user responds to the Open File dialog.
  CHROMEG_CALLBACK_1(GtkFileChooserOnUiThread,
                     void,
                     OnResponse,
                     GtkWidget*,
                     int);

  void RunCallback(FileChooser::Result file);
  void CleanUp();

  GObject* file_dialog_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> source_task_runner_;
  FileChooser::ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GtkFileChooserOnUiThread);
};

class FileChooserLinux : public FileChooser {
 public:
  FileChooserLinux(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                   ResultCallback callback);

  ~FileChooserLinux() override;

  // FileChooser implementation.
  void Show() override;

 private:
  base::SequenceBound<GtkFileChooserOnUiThread> gtk_file_chooser_;

  DISALLOW_COPY_AND_ASSIGN(FileChooserLinux);
};

GtkFileChooserOnUiThread::GtkFileChooserOnUiThread(
    scoped_refptr<base::SequencedTaskRunner> source_task_runner,
    FileChooser::ResultCallback callback)
    : source_task_runner_(std::move(source_task_runner)),
      callback_(std::move(callback)) {}

GtkFileChooserOnUiThread::~GtkFileChooserOnUiThread() {
  // Delete the dialog if it hasn't been already.
  CleanUp();
}

void GtkFileChooserOnUiThread::Show() {
#if GTK_CHECK_VERSION(3, 90, 0)
  // GTK+ 4.0 removes the stock items for the open and cancel buttons, with the
  // idea that one would instead use _("_Cancel") and _("_Open") directly (using
  // gettext to pull the appropriate translated strings from the translations
  // that ship with GTK+). To avoid needing to pull in the translated strings
  // from GTK+ using gettext, we can just use GtkFileChooserNative (available
  // since 3.20), and GTK+ will provide default, localized buttons.
  file_dialog_ = G_OBJECT(gtk_file_chooser_native_new(
      l10n_util::GetStringUTF8(IDS_DOWNLOAD_FILE_DIALOG_TITLE).c_str(), nullptr,
      GTK_FILE_CHOOSER_ACTION_OPEN, nullptr, nullptr));
#else
  // For older versions of GTK+, we can use GtkFileChooserDialog with stock
  // items for the buttons, and GTK+ will fetch the appropriate localized
  // strings for us. The stock items have been deprecated since 3.10, though, so
  // we need to suppress the warnings.
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  file_dialog_ = G_OBJECT(gtk_file_chooser_dialog_new(
      l10n_util::GetStringUTF8(IDS_DOWNLOAD_FILE_DIALOG_TITLE).c_str(), nullptr,
      GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, nullptr));
  G_GNUC_END_IGNORE_DEPRECATIONS;
#endif

  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(file_dialog_), false);
  g_signal_connect(file_dialog_, "response", G_CALLBACK(OnResponseThunk), this);

#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_native_dialog_show(GTK_NATIVE_DIALOG(file_dialog_));
#else
  gtk_widget_show_all(GTK_WIDGET(file_dialog_));
#endif
}

void GtkFileChooserOnUiThread::RunCallback(FileChooser::Result file) {
  source_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(file)));
}

void GtkFileChooserOnUiThread::CleanUp() {
#if GTK_CHECK_VERSION(3, 90, 0)
  g_clear_object(&file_dialog_);
#else
  g_clear_pointer(&file_dialog_, gtk_widget_destroy);
#endif
}

void GtkFileChooserOnUiThread::OnResponse(GtkWidget* dialog, int response_id) {
  gchar* filename = nullptr;
  if (response_id == GTK_RESPONSE_ACCEPT) {
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  }

  if (filename) {
    RunCallback(base::FilePath(filename));
    g_free(filename);
  } else {
    RunCallback(protocol::MakeFileTransferError(
        FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED));
  }
  CleanUp();
}

FileChooserLinux::FileChooserLinux(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback)
    : gtk_file_chooser_(ui_task_runner,
                        base::SequencedTaskRunnerHandle::Get(),
                        std::move(callback)) {}

void FileChooserLinux::Show() {
  gtk_file_chooser_.Post(FROM_HERE, &GtkFileChooserOnUiThread::Show);
}

FileChooserLinux::~FileChooserLinux() = default;

}  // namespace

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FileChooserLinux>(std::move(ui_task_runner),
                                            std::move(callback));
}

}  // namespace remoting
