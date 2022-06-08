#include "component/CopyJavaArtifactProgress.h"
#include "CommandID.h"
#include "ComponentState.h"
#include "Constants.h"
#include "TaskbarProgress.h"
#include "TemporaryDirectory.h"

using namespace juce;

namespace je2be::desktop::component {

class CopyJavaArtifactThread : public CopyJavaArtifactProgress::Worker {
public:
  CopyJavaArtifactThread(AsyncUpdater *updater, File from, File to, double *progress)
      : CopyJavaArtifactProgress::Worker("je2be::desktop::component::CopyJavaArtifactThread"),
        fUpdater(updater), fFrom(from), fTo(to), fProgress(progress) {}

  void run() override {
    try {
      unsafeRun();
    } catch (...) {
      fResult = CopyJavaArtifactProgress::Worker::Result::Failed;
    }
    fUpdater->triggerAsyncUpdate();
  }

  std::optional<CopyJavaArtifactProgress::Worker::Result> result() const override {
    return fResult;
  }

private:
  void unsafeRun() {
    RangedDirectoryIterator it(fFrom, true);
    for (auto const &item : it) {
      File const &from = item.getFile();
      if (from.isDirectory()) {
        continue;
      }
      auto relative = from.getRelativePathFrom(fFrom);
      auto destination = fTo.getChildFile(relative);
      auto dir = destination.getParentDirectory();
      if (!dir.exists()) {
        auto result = dir.createDirectory();
        if (!result.wasOk()) {
          fResult = CopyJavaArtifactProgress::Worker::Result::Failed;
          return;
        }
      }
      if (!from.copyFileTo(destination)) {
        fResult = CopyJavaArtifactProgress::Worker::Result::Failed;
        return;
      }
      *fProgress = (double)item.getEstimatedProgress();
    }
    fResult = CopyJavaArtifactProgress::Worker::Result::Success;
  }

private:
  AsyncUpdater *const fUpdater;
  File fFrom;
  File fTo;
  std::optional<CopyJavaArtifactProgress::Worker::Result> fResult;
  double *const fProgress;
};

CopyJavaArtifactProgress::CopyJavaArtifactProgress(JavaOutputChoosenState const &state) : fState(state) {
  auto width = kWindowWidth;
  auto height = kWindowHeight;
  setSize(width, height);

  fLabel.reset(new Label("", TRANS("Saving...")));
  fLabel->setBounds(kMargin, kMargin, width - 2 * kMargin, kButtonBaseHeight);
  fLabel->setJustificationType(Justification::topLeft);
  addAndMakeVisible(*fLabel);

  fProgressBar.reset(new ProgressBar(fProgress));
  fProgressBar->setBounds(kMargin, kMargin + kButtonBaseHeight + kMargin, width - 2 * kMargin, kButtonBaseHeight);
  addAndMakeVisible(*fProgressBar);

  fTaskbarProgress.reset(new TaskbarProgress());

  fCopyThread.reset(new CopyJavaArtifactThread(this, state.fConvertedState.fOutputDirectory, *state.fCopyDestination, &fProgress));
  fCopyThread->startThread();
  fTaskbarProgress->setState(TaskbarProgress::State::Normal);
  startTimerHz(12);
}

CopyJavaArtifactProgress::~CopyJavaArtifactProgress() {
  fTaskbarProgress->setState(TaskbarProgress::State::NoProgress);
}

void CopyJavaArtifactProgress::paint(juce::Graphics &g) {}

void CopyJavaArtifactProgress::handleAsyncUpdate() {
  struct InvokeToChooseOutput : public ModalComponentManager::Callback {
    void modalStateFinished(int returnValue) override {
      JUCEApplication::getInstance()->invoke(commands::toChooseJavaOutput, true);
    }
  };

  struct InvokeToModeSelect : public ModalComponentManager::Callback {
    void modalStateFinished(int returnValue) override {
      JUCEApplication::getInstance()->invoke(commands::toModeSelect, true);
    }
  };

  stopTimer();

  auto result = fCopyThread->result();
  if (!result || *result == CopyJavaArtifactProgress::Worker::Result::Failed) {
    fTaskbarProgress->setState(TaskbarProgress::State::Error);
    NativeMessageBox::showMessageBoxAsync(AlertWindow::AlertIconType::WarningIcon, TRANS("Failed"), TRANS("Saving failed."), nullptr, new InvokeToChooseOutput);
  } else if (*result == CopyJavaArtifactProgress::Worker::Result::Cancelled) {
    fTaskbarProgress->setState(TaskbarProgress::State::NoProgress);
    NativeMessageBox::showMessageBoxAsync(AlertWindow::AlertIconType::InfoIcon, TRANS("Cancelled"), TRANS("Saving cancelled."), nullptr, new InvokeToChooseOutput);
    JUCEApplication::getInstance()->invoke(commands::toChooseJavaOutput, true);
  } else {
    fTaskbarProgress->setState(TaskbarProgress::State::NoProgress);
    NativeMessageBox::showMessageBoxAsync(AlertWindow::AlertIconType::InfoIcon, TRANS("Completed"), TRANS("Saving completed.") + "\n" + fState.fCopyDestination->getFullPathName(), nullptr, new InvokeToModeSelect);
    if (fState.fConvertedState.fOutputDirectory.exists()) {
      TemporaryDirectory::QueueDeletingDirectory(fState.fConvertedState.fOutputDirectory);
    }
  }
}

void CopyJavaArtifactProgress::timerCallback() {
  double progress = fProgress;
  fTaskbarProgress->update(progress);
}

} // namespace je2be::desktop::component