#pragma once

#include "ComponentState.h"
#include <optional>

namespace je2be::gui {

class ChooseInputComponent : public juce::Component,
                             public J2BChooseInputStateProvider,
                             public juce::FileBrowserListener,
                             public juce::ChangeListener {
public:
  explicit ChooseInputComponent(std::optional<J2BChooseInputState> state);
  ~ChooseInputComponent() override;

  void paint(juce::Graphics &) override;

  J2BChooseInputState getChooseInputState() const override {
    return fState;
  }

  void selectionChanged() override;
  void fileClicked(const juce::File &file, const juce::MouseEvent &e) override;
  void fileDoubleClicked(const juce::File &file) override;
  void browserRootChanged(const juce::File &newRoot) override;

  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  void onNextButtonClicked();
  void onChooseCustomButtonClicked();
  void onAboutButtonClicked();

  void onCustomDirectorySelected(juce::FileChooser const &chooser);

private:
  static juce::File sLastDirectory;

  std::unique_ptr<juce::TextButton> fNextButton;
  std::unique_ptr<juce::TextButton> fChooseCustomButton;
  std::unique_ptr<juce::FileListComponent> fListComponent;
  std::unique_ptr<juce::DirectoryContentsList> fList;
  juce::TimeSliceThread fListThread;
  J2BChooseInputState fState;
  std::unique_ptr<juce::Label> fMessage;
  std::optional<juce::File> fInitialSelection;
  std::unique_ptr<juce::TextButton> fAboutButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChooseInputComponent)
};

} // namespace je2be::gui
