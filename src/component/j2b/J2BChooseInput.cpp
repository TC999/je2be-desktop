#include "component/j2b/J2BChooseInput.h"
#include "CommandID.h"
#include "Constants.h"
#include "GameDirectory.h"
#include "component/MainWindow.h"

using namespace juce;

namespace je2be::gui::component::j2b {

File J2BChooseInput::sLastDirectory;

J2BChooseInput::J2BChooseInput(std::optional<J2BChooseInputState> state) {
  if (state) {
    fState = *state;
  }

  auto width = kWindowWidth;
  auto height = kWindowHeight;

  setSize(width, height);
  {
    fMessage.reset(new Label("", TRANS("Select world to convert")));
    fMessage->setBounds(kMargin, kMargin, width - kMargin - kWorldListWidth - kMargin - kMargin, height - kMargin - kButtonBaseHeight - kMargin - kMargin);
    fMessage->setJustificationType(Justification::topLeft);
    fMessage->setMinimumHorizontalScale(1);
    addAndMakeVisible(*fMessage);
  }
  {
    fBackButton.reset(new component::TextButton(TRANS("Back")));
    fBackButton->setBounds(kMargin, height - kMargin - kButtonBaseHeight, kButtonMinWidth, kButtonBaseHeight);
    fBackButton->onClick = [this]() { onBackButtonClicked(); };
    addAndMakeVisible(*fBackButton);
  }
  {
    fNextButton.reset(new component::TextButton(TRANS("Next")));
    fNextButton->setBounds(width - kButtonMinWidth - kMargin, height - kButtonBaseHeight - kMargin, kButtonMinWidth, kButtonBaseHeight);
    fNextButton->onClick = [this]() { onNextButtonClicked(); };
    fNextButton->setEnabled(false);
    addAndMakeVisible(*fNextButton);
  }
  {
    auto w = 200;
    fChooseCustomButton.reset(new component::TextButton(TRANS("Select from other directories")));
    fChooseCustomButton->setBounds(width - kMargin - fNextButton->getWidth() - kMargin - w, height - kButtonBaseHeight - kMargin, w, kButtonBaseHeight);
    fChooseCustomButton->onClick = [this]() { onChooseCustomButtonClicked(); };
    addAndMakeVisible(*fChooseCustomButton);
  }

  Rectangle<int> listBoxBounds(width - kMargin - kWorldListWidth, kMargin, kWorldListWidth, height - 3 * kMargin - kButtonBaseHeight);
  {
    fListComponent.reset(new ListBox("", this));
    fListComponent->setBounds(listBoxBounds);
    fListComponent->setEnabled(false);
    fListComponent->setRowHeight(kWorldListRowHeight);
    addChildComponent(*fListComponent);
  }
  {
    fPlaceholder.reset(new Label({}, TRANS("Loading worlds in the save folder...")));
    fPlaceholder->setBounds(listBoxBounds);
    fPlaceholder->setColour(Label::ColourIds::backgroundColourId, fListComponent->findColour(ListBox::ColourIds::backgroundColourId));
    fPlaceholder->setJustificationType(Justification::centred);
    addAndMakeVisible(*fPlaceholder);
  }
  {
    fThread.reset(new GameDirectoryScanThreadJava(this));
    fThread->startThread();
  }
}

J2BChooseInput::~J2BChooseInput() {
  fListComponent.reset();
}

void J2BChooseInput::paint(juce::Graphics &g) {}

void J2BChooseInput::onNextButtonClicked() {
  JUCEApplication::getInstance()->invoke(gui::toJ2BConfig, true);
}

void J2BChooseInput::onChooseCustomButtonClicked() {
  if (sLastDirectory == File()) {
    sLastDirectory = GameDirectory::JavaSaveDirectory();
  }

  int flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories;
  MainWindow::sFileChooser.reset(new FileChooser(TRANS("Select save data folder of Minecraft"), sLastDirectory, {}, true));
  MainWindow::sFileChooser->launchAsync(flags, [this](FileChooser const &chooser) { onCustomDirectorySelected(chooser); });
}

void J2BChooseInput::onCustomDirectorySelected(juce::FileChooser const &chooser) {
  File result = chooser.getResult();
  if (result == File()) {
    return;
  }
  fState.fInputDirectory = result;
  sLastDirectory = result.getParentDirectory();
  JUCEApplication::getInstance()->invoke(gui::toJ2BConfig, true);
}

void J2BChooseInput::selectedRowsChanged(int lastRowSelected) {
  int num = fListComponent->getNumSelectedRows();
  if (num == 1 && 0 <= lastRowSelected && lastRowSelected < fGameDirectories.size()) {
    GameDirectory gd = fGameDirectories[lastRowSelected];
    fState.fInputDirectory = gd.fDirectory;
  } else {
    fState.fInputDirectory = std::nullopt;
  }
  if (fState.fInputDirectory != std::nullopt) {
    fNextButton->setEnabled(true);
  }
}

void J2BChooseInput::listBoxItemDoubleClicked(int row, const MouseEvent &) {
  if (row < 0 || fGameDirectories.size() <= row) {
    return;
  }
  GameDirectory gd = fGameDirectories[row];
  fState.fInputDirectory = gd.fDirectory;
  JUCEApplication::getInstance()->invoke(gui::toJ2BConfig, false);
}

void J2BChooseInput::onBackButtonClicked() {
  JUCEApplication::getInstance()->invoke(gui::toModeSelect, true);
}

int J2BChooseInput::getNumRows() {
  return fGameDirectories.size();
}

void J2BChooseInput::paintListBoxItem(int rowNumber,
                                      juce::Graphics &g,
                                      int width, int height,
                                      bool rowIsSelected) {
  if (rowNumber < 0 || fGameDirectories.size() <= rowNumber) {
    return;
  }
  GameDirectory gd = fGameDirectories[rowNumber];
  gd.paint(g, width, height, rowIsSelected, *this);
}

void J2BChooseInput::handleAsyncUpdate() {
  fGameDirectories.swap(fThread->fGameDirectories);
  if (fGameDirectories.empty()) {
    fPlaceholder->setText(TRANS("Nothing found in the save folder"), dontSendNotification);
  } else {
    fListComponent->updateContent();
    fListComponent->setEnabled(true);
    fListComponent->setVisible(true);
    fPlaceholder->setVisible(false);
  }
}

} // namespace je2be::gui::component::j2b