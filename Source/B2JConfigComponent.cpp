#include "B2JConfigComponent.h"
#include "CommandID.h"
#include "Constants.h"
#include "GameDirectories.h"
#include "nlohmann/json.hpp"

using namespace juce;

namespace je2be::gui::b2j {

B2JConfigComponent::B2JConfigComponent(B2JChooseInputState const &chooseInputState) : fState(chooseInputState) {
  auto width = kWindowWidth;
  auto height = kWindowHeight;
  setSize(width, height);

  int y = kMargin;
  String label = (*fState.fInputState.fInputFileOrDirectory).getFullPathName();
  fFileOrDirectory.reset(new Label("", TRANS("Selected world:") + " " + label));
  fFileOrDirectory->setBounds(kMargin, kMargin, width - kMargin * 2, kButtonBaseHeight);
  fFileOrDirectory->setJustificationType(Justification::topLeft);
  addAndMakeVisible(*fFileOrDirectory);

  y += 3 * kMargin;
  fImportAccountFromLauncher.reset(new ToggleButton(TRANS("Import account information from the Minecraft Launcher, in order to correctly embed player id to converted data")));
  fImportAccountFromLauncher->setBounds(kMargin, y, width - kMargin * 2, kButtonBaseHeight);
  fImportAccountFromLauncher->setMouseCursor(MouseCursor::PointingHandCursor);
  fImportAccountFromLauncher->onClick = [this] {
    onClickImportAccountFromLauncherButton();
  };
  addAndMakeVisible(*fImportAccountFromLauncher);
  y += fImportAccountFromLauncher->getHeight();

  {
    int x = 46;
    fAccountList.reset(new ComboBox());
    fAccountList->setBounds(x, y, width - x - kMargin, kButtonBaseHeight);
    fAccountList->setEnabled(false);
    addAndMakeVisible(*fAccountList);
    y += fAccountList->getHeight();
  }

  int messageComponentY = y + kMargin;

  fStartButton.reset(new TextButton(TRANS("Start")));
  fStartButton->setBounds(width - kMargin - kButtonMinWidth, height - kMargin - kButtonBaseHeight, kButtonMinWidth, kButtonBaseHeight);
  fStartButton->setEnabled(false);
  fStartButton->onClick = [this]() { onStartButtonClicked(); };
  addAndMakeVisible(*fStartButton);

  fBackButton.reset(new TextButton(TRANS("Back")));
  fBackButton->setBounds(kMargin, height - kMargin - kButtonBaseHeight, kButtonMinWidth, kButtonBaseHeight);
  fBackButton->setMouseCursor(MouseCursor::PointingHandCursor);
  fBackButton->onClick = [this]() { onBackButtonClicked(); };
  addAndMakeVisible(*fBackButton);

  // TODO: check given file or directory
  fOk = true;

  if (fOk) {
    fMessage.reset(new Label("", ""));
  } else {
    fMessage.reset(new Label("", TRANS("There doesn't seem to be any Minecraft save data "
                                       "in the specified directory.")));
    fMessage->setColour(Label::textColourId, kErrorTextColor);
  }
  fMessage->setBounds(kMargin, messageComponentY, width - 2 * kMargin, kButtonBaseHeight);
  addAndMakeVisible(*fMessage);

  startTimer(1000);
}

B2JConfigComponent::~B2JConfigComponent() {}

void B2JConfigComponent::timerCallback() {
  stopTimer();
  fStartButton->setEnabled(fOk);
  if (fOk) {
    fStartButton->setMouseCursor(MouseCursor::PointingHandCursor);
  }
}

void B2JConfigComponent::paint(juce::Graphics &g) {}

void B2JConfigComponent::onStartButtonClicked() {
  JUCEApplication::getInstance()->invoke(gui::toB2JConvert, true);
}

void B2JConfigComponent::onBackButtonClicked() {
  JUCEApplication::getInstance()->invoke(gui::toB2JChooseInput, true);
}

class B2JConfigComponent::ImportAccountWorker::Impl {
public:
  explicit Impl(B2JConfigComponent *parent) : fParent(parent) {}

