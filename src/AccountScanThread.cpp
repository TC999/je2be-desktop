#include <nlohmann/json.hpp>

#include "AccountScanThread.h"
#include "GameDirectory.h"

using namespace juce;

namespace je2be::gui {

class AccountScanThread::Impl {
public:
  explicit Impl(AsyncUpdater *parent) : fParent(parent) {}

  void run() {
    try {
      unsafeRun();
    } catch (...) {
    }
    fParent->triggerAsyncUpdate();
  }

  void unsafeRun() {
    using namespace std;
    File saves = GameDirectory::JavaSaveDirectory();
    File jsonFile = saves.getParentDirectory().getChildFile("launcher_accounts.json");
    if (!jsonFile.existsAsFile()) {
      return;
    }
    juce::String jsonString = jsonFile.loadFileAsString();
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
        juce::Uuid uuid(id);
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
    std::optional<Account> preferred;
    if (first != collected.end()) {
      fAccounts.push_back(first->second);
      preferred = first->second;
      collected.erase(first);
    }
    HashMap<juce::Uuid, Account> accountById;
    if (preferred) {
      accountById[preferred->fUuid] = *preferred;
    }
    for (auto const &it : collected) {
      if (accountById.contains(it.second.fUuid)) {
        if (it.second.fType == "Xbox") {
          accountById[it.second.fUuid] = it.second;
        }
      } else {
        accountById[it.second.fUuid] = it.second;
      }
    }
    if (preferred) {
      accountById.remove(preferred->fUuid);
    }
    for (auto const &it : accountById) {
      fAccounts.push_back(it);
    }
  }

  AsyncUpdater *const fParent;
  std::vector<Account> fAccounts;
};

AccountScanThread::AccountScanThread(AsyncUpdater *parent) : Thread("je2be::gui::AccountScanThread"), fImpl(new Impl(parent)) {}

AccountScanThread::~AccountScanThread() {}

void AccountScanThread::run() {
  fImpl->run();
}

void AccountScanThread::copyAccounts(std::vector<Account> &buffer) {
  fImpl->fAccounts.swap(buffer);
}

} // namespace je2be::gui