  void run() {
    try {
      unsafeRun();
    } catch (...) {
    }
    fParent->triggerAsyncUpdate();
  }

  void unsafeRun() {
    using namespace std;
    File saves = JavaSaveDirectory();
    File jsonFile = saves.getParentDirectory().getChildFile("launcher_accounts.json");
    if (!jsonFile.existsAsFile()) {
      return;
    }
    String jsonString = jsonFile.loadFileAsString();
    string jsonStdString(jsonString.toUTF8(), jsonString.getNumBytesAsUTF8());
    auto json = nlohmann::json::parse(jsonStdString);
    auto accounts = json["accounts"];
    unordered_map<string, Account> collected;
    for (auto const &it : accounts) {
      try {
        auto profile = it["minecraftProfile"];
        auto id = profile["id"].get<string>();
        auto name = profile["name"].get<string>();
        auto localId = it["localId"].get<string>();
        auto type = it["type"].get<string>();
        auto username = it["username"].get<string>();
        Uuid uuid(id);
        Account account;
        account.fName = name;
        account.fUuid = uuid;
        account.fType = type;
        account.fUsername = username;
        collected[localId] = account;
      } catch (...) {
      }
    }
    if (collected.empty()) {
      return;
    }
    if (collected.size() == 1) {
      fAccounts.push_back(collected.begin()->second);
      return;
    }
    string activeAccountLocalId;
    try {
      activeAccountLocalId = json["activeAccountLocalId"].get<string>();
    } catch (...) {
    }
    auto first = collected.find(activeAccountLocalId);
    if (first != collected.end()) {
      fAccounts.push_back(first->second);
      collected.erase(first);
    }
    for (auto const &it : collected) {
      fAccounts.push_back(it.second);
    }
  }

  B2JConfigComponent *const fParent;
  std::vector<B2JConfigComponent::Account> fAccounts;
};

B2JConfigComponent::ImportAccountWorker::ImportAccountWorker(B2JConfigComponent *parent) : Thread("je2be::gui::B2JConfigComponent::ImportAccountWorker"), fImpl(new Impl(parent)) {}

B2JConfigComponent::ImportAccountWorker::~ImportAccountWorker() {}

void B2JConfigComponent::ImportAccountWorker::run() {
  fImpl->run();
}

void B2JConfigComponent::ImportAccountWorker::copyAccounts(std::vector<B2JConfigComponent::Account> &buffer) {
  fImpl->fAccounts.swap(buffer);
}

void B2JConfigComponent::onClickImportAccountFromLauncherButton() {
  if (fImportAccountWorker) {
    return;
  }
  if (fImportAccountFromLauncher->getToggleState()) {
    if (fAccounts.empty()) {
      fImportAccountFromLauncher->setEnabled(false);
      stopTimer();
      fStartButton->setEnabled(false);
      fBackButton->setEnabled(false);
      fImportAccountWorker.reset(new ImportAccountWorker(this));
      fImportAccountWorker->startThread();
    } else {
      fAccountList->setEnabled(true);
    }
  } else {
    fAccountList->setEnabled(false);
  }
}

void B2JConfigComponent::handleAsyncUpdate() {
  if (fImportAccountWorker) {
    fImportAccountWorker->copyAccounts(fAccounts);
  }
  fImportAccountWorker.reset();
  fAccountList->clear();
  for (int i = 0; i < fAccounts.size(); i++) {
    Account account = fAccounts[i];
    fAccountList->addItem(account.toString(), i + 1);
  }
  if (fAccounts.size() >= 1) {
    fAccountList->setSelectedId(1);
  }
  fAccountList->setEnabled(true);
  fImportAccountFromLauncher->setEnabled(true);
  fStartButton->setEnabled(true);
  fBackButton->setEnabled(true);
}

} // namespace je2be::gui::b2j
